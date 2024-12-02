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
#include "LockFreeQueue.h"
#include <filesystem>
namespace fs = std::filesystem;

class SortManager {
private:
    struct Task {
        std::function<void()> func; // 使用 std::function 来存储可调用对象
    };

public:
    SortManager(const std::string& dir, int numThread, size_t bufferSize, size_t totalDataSize);

private:
    ThreadPool pool; // 线程池
    std::vector<std::unique_ptr<Heap>> heaps; // 每个线程的堆
    std::shared_ptr<long long[]> buffer; // 数据缓冲区
    LockFreeQueue<size_t> availableBlocks; // 用无锁队列存储可用块在buffer中的位置
    size_t bufferSize; // buffer大小
    size_t blockSize; // 每个块的大小

    void ReadOneBlockFromFiles(); // 从文件中读取一个块的数据
    void ReadToHeap(); // 从文件读取数据到堆
    void mergeHeaps(); // 将所有线程的堆合并成一个有序的中间文件
    void mergeIntermediate(); // 将中间文件合并
    
    fs::directory_iterator dirIter;
    fs::directory_iterator dirEndIter;
    std::ifstream fileStream;
};

#endif // SORTMANAGER_H