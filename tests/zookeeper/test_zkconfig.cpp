#include <iostream>
#include <map>

#include "distributed-allocator/mempool.hpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "please provide all arguments" << std::endl;
        return 1;
    }

    RDMAMemoryManager* memory_manager = new RDMAMemoryManager(argv[1], atoi(argv[2]));

    return 0;
}