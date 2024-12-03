#ifndef SORTMANAGER_H
#define SORTMANAGER_H

#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <fstream>
#include <functional>
#include "ThreadPool.h"
#include "Heap.h"
#include "LockFreeQueue.hpp"
#include <filesystem>
namespace fs = std::filesystem;

class SortManager {
public:
    SortManager(const std::string& dir, size_t numThread, size_t bufferSize);
    void Run();
private:
    ThreadPool pool; // 线程池
    size_t numThread;
    std::shared_ptr<long long[]> buffer; // 数据缓冲区
    LockFreeQueue<size_t> availableBlocks; // 用无锁队列存储 可用块在buffer中的索引
    LockFreeQueue<size_t> unavailableBlocks; // 用无锁队列存储 不可用块在buffer中的索引
    size_t bufferSize; // buffer大小
    size_t blockSize; // 每个块的大小

    //Tasks
    void ReadOneBlockToCache(); // 从文件中读数据到缓存
    void ReadToHeap(size_t i); // 把缓存数据读到堆中
    void MergeHeaps(); // 将所有线程的堆合并成一个有序的中间文件
    void MergeIntermediate(); // 将中间文件合并
    
    fs::directory_iterator dirIter;
    fs::directory_iterator dirEndIter;
    std::ifstream fileStream;

    size_t intermediateCount;
};

#endif // SORTMANAGER_H