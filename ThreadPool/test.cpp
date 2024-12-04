#include "SortManager.h"

const std::string DIR_Path = "/data";
const int NUM_THREAD = 4;
const size_t BUFFER_SIZE = 32 * 1024 * 1024;
const size_t TOTAL_FILE_SIZE = 100 * 1024 * 1024;

int main() {
    SortManager manager(DIR_Path, NUM_THREAD, BUFFER_SIZE, TOTAL_FILE_SIZE);
    manager.Run();
    return 0;
}