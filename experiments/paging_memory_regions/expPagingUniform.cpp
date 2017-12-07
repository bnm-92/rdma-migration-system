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
        std::cerr << "./expPagingSingle path_to_config server_id container_size page_size iter" << std::endl;
        return 1;
    }

    int id = atoi(argv[2]);
    size_t container_size = atoi(argv[3]);
    size_t page_size = atoi(argv[4]);
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
        manager->SetPageSize(memory->vaddr, page_size);
        

        void* addr = memory->vaddr;
        // void* end_addr = (void*)((char*)memory->vaddr + container_size);
        
        int num_pages = container_size/page_size;
        int arr_pages[num_pages];

        for(int i=0; i<num_pages; i++) {
            arr_pages[i] = i;
        }

        //lets shuffle them
        srand (time(NULL));
        int random;
        
        for (int i=0; i<num_pages; i++) {
            random = i + (rand() % static_cast<int>(num_pages - i));
            int temp = arr_pages[i];
            arr_pages[i] = arr_pages[random];
            arr_pages[random] = temp; 
        }

        for (int i=0; i<num_pages; i++) {
            addr = (void*) ((char*)memory->vaddr + (page_size*arr_pages[i]) );
            t.start();
            *((char*)addr) = 'a';
            t.stop();
        }
        // double time_taken = t.get_duration_usec();        
        // printf("time taken %f to pull page of size %lu\n", timer.get_duration_usec(), 4096);

        manager->close(memory->vaddr, container_size, 0);
    }

        std::vector<double> vec2 = t.getTime();
        std::vector<double> vec = multi_timer.getTime();
        
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
    
    return 0;
}

