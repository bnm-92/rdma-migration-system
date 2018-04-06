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

MultiTimer t1;
MultiTimer t2;

#if FAULT_TOLERANT

inline void allocate_n(RDMAMemoryManager* manager, size_t size, int iter, int app_id) {
    for (int i = 0; i<iter; i++) {
        t1.start();
        manager->allocate(size, app_id + i);
        t1.stop();
    }
}

inline void deallocate_n(RDMAMemoryManager* manager, size_t size, int iter, int app_id) {
    for (int i = 0; i<iter; i++) {
        t2.start();
        manager->deallocate(app_id + i);
        t2.stop();
    }
}
#else
std::vector<void*> addrs;

inline void allocate_n(RDMAMemoryManager* manager, size_t size, int iter) {
    for (int i = 0; i<iter; i++) {
        t1.start();
        void* addr = manager->allocate(size);
        t1.stop();
        addrs.push_back(addr);
    }
}

inline void deallocate_n(RDMAMemoryManager* manager, size_t size, int iter) {
    for (int i = 0; i<iter; i++) {
        t2.start();
        manager->deallocate(addrs.at(i));
        t2.stop();
    }
}
#endif

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "please provide all arguments" << std::endl;
        std::cerr << "./allocation path_to_config size" << std::endl;
        return 1;
    }

    int id = 0;
    size_t size = atoi(argv[2]); 
    manager = new RDMAMemoryManager(argv[1], id);
    initialize();

    MultiTimer m_allocate;
    MultiTimer m_deallocate;
    
    #if FAULT_TOLERANT
            m_allocate.start();
            allocate_n(manager, size, 100, 1);
            m_allocate.stop();

            m_deallocate.start();
            deallocate_n(manager, size, 100, 1);
            m_deallocate.stop();

    
    auto allocation_times = m_allocate.getTime();
    auto deallocation_times = m_deallocate.getTime();

    // for (unsigned int i =0; i< allocation_times.size(); i++) {
    //     std::cout << size << "," << allocation_times.at(i)/100000000 << "," << deallocation_times.at(i)/100000000 <<  "," << std::endl; 
    // }

    #else

    m_allocate.start();
    allocate_n(manager, size, 100);
    m_allocate.stop();

    m_deallocate.start();
    deallocate_n(manager, size, 100);
    m_deallocate.stop();

    auto allocation_times = m_allocate.getTime();
    auto deallocation_times = m_deallocate.getTime();

    // for (unsigned int i =0; i< allocation_times.size(); i++) {
    //     std::cout << size << "," << allocation_times.at(i)/100000000 << "," << deallocation_times.at(i)/100000000 << std::endl; 
    // }

    #endif
        
        std::vector<double> vec = t1.getTime();
        double sum1 = 0;
        double var1 = 0;
        double sd1 = 0;
        for(unsigned int i=0; i<vec.size(); i++) {
            sum1 += vec.at(i);
        }
        
        double mean1 = ((double)sum1/(double)vec.size());
        for(unsigned int i=0; i<vec.size(); i++) {
            var1 += (vec.at(i) - mean1) * (vec.at(i) - mean1);
        }

        var1 /= vec.size();
        sd1 = sqrt(var1);

        // printf("%lu, %f, %f\n",size, mean1, sd1);

        std::vector<double> vec2 = t2.getTime();
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

        printf("%lu, %f, %f, %f, %f\n",size, mean1/1000000, sd1/1000000, mean2/1000000, sd2/1000000);

    return 0;
}

