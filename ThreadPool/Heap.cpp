#include "Heap.h"

Heap::Heap() {
    // 初始化一个空堆
}

void Heap::push(long long value) {
    data.push_back(value);       // 将新元素添加到堆的末尾
    heapifyUp(data.size() - 1); // 调整堆以保持堆性质
}

long long Heap::pop() {
    if (data.empty()) {
        throw std::out_of_range("Heap is empty");
    }
    
    long long rootValue = data[0];     // 保存根节点的值
    data[0] = data.back();              // 将最后一个元素放到根节点
    data.pop_back();                    // 移除最后一个元素
    heapifyDown(0);                     // 调整堆以保持堆性质
    
    return rootValue;                   // 返回原根节点的值
}

void Heap::heapifyUp(int index) {
    while (index > 0) {
        int parentIndex = (index - 1) / 2;
        if (data[index] > data[parentIndex]) {
            std::swap(data[index], data[parentIndex]);
            index = parentIndex;
        } else {
            break;
        }
    }
}

void Heap::heapifyDown(int index) {
    int size = data.size();
    
    while (index < size) {
        int leftChild = 2 * index + 1;
        int rightChild = 2 * index + 2;
        int largest = index;

        if (leftChild < size && data[leftChild] > data[largest]) {
            largest = leftChild;
        }
        
        if (rightChild < size && data[rightChild] > data[largest]) {
            largest = rightChild;
        }

        if (largest != index) {
            std::swap(data[index], data[largest]);
            index = largest;
        } else {
            break;
        }
    }
}

void Heap::heapSort() {
    std::vector<long long> sortedData;
    
    while (!data.empty()) {
        sortedData.push_back(pop());
    }
    
    data = sortedData; // 将排序后的数据放回原堆中
}