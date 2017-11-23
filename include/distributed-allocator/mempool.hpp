#ifndef __MEMPOOL_HPP__
#define __MEMPOOL_HPP__

/*
This header provides:
- A standard memory pool implementation.
- A memory pool factory that creates memory pools,
  registering their addresses with an AbstractVaddrCoordinator.
*/

#include <unordered_set>
#include "miscutils.hpp"
// class MempoolFactory;

// A class that abstracts away an mmapped range of memory which we call a pool,
// and serves memory allocation/dellocation requests from that pool.
// These are created on the heap by and owned by the MempoolFactory.
//
// Implementation detail: This does not own the actual range of memory
// that it is using as its pool. The pool memory is owned by the MempoolFactory.
// The MemoryPool class is just an interface for managing use of that pool,
// as well as tracking the corresponding state information.
class MemoryPool {
public:
    // Constructor.
    MemoryPool(void* pool_addr, size_t pool_size);

    // Disable the copy constructor and copy assignment operators for now.
    // (We'll re-enable operators as necessary.)
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    // Allocate size_t bytes from this memory pool.
    // Returns the address you can use.
    void* allocate(size_t) noexcept;
    // Deallocate an address that was allocated from this pool.
    // Note: currently a no-op. You'll probably want to implement your
    // favourite memory allocation algorithm at some point.
    void deallocate(void*, size_t) noexcept;

    friend std::ostream& operator<<(std::ostream&, const MemoryPool&);

    // friend class MempoolFactory;

// protected:
    // MemoryPool metadata and state.
    void* pool_addr;
    size_t pool_size;
    // We should track how much space we actually use,
    // so if we ever need to rdma the memory, we know how much we need to move.
    // For now, we'll do this by keeping a pointer to the first unused address.
    void* unused_past;

    void* addr;
    size_t size;
};

#include "mempool.tpp"

#endif // __MEMPOOL_HPP__
