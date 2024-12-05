#ifndef SORTMANAGER_H
#define SORTMANAGER_H

#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <fstream>
#include <functional>
#include <filesystem>
#include <shared_mutex>
#include <mutex>
#include <condition_variable>
#include <unordered_set>
#include <queue>
#include "ThreadPool.h"
#include "Heap.h"

namespace fs = std::filesystem;

class SortManager {
public:
    enum class State {
        Running, // 运行中
        ReadyToReadToCache, // 准备读取到缓存
        ReadyToReadToHeaps, // 准备读取到堆
        ReadyToMergeHeaps, // 准备合并堆
        ReadyToMergeIntermediates, // 准备合并中间文件
        Stop // 停止运行
    };
public:
    SortManager(const std::string& dir, size_t numThread, size_t bufferSize, size_t totalFileSize);
    void Run();

private:
    void SetState(State newState);

private:
    ThreadPool pool; // 线程池
    size_t numThread;
    
    std::shared_mutex cacheMutex;
    std::unique_ptr<long long[]> buffer; // 数据缓冲区
    
    //LockFreeQueue<size_t> availableBlocks; // 用无锁队列存储 可用块在buffer中的索引
    //LockFreeQueue<size_t> unavailableBlocks; // 用无锁队列存储 不可用块在buffer中的索引
    size_t bufferSize; // buffer大小
    size_t blockSize; // 每个块的大小

    //Tasks
    void ReadToCache(); // 从文件中读取数据填满缓存
    //void ReadOneBlockToCache(); // 从文件中读一块数据到缓存
    void ReadToHeap(size_t i); // 把缓存数据读到堆中
    void MergeHeaps(); // 将所有线程的堆合并成一个有序的中间文件
    void MergeIntermediate(); // 将中间文件合并
    void MergeTwoIntermediate(size_t a, size_t b); // 将两个中间文件合并

    std::string interDir;
    fs::directory_iterator dirIter; // 目录迭代器
    fs::directory_iterator dirEndIter; //目录迭代器结束点
    std::ifstream fileStream; // 文件流

    size_t totalFileSize; // 文件总共的大小
    size_t numIntermediate; // 中间文件个数
    
    std::mutex stateMutex; 
    State state; //状态

    std::mutex readyHeapsCountMutex;
    std::atomic<size_t> readyHeapsCount; //就绪堆计数

    std::mutex intermediateQueueMutex;
    std::queue<size_t> intermediateQueue; // 中间文件队列

    std::atomic<size_t> runningMergeIntermediateCount;
    std::atomic<bool> terminate;

    std::condition_variable cvTask; //条件变量
    std::condition_variable cvTerminate; //条件变量
    std::mutex terminateMutex;
};

#endif // SORTMANAGER_H