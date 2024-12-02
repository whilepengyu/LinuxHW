#ifndef LOCKFREEQUEUE_H
#define LOCKFREEQUEUE_H

#include <atomic>
#include <iostream>

// 节点结构
template<typename T>
struct Node {
    T data;
    Node* next;
    Node(T value) : data(value), next(nullptr) {}
};

// 无锁队列类
template<typename T>
class LockFreeQueue {
public:
    LockFreeQueue(); // 构造函数
    ~LockFreeQueue(); // 析构函数
    void enqueue(T value); // 入队操作
    bool dequeue(T& result); // 出队操作

private:
    std::atomic<Node<T>*> head; // 队首指针
    std::atomic<Node<T>*> tail; // 队尾指针
};

#include "LockFreeQueue.cpp" // 包含实现文件

#endif // LOCKFREEQUEUE_H