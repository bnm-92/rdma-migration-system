#ifndef __RDMA_CONTAINER_BASE_HPP__
#define __RDMA_CONTAINER_BASE_HPP__

#include <scoped_allocator>

#include "rdma_server_prototype.hpp"

#include "pool_based_allocator.hpp"
#include "mempool.hpp"

#include "RDMAMemory.hpp"

// static RDMAMemoryManager* memory_manager; 

template <class T>
class RDMAContainerBase {
public:
    /*
        Standard constructor, which attaches a RDMA Memory Manager to this
        container. Does not instantiate anything else yet.
    */
    RDMAContainerBase(RDMAMemoryManager* manager);
    ~RDMAContainerBase();

    /*
        Interface for sending the container to remote host with ID
    */
    bool Prepare(int destination);
    bool PollForAccept();
    bool Transfer(); 

    /*
        Interface to receive a container, we are expecting 
    */
    bool PollForTransfer();
    
    bool PollForClose();
    void Close();

    /**
     * Standard Pull/Push operations, may only be used when PAGING is set OFF in config (utils/mscutils.hpp)
     * Pulls and pushes and entire container
     * May also be used after close operation
    */
    void Pull();
    void Push();

    /**
     * Standard Pulls with paging turned on
     * PullSync will pull the entire container synchronously one page at a time
     * PullAsync will pull the entire container asynchronously n pages at a time
     * params:
     * int rate_limiter: number of pending async ops allowed, a higher number will increase 
     *                      latency of demand pulling as we only open one connection 
    */
    void PullSync();
    void PullAsync(int rate_limiter);

    /**
     * If unsure whether all pages required have been imported (not to be used with containers, adding for completeness)
     * Pulls the entire underlying memory segment and closes the connection on completion
    */
    void PullAndClose();

    /*
        Releases all the associated resources
    */
    void destroy();


    /*
        configurations
    */
    void SetContainerSize(size_t size);

    /*
        set page size for this container
    */
    void SetPageSize(size_t size);

protected:
    /*
        Manager for cluster memory
    */
    RDMAMemoryManager *manager;
    RDMAMemory* rdma_memory;

    /*
        Memory Pool object is used by the container to allocate memory for all objects
    */
    MemoryPool* mempool;
    
    /*
        Typedef for our custom scoped allocator class.
    */
    typedef PoolBasedAllocator<T> AllocT;
    typedef std::scoped_allocator_adaptor<AllocT> ScopedAllocT;

    /* 
        The allocator we're using for this container.
    */
    ScopedAllocT alloc;

    /*
        Default block size for container
        TODO, add a configurable param from config
    */ 
    size_t DEFAULT_POOL_SIZE = (size_t)1024 * 1024 * 4;
    /*
        Set up the allocator for this container class
        This method requests rdma able memory segment from the manager and 
        sets up the memory pool
    */

    /*
        poller threads to be used for polling for accept message and transfer notification on the queue
        associated functions that will update the state of the enum
    */
    void set_up_allocator();

    virtual void* get_container_address() = 0;
    virtual void set_container_address(void* container_addr) = 0;
};

#include "rdma_container_base.tpp"
#endif // __RDMA_CONTAINER_BASE_HPP__
