#include <iostream>
#include <limits>
#include "SortManager.h"

SortManager::SortManager(const std::string& dir, size_t numThread, size_t bufferSize)
    :pool(numThread), numThread(numThread), bufferSize(bufferSize), intermediateCount(0) {
    
    for(size_t i = 0; i < numThread; i++){
        unavailableBlocks.enqueue(i);
    }

    // 初始化文件迭代器
    dirIter = fs::directory_iterator(dir);
    dirEndIter = fs::end(dirIter); // 获取目录结束迭代器

    // 打开第一个文件
    if (dirIter != dirEndIter) {
        fileStream.open(dirIter->path(), std::ios::binary);
        if (!fileStream) {
            throw std::runtime_error("Failed to open file: " + dirIter->path().string());
        }
    }

    blockSize = bufferSize / numThread; // 每个线程处理的数据块大小
    buffer = std::shared_ptr<long long[]>(new long long[bufferSize / sizeof(long long)]);
}

void SortManager::Run(){
    while(true){
        if(!unavailableBlocks.empty()){
            pool.addTask([this](size_t i) { this->ReadOneBlockToCache(); });
        }
        else if(!availableBlocks.empty()){
            pool.addTask([this](size_t i) { this->ReadToHeap(i); });
        }
        else if(unavailableBlocks.size() == 0 && availableBlocks.size() == numThread){
            pool.addTask([this]() { this->MergeHeaps(); });
        }
        // else if(heapMergeAllFinished){
        //     pool.addTask([this]() { this->MergeIntermediate(); });
        // }
        else if(pool.ifstop()){
            break;
        }
    }
}

void SortManager::ReadOneBlockToCache() {
    if(unavailableBlocks.empty()){
        return;
    }
    size_t blockIndex;
    unavailableBlocks.dequeue(blockIndex);
    size_t count = 0;
    while(count < blockSize){
        if (!fileStream.is_open()) {
            // 如果当前文件流未打开，尝试打开下一个文件
            if (++dirIter != dirEndIter) {
                fileStream.open(dirIter->path(), std::ios::binary);
                if (!fileStream) {
                    throw std::runtime_error("Failed to open file: " + dirIter->path().string());
                }
            } else {
                break; // 所有文件都已处理，退出循环
            }
        }

        // 计算剩余读取的字节数
        size_t remainingBytes = (blockSize - count) * sizeof(long long);
        
        // 从当前文件中批量读取数据到共享缓冲区
        fileStream.read(reinterpret_cast<char*>(buffer.get() + blockSize * blockIndex + count), remainingBytes);
        
        // 获取实际读取的字节数
        size_t bytesRead = fileStream.gcount();
        
        if (bytesRead > 0) {
            // 更新已读取的数量（以64位整数为单位）
            count += bytesRead / sizeof(long long);
        }

        // 检查是否到达文件末尾
        if (fileStream.eof()) {
            // 如果到达文件末尾，关闭当前文件流并准备读取下一个文件
            fileStream.close();
            continue; // 继续尝试下一个文件
        }

        if (!fileStream) {
            throw std::runtime_error("Error reading from file: " + dirIter->path().string());
        }
    }
    availableBlocks.enqueue(blockIndex);
}

void SortManager::ReadToHeap(size_t i) {
    Heap& heap = pool.getThread(i).getHeap(); // 获取当前线程的堆实例

    size_t blockIndex;
    availableBlocks.dequeue(blockIndex);

    // 从共享缓冲区读取数据并推入堆中
    for (size_t i = 0; i < blockSize; ++i) {
        long long value = buffer[blockIndex * blockSize + i]; // 从缓冲区读取值
        heap.push(value); // 将值推入当前线程的堆中
    }
}


void SortManager::MergeHeaps() {
    // 写入到中间文件
    std::string fileName = "itermediate/iter";
    std::ofstream outFile(fileName + std::to_string(intermediateCount) + ".bin");
    if (!outFile) {
        std::cerr << "无法打开输出文件！" << std::endl;
        return;
    }

    std::vector<Heap*> heaps; // 使用指针来存储堆的地址
    for (size_t i = 0; i < numThread; i++) {
        heaps.push_back(&pool.getThread(i).getHeap()); // 获取每个线程的堆
    }

    while (true) {
        long long minValue = std::numeric_limits<long long>::max();
        size_t minIndex = -1; // 初始化为无效索引
        size_t emptyCount = 0;

        for (size_t i = 0; i < numThread; i++) {
            if (heaps[i]->empty()) {
                emptyCount++;
                continue;
            }
            if (heaps[i]->top() < minValue) {
                minValue = heaps[i]->top();
                minIndex = i;
            }
        }

        if (emptyCount == numThread) {
            break; // 所有堆都为空，退出循环
        }

        heaps[minIndex]->pop(); // 从最小堆中弹出最小元素
        outFile << minValue << "\n"; // 写入文件并换行
    }

    intermediateCount++;
    outFile.close();
}

void SortManager::MergeIntermediate(){ // 将中间文件合并

}