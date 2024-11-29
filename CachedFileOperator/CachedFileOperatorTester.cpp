#include <iostream>
#include <chrono>
#include <cstring>
#include <random>
#include <fcntl.h>
#include <unistd.h> // for read and write
#include <iomanip> // for std::setprecision
#include <filesystem> // for file operations
#include "CachedFileOperator.h"

#define CACHED_TEST_FILE "test_with_cache.txt"
#define UNCACHED_TEST_FILE "text_without_cache.txt"
#define DATA_SIZE 1024 * 1024// 1 MB
#define NUM_OPERATIONS 10 // 每种操作的次数

// 检查并删除已存在的测试文件
void prepareTestFiles() {
    if (std::filesystem::exists(CACHED_TEST_FILE)) {
        std::filesystem::remove(CACHED_TEST_FILE);
    }
    if (std::filesystem::exists(UNCACHED_TEST_FILE)) {
        std::filesystem::remove(UNCACHED_TEST_FILE);
    }
}

char randomChar() {
    static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist(0, 25);
    return 'A' + dist(rng); // 生成 A-Z 的随机字母
}

void fillRandomData(char* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        data[i] = randomChar();
    }
}

bool compareFiles(const std::string& file1, const std::string& file2) {
    int fd1 = open(file1.c_str(), O_RDONLY);
    int fd2 = open(file2.c_str(), O_RDONLY);

    if (fd1 == -1 || fd2 == -1) {
        std::cerr << "Failed to open one of the files for comparison." << std::endl;
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

void testFileOperations() {
    CachedFileOperator* cfo = CachedFileOperator::getInstance();
    
    cfo->open(CACHED_TEST_FILE);
    int fd = open(UNCACHED_TEST_FILE, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

    double totalCachedWriteTime = 0.0;
    double totalUncachedWriteTime = 0.0;
    double totalCachedReadTime = 0.0;
    double totalUncachedReadTime = 0.0;

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        char* dataToWrite = new char[DATA_SIZE];
        fillRandomData(dataToWrite, DATA_SIZE);

        // 带缓存写
        cfo->lseek(0, SEEK_SET);
        auto startWriteCached = std::chrono::high_resolution_clock::now();
        cfo->write(dataToWrite, DATA_SIZE);
        auto endWriteCached = std::chrono::high_resolution_clock::now();
        totalCachedWriteTime += std::chrono::duration<double>(endWriteCached - startWriteCached).count();

        // 不带缓存写
        lseek(fd, 0, SEEK_SET);
        auto startWriteUncached = std::chrono::high_resolution_clock::now();
        write(fd, dataToWrite, DATA_SIZE);
        auto endWriteUncached = std::chrono::high_resolution_clock::now();
        totalUncachedWriteTime += std::chrono::duration<double>(endWriteUncached - startWriteUncached).count();

        delete[] dataToWrite;

        // 模拟频繁读取
        for (int j = 0; j < 5; ++j) { // 重复读取相同的数据块
            char cachedBuffer[11] = {0};
            cfo->lseek(0, SEEK_SET);
            auto startReadCached = std::chrono::high_resolution_clock::now();
            cfo->read(cachedBuffer, 10);
            auto endReadCached = std::chrono::high_resolution_clock::now();
            totalCachedReadTime += std::chrono::duration<double>(endReadCached - startReadCached).count();

            // 不带缓存读
            char uncachedBuffer[11] = {0};
            lseek(fd, 0, SEEK_SET);
            auto startReadUncached = std::chrono::high_resolution_clock::now();
            read(fd, uncachedBuffer, 10);
            auto endReadUncached = std::chrono::high_resolution_clock::now();
            totalUncachedReadTime += std::chrono::duration<double>(endReadUncached - startReadUncached).count();

            bool contentsAreEqual = (memcmp(cachedBuffer, uncachedBuffer, 10) == 0);

            if (contentsAreEqual) {
                std::cout << "Contents are consistent between cached and uncached files after operation " << (i + 1) << "." << std::endl;
            } else {
                std::cout << "Contents are inconsistent between cached and uncached files after operation " << (i + 1) << "!" << std::endl;
            }

            // 输出读取到的内容
            //std::cout << "Cached Read Content: " << cachedBuffer << std::endl;
            //std::cout << "Uncached Read Content: " << uncachedBuffer << std::endl;
        }
    }
    
    // 设置输出格式为固定小数点格式，并保留6位小数
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Total cached write time: " << totalCachedWriteTime << " seconds" << std::endl;
    std::cout << "Total uncached write time: " << totalUncachedWriteTime << " seconds" << std::endl;
    std::cout << "Total cached read time: " << totalCachedReadTime << " seconds" << std::endl;
    std::cout << "Total uncached read time: " << totalUncachedReadTime << " seconds" << std::endl;

    close(fd);
    cfo->flush();
}

int main() {
    prepareTestFiles(); // 检查并准备测试文件

    testFileOperations();

    if (compareFiles(CACHED_TEST_FILE, UNCACHED_TEST_FILE)) {
        std::cout << "Both files are consistent." << std::endl;
    } 
    else {
        std::cout << "Files are inconsistent!" << std::endl;
    }

    return 0;
}