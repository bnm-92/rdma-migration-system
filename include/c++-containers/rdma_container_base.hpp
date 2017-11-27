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

    /*
        Releases all the associated resources
    */
    void destroy();

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
        4 GB, Default block size for container
        TODO, add a configurable param from config
    */ 
    static const size_t DEFAULT_POOL_SIZE = (size_t)1024 * 1024 * 4;
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
