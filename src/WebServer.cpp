#include "../include/WebServer.h"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

// 工具函数：设置非阻塞
void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

WebServer::WebServer(int port, int threadNum) 
    : port_(port), 
      epoller_(new Epoller()), 
      threadPool_(new ThreadPool(threadNum)),
      timer_(new HeapTimer()),      // 初始化定时器
      timeoutMS_(30000) {           // 设定 30 秒超时
    initSocket();
}

WebServer::~WebServer() {
    close(listenFd_);
}

void WebServer::initSocket() {
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listenFd_, (struct sockaddr*)&addr, sizeof(addr));
    listen(listenFd_, 128);

    // 把监听 fd 加进 epoll，使用普通的 LT 模式
    epoller_->addFd(listenFd_, EPOLLIN);
    std::cout << "WebServer initialized on port " << port_ << std::endl;
}

void WebServer::start() {
    std::cout << "WebServer is starting..." << std::endl;
    // Boss 线程的终极死循环
    while (true) {
        int timeMS = timer_->getNextTick(); 
        
        // 如果 5000 毫秒内没有任何客户发数据，epoll_wait 就会按时醒来
        int eventCnt = epoller_->wait(timeMS);
        
        for (int i = 0; i < eventCnt; ++i) {
            int fd = epoller_->getEventFd(i);
            uint32_t events = epoller_->getEvents(i);

            if (fd == listenFd_) {
                handleNewConn();
            } else if (events & EPOLLIN) {
                //  客户发来数据了！赶紧给它续命！
                timer_->add(fd, timeoutMS_, std::bind(&WebServer::closeConn, this, fd));
                handleRead(fd);
            } else if (events & EPOLLOUT) {
                //  客户能接收数据了！也续命！
                timer_->add(fd, timeoutMS_, std::bind(&WebServer::closeConn, this, fd));
                handleWrite(fd);
            }
        }
    }
}

void WebServer::handleNewConn() {
    struct sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);
    int clientFd = accept(listenFd_, (struct sockaddr*)&clientAddr, &len);
    
    setNonBlocking(clientFd);
    
    // 初始化这个新客户专属的 HttpConn 对象
    users_[clientFd].init(clientFd);
    
    // 加进 epoll，监听可读事件，必须加上 ET 和 ONESHOT！
    epoller_->addFd(clientFd, EPOLLIN | EPOLLET | EPOLLONESHOT);
    timer_->add(clientFd, timeoutMS_, std::bind(&WebServer::closeConn, this, clientFd));
    std::cout << "[Boss] New Client Fd: " << clientFd << std::endl;
}

void WebServer::handleRead(int clientFd) {
    // 老板把读取和解析的任务，打包扔进线程池！
    threadPool_->enqueue([this, clientFd]() {
        if (users_[clientFd].read()) {
            // 读成功了，让 HttpConn 去解析业务并生成响应
            users_[clientFd].process();
            
            // 🌟 核心状态转移：业务处理完了，数据躺在 writeBuffer 里
            // 所以现在必须修改铃铛：当网卡可以发送数据时(EPOLLOUT)，叫醒我！
            epoller_->modFd(clientFd, EPOLLOUT | EPOLLET | EPOLLONESHOT);
        } else {
            // 读失败或对方断开，清理现场
            epoller_->delFd(clientFd);
            users_[clientFd].closeConn();
        }
    });
}

void WebServer::handleWrite(int clientFd) {
    // 老板把发送数据的任务，打包扔进线程池！
    threadPool_->enqueue([this, clientFd]() {
        if (users_[clientFd].write()) {
            // 写成功了！响应发完了！
            // 恢复监听可读事件，等待客户的下一次 HTTP 请求（Keep-Alive）
            epoller_->modFd(clientFd, EPOLLIN | EPOLLET | EPOLLONESHOT);
        } else {
            // 发送失败，清理现场
            epoller_->delFd(clientFd);
            users_[clientFd].closeConn();
        }
    });
}

void WebServer::closeConn(int clientFd) {
    // 只有还在哈希表里才清理，防止重复清理
    if(users_.count(clientFd)) {
        epoller_->delFd(clientFd);
        users_[clientFd].closeConn();
        users_.erase(clientFd); // 把内存彻底释放
        std::cout << "[Boss] Timeout or Closed. Kicked out Fd: " << clientFd << std::endl;
    }
}