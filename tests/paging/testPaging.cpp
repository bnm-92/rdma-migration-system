#include <stdint.h>

#include <cstddef>
#include <iostream>
#include <string>

#include "distributed-allocator/mempool.hpp"
#include "c++-containers/rdma_vector.hpp"
#include "c++-containers/rdma_unordered_map.hpp"
#include "paging/paging.hpp"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "./testPaging path_to_config serverid" << std::endl;
        return 1;
    }

    int id = atoi(argv[2]);
    RDMAMemoryManager* memory_manager = new RDMAMemoryManager(argv[1], id);
    manager = memory_manager;
    initialize();
    // using String = std::basic_string<char, std::char_traits<char>, PoolBasedAllocator<char>>;

    if (id == 0) {
        RDMAUnorderedMap<int, int> map(memory_manager);
        map.instantiate();
        map[0] = 0;
        map[1] = 1;
        map.Prepare(1);
        while(!map.PollForAccept()) {}
        
        std::cerr << "Contents: \n";
        for (auto i : map) {
            std::cerr << i.first << " " << i.second << "\n";
        }
        std::cerr << std::endl;

        map.Transfer();

        while(!map.PollForClose()) {}

    } else {
        RDMAUnorderedMap<int, int> map(memory_manager);
        while(!map.PollForTransfer()) {}

        map.remote_instantiate();
        std::thread t(&RDMAUnorderedMap<int, int>::PullAsync, &map, 10);
        // map.PullAsync(10);
        t.join();
        map[3] = 4;

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

