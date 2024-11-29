#ifndef LOCKFREEQUEUE_H
#define LOCKFREEQUEUE_H

#include <atomic>
#include <vector>

template<typename T>
class LockFreeQueue {
public:
    LockFreeQueue(size_t capacity);
    bool enqueue(const T& value);
    bool dequeue(T& result);

private:
    std::vector<T> buffer;
    const size_t capacity;
    std::atomic<size_t> head, tail;
};

#include "LockFreeQueue.tpp" // 包含实现，避免模板类定义问题

#endif // LOCKFREEQUEUE_H