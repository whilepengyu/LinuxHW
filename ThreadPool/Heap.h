#ifndef HEAP_H
#define HEAP_H

#include <vector>
#include <iostream>
#include <stdexcept>

class Heap {
public:
    Heap();
    void push(long long value);  // 向堆中添加元素
    long long pop();             // 从堆中移除并返回最大元素
    void heapSort();             // 堆排序
    
private:
    std::vector<long long> data; // 存储堆的数组
    void heapifyUp(int index);    // 向上调整堆
    void heapifyDown(int index);  // 向下调整堆
};

#endif // HEAP_H