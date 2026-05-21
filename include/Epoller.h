#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h>
#include <vector>

class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);
    ~Epoller();

    // 核心操作接口
    bool addFd(int fd, uint32_t events);
    bool modFd(int fd, uint32_t events);
    bool delFd(int fd);

    // 等待事件发生，返回发生事件的数量
    int wait(int timeoutMs = -1);

    // 获取某个发生事件的 fd 和具体的事件类型
    int getEventFd(size_t i) const;
    uint32_t getEvents(size_t i) const;

private:
    int epollFd_;                                // epoll 实例的句柄
    std::vector<struct epoll_event> events_;     // 用 vector 代替之前的原生数组，更安全
};

#endif // EPOLLER_H