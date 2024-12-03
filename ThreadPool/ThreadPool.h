#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include "LockFreeQueue.hpp"
#include "ThreadWithHeap.h"

// 线程池类
class ThreadPool {
private:
    std::function<void()> func; // 使用 std::function 来存储可调用对象
    
public:
    ThreadPool(size_t threads); // 构造函数
    ~ThreadPool(); // 析构函数

    void addTask(std::function<void(size_t)> func);
    ThreadWithHeap& getThread(size_t i);
    bool ifstop();
private:
    void workerRun(size_t i); // 工作线程执行的函数
    LockFreeQueue<std::function<void(size_t)>> taskQueue; // 无锁任务队列
    std::vector<ThreadWithHeap> workers; // 工作线程集合
    std::atomic<bool> stop{false}; // 停止标志
};

#endif // THREADPOOL_H