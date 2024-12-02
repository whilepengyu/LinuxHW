#include "ThreadPool.h"

// 构造函数
ThreadPool::ThreadPool(size_t threads) {
    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back(&ThreadPool::worker, this); // 创建工作线程并启动
    }
}

// 工作线程执行的函数
void ThreadPool::worker() {
    while (!stop.load()) { // 检查是否需要停止
        Task task;
        if (taskQueue.dequeue(task)) { // 从队列中取出任务
            task.func(); // 执行任务
        } else {
            std::this_thread::yield(); // 如果队列为空，放弃当前时间片，避免忙等待
        }
    }
}

// 将任务添加到任务队列
void ThreadPool::enqueue(Task task) {
    taskQueue.enqueue(task); // 将任务加入无锁队列
}

// 析构函数
ThreadPool::~ThreadPool() {
    stop.store(true); // 设置停止标志
    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join(); // 等待所有工作线程完成
        }
    }
}