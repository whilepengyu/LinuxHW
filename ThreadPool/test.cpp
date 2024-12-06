#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include "SortManager.h"
const std::string DIR_Path = "./data";
const int NUM_THREAD = 8;
const size_t BUFFER_SIZE = 64 * 1024 * 1024;
const size_t TOTAL_FILE_SIZE = 8ULL * 1024 * 1024 * 1024;

bool isSorted(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return false;
    }

    long long previous; // 用于存储上一个读取的整数
    long long current;

    file >> previous;

    // 逐行读取文件中的整数
    while (file >> current) {
        if (current < previous) {
            //std::cout << "文件中的数据未按升序排列。" << std::endl;
            return false; // 如果当前数小于前一个数，则返回 false
        }
        previous = current; // 更新前一个数
    }

    //std::cout << "文件中的数据按升序排列。" << std::endl;
    return true; // 如果顺序正确，返回 true
}
void excute(){
    auto start = std::chrono::high_resolution_clock::now();// 开始计时
    SortManager manager(DIR_Path, NUM_THREAD, BUFFER_SIZE, TOTAL_FILE_SIZE);
    manager.Run();
    auto end = std::chrono::high_resolution_clock::now(); // 结束计时
    std::chrono::duration<double> duration = end - start; // 计算持续时间
    std::cout << "excute() 执行时间: " << duration.count() << " 秒" << std::endl; // 输出执行时间
}

void test() {
    const std::string directoryPath = "./result/";

    // 遍历指定目录中的所有文件(一开始是为了测试中间排序结果，最终结果只剩一个文件，也可以测试)
    for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
        if (entry.is_regular_file()) { // 确保是常规文件
            std::string filePath = entry.path().string();
            if (!isSorted(filePath)) {
                std::cout << "文件 " << filePath << " 中的数据未按升序排列。" << std::endl;
                return;
            }
        }
    }
    std::cout << "数据按照升序排列。" << std::endl;
}


int main() {
    excute(); // 执行排序操作
    test();
    return 0;
}