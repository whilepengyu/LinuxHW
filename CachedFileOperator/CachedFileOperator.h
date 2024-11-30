#ifndef CachedFileOperator_H
#define CachedFileOperator_H
#include <memory>
#include <cstring>
#include <string>
#include <unordered_map>
#include <list>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <csignal> 

typedef struct BlcokInfo{
    size_t cacheBufferOffset;
    size_t blockValidSize;
}BlcokInfo;

class CachedFileOperator {
public:
    static const size_t CACHE_BUFFER_SIZE = 64 * 1024 * 1024; // 缓存大小：64MB
    static const size_t BLOCK_SIZE =  1024 * 1024; // 块大小：1KB
    static const size_t CACHE_MAX_BLOCK = 64; //1024个块
public:
    static CachedFileOperator* getInstance(); // 单例模式

    void open(const std::string& fileName); // 按文件名打开文件
    void lseek(size_t offset, int whence); // 按指定方式获取文件偏移量
    void read(char* buffer, size_t size); // 先尝试从缓存中读取数据，如果没有则读文件
    void write(const char* data, size_t size); //在缓存中写数据，如果缓存数据被淘汰则写入文件
    void close(); //关闭文件
    void flush(); //将缓存数据写入文件
private:
    static void OnProcessExit();

private:
    CachedFileOperator(); // 私有构造函数
    ~CachedFileOperator(); // 私有析构函数

    // LRU缓存相关
    void writeCache(size_t blockIndex, const char* dataBlock, size_t dataSize, size_t blockOffset); // 写缓存
    char* readCache(size_t blockIndex); //读缓存
    std::unique_ptr<char[]> p_cacheBuffer; // 缓存缓冲区

    std::unordered_map<size_t, BlcokInfo> m_cache; // 缓存哈希表 (块号 -> (缓冲区偏移量, 块有效大小))
    std::list<size_t> m_accessOrder; // 访问顺序
    
    int m_fd; // 文件描述符
    off_t m_pos; // 文件偏移量

    static CachedFileOperator* m_CFO;

private:
    static void signalHandler(int signal);
};

#endif // CachedFileOperator