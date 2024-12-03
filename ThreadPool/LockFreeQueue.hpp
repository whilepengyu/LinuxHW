#ifndef LOCKFREEQUEUE_H
#define LOCKFREEQUEUE_H

#include <atomic>
#include <iostream>

// 节点结构
template<typename T>
struct Node {
    T data;
    std::atomic<Node*> next;
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
    bool empty() const; // 判断队列是否为空
    size_t size() const; // 获取队列大小

private:
    std::atomic<Node<T>*> head; // 队首指针
    std::atomic<Node<T>*> tail; // 队尾指针
};

// 构造函数实现
template<typename T>
LockFreeQueue<T>::LockFreeQueue() {
    Node<T>* node = new Node<T>(T());
    head.store(node);
    tail.store(node);
}

// 入队操作实现
template<typename T>
void LockFreeQueue<T>::enqueue(T value) {
    Node<T>* newNode = new Node<T>(value);
    Node<T>* oldTail;

    while (true) {
        oldTail = tail.load();
        Node<T>* tailNext = oldTail->next.load();

        if (tailNext != nullptr) { 
            tail.compare_exchange_strong(oldTail, tailNext);
            continue;
        }

        if (oldTail->next.compare_exchange_strong(tailNext, newNode)) {
            tail.compare_exchange_strong(oldTail, newNode);
            return;
        }
    }
}

// 出队操作实现
template<typename T>
bool LockFreeQueue<T>::dequeue(T& result) {
    Node<T>* oldHead;

    while (true) {
        oldHead = head.load();
        Node<T>* tailNode = tail.load();
        Node<T>* headNext = oldHead->next.load();

        if (oldHead == tailNode) { 
            if (headNext == nullptr) {
                return false; 
            }
            tail.compare_exchange_strong(tailNode, headNext);
        } else {
            result = headNext->data; 
            if (head.compare_exchange_strong(oldHead, headNext)) {
                delete oldHead; 
                return true; 
            }
        }
    }
}

// 判断队列是否为空
template<typename T>
bool LockFreeQueue<T>::empty() const {
    Node<T>* currentHead = head.load();
    Node<T>* currentTail = tail.load();
    
    return currentHead == currentTail && currentHead->next.load() == nullptr;
}

// 获取队列大小
template<typename T>
size_t LockFreeQueue<T>::size() const {
    size_t count = 0;
    Node<T>* currentNode = head.load();

    while (currentNode != nullptr) {
        count++;
        currentNode = currentNode->next.load();
    }

    return count - 1; // 减去哨兵节点的计数
}

// 析构函数实现
template<typename T>
LockFreeQueue<T>::~LockFreeQueue() {
    while (head.load() != nullptr) {
        Node<T>* temp = head.load();
        head.store(temp->next.load());
        delete temp;
    }
}

#endif // LOCKFREEQUEUE_H