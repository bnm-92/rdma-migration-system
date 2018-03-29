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
    
    RDMAMemoryManager *manager = new RDMAMemoryManager(argv[1], id);
    // initialize();

    MultiTimer t = MultiTimer();
    int offset_id = iterations;
    for (int i=0; i<iterations; i++ ) {
        if (id == 0) {
            RDMAMemory* memory = nullptr;
            RDMAMemory* memory1 = nullptr;

            void* address = manager->allocate(size, i);

            manager->Prepare(address, size, 1);
            
            void* addr = nullptr;
            while((addr = manager->PollForPrepare()) == nullptr) {}

            manager->accept(addr, size, 1, i);
            
            while((manager->PollForAccept()) == nullptr) {}

            t.start();
            multi_timer.start();
            manager->Transfer(address, size, 1);
            while((memory1 = manager->PollForTransfer()) == nullptr) {}
            t.stop();

            // manager->deallocate(addr);
        } else {
            RDMAMemory* memory1 = nullptr;
            RDMAMemory* memory = nullptr;
            
            void * addr = nullptr;
            while((addr = manager->PollForPrepare()) == nullptr) {}
            
            void* address = manager->allocate(size, i + offset_id);
            manager->Prepare(address, size, 0);

            while((manager->PollForAccept()) == nullptr) {}
            manager->accept(addr, size, 0, i + offset_id);

            while((memory1 = manager->PollForTransfer()) == nullptr) {}
            multi_timer.start();
            
            manager->Transfer(address, size, 0);


            // manager->deallocate(addr);
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

        std::vector<double> vec2 = multi_timer.getTime();
        double sum2 = 0;
        double var2 = 0;
        double sd2 = 0;
        for(unsigned int i=0; i<vec2.size(); i++) {
            sum2 += vec2.at(i);
        }
        
        double mean2 = ((double)sum2/(double)vec2.size());
        for(unsigned int i=0; i<vec2.size(); i++) {
            var2 += (vec2.at(i) - mean2) * (vec2.at(i) - mean2);
        }

        var2 /= vec2.size();
        sd2 = sqrt(var2);


        printf("%lu, %f, %f, %f, %f\n",size, mean, sd, mean2, sd2);        
    }

        return 0;
}

