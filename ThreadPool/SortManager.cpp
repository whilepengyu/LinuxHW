#include "SortManager.h"
#include <iostream>

SortManager::SortManager(const std::string& dir, int numThread, size_t bufferSize, size_t totalDataSize)
    :pool(numThread), bufferSize(bufferSize), totalDataSize(totalDataSize) {
    
    // 初始化文件迭代器
    dirIter = fs::directory_iterator(dir);
    endIter = fs::end(dirIter); // 获取目录结束迭代器

    // 打开第一个文件
    if (dirIter != endIter) {
        fileStream.open(dirIter->path(), std::ios::binary);
        if (!fileStream) {
            throw std::runtime_error("Failed to open file: " + dirIter->path().string());
        }
    }

    blockSize = bufferSize / numThread; // 每个线程处理的数据块大小
    buffer = std::shared_ptr<long long[]>(new long long[bufferSize / sizeof(long long)]);
    
    heaps.resize(numThread);
    for (int i = 0; i < numThread; ++i) {
        heaps[i] = std::make_unique<Heap>(); // 初始化每个线程的堆        
    }
}


void SortManager::ReadToHeap() {
    for (size_t i = 0; i < filePaths.size(); ++i) {
        pool.enqueue([this, i]() {
            ReadOneBlockFromFiles(i);
        });
    }
}

void SortManager::ReadOneBlockFromFiles(size_t fileIndex) {
    size_t count = 0;
    while(count < blockSize){
        if (!fileStream.is_open()) {
            // 如果当前文件流未打开，尝试打开下一个文件
            if (++dirIter != endIter) {
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
        fileStream.read(reinterpret_cast<char*>(buffer.get() + count), remainingBytes);
        
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
}