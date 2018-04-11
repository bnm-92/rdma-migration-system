// rdma_container_base.tpp

#include <scoped_allocator>
#include <utility>

template <class T>
inline
#if FAULT_TOLERANT
RDMAContainerBase<T>::RDMAContainerBase(RDMAMemoryManager* manager, int64_t id) : 
#else
RDMAContainerBase<T>::RDMAContainerBase(RDMAMemoryManager* manager) : 
#endif
    manager(manager),
    rdma_memory(nullptr),
    mempool(nullptr){
        #if FAULT_TOLERANT
            this->id = id;
        #endif
    }

template <class T>
inline
RDMAContainerBase<T>::~RDMAContainerBase() {
    if (mempool->addr) {
        #if FAULT_TOLERANT
            manager->deallocate(id);
        #else 
            manager->deallocate(mempool->addr);
        #endif
    }
    
}

template <class T>
inline
void RDMAContainerBase<T>::set_up_allocator() {
    // allocate using the manager
    #if FAULT_TOLERANT
        void* memory = manager->allocate(DEFAULT_POOL_SIZE, id);
    #else
        void* memory = manager->allocate(DEFAULT_POOL_SIZE);
    #endif
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
    rdma_memory = manager->PeekAccept();
    if(rdma_memory == NULL)
        return false;
    if(rdma_memory->vaddr == mempool->addr)
        rdma_memory = manager->PollForAccept();        
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
    RDMAMemory* mem = manager->PollForTransfer();
    if(mem == nullptr)
        return false;
    this->rdma_memory = mem;
    return true; 
}

template <class T>
inline
bool RDMAContainerBase<T>::PollForClose() {
    RDMAMemory* r_memory = manager->PeekClose();
    if(r_memory == NULL)
        return false;
    if(r_memory->vaddr == mempool->addr) {
        rdma_memory = manager->PollForClose();
        #if FAULT_TOLERANT
            manager->deallocate(rdma_memory->application_id);
        #else 
            manager->deallocate(rdma_memory->vaddr);
        #endif
    } else {
        return false;
    }
    
    return true;
    
}

template <class T>
inline
void RDMAContainerBase<T>::Close() {
    manager->close(rdma_memory->vaddr, rdma_memory->size, rdma_memory->pair);
}

template <class T>
inline
void RDMAContainerBase<T>::Pull() {
    manager->Pull(
        this->rdma_memory->vaddr, 
        (size_t) ( (uintptr_t)this->mempool->unused_past - (uintptr_t)this->rdma_memory->vaddr),
        this->rdma_memory->pair);
}

template <class T>
inline
void RDMAContainerBase<T>::Push() {
    manager->Push(
        this->rdma_memory->vaddr, 
        (size_t) ( (uintptr_t)this->mempool->unused_past - (uintptr_t)this->rdma_memory->vaddr),
        this->rdma_memory->pair);
}

template <class T>
inline
void RDMAContainerBase<T>::PullSync() {
    manager->PullPagesSync(
        this->rdma_memory->vaddr, 
        (size_t) ( (uintptr_t)this->mempool->unused_past - (uintptr_t)this->rdma_memory->vaddr),
        this->rdma_memory->pair);
}

template <class T>
inline
void RDMAContainerBase<T>::PullAsync(int rate_limiter) {
    manager->PullPagesAsync(
        this->rdma_memory->vaddr, 
        (size_t) ( (uintptr_t)this->mempool->unused_past - (uintptr_t)this->rdma_memory->vaddr),
        this->rdma_memory->pair,
        rate_limiter);
}

template <class T>
inline
void RDMAContainerBase<T>::PullAndClose() {
    manager->PullAllPages(this->rdma_memory);
}

template <class T>
inline
void RDMAContainerBase<T>::SetContainerSize(size_t size) {
    if(mempool == nullptr)
        this->DEFAULT_POOL_SIZE = size;
}

template <class T>
inline
void RDMAContainerBase<T>::SetPageSize(size_t size) {
    if(rdma_memory != nullptr)
        this->rdma_memory->pages.setPageSize(size);
}
