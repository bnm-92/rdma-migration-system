#include "distributed-allocator/RDMAMemory.hpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("./basic_test config id server_id\n");
        return 1;
    }
    int server_id = atoi(argv[2]);
    RDMAMemoryManager manager(argv[1], server_id);
    
    printf("starting experiments\n");

    void* one = manager.allocate(1024*1024, 0);
    if(one != nullptr) {
        printf("one succeeded\n");
    } else {
        printf("one failed\n");
    }
    void* two = manager.allocate(1024*1024, 0);
    if(two != nullptr) {
        printf("two succeeded\n");
    } else {
        printf("two failed\n");
    }

    std::vector<std::map<std::string, std::string>> res = manager.coordinator.GetPartitionsForProcess(server_id);
    for (auto x : res) {
        for (auto it : x)
            std::cout << it.first << " " << it.second << std::endl;
    }

    return 0;
}
