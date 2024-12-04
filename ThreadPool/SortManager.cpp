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

    readyHeapsCount.store(0);
    numIntermediate = totalFileSize / bufferSize; 
    if(totalFileSize % bufferSize != 0) numIntermediate++;
    
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
            pool.addTask([this]() { this->MergeIntermediate(); });
            SetState(State::Running);
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
    std::string info = "ReadToHeap:" + std::to_string(i);
    std::cout << info << std::endl;
    std::shared_lock<std::shared_mutex> lock(cacheMutex);
    Heap& heap = pool.heaps[i];

    size_t blockIndex = i;

    for (size_t j = 0; j < blockSize; j += sizeof(long long)) {
        size_t index = (blockIndex * blockSize + j) / sizeof(long long);
        if (index >= bufferSize) break;
        long long value = buffer[index];
        heap.push(value); 
    }
    std::unique_lock<std::mutex> lock2(readyHeapsCountMutex);
    readyHeapsCount++;
    info = info + " finished";
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
    size_t interNum = intermediateSet.size();
    std::ofstream outFile(dir + fileName + std::to_string(interNum) + ".bin");
    if (!outFile) {
        std::cerr << "无法打开输出文件！" << std::endl;
        return;
    }

    size_t count = 0;
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
    std::unique_lock<std::mutex> lock(intermediateSetMutex);
    intermediateSet.insert(interNum);
    outFile.close();
    std::cout << "MergeHeapsFinished" << std::endl;
    if(intermediateSet.size() < numIntermediate){
        SetState(State::ReadyToReadToCache);
        cv.notify_one();
    }
    else{
        SetState(State::ReadyToMergeIntermediates);
        cv.notify_one();
    }
}

void SortManager::MergeIntermediate(){ // 将中间文件合并
    std::cout << "MergeIntermediate" << std::endl;
    std::unique_lock<std::mutex> lock(intermediateSetMutex);
    if(intermediateSet.size() <= 1) {
        SetState(State::Stop);
        cv.notify_one();
        return;
    }
    auto it = intermediateSet.begin();
    size_t a = *it;
    intermediateSet.erase(a);
    it = intermediateSet.begin();
    size_t b = *it;
    intermediateSet.erase(b);
    lock.unlock();    
    MergeTwoIntermediate(a, b);
}

void SortManager::MergeTwoIntermediate(size_t a, size_t b){
    if(a > b){
        size_t t = a;
        a = b;
        b = t;
    }
    std::string file1 = "./intermediate/Inter" + std::to_string(a) + ".bin";
    std::string file2 = "./intermediate/Inter" + std::to_string(b) + ".bin";
    std::string outputFile = "./intermediate/inter" + std::to_string(a) + ".bin";
    std::ifstream inFile1(file1, std::ios::binary);
    std::ifstream inFile2(file2, std::ios::binary);
    std::ofstream outFile(outputFile, std::ios::binary);

    if (!inFile1.is_open() || !inFile2.is_open() || !outFile.is_open()) {
        std::cerr << "无法打开文件。" << std::endl;
        return;
    }

    long long num1, num2;
    bool hasNum1 = (inFile1.read(reinterpret_cast<char*>(&num1), sizeof(num1)), inFile1.good());
    bool hasNum2 = (inFile2.read(reinterpret_cast<char*>(&num2), sizeof(num2)), inFile2.good());

    while (hasNum1 && hasNum2) {
        if (num1 < num2) {
            outFile.write(reinterpret_cast<char*>(&num1), sizeof(num1));
            hasNum1 = (inFile1.read(reinterpret_cast<char*>(&num1), sizeof(num1)), inFile1.good());
        } else {
            outFile.write(reinterpret_cast<char*>(&num2), sizeof(num2));
            hasNum2 = (inFile2.read(reinterpret_cast<char*>(&num2), sizeof(num2)), inFile2.good());
        }
    }

    // 处理剩余的元素
    while (hasNum1) {
        outFile.write(reinterpret_cast<char*>(&num1), sizeof(num1));
        hasNum1 = (inFile1.read(reinterpret_cast<char*>(&num1), sizeof(num1)), inFile1.good());
    }

    while (hasNum2) {
        outFile.write(reinterpret_cast<char*>(&num2), sizeof(num2));
        hasNum2 = (inFile2.read(reinterpret_cast<char*>(&num2), sizeof(num2)), inFile2.good());
    }

    // 关闭文件
    inFile1.close();
    inFile2.close();
    outFile.close();
    
    remove(file1.c_str());
    remove(file2.c_str());
    std::string newName = "./intermediate/Inter" + std::to_string(a) + ".bin";
    rename(outputFile.c_str(), newName.c_str());
    
    std::unique_lock<std::mutex> lock(intermediateSetMutex);
    intermediateSet.insert(a);
    lock.unlock();
    SetState(State::ReadyToMergeIntermediates);
    cv.notify_one();
}

void SortManager::SetState(State newState){
    state = newState;
}