#include "ThreadPool.h"

int main() {
    const int numThreads = std::thread::hardware_concurrency();
    ThreadPool pool(numThreads);

    const int numFiles = 100000; // 假设有10万个文件
    for (int i = 0; i < numFiles; ++i) {
        pool.enqueue("data/file" + std::to_string(i) + ".txt");
    }

    return 0;
}