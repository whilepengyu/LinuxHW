#include "LockFreeQueue.h"

// 构造函数
template<typename T>
LockFreeQueue<T>::LockFreeQueue() {
    Node<T>* node = new Node<T>(T());
    head.store(node);
    tail.store(node);
}

// 入队操作
template<typename T>
void LockFreeQueue<T>::enqueue(T value) {
    Node<T>* newNode = new Node<T>(value);
    Node<T>* oldTail;

    while (true) {
        oldTail = tail.load();
        Node<T>* tailNext = oldTail->next;

        if (tailNext != nullptr) { // 如果尾节点的下一个节点不为空，尝试移动尾指针
            tail.compare_exchange_strong(oldTail, tailNext);
            continue;
        }

        // 尝试将新节点添加到尾部
        if (oldTail->next == nullptr) {
            if (oldTail->next.compare_exchange_strong(tailNext, newNode)) {
                // 成功将新节点添加到队列，更新尾指针
                tail.compare_exchange_strong(oldTail, newNode);
                return;
            }
        }
    }
}

// 出队操作
template<typename T>
bool LockFreeQueue<T>::dequeue(T& result) {
    Node<T>* oldHead;

    while (true) {
        oldHead = head.load();
        Node<T>* tailNode = tail.load();
        Node<T>* headNext = oldHead->next;

        if (oldHead == tailNode) { // 队列为空
            if (headNext == nullptr) {
                return false; // 队列为空，无法出队
            }
            // 尝试移动尾指针
            tail.compare_exchange_strong(tailNode, headNext);
        }
        else {
            result = headNext->data; // 获取出队的数据
            if (head.compare_exchange_strong(oldHead, headNext)) {
                delete oldHead; // 删除旧头节点
                return true; // 出队成功
            }
        }
    }
}

// 析构函数
template<typename T>
LockFreeQueue<T>::~LockFreeQueue() {
    while (head.load() != nullptr) {
        Node<T>* temp = head.load();
        head.store(temp->next);
        delete temp;
    }
}