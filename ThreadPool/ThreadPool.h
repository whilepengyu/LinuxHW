#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include "LockFreeQueue.hpp"
#include "Heap.h"

// 线程池类
class ThreadPool {
private:
    std::function<void()> func; // 使用 std::function 来存储可调用对象
    
public:
    ThreadPool(size_t threads); // 构造函数
    ~ThreadPool(); // 析构函数

    void addTask(std::function<void()> func);
    std::thread& getThread(size_t i);
    bool ifStop();
    std::vector<Heap> heaps;
private:
    void workerRun(); // 工作线程执行的函数
    LockFreeQueue<std::function<void()>> taskQueue; // 无锁任务队列
    std::vector<std::thread> workers; // 工作线程集合
    std::atomic<bool> stop{false}; // 停止标志
};

#endif // THREADPOOL_H