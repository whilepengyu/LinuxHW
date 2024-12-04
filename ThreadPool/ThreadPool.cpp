#include "ThreadPool.h"

// 构造函数
ThreadPool::ThreadPool(size_t threads) {
    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back([this]() { this->workerRun(); }); // 创建工作线程并启动
        heaps.push_back(Heap());
    }
}

void ThreadPool::addTask(std::function<void()> func){
    std::lock_guard<std::mutex> lock(queueMutex);
    taskQueue.push(func);
}

// 工作线程执行的函数
void ThreadPool::workerRun() {
    while (!stop.load()) { // 检查是否需要停止
        std::function<void()> func;
        {
            std::lock_guard<std::mutex> lock(queueMutex); // 上锁以安全地访问任务队列
            
            if (!taskQueue.empty()) { // 从队列中取出任务
                func = taskQueue.front();
                taskQueue.pop();
            }
        }

        if (func) { 
            func(); // 执行任务
        } 
        else {
            std::this_thread::yield(); // 如果队列为空，放弃当前时间片，避免忙等待
        }
    }
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

bool ThreadPool::ifStop(){
    return stop.load();
}

std::thread& ThreadPool::getThread(size_t i){
    return workers[i];
}