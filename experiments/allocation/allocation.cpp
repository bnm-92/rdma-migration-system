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

#if FAULT_TOLERANT
inline void allocate_n(RDMAMemoryManager* manager, size_t size, int iter, int app_id) {
    for (int i = 0; i<iter; i++) {
        manager->allocate(size, app_id + i);
    }
}

inline void deallocate_n(RDMAMemoryManager* manager, size_t size, int iter, int app_id) {
    for (int i = 0; i<iter; i++) {
        manager->deallocate(app_id + i);
    }
}
#else
std::vector<void*> addrs;

inline void allocate_n(RDMAMemoryManager* manager, size_t size, int iter) {
    for (int i = 0; i<iter; i++) {
        void* addr = manager->allocate(size);
        addrs.push_back(addr);
    }
}

inline void deallocate_n(RDMAMemoryManager* manager, size_t size, int iter) {
    for (int i = 0; i<iter; i++) {
        manager->deallocate(addrs.at(i));
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

    double sum1 = 0;
    double sum2 = 0;
    double sum3 = 0;
    double sum4 = 0;
    double sum5 = 0;

    for (auto x : a1.getTime()) {
        sum1 +=x;
    }

    for (auto x : a2.getTime()) {
        sum2 +=x;
    }

    for (auto x : a3.getTime()) {
        sum3 +=x;
    }

    for (auto x : a4.getTime()) {
        sum4 +=x;
    }

    for (auto x : a5.getTime()) {
        sum5 +=x;
    }

    for (unsigned int i =0; i< allocation_times.size(); i++) {
        std::cout << size << "," << allocation_times.at(i)/100000000 << "," << deallocation_times.at(i)/100000000 << "," << sum1/100000000 << "," << sum2/100000000 << "," << sum3/100000000 << "," << sum4/100000000 << "," << sum5/100000000 << "," << std::endl; 
    }

    #else

    m_allocate.start();
    allocate_n(manager, size, 100);
    m_allocate.stop();

    m_deallocate.start();
    deallocate_n(manager, size, 100);
    m_deallocate.stop();

    auto allocation_times = m_allocate.getTime();
    auto deallocation_times = m_deallocate.getTime();

    for (unsigned int i =0; i< allocation_times.size(); i++) {
        std::cout << size << "," << allocation_times.at(i)/100000000 << "," << deallocation_times.at(i)/100000000 << std::endl; 
    }

    #endif

    return 0;
}

