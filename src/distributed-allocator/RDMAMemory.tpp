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

#if FAULT_TOLERANT

inline
RDMAMemory::RDMAMemory(int owner, void* addr, size_t size, int64_t app_id) :
    state(State::Clean),
    pair(-1),
    pages((uintptr_t)addr, size, 4096) {    
    this->owner = owner;
    this->vaddr = addr;
    this->size = size;
    this->application_id = app_id;
}

inline
RDMAMemory::RDMAMemory(int owner, void* addr, size_t size, size_t page_size, int64_t app_id) :
    state(State::Clean),
    pair(-1),
    pages((uintptr_t)addr, size, page_size) {    
    this->owner = owner;
    this->vaddr = addr;
    this->size = size;
    this->application_id = app_id;
}

#endif

inline
RDMAMemory::~RDMAMemory() {}

inline
RDMAMemoryManager::RDMAMemoryManager(std::string config, int serverid) : 
    coordinator(config, serverid), 
    incoming_transfers(), 
    incoming_accepts(),
    incoming_dones(),
    run(true),
    num_threads_pulling(0)
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
#if FAULT_TOLERANT
void* RDMAMemoryManager::allocate(size_t size, int64_t application_id){
    LogInfo("allocating using zookeeper, fetching memory address");
    RDMAMemory* r_memory = nullptr;
    void* address = coordinator.getAllocationAddress(size);

    // mmap this address.
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
    int fd = -1;
    off_t offset = 0;
    void* res = (void*) mmap(address, size, prot, flags, fd, offset);
    if (res == MAP_FAILED) {
        std::string str = strerror(errno);
        LogError("%s",str.c_str());
        return nullptr;
    }

    
    LogInfo("called mmap on %lu and returning address %lu", (uintptr_t)address, (uintptr_t)res);
    LogAssert((uintptr_t)address == (uintptr_t)res, "asserting the addresses match");

    if (coordinator.createMemorySegmentNode(address, size, this->server_id, -1, application_id) != 0) {
        goto failure_;
    }

    if (coordinator.addToProcessList(application_id) != 0) {
        goto failure_;
    }
    
    r_memory = new RDMAMemory(this->server_id, res, size, application_id);
    local_segments[res] = r_memory; 
    return r_memory->vaddr;

    failure_:
    int res_munmap = munmap(res, size);
    if(res_munmap == -1) {
        LogError("munmap failed beause %s", strerror(errno));
    }
    return nullptr;
}

int RDMAMemoryManager::deallocate(int64_t application_id) {
    //this memory needs to be in memory map
    LogInfo("deallocation");
    auto map = this->coordinator.getMemorySegment(application_id);
    size_t sz = 0;
    std::string input = map["address"];
    uintptr_t res = std::stoul(input, &sz, 0);
 
    void* v_addr = (void*)(res);
    size_t size = (size_t)stoull(map["size"]);   

    LogInfo("serverid is %d and source is %d", this->coordinator.server_id, atoi(map["source"].c_str()));

    if(this->coordinator.server_id == atoi(map["source"].c_str())) {
        // local deallocation
        int res_munmap = munmap(v_addr, size);
        if(res_munmap == -1) {
            LogError("munmap failed beause %s", strerror(errno));
        }
        RDMAMemory *memory = local_segments.find(v_addr)->second;

        if(this->coordinator.removeFromProcessList(memory->application_id) != 0) {
            LogError("could not update process list");
        }
        local_segments.erase(v_addr);
        
    }

    int rc = this->coordinator.deleteMemorySegment(application_id, this->coordinator.server_id);
    if(rc == -1) {
        LogWarning("deallocation failed");
        return -1;
    }
    
    return 0;
}

#else
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

#endif

#if FAULT_TOLERANT
inline
void* RDMAMemoryManager::allocate(void* v_addr, size_t size, int64_t application_id){
#else
inline
void* RDMAMemoryManager::allocate(void* v_addr, size_t size){
#endif

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
    
    
    #if FAULT_TOLERANT
        r_memory = new RDMAMemory(this->server_id, res, size, application_id);
        local_segments[res] = r_memory;
    #else
        r_memory = new RDMAMemory(this->server_id, res, size);
        memory_map[res] = r_memory;
    #endif

    return r_memory->vaddr;
}


inline
int RDMAMemoryManager::Prepare(void* v_addr, size_t size, int destination) {
    #if FAULT_TOLERANT
        LogAssert(local_segments.find(v_addr) != local_segments.end(), "memory not part of local list");
        this->UpdatePair(v_addr, destination);
    #else 
        LogAssert(memory_map.find(v_addr) != memory_map.end(), "memory not part of memory map");
        LogAssert(memory_map.find(v_addr)->second->state == RDMAMemory::State::Clean, "memory not clean, please ensure all memory is local before moving it along");
        this->UpdatePair(v_addr, destination);
    #endif

    uintptr_t conn_id = this->coordinator.connections[destination];
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
#if FAULT_TOLERANT

inline
void* RDMAMemoryManager::accept(void* v_addr, size_t size, int source, int64_t client_id){
    void* mem = this->allocate(v_addr, size, client_id);
    //add to process list
    this->coordinator.addToProcessList(client_id);
    
    uintptr_t conn_id = this->coordinator.connections[source];
    if (mem == nullptr) {
        LogInfo("could not allocate for accept, sending reject");
        this->coordinator.getServer(source, conn_id)->send_decline(conn_id, mem, size);
        return nullptr;
    }
    
    LogInfo("registering memory");
    this->coordinator.getServer(source, conn_id)->register_memory(this->coordinator.connections[source],v_addr, size, false);
    LogInfo("sending accept");
    this->coordinator.getServer(source, conn_id)->send_accept(conn_id, mem, size);
   
    return mem;
}
#else 
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
#endif

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

        #if PREFETCHING
            auto x = memory_map.find(v_addr);
            LogAssert(x != memory_map.end(), "could not find memory in allocated list");
            RDMAMemory* rmemory = x->second;
            // std::thread(&RDMAMemoryManager::poller_thread_method, this).detach();
            #if ASYNC_PREFETCHING
                std::thread(&RDMAMemoryManager::PullAllPagesWithoutCloseAsync, this, rmemory).detach();
            #else
                std::thread(&RDMAMemoryManager::PullAllPagesWithoutClose, this, rmemory).detach();
            #endif
        #endif

    #else
        this->Pull(v_addr, size, source);
        UpdateState(v_addr, RDMAMemory::State::Clean);
    #endif
}

inline
void RDMAMemoryManager::close(void* v_addr, size_t size, int source) {
    #if PAGING
        if(mprotect(v_addr, size, PROT_NONE)  != 0) {
            LogError("Mprotect failed");
            exit(errno);
        }
        UpdateState(v_addr, RDMAMemory::State::Clean);
    #endif

    #if FAULT_TOLERANT
        int64_t app_id = local_segments[v_addr]->application_id;
        this->coordinator.cleanMemorySegment(app_id);
    #endif

    uintptr_t conn_id = this->coordinator.connections[source];
    this->coordinator.getServer(source, conn_id)->send_close(conn_id, v_addr, size);
    this->deregister_memory(v_addr, size, source);
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
int RDMAMemoryManager::PullAsync(void* v_addr, size_t size, int source, void (*callback)(void*), void* data){
    uintptr_t conn_id = this->coordinator.connections[source];
    LogInfo("pulling memory at %p of size %zu", v_addr, size);
    this->coordinator.getServer(source, conn_id)->rdma_read_async(conn_id, v_addr, v_addr, size, callback, data);
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
    
    if(x == memory_map.end()) {
        LogError("Memory invalid");
        return -1;
    }

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
    } else if(type == rdma_message::MessageType::MSG_USER) {
        return RDMAMessage::Type::USER;
    } else if(type == rdma_message::MessageType::MSG_GET_PARTITIONS) {
        return RDMAMessage::Type::GETPARTITIONS;
    } else if(type == rdma_message::MessageType::MSG_SENT_PARTITIONS) {
        return RDMAMessage::Type::SENTPARTITIONS;
    } else {
        return RDMAMessage::Type::DONE;
    }
}

inline
RDMAMemoryManager::RDMAMessage* RDMAMemoryManager::GetMessage(int source) {
    uintptr_t conn_id = this->coordinator.connections[source];
    std::pair<void*, size_t> message = this->coordinator.getServer(source, conn_id)->receive(conn_id);

    struct rdma_message* msg = (struct rdma_message*) message.first;
    return new RDMAMessage(msg->region_info.addr, msg->region_info.length, this->getMessageType(msg->message_type), msg->data);
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
    #if FAULT_TOLERANT
        std::unordered_map<void*, RDMAMemory*>::iterator it = this->local_segments.find(addr);
        LogAssert(it != local_segments.end(), "memory not allocated");
    #else
        std::unordered_map<void*, RDMAMemory*>::iterator it = this->memory_map.find(addr);
        LogAssert(it != memory_map.end(), "memory not allocated");
    #endif

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
            #if FAULT_TOLERANT
                int64_t client_id = atol(message->data);
                this->accept(addr, size, source, client_id);
            #else
                this->accept(addr, size, source);
            #endif
        } else if(message->type == RDMAMessage::Type::ACCEPT) {
            #if FAULT_TOLERANT
                std::unordered_map<void*, RDMAMemory*>::iterator it = this->local_segments.find(addr);
                LogAssert(it != local_segments.end(), "memory not allocated");
                int64_t client_id = it->second->application_id;
                this->coordinator.updateSegmentDestination(client_id, it->second->pair);                
            #else
                std::unordered_map<void*, RDMAMemory*>::iterator it = this->memory_map.find(addr);
                LogAssert(it != memory_map.end(), "memory not allocated");            
            #endif
            this->incoming_accepts.enqueue(it->second);
        } else if(message->type == RDMAMessage::Type::DECLINE) {
            this->deregister_memory(addr, size, source);
            //maybe add notification to top
        } else if(message->type == RDMAMessage::Type::TRANSFER) {
            this->on_transfer(addr, size, source);
            #if FAULT_TOLERANT
                std::unordered_map<void*, RDMAMemory*>::iterator it = this->local_segments.find(addr);
                LogAssert(it != local_segments.end(), "memory not allocated");
            #else
                std::unordered_map<void*, RDMAMemory*>::iterator it = this->memory_map.find(addr);
                LogAssert(it != memory_map.end(), "memory not allocated");
            #endif

            this->incoming_transfers.enqueue(it->second);
        } else if(message->type == RDMAMessage::Type::DONE) {
            this->on_close(addr, size, source);
        } else if(message->type == RDMAMessage::Type::GETPARTITIONS) {
            #if FAULT_TOLERANT
                std::string str;
                for (auto x : local_segments) {
                    if(x.second->state == RDMAMemory::State::Clean){
                        str.append(std::to_string(x.second->application_id));
                        str.append(",");
                    }
                }
                uintptr_t conn_id = this->coordinator.connections[source];
                this->coordinator.getServer(source, conn_id)->sendPartitionList(conn_id,str);
            #endif
        }else if(message->type == RDMAMessage::Type::SENTPARTITIONS) {
            std::string data = message->data;
            #if FAULT_TOLERANT
                this->coordinator.partition_list = data;
                sem_post(&this->coordinator.sem_get_partition_list);
            #endif
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
void RDMAMemoryManager::SetPageSize(void* address, size_t page_size){
    auto x = memory_map.find(address);
    LogAssert(x != memory_map.end(), "address not found");
    RDMAMemory* memory = x->second;
    memory->pages.setPageSize(page_size);
}

inline 
void RDMAMemoryManager::MarkPageLocal(RDMAMemory* memory, void* address, size_t size) {
    if(mprotect(address, size, PROT_READ | PROT_WRITE)) {
        perror("couldnt mprotect in pull all pages");
        exit(errno);
    }    

    memory->pages.setPageState(address, PageState::Local);
}

inline 
void MarkPageLocalCB(void* data) {
    RDMAMemory* memory = (RDMAMemory*)(*((void**)data));
    data = (void*)((char*)data + sizeof(RDMAMemory*));

    void* address = *((void**)data);
    data = (void*)((char*)data + sizeof(void*));
     
    size_t size = *((size_t*)data);
    data = (void*)((char*)data + sizeof(size_t));    
    if(mprotect(address, size, PROT_READ | PROT_WRITE)) {
        perror("couldnt mprotect in pull all pages");
        exit(errno);
    }    
    
    std::atomic<int64_t>* x = (std::atomic<int64_t>*)*((void**)data);
        
    (*x).fetch_sub(1);

    memory->pages.setPageState(address, PageState::Local);
    free(data);
}

inline
void RDMAMemoryManager::PullAllPagesWithoutCloseAsync(RDMAMemory* memory){
    // auto x = memory_map.find(address);
    // RDMAMemory* memory = x->second;
    unsigned int id = 0;
    int source = memory->pair;
    std::atomic<int64_t>* rate_limiter = (std::atomic<int64_t>*) malloc(sizeof(std::atomic<int64_t>));

    LogAssert(source != -1, "source not set");

    vector<Page> p = memory->pages.pages;
    for (; id<p.size(); id++) {
        if(p.at(id).ps == PageState::Local)
            continue;

        while (*rate_limiter > max_async_pending){}

        (*rate_limiter).fetch_add(1);

        void* addr = memory->pages.getPageAddress(id);
        size_t pagesize = memory->pages.getPageSize(id);

        if(!memory->pages.setPageStateCAS(addr, PageState::Remote, PageState::InFlight)) {
            //page was not set to remote, so its either a inflight or local
            //either way, we do not need to do any operations, just continue
            continue;
        }

        void* data_ = (void*)malloc(sizeof(RDMAMemory*) + sizeof(void*) + sizeof(size_t) + sizeof(std::atomic<int64_t>*));
        void* data = data_;

        *((void**)data) = (void*)memory;
        data = (void*)((char*)data + sizeof(RDMAMemory*));

        *((void**)data) = addr;
        data = (void*)((char*)data + sizeof(void*));
        memcpy(data, &pagesize, sizeof(pagesize));
        
        data = (void*)((char*)data + sizeof(pagesize));
        
        *((void**)data) = (void*)rate_limiter;

        void(*callback)(void*);
        callback = MarkPageLocalCB;
        this->PullAsync(addr, pagesize, source, callback, data_);
    } 
}

inline
void RDMAMemoryManager::PullAllPagesWithoutClose(RDMAMemory* memory){
    // auto x = memory_map.find(address);
    // RDMAMemory* memory = x->second;
    unsigned int id = 0;
    int source = memory->pair;

    // LogAssert(x != memory_map.end(), "cannot find memory");
    LogAssert(source != -1, "source not set");

    vector<Page> p = memory->pages.pages;
    for (; id<p.size(); id++) {
        if(p.at(id).ps == PageState::Local)
            continue;

        void* addr = memory->pages.getPageAddress(id);
        size_t pagesize = memory->pages.getPageSize(id);

        if(!memory->pages.setPageStateCAS(addr, PageState::Remote, PageState::InFlight)) {
            //page was not set to remote, so its either a inflight or local
            //either way, we do not need to do any operations, just continue
            continue;
        }

        this->Pull(addr, pagesize, source);
        this->MarkPageLocal(memory, addr, pagesize);
    }
}

inline
void RDMAMemoryManager::PullAllPages(RDMAMemory* memory){
    check_pages_again:
    this->PullAllPagesWithoutClose(memory);
    int check_local_pages = memory->pages.local_pages.load();
    if(check_local_pages < memory->pages.num_pages) {
        LogInfo("did not have all pages, trying again");
        goto check_pages_again;
    }   
    int source = memory->pair;
    this->UpdateState(memory->vaddr, RDMAMemory::State::Clean);
    this->close(memory->vaddr, memory->size, source);
}

inline
int RDMAMemoryManager::PullPagesAsync(void* v_addr, size_t size, int source, int max_async_limit) {
    auto x = memory_map.find(v_addr);
    RDMAMemory* memory = x->second;
    
    std::atomic<int64_t>* rate_limiter = (std::atomic<int64_t>*) malloc(sizeof(std::atomic<int64_t>));

    size_t page_size = memory->pages.getPageSize();    
    uintptr_t segment_pull = (uintptr_t)v_addr;
    uintptr_t end_segment_pull = (uintptr_t)v_addr + size;

    for (;segment_pull<end_segment_pull; segment_pull+=page_size) {
        if(memory->pages.getPageState(segment_pull) == PageState::Local) {
            continue;
        }
        while (*rate_limiter > max_async_limit){}

        (*rate_limiter).fetch_add(1);

        void* addr = memory->pages.getPageAddress((void*)segment_pull);
        size_t pagesize = memory->pages.getPageSize((void*)segment_pull);

        if(!memory->pages.setPageStateCAS(addr, PageState::Remote, PageState::InFlight)) {
            //page was not set to remote, so its either a inflight or local
            //either way, we do not need to do any operations, just continue
            continue;
        }

        void* data_ = (void*)malloc(sizeof(RDMAMemory*) + sizeof(void*) + sizeof(size_t) + sizeof(std::atomic<int64_t>*));
        void* data = data_;

        *((void**)data) = (void*)memory;
        data = (void*)((char*)data + sizeof(RDMAMemory*));

        *((void**)data) = addr;
        data = (void*)((char*)data + sizeof(void*));
        memcpy(data, &pagesize, sizeof(pagesize));
        
        data = (void*)((char*)data + sizeof(pagesize));
        
        *((void**)data) = (void*)rate_limiter;

        void(*callback)(void*);
        callback = MarkPageLocalCB;
        this->PullAsync(addr, pagesize, source, callback, data_);
    }

    while((*rate_limiter) > 0);
    free(rate_limiter);
    return 0;
}

inline
int RDMAMemoryManager::PullPagesSync(void* v_addr, size_t size, int source) {
    auto x = memory_map.find(v_addr);
    RDMAMemory* memory = x->second;
    
    size_t page_size = memory->pages.getPageSize();    
    uintptr_t segment_pull = (uintptr_t)v_addr;
    uintptr_t end_segment_pull = (uintptr_t)v_addr + size;

    for (;segment_pull<end_segment_pull; segment_pull+=page_size) {
        if(memory->pages.getPageState(segment_pull) == PageState::Local) {
            continue;
        }

        void* addr = memory->pages.getPageAddress((void*)segment_pull);
        size_t pagesize = memory->pages.getPageSize((void*)segment_pull);

        if(!memory->pages.setPageStateCAS(addr, PageState::Remote, PageState::InFlight)) {
            //page was not set to remote, so its either a inflight or local
            //either way, we do not need to do any operations, just continue
            continue;
        }

        this->Pull(addr, pagesize, source);
        this->MarkPageLocal(memory, addr, pagesize);
    }
    
    return 0;
}
