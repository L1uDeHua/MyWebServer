#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <string>
#include <arpa/inet.h>

class HttpConn {
public:
    HttpConn();
    ~HttpConn();

    // 初始化这个连接
    void init(int fd);
    
    // 关闭连接
    void closeConn();

    // 🌟 核心接口：被线程池调用的三个方法
    bool read();      // 一次性把内核缓冲区的数据全读进 readBuffer_
    bool write();     // 一次性把 writeBuffer_ 的数据全写进内核缓冲区
    void process();   // 处理业务逻辑：解析 readBuffer_ -> 生成响应存入 writeBuffer_

    // 获取当前连接的 fd
    int getFd() const;

private:
    int fd_;                  // 这个连接对应的 Socket fd
    bool isClose_;            // 标记这个连接是否已经关闭

    std::string readBuffer_;  // 专属读缓冲区：存浏览器发来的数据
    std::string writeBuffer_; // 专属写缓冲区：存我们要发给浏览器的数据

    // TODO: 这里未来可以加入你写的状态机变量，比如 CheckState checkState_;
};

#endif // HTTP_CONN_H