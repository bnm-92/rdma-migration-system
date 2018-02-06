#ifndef __RDMAMemory
#define __RDMAMemory

#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "utils/miscutils.hpp"
#include "distributed-allocator/RDMAMemNode.hpp"
#include "paging/paging.hpp"

/*
    This is the basic unit of RDMAable memory that can be called by the application 
    above it
*/

class RDMAMemory{
public:
    RDMAMemory(int owner, void* addr, size_t size);
    RDMAMemory(int owner, void* addr, size_t size, size_t page_size);
    #if FAULT_TOLERANT
        RDMAMemory(int owner, void* addr, size_t size, int64_t app_id);
        RDMAMemory(int owner, void* addr, size_t size, size_t page_size, int64_t app_id);
    #endif
    ~RDMAMemory();
    enum class State {         
        Dirty,         
        Invalid,         
        Shared,         
        Clean,         
        Owned,     
    };
    void* vaddr;
    size_t size;
    int owner;
    State state;
    int pair;

    #if FAULT_TOLERANT
        int64_t application_id;
    #endif

    Pages pages;
};

/*
    global functions/headers for async pulls
*/

void MarkPageLocalCB(void* data);

/*
    A server will access memory regions from RDMAMemoryFactory
    - it will be initialized with the upper and lower region along with a coordinator
    - its also useed for managing the servers RDMAableMemory Regions
*/

class RDMAMemoryManager {

public:
    RDMAMemoryManager(std::string config, int serverid);
    ~RDMAMemoryManager();
    
public:
    RDMAMemory* getRDMAMemory(void* address);
    #if FAULT_TOLERANT
    void* allocate(size_t size, int64_t id);
    int deallocate(int64_t application_id);
    #else
    void* allocate(size_t size);
    void deallocate(void* v_addr);
    #endif
    

    // sending routines
    int Prepare(void* v_addr, size_t size, int destination);
    RDMAMemory* PollForAccept();
    int Transfer(void* v_addr, size_t size, int destination);

    //receiving routines
    RDMAMemory* PollForTransfer();
    int Pull(void* v_addr, size_t size, int source);
    int Push(void* v_addr, size_t size, int source);

    /**
     *  this should give you access to the entire memory 
     *  because the user has to ensure they have brought over all the memory required for closing the segment
     */
    void close(void* v_addr, size_t size, int source);
    
    void SetPageSize(void* address, size_t page_size);

    #if FAULT_TOLERANT
        void* allocate(void* v_addr, size_t size, int64_t applicaiton_id);
    #else 
        void* allocate(void* v_addr, size_t size);
    #endif
    
    RDMAMemory* PeekAccept();

        /**
     * detaches a thread for pulling in pages from adress to size provided
     * rate limiting at number of pages,
    */

    int PullPagesAsync(void* v_addr, size_t size, int source, int rate_limiter);
    
    /**
     * detaches a thread for pulling in pages starting at the address to size provided
     * syncs at every pull
     * ie it will pull in one page at a time with the rate limitation of one page 
     * and then finish the thread
    */
    int PullPagesSync(void* v_addr, size_t size, int source);


    /**
     * Pull methods for bringing over the entire segment
    */
    void PullAllPagesWithoutCloseAsync(RDMAMemory* memory);
    void PullAllPagesWithoutClose(RDMAMemory* memory);
    void PullAllPages(RDMAMemory* memory);

    /**
     * Methods for detecting close
    */
    RDMAMemory* PollForClose();
    RDMAMemory* PeekClose();


    int PullAsync(void* v_addr, size_t size, int source, void (*callback)(void*), void* data);

    void MarkPageLocal(RDMAMemory* memory, void* address, size_t size);
private:
    int pull(void* v_addr, int source);
    int push(void* v_addr, int destination);
    int pull(void* v_addr, size_t size);
    int push(void* v_addr, size_t size);
    int pull(void* v_addr);
    int push(void* v_addr);

    int prepare(void* v_addr, int destination);
    void* accept(void* v_addr, size_t size, int source);
    void* accept(void* v_addr, size_t size, int source, int64_t client_id);
    int transfer(void* v_addr, size_t size, int destination);
    void on_transfer(void* v_addr, size_t size, int source);

    void register_memory(void* v_addr, size_t size, int destination);
    void deregister_memory(void* v_addr, size_t size, int destination);

    void on_close(void* addr, size_t size, int pair);

    int UpdateState(void* memory, RDMAMemory::State state);
    int UpdatePair(void* memory, int id); 
    
    class RDMAMessage {
    public:
        enum class Type{
            PREPARE,
            ACCEPT,
            DECLINE,
            TRANSFER,
            DONE,
            GETPARTITIONS,
            SENTPARTITIONS,
            USER,
        } type;

        void* addr;
        size_t size;
        void* container_address;
        size_t used;
        char* data;
        RDMAMessage(void* addr, size_t size, Type type, char* data) {
            this->addr = addr;
            this->size = size;
            this->type = type;
            this->data = data;
        }

        RDMAMessage(void* addr, size_t size, Type type, void* container_address, size_t used) {
            this->addr = addr;
            this->size = size;
            this->type = type;
            this->container_address = container_address;
            this->used = used;
        }

    };

    int HasMessage();
    bool HasMessage(int source);
    RDMAMessage::Type getMessageType(rdma_message::MessageType type);
    RDMAMessage* GetMessage(int source);

    int server_id = -1;
    int message_check = 0;

    uintptr_t start_address;
    uintptr_t end_address;
    
    uintptr_t alloc_address;

    RDMAMemNode coordinator;
    //memory list
    std::unordered_map<void*, RDMAMemory*> memory_map;
    //free list
    std::unordered_map<size_t, std::vector<RDMAMemory*>> free_map;
    #if FAULT_TOLERANT
    std::unordered_map<void*, RDMAMemory*> local_segments;
    #endif

    //thread for polling the queue
    std::thread poller_thread;
    void poller_thread_method();
    ThreadsafeQueue<RDMAMemory*> incoming_transfers;
    ThreadsafeQueue<RDMAMemory*> incoming_accepts;
    ThreadsafeQueue<RDMAMemory*> incoming_dones;
    volatile bool run;

    std::atomic<int> num_threads_pulling;                
};

#include "distributed-allocator/RDMAMemory.tpp"
#if PAGING
static RDMAMemoryManager* manager = nullptr;
static struct sigaction act;

/*
    this method works with simple single threaded paging for now
    methods for active paging are written, we would need to add
    thread synchronization and waiting for threads by setting 
    and getting the page state
*/

static void sigsegv_advance(int signum, siginfo_t *info_, void* ptr) { 
    if(manager == nullptr) {
        LogError("Manager is null");
        perror("Manager not declared for static access in sigsegv fault handler");
        exit(errno);
    }

    // memory location of the fault
    void* addr = info_->si_addr;
    RDMAMemory* memory = manager->getRDMAMemory(addr);
    LogAssert(memory != nullptr, "memory not found");
    int source = memory->pair;
    
    addr = memory->pages.getPageAddress(addr);
    size_t page_size = memory->pages.getPageSize(addr); 

    if(!memory->pages.setPageStateCAS(addr, PageState::Remote, PageState::InFlight)) {
        // its either in flight or local, if its in flight, we will generate another sigsegv
        //if its now local then processing should continue
        return;
    }
    
    manager->Pull(addr, page_size, source);
    memory->pages.setPageState(addr, PageState::Local);    

    if(mprotect(addr, page_size, PROT_READ | PROT_WRITE)) {
        perror("couldnt mprotect in sigsegv");
        exit(errno);
    }
}

static void initialize() {
    memset(&act, 0, sizeof(act));
    act.sa_sigaction = sigsegv_advance;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &act, NULL);
}
#endif //PAGING
#endif //RDMAMemoryfire