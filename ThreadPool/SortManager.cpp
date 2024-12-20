#include <iostream>
#include <limits>
#include <unistd.h>
#include "SortManager.h"
#define DEBUG_MODE 0
#if DEBUG_MODE
    #define LOG(text) std::cout << text;
#else
    #define LOG(text) // 不执行任何操作
#endif


SortManager::SortManager(const std::string& dir, size_t numThread, size_t bufferSize, size_t totalFileSize)
    :pool(numThread), numThread(numThread), bufferSize(bufferSize), totalFileSize(totalFileSize) {

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
    
    numIntermediate = totalFileSize / bufferSize; // 中间文件的个数
    if(totalFileSize % bufferSize != 0) numIntermediate++;
    
    buffer = std::make_unique<long long[]>(bufferSize / sizeof(long long));
    buffer2 = std::make_unique<long long[]>(bufferSize / sizeof(long long));
    state = State::Running;

    terminate.store(0);
    readyHeapsCount.store(0);
    runningMergeIntermediateCount.store(0);

    interDir = "./intermediate/"; // 中间文件目录
    fs::path dirPath(interDir);
    if (!fs::exists(dirPath)) { // 如果中间文件目录不存在，则创建中间目录
        if (!fs::create_directory(dirPath)) {
            LOG("无法创建目录：" + interDir +"\n");
            return;
        }
    }
    else{ // 如果中间文件目录已存在，则删除其内部的所有文件
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (fs::is_regular_file(entry.status())) {
                fs::remove(entry.path()); // 删除文件
            } else if (fs::is_directory(entry.status())) {
                fs::remove_all(entry.path()); // 删除子目录及其内容
            }
        }
    }
}

void SortManager::Run(){
    state = State::ReadyToReadToCache;
    while(true){
        std::unique_lock<std::mutex> lock(stateMutex);
        cvTask.wait(lock, [this]() { return state != State::Running; }); // 用条件变量控制主线程
        switch (state) {
        case State::Running:
            break;
        case State::ReadyToReadToCache: // 准备读数据到缓存
            pool.addTask([this]() { this->ReadToCache(); });
            SetState(State::Running);
            break;
        case State::ReadyToReadToHeaps: // 准备将缓存的数据读到堆中
            readyHeapsCount.store(0);
            for(size_t i = 0; i < numThread; i++){
                size_t threadIndex = i;
                pool.addTask([this, threadIndex]() { this->ReadToHeap(threadIndex); });
            }
            SetState(State::Running);
            break;
        case State::ReadyToMergeHeaps: // 准备合并堆
            pool.addTask([this]() { this->MergeHeaps(); });
            SetState(State::Running);
            break;
        case State::ReadyToMergeIntermediates:{ //准备合并中间文件
            size_t threadsToUse = std::min(numIntermediate, numThread);
            for(size_t i = 0; i < threadsToUse; i++){
                pool.addTask([this]() { this->MergeIntermediate(); });
            }
            SetState(State::Running);
            break;
        }
        case State::Stop: //停止
            return; // 退出循环
        }
    }
}

void SortManager::ReadToCache() { // 读取数据填满整块缓存
    // TODO：分块读取，提高并发度。思路：用fileStream存储当前读取位置，要读到缓存时，用临时变量存储当前读取位置，然后挪动fileStream到下一个块的读取位置。修改fileStream时加锁。
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    LOG("ReadToCache\n");
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
        // 获取当前文件剩余大小
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
    LOG("ReadToCacheFinished\n\n");
    std::unique_lock<std::mutex> lock2(stateMutex);
    SetState(State::ReadyToReadToHeaps);
    cvTask.notify_one();
}


void SortManager::ReadToHeap(size_t i) { // 将缓存数据中第i块读取到对应的堆中
    // TODO：如果能够分块读取缓存，ReadToCache和ReadToHeap可以合并，即读取完一块缓存后，直接将该块内容读到对应的堆中。
    std::string info = "ReadToHeap:" + std::to_string(i);
    LOG(info + "\n");
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
    LOG(info + "\n");
    if (readyHeapsCount.load() == numThread){
        LOG("ReadToHeap all finished\n\n");
        std::unique_lock<std::mutex> lock3(stateMutex);
        SetState(State::ReadyToMergeHeaps);
        cvTask.notify_one();
    }
}


void SortManager::MergeHeaps() {
    // 合并堆生成中间文件
    LOG("MergeHeaps\n");

    std::string fileName = "Inter";
    std::unique_lock<std::mutex> lock(intermediateQueueMutex);
    size_t interNum = intermediateQueue.size();
    lock.unlock();
    std::ofstream outFile(interDir + fileName + std::to_string(interNum) + ".bin");
    if (!outFile) {
        LOG("无法打开输出文件！\n");
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
    std::unique_lock<std::mutex> lock2(intermediateQueueMutex);
    intermediateQueue.push(interNum);
    outFile.close();
    LOG("MergeHeapsFinished\n\n");
    if(intermediateQueue.size() < numIntermediate){
        SetState(State::ReadyToReadToCache);
        cvTask.notify_one();
    }
    else{
        SetState(State::ReadyToMergeIntermediates);
        cvTask.notify_one();
    }
}

void SortManager::MergeIntermediate(){ // 将中间文件合并
    std::unique_lock<std::mutex> lock(intermediateQueueMutex);
    if(intermediateQueue.size() + runningMergeIntermediateCount.load() <= 1 && !terminate.load()) {
        std::unique_lock<std::mutex> lock2(terminateMutex);
        terminate.store(true);
        lock.unlock();
        cvTerminate.wait(lock2, [this]() { return runningMergeIntermediateCount.load() == 0; }); //等待剩下的中间文件合并任务执行完毕
        
        LOG("MergeIntermediateAllFinished\n\n");
        // 将中间文件重命名
        std::string oldName = interDir + "Inter0.bin";
        std::string newDir = "./result/";
        std::string newName = newDir + "sorted.bin";
        
        fs::path dirPath(newDir);
        if (!fs::exists(newDir)) {
            if (!fs::create_directory(newDir)) {
                LOG("无法创建目录: "+ newDir + "\n");
            }
        }

        if (rename(oldName.c_str(), newName.c_str()) != 0) {
            LOG("重命名文件失败: \n" + oldName);
        }
        rmdir(interDir.c_str());

        SetState(State::Stop);
        cvTask.notify_one();
        return;
    }
    else if(intermediateQueue.size() <= 1){
        return;
    }
    LOG("MergeIntermediate\n");
    runningMergeIntermediateCount.fetch_add(1, std::memory_order_relaxed);
    size_t a = intermediateQueue.front();
    intermediateQueue.pop();
    size_t b = intermediateQueue.front();
    intermediateQueue.pop();
    lock.unlock();

    MergeTwoIntermediate(a, b);
}

void SortManager::MergeTwoIntermediate(size_t a, size_t b){ // 将中间文件a和中间文件b合并
    if(a > b){
        size_t t = a;
        a = b;
        b = t;
    }
    std::string file1 = interDir + "Inter" + std::to_string(a) + ".bin";
    std::string file2 = interDir + "Inter" + std::to_string(b) + ".bin";
    std::string outputFile = interDir + "inter" + std::to_string(a) + ".bin";
    std::ifstream inFile1(file1, std::ios::binary);
    std::ifstream inFile2(file2, std::ios::binary);
    std::ofstream outFile(outputFile, std::ios::binary);

    if (!inFile1.is_open() || !inFile2.is_open() || !outFile.is_open()) {
        LOG("无法打开文件。\n");
        return;
    }
    
    // long long num1, num2;
    // bool hasNum1 = (inFile1.read(reinterpret_cast<char*>(&num1), sizeof(num1)), inFile1.good());
    // bool hasNum2 = (inFile2.read(reinterpret_cast<char*>(&num2), sizeof(num2)), inFile2.good());

    // while (hasNum1 && hasNum2) {
    //     if (num1 < num2) {
    //         outFile.write(reinterpret_cast<char*>(&num1), sizeof(num1));
    //         hasNum1 = (inFile1.read(reinterpret_cast<char*>(&num1), sizeof(num1)), inFile1.good());
    //     } else {
    //         outFile.write(reinterpret_cast<char*>(&num2), sizeof(num2));
    //         hasNum2 = (inFile2.read(reinterpret_cast<char*>(&num2), sizeof(num2)), inFile2.good());
    //     }
    // }

    // // 处理剩余的元素
    // while (hasNum1) {
    //     outFile.write(reinterpret_cast<char*>(&num1), sizeof(num1));
    //     hasNum1 = (inFile1.read(reinterpret_cast<char*>(&num1), sizeof(num1)), inFile1.good());
    // }

    // while (hasNum2) {
    //     outFile.write(reinterpret_cast<char*>(&num2), sizeof(num2));
    //     hasNum2 = (inFile2.read(reinterpret_cast<char*>(&num2), sizeof(num2)), inFile2.good());
    // }

    std::unique_lock<std::shared_mutex> cacheLock(cacheMutex);
    size_t idxMax = bufferSize / sizeof(long long);
    while(!inFile1.eof() && !inFile2.eof()){
        inFile1.read(reinterpret_cast<char*>(buffer.get()), bufferSize);
        inFile2.read(reinterpret_cast<char*>(buffer2.get()), bufferSize);
        size_t idx1 = 0, idx2 = 0;
        long long num1, num2;
        while(idx1 < idxMax && idx2 < idxMax){
            num1 = buffer[idx1];
            num2 = buffer2[idx2];
            if(num1 < num2){
                idx1++;
                outFile.write(reinterpret_cast<char*>(&num1), sizeof(num1));
            }
            else{
                idx2++;
                outFile.write(reinterpret_cast<char*>(&num2), sizeof(num2));
            }
        }
        while(idx1 < idxMax){
            num1 = buffer[idx1];
            idx1++;
            outFile.write(reinterpret_cast<char*>(&num1), sizeof(num1));
        }
        while(idx2 < idxMax){
            num2 = buffer2[idx2];
            idx2++;
            outFile.write(reinterpret_cast<char*>(&num1), sizeof(num2));
        }
    }
    while(!inFile1.eof()){
        inFile1.read(reinterpret_cast<char*>(buffer.get()), idxMax);
        size_t idx1 = 0;
        long long num1;
        while(idx1 < idxMax){
            num1 = buffer[idx1];
            idx1++;
            outFile.write(reinterpret_cast<char*>(&num1), sizeof(num1));
        }
    }
    while(!inFile2.eof()){
        inFile2.read(reinterpret_cast<char*>(buffer.get()), idxMax);
        size_t idx2 = 0;
        long long num2;
        while(idx2 < idxMax){
            num2 = buffer[idx2];
            idx2++;
            outFile.write(reinterpret_cast<char*>(&num2), sizeof(num2));
        }
    }
    cacheLock.unlock();

    // 关闭文件
    inFile1.close();
    inFile2.close();
    outFile.close();
    
    std::string newName = interDir + "Inter" + std::to_string(a) + ".bin";
    if (remove(file1.c_str()) != 0) {
        LOG("删除文件失败: \n" + file1);
    }
    if (remove(file2.c_str()) != 0) {
        LOG("删除文件失败: \n" + file2);
    }
    if (rename(outputFile.c_str(), newName.c_str()) != 0) {
        LOG("重命名文件失败: \n" + outputFile);
    }
    std::unique_lock<std::mutex> lock(intermediateQueueMutex);
    intermediateQueue.push(a);
    lock.unlock();
    runningMergeIntermediateCount.fetch_sub(1, std::memory_order_relaxed);
    if(terminate.load()){
        std::unique_lock<std::mutex> terminatelock(terminateMutex);
        cvTerminate.notify_one();
    }
    
    SetState(State::ReadyToMergeIntermediates);
    cvTask.notify_one();
}

void SortManager::SetState(State newState){ //修改当前状态
    state = newState;
}