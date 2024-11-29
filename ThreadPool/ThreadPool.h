#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <vector>
#include <atomic>
#include "LockFreeQueue.h"

class ThreadPool {
public:
    ThreadPool(size_t threads);
    ~ThreadPool();
    
    void enqueue(const std::string& filename);

private:
    void worker();
    LockFreeQueue<std::string> taskQueue;
    std::vector<std::thread> workers;
    std::atomic<bool> stop{false};
};

#endif // THREADPOOL_H