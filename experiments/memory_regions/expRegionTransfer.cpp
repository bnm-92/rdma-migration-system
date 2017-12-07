#include <stdint.h>

#include <cstddef>
#include <iostream>
#include <string>
#include <math.h>

#include "mempool.hpp"
#include "rdma_vector.hpp"
#include "rdma_unordered_map.hpp"

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
    
    RDMAMemoryManager* manager = new RDMAMemoryManager(argv[1], id);

    MultiTimer t = MultiTimer();

    for (int i=0; i<iterations; i++ ) {
        if (id == 0) {
            void* address = manager->allocate(size);
            manager->Prepare(address, size, 1);
            RDMAMemory* memory = nullptr;
            while((memory = manager->PollForAccept()) == nullptr) {}
            manager->Transfer(address, size, 1);
            while(manager->PollForClose() == nullptr) {}

        } else {
            t.start();
            RDMAMemory* memory = nullptr;
            while((memory = manager->PollForTransfer()) == nullptr) {}
            manager->close(memory->vaddr, size, 0);
            t.stop();
        }
    }
    
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

    printf("region transfer time,%lu, %f, %f\n",size, mean, sd);
    return 0;
}

