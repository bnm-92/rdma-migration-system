// rdma_container_base.tpp

#include <scoped_allocator>
#include <utility>

template <class T>
inline
RDMAContainerBase<T>::RDMAContainerBase(RDMAMemoryManager* manager)
    : manager(manager), 
      rdma_memory(nullptr)
    {}

template <class T>
inline
RDMAContainerBase<T>::~RDMAContainerBase() {
    manager->deallocate(mempool->addr);
}

template <class T>
inline
void RDMAContainerBase<T>::set_up_allocator() {
    // allocate using the manager
    void* memory = manager->allocate(DEFAULT_POOL_SIZE);

    /*
        set up pool information within the memory
        we need the pointer to the pool to reside in the pool for binding access
        this will allow for allocations within the containers after migrations
    */
    MemoryPool** pool_ = (MemoryPool**)memory;
    (*pool_) = (MemoryPool*)((char*)memory + sizeof(MemoryPool**));
    //set up the pool
    (*pool_)->pool_addr = (void*)((char*)memory + sizeof(MemoryPool**) + sizeof(MemoryPool) + sizeof(void*));
    (*pool_)->unused_past = (*pool_)->pool_addr;
    (*pool_)->pool_size = DEFAULT_POOL_SIZE - (sizeof(MemoryPool**) + sizeof(MemoryPool) + sizeof(void*));
    (*pool_)->size = DEFAULT_POOL_SIZE;
    (*pool_)->addr = memory;

    mempool = *pool_;
    alloc = PoolBasedAllocator<T>(&(*pool_));
}

template <class T>
inline
bool RDMAContainerBase<T>::Prepare(int destination) {
    void* addr = mempool->addr;
    size_t size = mempool->size;
    void** destination_address = (void**)((char*)addr + sizeof(MemoryPool**) + sizeof(MemoryPool));
    *destination_address = this->get_container_address(); 
    void* copied_address = *destination_address; 
    LogInfo("container address destination is packed is: %p", destination_address);
    LogInfo("container address packed is: %p", copied_address);

    if (!manager->Prepare(addr, size, destination))
        return true;
    return false;
}

template <class T>
inline
bool RDMAContainerBase<T>::PollForAccept() {
    rdma_memory = manager->PollForAccept();
    if(rdma_memory == nullptr)
        return false;
    return true;
}

template <class T>
inline
bool RDMAContainerBase<T>::Transfer() {
    LogAssert(rdma_memory!=nullptr, "Prepare not sent");
    if(this->manager->Transfer(rdma_memory->vaddr, rdma_memory->size, rdma_memory->pair))
        return false;
    return true;
}

template <class T>
inline
bool RDMAContainerBase<T>::PollForTransfer() {
    RDMAMemory* mem = manager->PollForTransfer();;
    if(mem == nullptr)
        return false;
    this->rdma_memory = mem;
    return true; 
}

template <class T>
inline
RDMAMemory* RDMAContainerBase<T>::PollForClose() {
    return manager->PollForClose();
}

template <class T>
inline
void RDMAContainerBase<T>::Close() {
    manager->close(rdma_memory->vaddr, rdma_memory->size, rdma_memory->pair);
}