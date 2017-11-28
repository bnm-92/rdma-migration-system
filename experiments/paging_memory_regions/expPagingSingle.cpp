#include <stdint.h>

#include <cstddef>
#include <iostream>
#include <string>

#include "mempool.hpp"
#include "rdma_vector.hpp"
#include "rdma_unordered_map.hpp"
#include "paging.hpp"

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "./expPagingSingle path_to_config server_id container_size page_size" << std::endl;
        return 1;
    }

    int id = atoi(argv[2]);
    size_t container_size = atoi(argv[3]);
    size_t page_size = atoi(argv[4]);
    RDMAMemoryManager* memory_manager = new RDMAMemoryManager(argv[1], id);
    manager = memory_manager;
    initialize();

    void* address = manager->allocate(container_size);
    manager->SetPageSize(address, page_size);

    if (id == 0) {
        manager->Prepare(address, container_size, 1);
        RDMAMemory* memory = nullptr;
        while((memory = manager->PollForAccept()) == nullptr) {}
        manager->Transfer(address, container_size, 1);
        while(manager->PollForClose() == nullptr) {}

    } else {

        RDMAMemory* memory = nullptr;
        while((memory = manager->PollForTransfer()) == nullptr) {}

        multi_timer = MultiTimer();
        MultiTimer t = MultiTimer();
        void* addr = memory->vaddr;
        void* end_addr = (void*)((char*)memory->vaddr + container_size);
        LogInfo("addr is %p\n", addr);
        while((uintptr_t)addr < (uintptr_t)end_addr) {
            t.start();
            *((char*)addr) = 'a';
            t.stop();
            addr = (void*) ((char*)addr + page_size);
        }
        std::vector<double> vec = multi_timer.getTime();
        std::vector<double> vec2 = t.getTime();
        for(unsigned int i=0; i<vec.size(); i++) {
            printf("fault, %f, pull in fault, %f, pagesize, %lu\n", vec2.at(i), vec.at(i), page_size);
        }

        // double time_taken = t.get_duration_usec();        
        // printf("time taken %f to pull page of size %lu\n", timer.get_duration_usec(), 4096);


        manager->close(address, container_size, 0);
    }

    LogInfo("EXPERIMENT COMPLETE");
    
    return 0;
}

