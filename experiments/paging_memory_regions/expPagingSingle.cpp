#include <stdint.h>

#include <cstddef>
#include <iostream>
#include <string>
#include <math.h>
#include "mempool.hpp"
#include "rdma_vector.hpp"
#include "rdma_unordered_map.hpp"
#include "paging.hpp"

int main(int argc, char* argv[]) {
    if (argc < 6) {
        std::cerr << "./expPagingSingle path_to_config server_id container_size page_size iter" << std::endl;
        return 1;
    }

    int id = atoi(argv[2]);
    size_t container_size = atoi(argv[3]);
    size_t page_size = atoi(argv[4]);

    //initialize
    RDMAMemoryManager* memory_manager = new RDMAMemoryManager(argv[1], id);
    manager = memory_manager;
    initialize();

    multi_timer = MultiTimer();
    MultiTimer t = MultiTimer();

    if (id == 0) {
        void* address = manager->allocate(container_size);
        manager->SetPageSize(address, page_size);
        manager->Prepare(address, container_size, 1);
        RDMAMemory* memory = nullptr;
        while((memory = manager->PollForAccept()) == nullptr) {}
        manager->Transfer(address, container_size, 1);
        while(manager->PollForClose() == nullptr) {}

    } else {

        RDMAMemory* memory = nullptr;
        while((memory = manager->PollForTransfer()) == nullptr) {}

        void* addr = memory->vaddr;
        manager->SetPageSize(addr, page_size);
        void* end_addr = (void*)((char*)memory->vaddr + container_size);
        LogInfo("addr is %p\n", addr);
        while((uintptr_t)addr < (uintptr_t)end_addr) {
            t.start();
            *((char*)addr) = 'a';
            t.stop();
            addr = (void*) ((char*)addr + page_size);
        }
        manager->close(memory->vaddr, container_size, 0);
    }

        std::vector<double> vec = multi_timer.getTime();
        std::vector<double> vec2 = t.getTime();
        // for(unsigned int i=0; i<vec.size(); i++) {
        //     printf("fault, %f, pull in fault, %f, pagesize, %lu\n", vec2.at(i), vec.at(i), page_size);
        // }

    if (id != 0) {
        //this is the pulls
        double sum = 0;
        double var = 0;
        double sd = 0;
        for(unsigned int i=0; i<vec.size(); i++) {
            sum += vec.at(i);
        }
        
        double mean = ((double)sum/(double)vec.size());
        for(unsigned int i=0; i<vec.size(); i++) {
            var += (vec.at(i) - mean) * (vec.at(i) - mean);
        }

        var /= vec.size();
        sd = sqrt(var);
        
        printf("size, %lu, pull, %f, %f, ", page_size, mean, sd);
        
        //this is the total
        sum = 0;
        var = 0;
        sd = 0;
        for(unsigned int i=0; i<vec2.size(); i++) {
            sum += vec2.at(i);
        }
        
        mean = ((double)sum/(double)vec2.size());
        for(unsigned int i=0; i<vec2.size(); i++) {
            var += (vec2.at(i) - mean) * (vec2.at(i) - mean);
        }

        var /= vec2.size();
        sd = sqrt(var);
        printf("fault and pull, %f, %f\n", mean, sd);        
    }

    LogInfo("EXPERIMENT COMPLETE");
    
    return 0;
}

