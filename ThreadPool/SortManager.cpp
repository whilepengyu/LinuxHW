#include <iostream>
#include <limits>
#include "SortManager.h"

SortManager::SortManager(const std::string& dir, size_t numThread, size_t bufferSize, size_t totalFileSize)
    :pool(numThread), numThread(numThread), bufferSize(bufferSize), totalFileSize(totalFileSize) {
    
    // for(size_t i = 0; i < numThread; i++){
    //     unavailableBlocks.enqueue(i);
    // }

    // 初始化文件迭代器
    dirIter = fs::directory_iterator(dir);
    dirEndIter = fs::directory_iterator(); // 获取目录结束迭代器

    // 打开第一个文件
    if (dirIter != dirEndIter) {
        fileStream.open(dirIter->path(), std::ios::binary);
        if (!fileStream) {
            throw std::runtime_error("Failed to open file: " + dirIter->path().string());
        }
    }

    blockSize = bufferSize / numThread; // 每个线程处理的数据块大小
    buffer = std::make_unique<long long[]>(bufferSize / sizeof(long long));

    intermediateCount.store(0);
    readyHeapsCount.store(0);
    size_t totalInter = totalFileSize / bufferSize; 
    if(totalFileSize % bufferSize != 0) totalInter++;
    numIntermediate.store(totalInter);
    
    state=(State::Running);
}

void SortManager::Run(){
    state=(State::ReadyToReadToCache);
    while(true){
        std::unique_lock<std::mutex> lock(stateMutex);
        cv.wait(lock, [this]() { return state != State::Running; });
        switch (state) {
        case State::Running:
            break;
        case State::ReadyToReadToCache:
            pool.addTask([this]() { this->ReadToCache(); });
            SetState(State::Running);
            break;
        case State::ReadyToReadToHeaps:
            readyHeapsCount.store(0);
            readToHeapFlag.store(false);
            for(size_t i = 0; i < numThread; i++){
                size_t threadIndex = i;
                pool.addTask([this, threadIndex]() { this->ReadToHeap(threadIndex); });
            }
            SetState(State::Running);
            break;
        case State::ReadyToMergeHeaps:
            pool.addTask([this]() { this->MergeHeaps(); });
            SetState(State::Running);
            break;
        case State::ReadyToMergeIntermediates:
            // 合并中间文件
            break;
        case State::Stop:
            return; // 退出循环
        }
    }
}

void SortManager::ReadToCache() {
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    std::cout << "ReadToCache" << std::endl;
    size_t count = 0;
    size_t remainingBytes = bufferSize;
    while(true){
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
        
        std::streampos currentPos = fileStream.tellg();
        fileStream.seekg(0, std::ios::end);
        std::streampos endPos = fileStream.tellg();
        size_t fileRemainingBytes = endPos - currentPos;
        fileStream.seekg(currentPos);

        // 计算剩余读取的字节数
        remainingBytes = bufferSize - count;
        
        if(remainingBytes == 0){
            break;
        }
        else if(remainingBytes < 0){
            throw std::runtime_error("remainingBytes should not less than 0.");
        }
        // 从当前文件中批量读取数据到共享缓冲区
        size_t offset = count / sizeof(long long);
        fileStream.read(reinterpret_cast<char*>(buffer.get() + offset), std::min(remainingBytes, fileRemainingBytes));
        //std::cout << fileStream.eof() <<std:: endl;
        // 获取实际读取的字节数
        size_t bytesRead = fileStream.gcount();
        
        if (bytesRead > 0) { 
            // 更新已读取的数量（以64位整数为单位）
            count += bytesRead;
        }


        // 检查是否到达文件末尾
        if (fileRemainingBytes - bytesRead == 0) {
            // 如果到达文件末尾，关闭当前文件流并准备读取下一个文件
            fileStream.close();
            continue; // 继续尝试下一个文件
        }

        if (fileStream.fail()) {
            throw std::runtime_error("Error reading from file: " + dirIter->path().string());
        }
    }
    std::cout << "ReadToCacheFinished" << std::endl;
    std::unique_lock<std::mutex> lock2(stateMutex);
    SetState(State::ReadyToReadToHeaps);
    cv.notify_one();
}


void SortManager::ReadToHeap(size_t i) {
    std::cout << "ReadToHeap:" << i << std::endl;
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    Heap& heap = pool.heaps[i];

    size_t blockIndex = i;

    for (size_t j = 0; j < blockSize; ++j) {
        size_t index = (blockIndex * blockSize + j) / sizeof(long long);
        if (index >= bufferSize) break;
        long long value = buffer[index];
        heap.push(value); 
    }
    std::unique_lock<std::mutex> lock2(readyHeapsCountMutex);
    readyHeapsCount++;
    std::string info = "ReadToHeap:" + std::to_string(i) + " finished";
    std::cout << info << std::endl;
    if (readyHeapsCount.load() == numThread){
        std::cout << "ReadToHeap all finished" << std::endl;
        std::unique_lock<std::mutex> lock3(stateMutex);
        SetState(State::ReadyToMergeHeaps);
        cv.notify_one();
    }
}


void SortManager::MergeHeaps() {
    // 写入到中间文件
    std::cout << "MergeHeaps" << std::endl;
    std::string dir = "./intermediate/";

    fs::path dirPath(dir);
    if (!fs::exists(dirPath)) {
        if (!fs::create_directory(dirPath)) {
            std::cerr << "无法创建目录！" << std::endl;
            return;
        }
    }

    std::string fileName = "Inter"; 
    std::ofstream outFile(dir + fileName + std::to_string(intermediateCount) + ".bin");
    if (!outFile) {
        std::cerr << "无法打开输出文件！" << std::endl;
        return;
    }

    size_t count = 0;
    for (size_t i = 0; i < numThread; i++){
        std::cout << pool.heaps[i].size() << std::endl;
    }
    while (true) {
        long long minValue = std::numeric_limits<long long>::max();
        ssize_t minIndex = -1; // 初始化为无效索引
        size_t emptyCount = 0;

        for (size_t i = 0; i < numThread; i++) {
            if (pool.heaps[i].empty()) {
                emptyCount++;
                continue;
            }
            if (pool.heaps[i].top() < minValue) {
                minValue = pool.heaps[i].top();
                minIndex = i;
            }
        }

        if (emptyCount == numThread) {
            break; // 所有堆都为空，退出循环
        }
        if (minIndex == -1)
            throw std::runtime_error("minIndex should not be -1");
        pool.heaps[minIndex].pop(); // 从最小堆中弹出最小元素
        buffer[count] = minValue;
        count++;
    }
    outFile.write(reinterpret_cast<const char*>(buffer.get()), count * sizeof(long long));
    intermediateCount.fetch_add(1, std::memory_order_relaxed);
    outFile.close();
    SetState(State::Stop);
    std::cout << "MergeHeapsFinished" << std::endl;
}

void SortManager::MergeIntermediate(){ // 将中间文件合并

}

void SortManager::SetState(State newState){
    state = newState;
}