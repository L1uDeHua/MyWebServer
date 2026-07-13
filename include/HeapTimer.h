#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <vector>

// 定义一个别名，代表超时时间点
typedef std::chrono::high_resolution_clock::time_point Timeout;
// 定义一个别名，代表时钟的毫秒数
typedef std::chrono::milliseconds MS;

// 定时器节点结构体
struct TimerNode {
    int id;             // 连接的 fd
    Timeout expires;    // 绝对的超时时间点
    std::function<void()> cb; // 超时后的回调函数（比如用来关闭连接）
    
    // 重载比较运算符：因为我们要实现最小堆，时间越小（越早过期）的节点，优先级越高
    bool operator<(const TimerNode& t) const {
        return expires < t.expires;
    }
};

class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }
    ~HeapTimer() { clear(); }

    // 添加或更新一个定时器 (给某个 fd 续命)
    void add(int id, int timeout, const std::function<void()>& cb);

    // 清理所有超时的节点 (核心运转机制)
    void tick();

    // 🌟 极其重要：告诉 epoll_wait 下一次还需要等多久，才会有连接超时
    int getNextTick();

private:
    void del_(size_t i);   // 删除指定位置的节点
    void siftup_(size_t i);// 向上调整堆
    bool siftdown_(size_t index, size_t n); // 向下调整堆
    void swapNode_(size_t i, size_t j);     // 交换两个节点

    void clear();

    std::vector<TimerNode> heap_; // 用数组来模拟二叉树（堆）
    std::unordered_map<int, size_t> ref_; // 映射表：用来光速查找 fd 在数组里的哪个位置
};

#endif // HEAP_TIMER_H