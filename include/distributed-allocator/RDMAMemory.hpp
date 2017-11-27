#ifndef __RDMAMemory
#define __RDMAMemory

#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "miscutils.hpp"
#include "RDMAMemNode.hpp"

/*
    This is the basic unit of RDMAable memory that can be called by the application 
    above it
*/

class RDMAMemory{
public:
    RDMAMemory(int owner, void* addr, size_t size);
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
};

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

    void* allocate(void* v_addr, size_t size);
    void* allocate(size_t size);
    void deallocate(void* v_addr);
    void deallocate(void* v_addr, size_t size);

    // sending routines
    int Prepare(void* v_addr, size_t size, int destination);
    RDMAMemory* PollForAccept();
    int Transfer(void* v_addr, size_t size, int destination);

    //receiving routines
    RDMAMemory* PollForTransfer();
    int Pull(void* v_addr, size_t size, int source);
    int Push(void* v_addr, size_t size, int source);

    //clean up
    void close(void* v_addr, size_t size, int source);
    RDMAMemory* PollForClose();
    
//TODO Make private or REMOVE
private:
    int pull(void* v_addr, int source);
    int push(void* v_addr, int destination);
    int pull(void* v_addr, size_t size);
    int push(void* v_addr, size_t size);
    int pull(void* v_addr);
    int push(void* v_addr);

    int prepare(void* v_addr, int destination);
    void* accept(void* v_addr, size_t size, int source);
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
        } type;

        void* addr;
        size_t size;
        void* container_address;
        size_t used;
        RDMAMessage(void* addr, size_t size, Type type) {
            this->addr = addr;
            this->size = size;
            this->type = type;
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

    //thread for polling the queue
    std::thread poller_thread;
    void poller_thread_method();
    ThreadsafeQueue<RDMAMemory*> incoming_transfers;
    ThreadsafeQueue<RDMAMemory*> incoming_accepts;
    ThreadsafeQueue<RDMAMemory*> incoming_dones;
    volatile bool run;
};

#include "RDMAMemory.tpp"

#endif //RDMAMemory