// RDMAMemory.tpp

#include <string.h>

inline
RDMAMemory::RDMAMemory(int owner, void* addr, size_t size) :
    state(State::Clean),
    pair(-1),
    pages((uintptr_t)addr, size, 4096) {    
    this->owner = owner;
    this->vaddr = addr;
    this->size = size;
}

inline
RDMAMemory::RDMAMemory(int owner, void* addr, size_t size, size_t page_size) :
    state(State::Clean),
    pair(-1),
    pages((uintptr_t)addr, size, page_size) {    
    this->owner = owner;
    this->vaddr = addr;
    this->size = size;
}

inline
RDMAMemory::~RDMAMemory() {}

inline
RDMAMemoryManager::RDMAMemoryManager(std::string config, int serverid) : 
    coordinator(config, serverid), 
    incoming_transfers(), 
    incoming_accepts(),
    incoming_dones(),
    run(true)
{
    // LogAssert();
    this->server_id = serverid;
    int num_servers = coordinator.cfg.getNumServers();
    
    start_address = coordinator.cfg.getStartAddress();
    end_address = coordinator.cfg.getEndAddress();
    
    ptrdiff_t diff = end_address - start_address;
    ptrdiff_t per_server = diff/num_servers;

    start_address = start_address + server_id*per_server;
    end_address = start_address + per_server;

    /*
        think about running mprotect or mmap here to reserve these virtual addresses
    */
    alloc_address = start_address;

    // the memlist and free list do not need allocation
    this->coordinator.connect_mesh();
    this->poller_thread = std::thread(&RDMAMemoryManager::poller_thread_method, this);
    this->poller_thread.detach();
}

inline
RDMAMemoryManager::~RDMAMemoryManager() {
    // TO:DO, track all the RDMA messages and delete them with ref count or something
    run = false; // could also do a join here, probably better design
    for (int i=0; i<this->coordinator.cfg.getNumServers(); i++) {
        if(this->server_id == i) continue;
        uintptr_t conn_id = this->coordinator.connections[i];
        this->coordinator.getServer(i, conn_id)->done(conn_id);
    }
}

inline
void* RDMAMemoryManager::allocate(void* v_addr, size_t size){
        // mmap this address.
        RDMAMemory* r_memory = nullptr; 
        int prot = PROT_READ | PROT_WRITE;
        int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
        int fd = -1;
        off_t offset = 0;
        void* res = (void*) mmap((void*)v_addr, size, prot, flags, fd, offset);
        if (res == MAP_FAILED) {
            std::string str = strerror(errno);
            LogError("%s",str.c_str());
            return nullptr;
        }
    
        LogInfo("called mmap on %p and returning address %p", v_addr, res);
        LogAssert(v_addr == res, "asserting the addresses match");
        
        r_memory = new RDMAMemory(this->server_id, res, size);
        memory_map[res] = r_memory; 
        return r_memory->vaddr;
}

inline
void* RDMAMemoryManager::allocate(size_t size){    
    RDMAMemory* r_memory = nullptr; 
    
    std::unordered_map<size_t, std::vector<RDMAMemory*>>::const_iterator x = this->free_map.find(size);
    if(x != free_map.end()){
        LogInfo("memory found in free map");

        std::vector<RDMAMemory*> vec = x->second;
        RDMAMemory* memory  = vec.at(0);
        
        //allocate the memory using mmap
        int prot = PROT_READ | PROT_WRITE;
        int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
        int fd = -1;
        off_t offset = 0;
        void* res = (void*) mmap(memory->vaddr, size, prot, flags, fd, offset);
        if (res == MAP_FAILED) {
            std::string str = strerror(errno);
            LogError("%s",str.c_str());
            // return nullptr;
            goto alloc;
        }
        LogInfo("called mmap on %p and returning address %p", memory->vaddr, res);

        vec.erase(vec.begin());
        
        if(vec.size() == 0) {
            free_map.erase(size);
        } else {
            free_map[size] = vec;
        }

        memory_map[memory->vaddr] = memory;
        return memory->vaddr;
    }

    alloc:
    LogInfo("memory not found in free map, will try to allocate from shared space");

    uintptr_t end = this->alloc_address + size;
    if (end > this->end_address) {
        LogError("shared memory block exhausted");
        return nullptr;
    }

    // mmap this address.
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
    int fd = -1;
    off_t offset = 0;
    void* res = (void*) mmap((void*)this->alloc_address, size, prot, flags, fd, offset);
    if (res == MAP_FAILED) {
        std::string str = strerror(errno);
        LogError("%s",str.c_str());
        return nullptr;
    }

    
    LogInfo("called mmap on %lu and returning address %lu", this->alloc_address, (uintptr_t)res);
    LogAssert(this->alloc_address == (uintptr_t)res, "asserting the addresses match");
    this->alloc_address += size;

    r_memory = new RDMAMemory(this->server_id, res, size);
    memory_map[res] = r_memory; 
    return r_memory->vaddr;
}

inline
void RDMAMemoryManager::deallocate(void* v_addr){
    //this memory needs to be in memory map
    LogAssert(memory_map.find(v_addr) != memory_map.end(), "memory not found in memory map");

    RDMAMemory *memory = memory_map.find(v_addr)->second;
    int res = munmap(memory->vaddr, memory->size);
    if(res == -1) {
        LogError("munmap failed beause %s", strerror(errno));
    } 

    // add this to the free list
    auto x = free_map.find(memory->size);
    if (x == free_map.end()) {
        std::vector<RDMAMemory*> vec;
        vec.push_back(memory);
        free_map[memory->size] = vec; 
    } else {
        //entry exists
        std::vector<RDMAMemory*> vec = x->second;
        vec.push_back(memory);
        free_map[memory->size] = vec;
    }

}

inline
void RDMAMemoryManager::deallocate(void* v_addr, size_t size){
    this->deallocate(v_addr);    
}

inline
int RDMAMemoryManager::Prepare(void* v_addr, size_t size, int destination) {
    LogAssert(memory_map.find(v_addr) != memory_map.end(), "memory not part of memory map");
    LogAssert(memory_map.find(v_addr)->second->state == RDMAMemory::State::Clean, "memory not clean, please ensure all memory is local before moving it along");
    uintptr_t conn_id = this->coordinator.connections[destination];
    this->UpdatePair(v_addr, destination);
    this->register_memory(v_addr, size, destination);
    this->coordinator.getServer(destination, conn_id)->send_prepare(conn_id, v_addr, size);
    return 0;
}

inline
int RDMAMemoryManager::prepare(void* v_addr, int destination){
    LogAssert(memory_map.find(v_addr) != memory_map.end(), "memory not part of memory map");
    uintptr_t conn_id = this->coordinator.connections[destination];
    size_t size = memory_map[v_addr]->size;
    this->UpdatePair(v_addr, destination);
    this->register_memory(v_addr, size, destination);
    this->coordinator.getServer(destination, conn_id)->send_prepare(conn_id, v_addr, size);
    return 0;
}

/*
    prior to this call the user should wait for the queue message for prepare
    and should be able to allocate and register the required memory address 
    TODO write a accept helper to do this
    
*/

inline
void* RDMAMemoryManager::accept(void* v_addr, size_t size, int source){
    void* mem = this->allocate(v_addr, size);
    uintptr_t conn_id = this->coordinator.connections[source];
    if (mem == nullptr) {
        LogInfo("could not allocate for accept, sending reject");
        this->coordinator.getServer(source, conn_id)->send_decline(conn_id, mem, size);
        return nullptr;
    }

    this->UpdatePair(mem, source);

    LogInfo("registering memory");
    this->coordinator.getServer(source, conn_id)->register_memory(this->coordinator.connections[source],v_addr, size, false);
    LogInfo("sending accept");
    this->coordinator.getServer(source, conn_id)->send_accept(conn_id, mem, size);
   
    return mem;
}

inline
int RDMAMemoryManager::Transfer(void* v_addr, size_t size, int destination){
    auto x = memory_map.find(v_addr);
    LogAssert(x != memory_map.end(), "could not find memory in allocated list");

    RDMAMemory* rmemory = x->second;
    rmemory->owner = destination;
    rmemory->state = RDMAMemory::State::Shared;
    uintptr_t conn_id = this->coordinator.connections[destination];
    this->coordinator.getServer(destination, conn_id)->send_transfer(conn_id, v_addr, size);

    return 0;
}


inline
int RDMAMemoryManager::transfer(void* v_addr, size_t size, int destination){
    auto x = memory_map.find(v_addr);
    LogAssert(x != memory_map.end(), "could not find memory in allocated list");

    RDMAMemory* rmemory = x->second;
    rmemory->owner = destination;
    rmemory->state = RDMAMemory::State::Shared;
    uintptr_t conn_id = this->coordinator.connections[destination];
    this->coordinator.getServer(destination, conn_id)->send_transfer(conn_id, v_addr, size);

    return 0;
}

inline
void RDMAMemoryManager::on_transfer(void* v_addr, size_t size, int source) {
    // updateState(v_addr, RDMAMemory::State::Shared);
    #if PAGING
        if(mprotect(v_addr, size, PROT_NONE)  != 0) {
            LogError("Mprotect failed");
            exit(errno);
        }
        UpdateState(v_addr, RDMAMemory::State::Shared);
    #else
        this->Pull(v_addr, size, source);
        UpdateState(v_addr, RDMAMemory::State::Clean);
    #endif
}

inline
void RDMAMemoryManager::close(void* v_addr, size_t size, int source) {
    uintptr_t conn_id = this->coordinator.connections[source];
    this->coordinator.getServer(source, conn_id)->send_close(conn_id, v_addr, size);
}

/*
    for internal use only, used within prepare and transfer
*/

inline
int RDMAMemoryManager::pull(void* v_addr, int source){
    uintptr_t conn_id = this->coordinator.connections[source];
    this->coordinator.getServer(source, conn_id)->rdma_read(conn_id, v_addr, v_addr, this->memory_map.find(v_addr)->second->size);
    return 0;
}

inline
int RDMAMemoryManager::Pull(void* v_addr, size_t size, int source){
    uintptr_t conn_id = this->coordinator.connections[source];
    LogInfo("pulling memory at %p of size %zu", v_addr, size);
    this->coordinator.getServer(source, conn_id)->rdma_read(conn_id, v_addr, v_addr, size);
    return 0;
}

inline
int RDMAMemoryManager::push(void* v_addr, int destination){
    uintptr_t conn_id = this->coordinator.connections[destination];
    this->coordinator.getServer(destination, conn_id)->rdma_write(conn_id, v_addr, v_addr, this->memory_map.find(v_addr)->second->size);
    return 0;
}

inline
int RDMAMemoryManager::Push(void* v_addr, size_t size, int destination){
    uintptr_t conn_id = this->coordinator.connections[destination];
    this->coordinator.getServer(destination, conn_id)->rdma_write(conn_id, v_addr, v_addr, size);
    return 0;
}

inline
void RDMAMemoryManager::register_memory(void* v_addr, size_t size, int destination){
    uintptr_t conn_id = this->coordinator.connections[destination];
    this->coordinator.getServer(destination, conn_id)->register_memory(conn_id, v_addr, size);
}

inline
void RDMAMemoryManager::deregister_memory(void* v_addr, size_t size, int destination){
    uintptr_t conn_id = this->coordinator.connections[destination];
    this->coordinator.getServer(destination, conn_id)->deregister_memory(conn_id, v_addr);
}

inline
int RDMAMemoryManager::UpdateState(void* memory, RDMAMemory::State state) {
    auto x = this->memory_map.find(memory);
    LogAssert(x != memory_map.end(), "could not find RDMA memory at specified location");
    
    // if(x == memory_map.end()) {
    //     LogError("");
    //     return -1;
    // }

    RDMAMemory* mem = x->second;
    mem->state = state;
    return 0;
}

inline
int RDMAMemoryManager::HasMessage() {
    for (int i=message_check; i <coordinator.cfg.getNumServers(); i++) {
        if(i == this->server_id) continue;

        if (HasMessage(i)) {
            message_check = i+1 < coordinator.cfg.getNumServers() ? i+1 : 0;
            return i;
        }
    }
    message_check = 0;
    return -1;
}

inline
bool RDMAMemoryManager::HasMessage(int source) {
    uintptr_t conn_id = this->coordinator.connections[source];
    return this->coordinator.getServer(source, conn_id)->checkForMessage(conn_id);
}

inline
RDMAMemoryManager::RDMAMessage::Type RDMAMemoryManager::getMessageType(rdma_message::MessageType type) {
    if(type == rdma_message::MessageType::MSG_PREPARE) {
        return RDMAMessage::Type::PREPARE;
    } else if(type == rdma_message::MessageType::MSG_ACCEPT) {
        return RDMAMessage::Type::ACCEPT;
    } else if(type == rdma_message::MessageType::MSG_DECLINE) {
        return RDMAMessage::Type::DECLINE;
    } else if(type == rdma_message::MessageType::MSG_TRANSFER) {
        return RDMAMessage::Type::TRANSFER;
    } else {
        return RDMAMessage::Type::DONE;
    }
}

inline
RDMAMemoryManager::RDMAMessage* RDMAMemoryManager::GetMessage(int source) {
    uintptr_t conn_id = this->coordinator.connections[source];
    std::pair<void*, size_t> message = this->coordinator.getServer(source, conn_id)->receive(conn_id);

    struct rdma_message* msg = (struct rdma_message*) message.first;
    return new RDMAMessage(msg->region_info.addr, msg->region_info.length, this->getMessageType(msg->message_type));
}

inline
int RDMAMemoryManager::UpdatePair(void* memory, int id) {
    auto x = this->memory_map.find(memory);
    LogAssert(x != memory_map.end(), "could not find RDMA memory at specified location");
    
    RDMAMemory* mem = x->second;
    mem->pair = id;
    
    return 0;
}

inline
int RDMAMemoryManager::pull(void* v_addr, size_t size){
    RDMAMemory* mem = this->memory_map.find(v_addr)->second;
    int pair = mem->pair;
    uintptr_t conn_id = this->coordinator.connections[pair];
    this->coordinator.getServer(pair, conn_id)->rdma_read(conn_id, v_addr, v_addr, size);
    return 0;
}

inline
int RDMAMemoryManager::push(void* v_addr, size_t size){
    RDMAMemory* mem = this->memory_map.find(v_addr)->second;
    int pair = mem->pair;
    uintptr_t conn_id = this->coordinator.connections[pair];
    this->coordinator.getServer(pair, conn_id)->rdma_write(conn_id, v_addr, v_addr, size);
    return 0;
}

/*
    must ensure that the v_addr +size is allocated and within limits
*/
inline
int RDMAMemoryManager::pull(void* v_addr){
    RDMAMemory* mem = this->memory_map.find(v_addr)->second;
    int pair = mem->pair;
    uintptr_t conn_id = this->coordinator.connections[pair];
    this->coordinator.getServer(pair, conn_id)->rdma_read(conn_id, v_addr, v_addr, mem->size);
    return 0;
}

inline
int RDMAMemoryManager::push(void* v_addr){
    RDMAMemory* mem = this->memory_map.find(v_addr)->second;
    int pair = mem->pair;
    uintptr_t conn_id = this->coordinator.connections[pair];
    this->coordinator.getServer(pair, conn_id)->rdma_write(conn_id, v_addr, v_addr, mem->size);
    return 0;
}

inline 
void RDMAMemoryManager::on_close(void* addr, size_t size, int pair) {
    this->deregister_memory(addr, size, pair);
    std::unordered_map<void*, RDMAMemory*>::iterator it = this->memory_map.find(addr);
    LogAssert(it != memory_map.end(), "memory not allocated");
    this->incoming_dones.enqueue(it->second);
}

inline
void RDMAMemoryManager::poller_thread_method() {
    while(run) {
        int source = this->HasMessage();
        if(source == -1)
            continue;
        
        RDMAMessage* message = this->GetMessage(source);
        void* addr = message->addr;
        size_t size = message->size;

        if(message->type == RDMAMessage::Type::PREPARE) {
            this->accept(addr, size, source);
        } else if(message->type == RDMAMessage::Type::ACCEPT) {
            std::unordered_map<void*, RDMAMemory*>::iterator it = this->memory_map.find(addr);
            LogAssert(it != memory_map.end(), "memory not allocated");
            this->incoming_accepts.enqueue(it->second);
        } else if(message->type == RDMAMessage::Type::DECLINE) {
            this->deregister_memory(addr, size, source);
            //maybe add notification to top
        } else if(message->type == RDMAMessage::Type::TRANSFER) {
            this->on_transfer(addr, size, source);
            std::unordered_map<void*, RDMAMemory*>::iterator it = this->memory_map.find(addr);
            LogAssert(it != memory_map.end(), "memory not allocated");
            this->incoming_transfers.enqueue(it->second);
        } else if(message->type == RDMAMessage::Type::DONE) {
            this->on_close(addr, size, source);
        }
    }
}

inline
RDMAMemory* RDMAMemoryManager::PollForAccept() {
    if(incoming_accepts.empty())
        return nullptr;
    return incoming_accepts.dequeue(); 
}

inline
RDMAMemory* RDMAMemoryManager::PeekAccept() {
    return incoming_accepts.peek(); 
}

inline
RDMAMemory* RDMAMemoryManager::PollForTransfer() {
    if(incoming_transfers.empty())
        return nullptr;
    return incoming_transfers.dequeue();
}

inline
RDMAMemory* RDMAMemoryManager::PollForClose() {
    if(incoming_dones.empty())
        return nullptr;
    return incoming_dones.dequeue();
}

inline
RDMAMemory* RDMAMemoryManager::PeekClose() {
    return incoming_dones.peek(); 
}

inline
RDMAMemory* RDMAMemoryManager::getRDMAMemory(void* address) {
    //TODO change to binary search, but for that we'll have to change the underlying data structure
    std::unordered_map<void*,RDMAMemory*>::iterator it;
    for (it = memory_map.begin(); it != memory_map.end(); it++ ) {
        void* addr1 = it->first;
        RDMAMemory* memory = it->second;
        void* addr2 = (void*) ((char*)memory->vaddr + memory->size);
        if(address >= addr1 && address < addr2)
            return memory;
    }
    return nullptr;
}

inline
void RDMAMemoryManager::PullAllPages(RDMAMemory* memory){
    // auto x = memory_map.find(address);
    // RDMAMemory* memory = x->second;
    unsigned int id = 0;
    int source = memory->pair;

    // LogAssert(x != memory_map.end(), "cannot find memory");
    LogAssert(source != -1, "source not set");

    vector<Page> p = memory->pages.pages;
    for (; id<p.size(); id++) {
        if(p.at(id).pagestate == Page::PageState::Local)
            continue;

        void* addr = memory->pages.getPageAddress(id);
        size_t pagesize = memory->pages.getPageSize(id);

        memory->pages.setPageState(addr, Page::PageState::InFlight);

        if(mprotect(addr, pagesize, PROT_READ | PROT_WRITE)) {
            perror("couldnt mprotect3");
            exit(errno);
        }
    
        this->Pull(addr, pagesize, source);
        memory->pages.setPageState(addr, Page::PageState::Local);
    }

    this->close(memory->vaddr, memory->size, source);
}