#include <cstddef>
#include <iostream>
#include <chrono>
#include <cstring>
#include <random>
#include <fcntl.h>
#include <unistd.h> // for read and write
#include <iomanip> // for std::setprecision
#include "CachedFileOperator.h"

#define CACHED_TEST_FILE "test_with_cache.txt"
#define UNCACHED_TEST_FILE "text_without_cache.txt"
#define DATA_SIZE 1024 * 1024 // 1 MB
#define NUM_OPERATIONS 50 // Number of operations
#define MAX_OFFSET (DATA_SIZE - 1024) // Maximum offset for reads/writes

// Check and delete existing test files
void prepareTestFiles() {
    if (std::fopen(CACHED_TEST_FILE, "r")) {
        std::remove(CACHED_TEST_FILE);
    }
    if (std::fopen(UNCACHED_TEST_FILE, "r")) {
        std::remove(UNCACHED_TEST_FILE);
    }
}

char randomChar() {
    static std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    std::uniform_int_distribution<int> dist(0, 25);
    return 'A' + dist(rng); // Generate random uppercase letters A-Z
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
        return false; // Different sizes
    }

    return memcmp(buffer1, buffer2, bytesRead1) == 0; // Compare contents
}

void testFileOperations() {
    CachedFileOperator* cfo = CachedFileOperator::getInstance();
    
    cfo->open(CACHED_TEST_FILE);
    int fd = open(UNCACHED_TEST_FILE, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | O_DIRECT);

    double totalCachedWriteTime = 0.0;
    double totalUncachedWriteTime = 0.0;
    double totalCachedReadTime = 0.0;
    double totalUncachedReadTime = 0.0;

    std::uniform_int_distribution<size_t> offsetDist(0, MAX_OFFSET);
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        size_t off = offsetDist(rng); // Random offset for each operation
        char* dataToWrite = new char[DATA_SIZE];
        fillRandomData(dataToWrite, DATA_SIZE);

        // Cached write
        cfo->lseek(off, SEEK_SET);
        auto startWriteCached = std::chrono::high_resolution_clock::now();
        cfo->write(dataToWrite, DATA_SIZE);
        auto endWriteCached = std::chrono::high_resolution_clock::now();
        totalCachedWriteTime += std::chrono::duration<double>(endWriteCached - startWriteCached).count();

        // Uncached write
        lseek(fd, off, SEEK_SET);
        auto startWriteUncached = std::chrono::high_resolution_clock::now();
        write(fd, dataToWrite, DATA_SIZE);
        auto endWriteUncached = std::chrono::high_resolution_clock::now();
        totalUncachedWriteTime += std::chrono::duration<double>(endWriteUncached - startWriteUncached).count();

        delete[] dataToWrite;

        // Simulate frequent reads from random offsets
        for (int j = 0; j < 1024; ++j) { // Repeat reading from different offsets
            size_t readOff = offsetDist(rng); // Randomize read offset
            char cachedBuffer[4097] = {0};
            cfo->lseek(readOff, SEEK_SET);
            auto startReadCached = std::chrono::high_resolution_clock::now();
            cfo->read(cachedBuffer, 4096);
            auto endReadCached = std::chrono::high_resolution_clock::now();
            totalCachedReadTime += std::chrono::duration<double>(endReadCached - startReadCached).count();

            // Uncached read
            char uncachedBuffer[4097] = {0};
            lseek(fd, readOff, SEEK_SET);
            auto startReadUncached = std::chrono::high_resolution_clock::now();
            read(fd, uncachedBuffer, 4096);
            auto endReadUncached = std::chrono::high_resolution_clock::now();
            totalUncachedReadTime += std::chrono::duration<double>(endReadUncached - startReadUncached).count();

            // bool contentsAreEqual = (memcmp(cachedBuffer, uncachedBuffer, 10) == 0);

            // if (contentsAreEqual) {
            //     std::cout << "Contents are consistent between cached and uncached files after operation " << (i + 1) << "." << std::endl;
            // } else {
            //     std::cout << "Contents are inconsistent between cached and uncached files after operation " << (i + 1) << "!" << std::endl;
            // }
        }
    }

    // Output formatted results
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Total cached write time: " << totalCachedWriteTime << " seconds" << std::endl;
    std::cout << "Total uncached write time: " << totalUncachedWriteTime << " seconds" << std::endl;
    std::cout << "Total cached read time: " << totalCachedReadTime << " seconds" << std::endl;
    std::cout << "Total uncached read time: " << totalUncachedReadTime << " seconds" << std::endl;

    close(fd);
    cfo->flush();
}

int main() {
    prepareTestFiles(); // Prepare test files

    testFileOperations();

    if (compareFiles(CACHED_TEST_FILE, UNCACHED_TEST_FILE)) {
        std::cout << "Both files are consistent." << std::endl;
    } else {
        std::cout << "Files are inconsistent!" << std::endl;
    }

    return 0;
}