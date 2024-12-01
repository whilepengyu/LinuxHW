#include <cstddef>
#include <iostream>
#include <chrono>
#include <cstring>
#include <random>
#include <fcntl.h>
#include <unistd.h>
#include <iomanip>
#include "CachedFileOperator.h"

#define CACHED_TEST_FILE "test_with_cache.txt" // 带缓存的测试文件
#define UNCACHED_TEST_FILE "text_without_cache.txt" // 不带缓存的测试文件
#define DATA_SIZE 1024 * 1024 // 单次写数据大小：1 MB
#define READ_SIZE 1024 * 1024 // 一次读取大小
#define NUM_OPERATIONS 1024 // 操作次数
#define NUM_REPEAT 128 //重复次数
// 每次测试重新生成测试文件
void prepareTestFiles() {
    if (std::fopen(CACHED_TEST_FILE, "r")) {
        std::remove(CACHED_TEST_FILE); // 删除带缓存的测试文件
    }
    if (std::fopen(UNCACHED_TEST_FILE, "r")) {
        std::remove(UNCACHED_TEST_FILE); // 删除不带缓存的测试文件
    }
}

// 生成随机字符
char randomChar() {
    static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist(0, 25);
    return 'A' + dist(rng); // 生成随机的大写字母 A-Z
}

// 填充随机数据
void fillRandomData(char* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        data[i] = randomChar();
    }
}

// 比较两个文件是否相同
bool compareFiles(const std::string& file1, const std::string& file2) {
    int fd1 = open(file1.c_str(), O_RDONLY);
    int fd2 = open(file2.c_str(), O_RDONLY);

    if (fd1 == -1 || fd2 == -1) {
        std::cerr << "打开文件进行比较失败。" << std::endl; // 输出错误信息
        return false;
    }

    char buffer1[DATA_SIZE];
    char buffer2[DATA_SIZE];

    ssize_t bytesRead1 = read(fd1, buffer1, DATA_SIZE);
    ssize_t bytesRead2 = read(fd2, buffer2, DATA_SIZE);

    close(fd1);
    close(fd2);

    if (bytesRead1 != bytesRead2) {
        return false; // 文件大小不同
    }

    return memcmp(buffer1, buffer2, bytesRead1) == 0; // 比较内容
}

// 测试文件操作
void testFileOperations() {
    CachedFileOperator* cfo = CachedFileOperator::getInstance();
    
    cfo->open(CACHED_TEST_FILE); // 打开带缓存的测试文件
    int fd = open(UNCACHED_TEST_FILE, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | O_DIRECT); // 打开不带缓存的测试文件

    double totalCachedWriteTime = 0.0; // 总缓存写入时间
    double totalUncachedWriteTime = 0.0; // 总不带缓存写入时间
    double totalCachedReadTime = 0.0; // 总缓存读取时间
    double totalUncachedReadTime = 0.0; // 总不带缓存读取时间

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        // 获取当前文件的大小以计算最大偏移量
        off_t fileSize = lseek(fd, 0, SEEK_END); // 获取当前文件大小
        size_t maxOffset = fileSize; // 最大偏移量
        std::uniform_int_distribution<size_t> offsetDist(0, maxOffset); // 随机偏移量分布
        std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count()); // 随机数生成器
        size_t off = offsetDist(rng); // 每次操作的随机偏移量
        char* dataToWrite = new char[DATA_SIZE];
        fillRandomData(dataToWrite, DATA_SIZE); // 填充随机数据
        for (int j = 0; j < NUM_REPEAT; ++j){
            // 带缓存写入
            cfo->lseek(off, SEEK_SET);
            auto startWriteCached = std::chrono::high_resolution_clock::now();
            cfo->write(dataToWrite, DATA_SIZE);
            auto endWriteCached = std::chrono::high_resolution_clock::now();
            totalCachedWriteTime += std::chrono::duration<double>(endWriteCached - startWriteCached).count();

            // 不带缓存写入
            lseek(fd, off, SEEK_SET);
            auto startWriteUncached = std::chrono::high_resolution_clock::now();
            write(fd, dataToWrite, DATA_SIZE);
            auto endWriteUncached = std::chrono::high_resolution_clock::now();
            totalUncachedWriteTime += std::chrono::duration<double>(endWriteUncached - startWriteUncached).count();
        }

        delete[] dataToWrite;

        size_t readOff = offsetDist(rng); // 随机化读取偏移量Q
        // 模拟从随机偏移量频繁读取数据
        for (int j = 0; j < NUM_REPEAT; ++j) { // 从不同偏移量重复读取数据
            char cachedBuffer[READ_SIZE + 1] = {0};
            cfo->lseek(readOff, SEEK_SET);
            auto startReadCached = std::chrono::high_resolution_clock::now();
            cfo->read(cachedBuffer, READ_SIZE);
            auto endReadCached = std::chrono::high_resolution_clock::now();
            totalCachedReadTime += std::chrono::duration<double>(endReadCached - startReadCached).count();

            // 不带缓存读取
            char uncachedBuffer[READ_SIZE + 1] = {0};
            lseek(fd, readOff, SEEK_SET);
            auto startReadUncached = std::chrono::high_resolution_clock::now();
            read(fd, uncachedBuffer, READ_SIZE);
            auto endReadUncached = std::chrono::high_resolution_clock::now();
            totalUncachedReadTime += std::chrono::duration<double>(endReadUncached - startReadUncached).count();

            bool contentsAreEqual = (memcmp(cachedBuffer, uncachedBuffer, 10) == 0);

            if (contentsAreEqual) {
                // std::cout << "操作 " << (i + 1) << " 后，缓存和不缓存文件内容一致。" << std::endl;
            } 
            else {
                std::cout << "操作 " << (i + 1) << " 后，缓存和不缓存文件内容不一致！" << std::endl;
            }   
        }
    }

    // 输出格式化结果
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "带缓存-总写入时间: " << totalCachedWriteTime << " 秒" << std::endl;
    std::cout << "不带缓存-总写入时间: " << totalUncachedWriteTime << " 秒" << std::endl;
    std::cout << "带缓存-总读取时间: " << totalCachedReadTime << " 秒" << std::endl;
    std::cout << "不带缓存-总读取时间: " << totalUncachedReadTime << " 秒" << std::endl;

    close(fd); // 关闭不带缓存的文件描述符
    cfo->flush(); // 刷新缓存数据到文件
}

int main() {
    prepareTestFiles(); // 准备测试文件

    testFileOperations(); // 执行文件操作测试

    if (compareFiles(CACHED_TEST_FILE, UNCACHED_TEST_FILE)) {
        std::cout << "两个文件内容一致。" << std::endl; // 输出一致性检查结果
    } else {
        std::cout << "文件内容不一致！" << std::endl; // 输出不一致性检查结果
    }

    return 0;
}