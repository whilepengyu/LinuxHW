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
#include "SortManager.h"

const std::string DIR_Path = "/data";
const int NUM_THREAD = 4;
const size_t BUFFER_SIZE = 32 * 1024 * 1024;

int main() {
    SortManager manager(DIR_Path, NUM_THREAD, BUFFER_SIZE);
    manager.Run();
    return 0;
}