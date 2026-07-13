#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <string>
#include <unordered_map>

// 🌟 主状态机的三种状态
enum class ParseState {
    REQUEST_LINE, // 正在解析请求行
    HEADERS,      // 正在解析请求头
    BODY,         // 正在解析请求体
    FINISH        // 解析完成
};

class HttpConn {
public:
    HttpConn();
    ~HttpConn();

    void init(int fd);
    void closeConn();

    bool read();
    bool write();
    void process();

    int getFd() const;

private:
    // 🌟 状态机核心解析函数
    bool parseRequest();
    bool parseRequestLine(const std::string& line);
    void parseHeader(const std::string& line);
    void makeResponse(); // 专门用来生成响应

    int fd_;
    bool isClose_;

    std::string readBuffer_;
    std::string writeBuffer_;

    // 🌟 状态机专属变量
    ParseState state_;                             // 当前所处的状态
    std::string method_;                           // 请求方法 (GET, POST)
    std::string path_;                             // 请求路径 (/test, /index.html)
    std::string version_;                          // HTTP 版本 (HTTP/1.1)
    std::unordered_map<std::string, std::string> headers_; // 存储所有的请求头
};

#endif // HTTP_CONN_H