#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <iostream>

class ThreadPool {
private:
    std::vector<std::thread> workers;          // 线程数组（工人们）
    std::queue<std::function<void()>> tasks;   // 任务队列（任务清单）
    
    std::mutex queue_mutex;                    // 互斥锁（保护任务队列，防止工人们抢夺时发生冲突）
    std::condition_variable condition;         // 条件变量（负责让没有任务的工人休眠，并在有任务时唤醒他们）
    
    bool stop;                                 // 停止标志（老板喊下班的信号）

public:
    // 构造函数：初始化线程池，创建固定数量的线程
    ThreadPool(size_t threads) : stop(false) {
        for(size_t i = 0; i < threads; ++i) {
            // 给每个工人分配一段死循环的代码（相当于他们的日常工作规范）
            workers.emplace_back([this] {
                while(true) {
                    std::function<void()> task;

                    { // 🌟 这是一个局部作用域，为了让锁能自动释放
                        std::unique_lock<std::mutex> lock(this->queue_mutex); // 1. 先加锁
                        
                        // 2. 🌟 核心休眠逻辑：工人在这里等待！
                        // 什么时候会被唤醒？ 答：(老板喊下班了) 或者 (任务队列不是空的)
                        this->condition.wait(lock, [this] { 
                            return this->stop || !this->tasks.empty(); 
                        });

                        // 3. 醒来后检查：如果是老板喊下班了，且队列里没任务了，就脱下制服回家（退出死循环）
                        if(this->stop && this->tasks.empty()) {
                            return; 
                        }

                        // 4. 走到这里，说明队列里有任务，拿走最前面的任务
                        task = std::move(this->tasks.front());
                        this->tasks.pop(); // 从队列里把这个任务踢掉
                        
                    } // 🌟 离开局部作用域，lock 自动解锁！别的工人可以接着去队列抢任务了

                    // 5. 干活！执行刚刚拿到的任务。注意：干活的时候是没有锁的，所以多个工人可以同时干活！
                    task();
                }
            });
        }
    }

    // 添加任务的接口：老板用的方法
    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex); // 1. 老板加锁
            
            // 如果已经下班了，就不允许再接新任务了
            if(stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            
            tasks.push(task); // 2. 把任务塞进队列
        } // 3. 老板自动解锁
        
        condition.notify_one(); // 🌟 4. 敲锣！唤醒一个正在休眠的工人来干活
    }

    // 析构函数：销毁线程池（老板宣布公司倒闭）
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true; // 1. 挂起下班牌子
        }
        condition.notify_all(); // 🌟 2. 敲响绝命大锣！把所有正在休眠的工人都叫醒！让他们看到下班牌子

        // 3. 等待所有工人都把手头的活干完，然后正式解散他们
        for(std::thread &worker : workers) {
            if(worker.joinable()) {
                worker.join();
            }
        }
    }
};

#endif