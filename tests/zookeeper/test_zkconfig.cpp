#include <iostream>
#include <map>

#include "distributed-allocator/RDMAMemory.hpp"
#include "distributed-allocator/mempool.hpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "please provide all arguments" << std::endl;
        std::cerr << "./test_zkconfig path_to_config server_id" << std::endl;
        return 1;
    }

    initialize();
    RDMAMemoryManager* memory_manager = new RDMAMemoryManager(argv[1], atoi(argv[2]));
    while(1){} 
    return 0;
}