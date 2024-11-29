#ifndef LOCKFREEQUEUE_TPP
#define LOCKFREEQUEUE_TPP

template<typename T>
LockFreeQueue<T>::LockFreeQueue(size_t capacity) 
    : capacity(capacity), head(0), tail(0) {
    buffer.resize(capacity);
}

template<typename T>
bool LockFreeQueue<T>::enqueue(const T& value) {
    size_t currentTail = tail.load(std::memory_order_relaxed);
    size_t nextTail = (currentTail + 1) % capacity;

    if (nextTail == head.load(std::memory_order_acquire)) {
        return false; // Queue is full
    }

    buffer[currentTail] = value;
    tail.store(nextTail, std::memory_order_release);
    return true;
}

template<typename T>
bool LockFreeQueue<T>::dequeue(T& result) {
    size_t currentHead = head.load(std::memory_order_relaxed);

    if (currentHead == tail.load(std::memory_order_acquire)) {
        return false; // Queue is empty
    }

    result = buffer[currentHead];
    head.store((currentHead + 1) % capacity, std::memory_order_release);
    return true;
}

#endif // LOCKFREEQUEUE_TPP