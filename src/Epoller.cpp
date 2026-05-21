#include "../include/Epoller.h"
#include <unistd.h>
#include <iostream>

// 构造函数：创建 epoll 实例，并初始化 vector 数组大小
Epoller::Epoller(int maxEvent) : epollFd_(epoll_create1(0)), events_(maxEvent) {
    if (epollFd_ < 0) {
        std::cerr << "Epoller creation failed!" << std::endl;
    }
}

// 析构函数：释放资源
Epoller::~Epoller() {
    close(epollFd_);
}

// 添加监听
bool Epoller::addFd(int fd, uint32_t events) {
    if (fd < 0) return false;
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

// 修改监听
bool Epoller::modFd(int fd, uint32_t events) {
    if (fd < 0) return false;
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = events;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

// 删除监听
bool Epoller::delFd(int fd) {
    if (fd < 0) return false;
    return 0 == epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, nullptr);
}

// 核心 wait 函数：封装了原本的 epoll_wait
int Epoller::wait(int timeoutMs) {
    // 这里的 &events_[0] 是拿到 vector 内部底层数组的首地址，完美兼容 C 语言的 API
    return epoll_wait(epollFd_, &events_[0], static_cast<int>(events_.size()), timeoutMs);
}

// 获取第 i 个事件的 fd
int Epoller::getEventFd(size_t i) const {
    return events_[i].data.fd;
}

// 获取第 i 个事件的具体类型
uint32_t Epoller::getEvents(size_t i) const {
    return events_[i].events;
}