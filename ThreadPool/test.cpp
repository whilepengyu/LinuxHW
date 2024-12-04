#include "SortManager.h"

const std::string DIR_Path = "/home/soyo/Projects/LinuxHW/ThreadPool/data";
const int NUM_THREAD = 4;
const size_t BUFFER_SIZE = 128 * 1024;
const size_t TOTAL_FILE_SIZE = 10 * 1024 * 1024;

int main() {
    SortManager manager(DIR_Path, NUM_THREAD, BUFFER_SIZE, TOTAL_FILE_SIZE);
    manager.Run();
    return 0;
}