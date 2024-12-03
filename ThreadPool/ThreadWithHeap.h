#ifndef THREAD_WITH_HEAP_H
#define THREAD_WITH_HEAP_H

#include <thread>
#include <functional>
#include "Heap.h"

class ThreadWithHeap : public std::thread {
public:
    ThreadWithHeap();
    ThreadWithHeap(std::function<void()> func); // 构造函数
    ~ThreadWithHeap(); // 析构函数

    Heap& getHeap();

    // 禁用拷贝构造和拷贝赋值
    ThreadWithHeap(const ThreadWithHeap&) = delete;
    ThreadWithHeap& operator=(const ThreadWithHeap&) = delete;

    // 允许移动构造和移动赋值
    ThreadWithHeap(ThreadWithHeap&&) noexcept = default;
    ThreadWithHeap& operator=(ThreadWithHeap&&) noexcept = default;
private:
    Heap heap; // Heap 类的实例
};

#endif // THREAD_WITH_HEAP_H