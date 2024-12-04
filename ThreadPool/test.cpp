#include "SortManager.h"

const std::string DIR_Path = "./data";
const int NUM_THREAD = 4;
const size_t BUFFER_SIZE = 512 * 1024;
const size_t TOTAL_FILE_SIZE = 10 * 1024 * 1024;

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
    SortManager manager(DIR_Path, NUM_THREAD, BUFFER_SIZE, TOTAL_FILE_SIZE);
    manager.Run();
}

void test() {
    const std::string directoryPath = "./intermediate/";

    // 遍历指定目录中的所有文件
    for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
        if (entry.is_regular_file()) { // 确保是常规文件
            std::string filePath = entry.path().string();
            if (!isSorted(filePath)) {
                std::cout << "文件 " << filePath << " 中的数据未按升序排列。" << std::endl;
                return;
            }
        }
    }
    std::cout << "所有文件中的数据按升序排列。" << std::endl;
}


int main() {
    //excute();
    test();
    return 0;
}