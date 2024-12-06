#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <cassert>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

// 生成指定数量的随机64位有符号整数
std::vector<int64_t> generate_random_data(size_t num_integers) {
    std::vector<int64_t> data;
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int64_t> dist(-9223372036854775807LL - 1, 9223372036854775807LL);
    
    for (size_t i = 0; i < num_integers; ++i) {
        data.push_back(dist(gen));
    }
    
    return data;
}

// 将数据写入指定文件
void write_file(const std::string &file_path, const std::vector<int64_t> &data) {
    std::ofstream ofs(file_path, std::ios::binary);
    if (!ofs) {
        throw std::runtime_error("无法打开文件: " + file_path);
    }
    
    ofs.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(int64_t));
}

// 生成文件大小数组，确保总大小为目标值且每个文件大小不一样
std::vector<size_t> generate_file_sizes(size_t total_size, size_t total_files) {
    const size_t base_size_kb = 64; // 每个文件初始为64KB
    std::vector<size_t> sizes(total_files, base_size_kb * 1024); // 初始化

    size_t target_size_bytes = total_size;
    size_t current_total_size = sizes.size() * sizes[0];
    ssize_t remaining_size = target_size_bytes - current_total_size;

    // 随机分配剩余的字节数
    std::random_device rd;
    std::mt19937 gen(rd());
    
    while (remaining_size > 0) {
        for (size_t i = 0; i < total_files; ++i) {
            if (remaining_size <= 0) break;

            // 随机增加文件大小，确保不超过一定上限（例如2GB），并保持为8的倍数
            std::uniform_int_distribution<ssize_t> dist(0, size_t(32) * (1024 * 1024));
            ssize_t additional_size = std::min(remaining_size, dist(gen));
            additional_size -= additional_size % 8; // 确保增加的大小是8的倍数
            
            sizes[i] += additional_size;
            remaining_size -= additional_size;
        }
    }

    // 打乱文件大小顺序以确保每个文件大小不一样
    std::shuffle(sizes.begin(), sizes.end(), gen);

    assert(remaining_size >= 0 && "Remaining size should not be negative!");

    // 确保所有文件大小都是8的倍数
    for (size_t &size : sizes) {
        size -= (size % 8);
    }

    return sizes;
}

// 在指定目录中生成固定数量的文件，确保总大小达到目标且每个文件大小不一样
void generate_files(const std::string &directory, size_t total_size, size_t total_files) {
    auto sizes = generate_file_sizes(total_size, total_files);

    // 创建文件并写入数据
    for (size_t file_count = 0; file_count < total_files; ++file_count) {
        size_t num_integers = sizes[file_count] / sizeof(int64_t); // 确保整数数量正确
        assert(sizes[file_count] % sizeof(int64_t) == 0 && "整除失败");
        
        auto data = generate_random_data(num_integers);

        // 创建文件路径
        std::string file_name = "data_file_" + std::to_string(file_count + 1) + ".bin";
        std::string file_path = fs::path(directory) / file_name;

        write_file(file_path, data);

        std::cout << "生成文件: " << file_name << ", 大小: " << fs::file_size(file_path) << " 字节" << std::endl;
    }
}

void generate_big_files(const std::string &directory, size_t total_size, size_t total_files, size_t small_files) {
    // std::random_device rd; // 用于生成随机种子
    // std::mt19937 gen(rd()); // Mersenne Twister引擎

    // // 设置均匀分布，范围从0到10（包含10）
    // std::uniform_int_distribution<int> dist(-10, 10);
    std::vector<size_t> sizes;
    for(size_t i = 0; i < total_files; i++){
        sizes.push_back(1ULL * 1024 * 1024 * 1024);
    }

    // 创建文件并写入数据
    for (size_t file_count = 0; file_count < total_files; ++file_count) {
        size_t num_integers = sizes[file_count] / sizeof(int64_t); // 确保整数数量正确
        assert(sizes[file_count] % sizeof(int64_t) == 0 && "整除失败");
        
        auto data = generate_random_data(num_integers);

        // 创建文件路径
        std::string file_name = "data_file_" + std::to_string(file_count + small_files) + ".bin";
        std::string file_path = fs::path(directory) / file_name;

        write_file(file_path, data);

        std::cout << "生成文件: " << file_name << ", 大小: " << fs::file_size(file_path) << " 字节" << std::endl;
    }
}

int main() {
    const std::string output_directory = "./data"; // 指定输出目录
    fs::create_directories(output_directory); // 创建目录
    
    size_t target_size = 4ULL * 1024 * 1024 * 1024; // 目标大小
    size_t total_files = 1000 - 4; // 文件数量
    
    generate_files(output_directory, target_size, total_files);
    size_t big_target_size = 8ULL * 1024 * 1024 * 1024;
    size_t bit_total_files = 4;
    generate_big_files(output_directory, big_target_size, bit_total_files, total_files);
    return 0;
}