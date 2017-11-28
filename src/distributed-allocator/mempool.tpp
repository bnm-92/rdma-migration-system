// mempool.tpp

#include <string>

inline
MemoryPool::MemoryPool(void* pool_addr, size_t pool_size)
: pool_addr(pool_addr), pool_size(pool_size), unused_past(pool_addr) {
}

inline
void* MemoryPool::allocate(size_t bytes) noexcept {
    
    LogInfo("allocate(bytes = %p) called on %p", (void*) bytes, this);

    // For now, we'll use the next free address.
    void* result = unused_past;
    // But before we return, update the next free address.
    unused_past = (void*) ((char*)unused_past + bytes);
    LogAssert((uintptr_t)unused_past < (uintptr_t)((char*)addr + size), "OUT OF MEMORY");
    return result;
}

inline
void MemoryPool::deallocate(void* addr, size_t bytes) noexcept {
    LogInfo("deallocate(addr = %p, bytes = %p)",addr, (void*) bytes);
}

inline
std::ostream& operator<<(std::ostream& os, const MemoryPool& pool) {
    os << "MemoryPool(pool_addr = " << pool.pool_addr
       << ", pool_size = " << pool.pool_size
       << ", unused_past = " << pool.unused_past << ")";
    return os;
}