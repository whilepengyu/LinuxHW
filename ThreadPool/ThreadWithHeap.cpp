#include "ThreadWithHeap.h"

ThreadWithHeap::ThreadWithHeap() : std::thread() {}

// 构造函数
ThreadWithHeap::ThreadWithHeap(std::function<void()> func) : std::thread(func) {
}

// 析构函数
ThreadWithHeap::~ThreadWithHeap() {
    if (this->joinable()) {
        this->join(); // 等待线程结束
    }
}
Heap& ThreadWithHeap::getHeap() { return heap; }