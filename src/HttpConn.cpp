#include "../include/HttpConn.h"
#include <unistd.h>
#include <iostream>
#include <cerrno>
#include <cstring>

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

// 🌟 绝招 3：业务中枢 (这里先写一个超级简化的状态机模型)
void HttpConn::process() {
    // 1. 理论上这里应该调用你的 HttpParser 状态机，去解析 readBuffer_
    std::cout << "[HttpConn] 解析请求:\n" << readBuffer_ << std::endl;

    // 2. 清空读缓冲 (实际项目中，如果发生半包，这里不能全清空，要根据状态机的指示来)
    readBuffer_.clear(); 

    // 3. 生成 HTTP 响应报文，存入写缓冲区
    const char* body = "<h1>Hello from Encapsulated HttpConn!</h1>";
    char response[512];
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n"
             "Connection: keep-alive\r\n"
             "\r\n"
             "%s", strlen(body), body);

    writeBuffer_ = response;
    
    // 注意：走到这里，数据并没有真正发送出去！
    // 它只是躺在 writeBuffer_ 里。发送的动作交给了外面的 write() 函数。
}