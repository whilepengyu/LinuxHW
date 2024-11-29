#include "ThreadPool.h"
#include <fstream>
#include <algorithm>

ThreadPool::ThreadPool(size_t threads)
    : taskQueue(1024) { // 设置任务队列容量
    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back(&ThreadPool::worker, this);
    }
}

ThreadPool::~ThreadPool() {
    stop.store(true, std::memory_order_release);
    for (auto& worker : workers) {
        worker.join();
    }
}

void ThreadPool::enqueue(const std::string& filename) {
    while (!taskQueue.enqueue(filename)) {
        std::this_thread::yield(); // 如果队列满了，可以选择等待或丢弃任务
    }
}

void ThreadPool::worker() {
    while (!stop.load(std::memory_order_acquire)) {
        std::string filename;
        if (taskQueue.dequeue(filename)) {
            // 处理文件排序
            sortAndWrite(filename);
        } else {
            std::this_thread::yield(); // 队列为空时让出CPU
        }
    }
}

void sortAndWrite(const std::string& filename) {
    // 读取文件数据
    std::ifstream file(filename);
    std::vector<int64_t> data;

    int64_t number;
    while (file >> number) {
        data.push_back(number);
        if (data.size() * sizeof(int64_t) >= 64 * 1024 * 1024) { // 内存限制为64MB
            break; // 超过限制则停止读取
        }
    }

    // 排序
    std::sort(data.begin(), data.end());

    // 写入临时文件
    std::ofstream outFile(filename + ".sorted");
    for (const auto& num : data) {
        outFile << num << "\n";
    }
}