#include <stdint.h>

#include <cstddef>
#include <iostream>
#include <string>

// #include "mempool.hpp"
#include "distributed-allocator/mempool.hpp"
#include "c++-containers/rdma_vector.hpp"
#include "c++-containers/rdma_unordered_map.hpp"

/*
    
*/

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "please provide all arguments" << std::endl;
        std::cerr << "./exec path_to_config server_id container_size" << std::endl;
        return 1;
    }

    int id = atoi(argv[2]);
    size_t size = atoi(argv[3]);
    
    RDMAMemoryManager* memory_manager = new RDMAMemoryManager(argv[1], id);
    manager = memory_manager;
    // using String = std::basic_string<char, std::char_traits<char>, PoolBasedAllocator<char>>;


    timer = TestTimer();
    if (id == 0) {    
        RDMAUnorderedMap<int, int> map(memory_manager);
        map.SetContainerSize(size);
        map.instantiate();
        map[0] = 0;
        map[1] = 1;

        map.Prepare(1);
        while(!map.PollForAccept()) {}
        // std::cerr << "Contents: \n";
        // for (auto i : map) {
        //     std::cerr << i.first << " " << i.second << "\n";
        // }
        std::cerr << std::endl;        
        map.Transfer();
        while(!map.PollForClose()) {}

    } else {
        RDMAUnorderedMap<int, int> map(memory_manager);
        map.SetContainerSize(size);
        timer.start();
        while(!map.PollForTransfer()) {}
        timer.stop();
        double transfer_time = timer.get_duration_usec();
        map.remote_instantiate();
        map[3] = 4;
        // std::cerr << "Contents: \n";
        // for (auto i : map) {
        //     std::cerr << i.first << " " << i.second << "\n";
        // }
        std::cerr << std::endl;
        map.Close();
        printf("Transfer Timer, %f, Size in Bytes, %lu\n", transfer_time, size);
    }


    return 0;
}

