#include "../include/HeapTimer.h"
#include <cassert>

void HeapTimer::swapNode_(size_t i, size_t j) {
    std::swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;
    ref_[heap_[j].id] = j;
}

void HeapTimer::siftup_(size_t i) {
    size_t j = (i - 1) / 2; // 找到父节点
    while(j >= 0) {
        if(heap_[j] < heap_[i]) { break; } // 如果父节点已经比我小了，停止上浮
        swapNode_(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

bool HeapTimer::siftdown_(size_t index, size_t n) {
    size_t i = index;
    size_t j = i * 2 + 1; // 找到左子节点
    while(j < n) {
        // 如果右子节点更小，就和右边比
        if(j + 1 < n && heap_[j + 1] < heap_[j]) j++;
        // 如果我已经比最小的子节点还小了，停止下沉
        if(heap_[i] < heap_[j]) break;
        swapNode_(i, j);
        i = j;
        j = i * 2 + 1;
    }
    return i > index;
}

// 核心功能：添加定时器（或者续命）
void HeapTimer::add(int id, int timeout, const std::function<void()>& cb) {
    assert(id >= 0);
    size_t i;
    // 如果是一个全新的连接
    if(ref_.count(id) == 0) {
        i = heap_.size();
        ref_[id] = i;
        heap_.push_back({id, std::chrono::high_resolution_clock::now() + MS(timeout), cb});
        siftup_(i); // 放到最后，然后向上冒泡
    } 
    // 如果是老连接发来了新数据（续命）
    else {
        i = ref_[id];
        heap_[i].expires = std::chrono::high_resolution_clock::now() + MS(timeout);
        heap_[i].cb = cb;
        // 时间往后延了，说明它能活更久，所以向下沉
        if(!siftdown_(i, heap_.size())) {
            siftup_(i);
        }
    }
}

void HeapTimer::del_(size_t i) {
    assert(!heap_.empty() && i >= 0 && i < heap_.size());
    size_t n = heap_.size() - 1;
    assert(i <= n);
    if(i < n) {
        swapNode_(i, n);
        if(!siftdown_(i, n)) {
            siftup_(i);
        }
    }
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

// 核心机制：剔除超时的连接
void HeapTimer::tick() {
    if(heap_.empty()) return;
    
    // 只要堆顶的连接过期了，就一直杀，直到堆顶的连接还没过期为止
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires - std::chrono::high_resolution_clock::now()).count() > 0) { 
            break; // 最顶上的都没过期，底下的绝对安全！直接退出！
        }
        node.cb(); // 执行回调函数（比如关闭连接）
        del_(0);   // 把堆顶踢掉
    }
}

// 极其精妙的设计：计算离下一次有连接超时还有多少毫秒
int HeapTimer::getNextTick() {
    tick(); // 顺手清理一下已超时的
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - std::chrono::high_resolution_clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}