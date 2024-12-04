#include "Heap.h"

Heap::Heap() {
    // 初始化一个空堆
}

void Heap::push(long long value) {
    data.push_back(value);       // 将新元素添加到堆的末尾
    heapifyUp(data.size() - 1); // 调整堆以保持堆性质
}

long long Heap::top() {
    if (data.empty()) {
        throw std::out_of_range("Heap is empty");
    }
    return data[0];                     // 返回根节点的值（即最小值）
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
        if (data[index] < data[parentIndex]) {
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
        int smallest = index;
        if (leftChild < size && data[leftChild] < data[smallest]) {
            smallest = leftChild;
        }
        
        if (rightChild < size && data[rightChild] < data[smallest]) {
            smallest = rightChild;
        }

        if (smallest != index) {
            std::swap(data[index], data[smallest]);
            index = smallest;
        } else {
            break;
        }
    }
}

bool Heap::empty() const {
    return data.empty();                // 检查数据向量是否为空
}

size_t Heap::size() const {
    return data.size();
}