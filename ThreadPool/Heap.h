#ifndef HEAP_H
#define HEAP_H

#include <vector>
#include <iostream>
#include <stdexcept>

class Heap {
public:
    Heap();
    void push(long long value);  // 向堆中添加元素
    long long top();             // 返回堆顶元素
    long long pop();             // 从堆中移除并返回最小元素
    bool empty() const;          // 检查堆是否为空
    size_t size() const;         // 返回堆中元素的数量
    
private:
    std::vector<long long> data; // 存储堆的数组
    void heapifyUp(int index);    // 向上调整堆
    void heapifyDown(int index);  // 向下调整堆
};

#endif // HEAP_H