#include <stdint.h>

#include <cstddef>
#include <iostream>
#include <string>
#include <math.h>

#include "utils/miscutils.hpp"
#include "distributed-allocator/mempool.hpp"
#include "distributed-allocator/RDMAMemory.hpp"

/*
    
*/

int main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "please provide all arguments" << std::endl;
        std::cerr << "./expRegionTransfer path_to_config server_id container_size iterations" << std::endl;
        return 1;
    }

    int id = atoi(argv[2]);
    size_t size = atoi(argv[3]);
    int iterations = atoi(argv[4]);
    #if PAGING
    manager = new RDMAMemoryManager(argv[1], id);
    initialize();
    #else 
    RDMAMemoryManager *manager = new RDMAMemoryManager(argv[1], id);
    #endif
    MultiTimer t = MultiTimer();
    std::vector<void*> addrs;

    if (id == 0) {
        for (int i=0; i<iterations; i++ ) {
            #if FAULT_TOLERANT
                void* address = manager->allocate(size, i);
            #else
                void* address = manager->allocate(size);
            #endif
            addrs.push_back(address);
        }
    }

    for (int i=0; i<iterations; i++ ) {
        if (id == 0) {

            t.start();
            manager->Prepare(addrs.at(i), size, 1);
            RDMAMemory* memory = nullptr;
            while((memory = manager->PollForAccept()) == nullptr) {}
            manager->Transfer(addrs.at(i), size, 1);
            while(manager->PollForClose() == nullptr) {}
            t.stop();

        } else {
            
            RDMAMemory* memory = nullptr;
            while((memory = manager->PollForTransfer()) == nullptr) {}
            manager->close(memory->vaddr, size, 0);
            
        }
    }
    
    if (id == 0) {
        std::vector<double> vec = t.getTime();
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

        printf("%lu, %f, %f\n",size, mean, sd);        
        
        for (int i=0; i<iterations; i++ ) {
            #if FAULT_TOLERANT
                manager->deallocate(i);
            #else
                manager->deallocate(addrs.at(i));
            #endif
        }
    }

        return 0;
}
