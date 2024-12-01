#include "CachedFileOperator.h"

CachedFileOperator* CachedFileOperator::m_CFO = nullptr;

CachedFileOperator::CachedFileOperator()
    : p_cacheBuffer(new char[CACHE_BUFFER_SIZE]), m_cache(CACHE_MAX_BLOCK), m_fd(-1) {
}

CachedFileOperator::~CachedFileOperator() {
    
}

void CachedFileOperator::OnProcessExit()
{
    CachedFileOperator* cfo = CachedFileOperator::getInstance();
    if(cfo != nullptr){
        try {
            cfo->flush(); // 确保缓存数据写入文件
        } 
        catch (const std::runtime_error& e) {
            std::cerr << "Error during flush: " << e.what() << std::endl;
        }
        cfo->close(); //退出时关闭文件
    }
}

void CachedFileOperator::signalHandler(int signal) {
    // 处理信号时调用 OnProcessExit
    OnProcessExit();
    exit(signal); // 退出程序
}

void CachedFileOperator::flush() {
    if(m_fd == -1){
        std::cout << "In flush(): No specified file" << std::endl;
        return;
    }
    for (const auto& block : m_accessOrder) { // 遍历访问顺序列表
        auto it = m_cache.find(block);
        if (it != m_cache.end()) { // 如果缓存中有该块
            size_t blockIndex = it->first;
            size_t offset = it->second.cacheBufferOffset; // 获取缓存偏移量
            size_t blockValidSize = it->second.blockValidSize; // 获取块有效大小
            lseek(blockIndex * BLOCK_SIZE, SEEK_SET);
            ssize_t writtenBytes = ::write(m_fd, p_cacheBuffer.get() + offset, blockValidSize); // 写入文件
            if (writtenBytes == -1) { 
                throw std::runtime_error("Failed to write cache to file");
            }
        }
        else{
            std::cout <<"In flush(): Loop by access order, but can not find block in cache" << std::endl;
        }
    }
    m_accessOrder.clear();
    m_cache.clear();
}

void CachedFileOperator::close() {
    if (m_fd != -1) { //如果当前打开了某个文件，将其关闭
        ::close(m_fd);
        m_fd = -1;
    }
}

//单例模式
CachedFileOperator* CachedFileOperator::getInstance() {
    if(m_CFO == nullptr){
        m_CFO = new CachedFileOperator;
        atexit(&CachedFileOperator::OnProcessExit); //处理程序的正常退出和意外退出
        std::signal(SIGSEGV, signalHandler); 
        std::signal(SIGTERM, signalHandler); 
        std::signal(SIGABRT, signalHandler); 
    }
    return m_CFO;
}

void CachedFileOperator::open(const std::string& fileName) { // 按文件名打开文件
    m_fd = ::open(fileName.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | O_DIRECT);
    if (m_fd == -1) {
        throw std::runtime_error("Failed to open file: " + fileName);
    }
}

void CachedFileOperator::lseek(size_t offset, int whence) { //按指定方式获取文件偏移量
    if(m_fd == -1){
        throw std::runtime_error("In lseek(): File not specified");
    }
    m_pos = ::lseek(m_fd, offset, whence); //保存文件当前偏移量
    if (m_pos == -1) {
        throw std::runtime_error("Failed to seek in file");
    }
}

void CachedFileOperator::read(char* buffer, size_t size) {
    /*
    从当前文件所在位置读取size大小的数据，输出到buffer中。

    bufferOffset：缓冲区偏移量
    fileOffset：文件偏移量
    blockIndex：块索引
    blockOffset：块内偏移量
    */
    if(m_fd == -1){
        throw std::runtime_error("In read(): File not specified");
    }
    size_t bufferOffset = 0, fileOffset = m_pos;
    size_t blockIndex = fileOffset / BLOCK_SIZE;
    size_t blockOffset = fileOffset % BLOCK_SIZE;
    if(blockOffset != 0){
        // 将第一块需要的部分复制到buffer中
        size_t firstBlockSize = std::min(size, BLOCK_SIZE - blockOffset);
        memcpy(buffer + bufferOffset, readCache(blockIndex) + blockOffset, firstBlockSize);
        blockIndex++;
        bufferOffset += firstBlockSize;
    }
    while(bufferOffset + BLOCK_SIZE <= size){
        // 整块读取
        memcpy(buffer + bufferOffset, readCache(blockIndex), BLOCK_SIZE);
        blockIndex++;
        bufferOffset += BLOCK_SIZE;
    }
    size_t remainingBytes = size - bufferOffset;
    if(remainingBytes > 0){
        // 将剩余部分复制到buffer中
        memcpy(buffer + bufferOffset, readCache(blockIndex), remainingBytes);
    }
}

void CachedFileOperator::write(const char* data, size_t size) {
    /*
    将data从当前文件所在位置开始写入size大小的数据

    dataOffset：待写入数据偏移量
    fileOffset：文件偏移量
    blockIndex：块索引
    blockOffset：块内偏移量
    */
    if(m_fd == -1){
        throw std::runtime_error("In write(): File not specified");
    }
    size_t dataOffset = 0, fileOffset = m_pos;
    size_t blockIndex = fileOffset / BLOCK_SIZE;
    size_t blockOffset = fileOffset % BLOCK_SIZE;
    if(blockOffset != 0){
        // 把数据写入到第一个块中需要写到的地方
        size_t firstBlockSize = std::min(size, BLOCK_SIZE - blockOffset);
        readCache(blockIndex); // 确保缓存块没有左空缺
        writeCache(blockIndex, data + dataOffset, firstBlockSize, blockOffset);
        blockIndex++;
        dataOffset += firstBlockSize;
    }
    while(dataOffset + BLOCK_SIZE <= size){
        // 整块写入
        writeCache(blockIndex, data + dataOffset, BLOCK_SIZE, 0);
        blockIndex++;
        dataOffset += BLOCK_SIZE;
    }
    ssize_t remainingBytes = size - dataOffset;
    if(remainingBytes > 0){
        // 写入剩余部分
        writeCache(blockIndex, data + dataOffset, remainingBytes, 0);
    }
}


void CachedFileOperator::writeCache(size_t blockIndex, const char* dataBlock, size_t dataSize, size_t blockOffset) {
    size_t offset;
    auto it = m_cache.find(blockIndex);
    
    // 如果缓存中已有对应的块, 则直接修改缓存
    if (it != m_cache.end()) {
        offset = it->second.cacheBufferOffset;
        it->second.blockValidSize = std::max(it->second.blockValidSize, blockOffset + dataSize);
        memcpy(p_cacheBuffer.get() + offset + blockOffset, dataBlock, dataSize);

        // 更新访问顺序
        m_accessOrder.erase(it->second.accessOrderIterator); // 移除旧的访问顺序
        it->second.accessOrderIterator = m_accessOrder.insert(m_accessOrder.begin(), blockIndex); // 插入到头部
        return;
    }
    
    // 如果缓存没有对应的块
    if (m_cache.size() >= CACHE_MAX_BLOCK) { // 如果缓存已满，移除最久未使用的块
        size_t oldestBlock = m_accessOrder.back(); // 获取最旧的块索引
        offset = m_cache[oldestBlock].cacheBufferOffset;
        lseek(offset, SEEK_SET);
        ::write(m_fd, p_cacheBuffer.get() + offset, m_cache[oldestBlock].blockValidSize);
        
        m_cache.erase(oldestBlock); // 从缓存中移除该块
        m_accessOrder.pop_back(); // 更新访问顺序，移除最旧的块
    } else { // 如果是缓存未满，计算偏移量
        offset = (m_cache.size() * BLOCK_SIZE); 
    }

    // 将新数据放入缓存
    if (blockOffset != 0) {
        readCache(blockIndex); // 填满左空缺
    }
    
    memcpy(p_cacheBuffer.get() + offset + blockOffset, dataBlock, dataSize);
    
    // 更新缓存映射和访问顺序
    BlockInfo newBlockInfo;
    newBlockInfo.cacheBufferOffset = offset;
    newBlockInfo.blockValidSize = blockOffset + dataSize;
    newBlockInfo.accessOrderIterator = m_accessOrder.insert(m_accessOrder.begin(), blockIndex); // 插入到头部
    
    m_cache[blockIndex] = newBlockInfo; // 添加到缓存中
}

char* CachedFileOperator::readCache(size_t blockIndex) {
    auto it = m_cache.find(blockIndex);
    
    if (it != m_cache.end()) { // 如果已在缓存中，更新块的值
        size_t offset = it->second.cacheBufferOffset;
        
        if (it->second.blockValidSize < BLOCK_SIZE) {
            ssize_t readBytes = ::read(m_fd, p_cacheBuffer.get() + offset, BLOCK_SIZE);
            if (readBytes == -1) {
                throw std::runtime_error("Failed to read");
            }
            if (readBytes != 0)
                it->second.blockValidSize = readBytes;
        }

        // 更新访问顺序
        m_accessOrder.erase(it->second.accessOrderIterator); // 移除旧的访问顺序
        it->second.accessOrderIterator = m_accessOrder.insert(m_accessOrder.begin(), blockIndex); // 插入到头部
        
        return p_cacheBuffer.get() + offset; // 返回缓存数据
    } 
    // 如果不在缓存中，从文件读取一整块
    std::unique_ptr<char[]> buf(new char[BLOCK_SIZE]); 
    ssize_t readBytes = ::read(m_fd, buf.get(), BLOCK_SIZE);
    
    if (readBytes == -1) {
        throw std::runtime_error("Failed to read");
    }
    
    writeCache(blockIndex, buf.get(), readBytes, 0); // 将读取的数据写入缓存中
    return p_cacheBuffer.get() + m_cache[blockIndex].cacheBufferOffset;
}