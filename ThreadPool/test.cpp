#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <thread>
#include <functional>
#include <atomic>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>

#define TOTAL_CACHE_SIZE (64 * 1024 * 1024) // 64MB
#define THREAD_NUM 4
std::shared_ptr<long long[]> cacheBuffer(new long long[TOTAL_CACHE_SIZE / sizeof(long long)]); // 使用 shared_ptr 管理缓存

// 定义任务结构体
struct Task {
    std::function<void()> func; // 使用 std::function 来存储可调用对象
};

void ReadToHeap()


// 主函数
int main() {
    ThreadPool pool(THREAD_NUM); // 创建一个线程池，包含4个工作线程

   
    return 0;
}