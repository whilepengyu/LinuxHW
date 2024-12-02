#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include "LockFreeQueue.h"
#include "Task.h"

// 线程池类
class ThreadPool {
public:
    ThreadPool(size_t threads); // 构造函数
    ~ThreadPool(); // 析构函数

    void enqueue(Task task); // 将任务添加到任务队列

private:
    void worker(); // 工作线程执行的函数
    LockFreeQueue<Task> taskQueue; // 无锁任务队列
    std::vector<std::thread> workers; // 工作线程集合
    std::atomic<bool> stop{false}; // 停止标志
};

#endif // THREADPOOL_H