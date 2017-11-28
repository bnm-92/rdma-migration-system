#include <stdint.h>

#include <cstddef>
#include <iostream>
#include <string>

#include "mempool.hpp"
#include "rdma_vector.hpp"
#include "rdma_unordered_map.hpp"
#include "paging.hpp"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "please provide all arguments" << std::endl;
        return 1;
    }

    int id = atoi(argv[2]);
    size_t size = 4096;
    RDMAMemoryManager* memory_manager = new RDMAMemoryManager(argv[1], id);
    manager = memory_manager;
    initialize();
    // using String = std::basic_string<char, std::char_traits<char>, PoolBasedAllocator<char>>;

    if (id == 0) {
        RDMAUnorderedMap<int, int> map(memory_manager);
        map.SetContainerSize(size);
        map.instantiate();
        map[0] = 0;
        map[1] = 1;
        map.Prepare(1);
        while(!map.PollForAccept()) {}
        map.Transfer();
        while(!map.PollForClose()) {}

    } else {
        RDMAUnorderedMap<int, int> map(memory_manager);
        map.SetContainerSize(size);

        while(!map.PollForTransfer()) {}
        timer = TestTimer();
        TestTimer t = TestTimer();
        t.start();
        map.remote_instantiate();
        t.stop();

        double time_taken = t.get_duration_usec();
        
        printf("time taken %f to pull page of size %lu\n", timer.get_duration_usec(), 4096);
        printf("time taken to instantiate %f with pagesize %d\n", time_taken, 4096);
        map.Close();
    }
    LogInfo("EXPERIMENT COMPLETE");
    return 0;
}

