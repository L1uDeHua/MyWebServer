#include "../include/HttpConn.h"
#include <unistd.h>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>

HttpConn::HttpConn() : fd_(-1), isClose_(true) {}

HttpConn::~HttpConn() {
    closeConn();
}

// 初始化连接
void HttpConn::init(int fd) {
    fd_ = fd;
    isClose_ = false;
    readBuffer_.clear();
    writeBuffer_.clear();
    
    // 初始化状态机
    state_ = ParseState::REQUEST_LINE;
    headers_.clear();
}

// 关闭连接
void HttpConn::closeConn() {
    if (!isClose_) {
        isClose_ = true;
        close(fd_);
        fd_ = -1;
    }
}

int HttpConn::getFd() const {
    return fd_;
}

// 🌟 绝招 1：ET 模式下的疯狂读取
bool HttpConn::read() {
    if (isClose_) return false;

    // 必须用 while 循环，直到返回 EAGAIN
    while (true) {
        char buf[2048] = {0};
        ssize_t bytes_read = recv(fd_, buf, sizeof(buf) - 1, 0);

        if (bytes_read > 0) {
            // 读到数据，追加到我们自己的专属缓冲区里！
            readBuffer_ += buf;
        } 
        else if (bytes_read == -1) {
            // 掏空了！读取成功结束
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            // 真出错了
            return false; 
        } 
        else if (bytes_read == 0) {
            // 客户端断开连接
            return false;
        }
    }
    return true; // 数据全部安全存入 readBuffer_
}

// 🌟 绝招 2：ET 模式下的疯狂写入
bool HttpConn::write() {
    if (isClose_) return false;
    if (writeBuffer_.empty()) return true;

    while (true) {
        // 尝试把 writeBuffer_ 里的数据全发出去
        ssize_t bytes_write = send(fd_, writeBuffer_.c_str(), writeBuffer_.size(), 0);

        if (bytes_write > 0) {
            // 发送成功了 bytes_write 字节，把它从缓冲区里删掉
            writeBuffer_.erase(0, bytes_write);
            
            // 如果缓冲区空了，说明全发完了！
            if (writeBuffer_.empty()) {
                return true; 
            }
        } 
        else if (bytes_write == -1) {
            // 内核的发送缓冲区满了！(EAGAIN)
            // 这时候不能再发了，必须等下一次 epoll 唤醒 EPOLLOUT 事件
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 返回 true 表示并不是出错了，只是暂时发不进去了，保留剩下的 writeBuffer_ 等下次发
                return true; 
            }
            return false; // 真出错了
        }
    }
}

//  业务中枢 
void HttpConn::process() {
    // 只有当状态机完整解析完一个 HTTP 请求时，才去生成响应
    if (parseRequest()) {
        makeResponse();
        
        // 响应存入写缓冲后，把状态机重置，准备迎接下一个请求（Keep-Alive 长连接支持！）
        state_ = ParseState::REQUEST_LINE;
        headers_.clear();
    }
}

//  主从状态机引擎
bool HttpConn::parseRequest() {
    // 主循环：只要没解析完，就一直解析
    while (state_ != ParseState::FINISH) {
        
        // --- 从状态机：试图从 readBuffer_ 中切出一行 (\r\n) ---
        size_t lineEnd = readBuffer_.find("\r\n");
        
        if (lineEnd == std::string::npos) {
            // 没找到 \r\n，说明数据不够（半包），立刻退出，等下次 epoll 唤醒再接着解
            return false; 
        }

        // 成功切出一行数据
        std::string line = readBuffer_.substr(0, lineEnd);
        // 把这一行连同 \r\n 从读缓冲区里抹掉
        readBuffer_.erase(0, lineEnd + 2); 

        // --- 主状态机：根据当前状态，处理这一行 ---
        switch (state_) {
            case ParseState::REQUEST_LINE: {
                if (!parseRequestLine(line)) return false;
                state_ = ParseState::HEADERS; // 解析成功，状态转移到 HEADERS
                break;
            }
            case ParseState::HEADERS: {
                if (line.empty()) {
                    // 遇到空行！说明 Header 彻底结束了！
                    std::cout << "[FSM] Headers parsed completely!\n";
                    // 如果是 GET 请求，到空行就结束了。如果是 POST，还要去解析 BODY
                    if (method_ == "POST") {
                        state_ = ParseState::BODY;
                    } else {
                        state_ = ParseState::FINISH;
                    }
                } else {
                    parseHeader(line); // 还没遇到空行，继续提取 Header
                }
                break;
            }
            case ParseState::BODY: {
                // 这里暂时略过 POST Body 的处理，直接完成
                state_ = ParseState::FINISH;
                break;
            }
            default:
                break;
        }
    }
    return true; // 走到这里说明状态是 FINISH，解析大功告成！
}

// 解析请求行 (例如: "GET /index.html HTTP/1.1")
bool HttpConn::parseRequestLine(const std::string& line) {
    size_t pos1 = line.find(' ');
    size_t pos2 = line.find(' ', pos1 + 1);
    if (pos1 == std::string::npos || pos2 == std::string::npos) {
        return false; // 格式不对
    }
    method_ = line.substr(0, pos1);
    path_ = line.substr(pos1 + 1, pos2 - pos1 - 1);
    version_ = line.substr(pos2 + 1);
    
    std::cout << "[FSM] Method: " << method_ << ", Path: " << path_ << ", Version: " << version_ << std::endl;
    return true;
}

//  解析请求头 (例如: "Host: 127.0.0.1:8080")
void HttpConn::parseHeader(const std::string& line) {
    size_t pos = line.find(':');
    if (pos != std::string::npos) {
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 2); // 跳过冒号和后面的空格
        headers_[key] = value;
    }
}

//  根据状态机提取的 path_ 进行精准路由
void HttpConn::makeResponse() {
    std::string responseBody;
    std::string statusLine;

    // 精准路由判定
    if (path_ == "/") {
        statusLine = "HTTP/1.1 200 OK\r\n";
        responseBody = "<h1>Welcome to Epoll Web Server!</h1><p>Status Machine works!</p>";
    } else if (path_ == "/api") {
        statusLine = "HTTP/1.1 200 OK\r\n";
        // 我们可以把状态机解析出来的 User-Agent 打印到网页上！
        responseBody = "{\"message\": \"API OK\", \"your_browser\": \"" + headers_["User-Agent"] + "\"}";
    } else {
        statusLine = "HTTP/1.1 404 Not Found\r\n";
        responseBody = "<h1>404 Error: Page Not Found</h1>";
    }

    char response[1024];
    snprintf(response, sizeof(response),
             "%s"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "Connection: keep-alive\r\n"
             "\r\n"
             "%s", 
             statusLine.c_str(), responseBody.size(), responseBody.c_str());

    writeBuffer_ = response;
}