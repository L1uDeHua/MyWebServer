#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "Epoller.h"
#include "ThreadPool.h"
#include "HttpConn.h"
#include <unordered_map>
#include <memory>

class WebServer {
public:
    WebServer(int port, int threadNum);
    ~WebServer();

    void start(); // 启动服务器的核心循环

private:
    void initSocket();             // 初始化监听 Socket
    void handleNewConn();          // 处理新客户端连接
    void handleRead(int clientFd); // 处理客户端发来数据
    void handleWrite(int clientFd);// 处理向客户端发送数据

    int port_;
    int listenFd_;
    
    std::unique_ptr<Epoller> epoller_;       // 封装好的 epoll
    std::unique_ptr<ThreadPool> threadPool_; // 线程池
    std::unordered_map<int, HttpConn> users_;// fd 到 HttpConn 的映射表
};

#endif // WEBSERVER_H