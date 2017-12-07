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
    size_t size = atoi(argv[3]);
    RDMAMemoryManager* memory_manager = new RDMAMemoryManager(argv[1], id);
    manager = memory_manager;
    initialize();
    // using String = std::basic_string<char, std::char_traits<char>, PoolBasedAllocator<char>>;

    if (id == 0) {
        RDMAUnorderedMap<int, int> map(memory_manager);
        map.SetContainerSize(size);
        map.instantiate();
        
        for (int i=0; i<10000000; i++) {
            map[i] = 1;
        }

        map.Prepare(1);
        while(!map.PollForAccept()) {}
        
        // std::cerr << "Contents: \n";
        // for (auto i : map) {
        //     std::cerr << i.first << " " << i.second << "\n";
        // }
        // std::cerr << std::endl;

        map.Transfer();

        while(!map.PollForClose()) {}

    } else {
        RDMAUnorderedMap<int, int> map(memory_manager);
        map.SetContainerSize(size);

        while(!map.PollForTransfer()) {}

        map.remote_instantiate();
        
        for (int i=0; i<10000000; i++) {
            map[i];
        }

        std::cerr << "Contents: \n";
        for (auto i : map) {
            std::cerr << i.first << " " << i.second << "\n";
        }
        std::cerr << std::endl;

        map.Close();
    }
    LogInfo("EXPERIMENT COMPLETE");
    return 0;
}

