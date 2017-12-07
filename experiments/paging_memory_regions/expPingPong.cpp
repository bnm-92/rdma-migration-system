#include <stdint.h>

#include <cstddef>
#include <iostream>
#include <string>
#include <random>

#include "mempool.hpp"
#include "rdma_vector.hpp"
#include "rdma_unordered_map.hpp"
#include "paging.hpp"

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "./expPingPong path_to_config server_id container_size page_size" << std::endl;
        return 1;
    }

    int id = atoi(argv[2]);
    size_t container_size = atoi(argv[3]);
    size_t page_size = atoi(argv[4]);

    RDMAMemoryManager* memory_manager = new RDMAMemoryManager(argv[1], id);
    manager = memory_manager;
    initialize();
    
    // int exp_duration = 1;
    int exp_iterations = 10000;
    RDMAMemory* memory = nullptr;
    void* address = nullptr;
    MultiTimer t = MultiTimer();    
    t.start();
    for (int i=0; i< exp_iterations; i++) {
        if (id == 0) {
            // printf("server 0\n");
            address = manager->allocate(container_size);
            memory = nullptr;

            manager->Prepare(address, container_size, 1);
            memory = nullptr;
            while((memory = manager->PollForAccept()) == nullptr) {}
            manager->Transfer(address, container_size, 1);
            
            // printf("TRANSFERRED\n");

            // memory = nullptr;
            // while((memory = manager->PollForTransfer()) == nullptr) {}
            // printf("RECEIVED\n");
        } else {
            // printf("server 1\n");
            
            while((memory = manager->PollForTransfer()) == nullptr) {}
            

            // printf("RECEIVED\n");
            // manager->Prepare(address, container_size, 0);
            // memory = nullptr;
            // while((memory = manager->PollForAccept()) == nullptr) {}
            // manager->Transfer(address, container_size, 0);
            // printf("TRANSFERRED\n");

        }
    }
    t.stop();
    printf("EXPERIMENT COMPLETE");
    std::vector<double> vec2 = t.getTime();
    double x = 0;
    for(unsigned int i=0; i<vec2.size(); i++) {
        x += vec2.at(i);
        printf("Time Taken: %f\n", vec2.at(i));
    }

    printf("average time taken: %f", double(x/exp_iterations));
    fflush(stdout);
    
    return 0;
}

