#include "distributed-allocator/RDMAMemory.hpp"
#include "zookeeper/zookeeper.hpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("./basic_test config id server_id\n");
        return 1;
    }

    int server_id = atoi(argv[2]);
    RDMAMemoryManager manager(argv[1], server_id);
    
    printf("starting experiments\n");

    printf("connecting to zookeeper for leader election algorithm");
    ZooKeeper zk("10.70.0.4:2181" , "1000" ,"null");

    // set up failure detection
    Stat stat;
    std::string node = "/faults";
    int rc = zk.exists(node, &stat);
    if(rc == ZNONODE) {
        std::string** res = ;
        zk.create(node, "", ZOO_OPEN_ACL_UNSAFE,0, );
    }

        // node exists so you may simply add your node to it
        std::string** res;
        std::string path = node + "/" +std::to_string(server_id);
        zk.create(path, "", ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, res);


    void* one = manager.allocate(1024*1024, 0);
    if(one != nullptr) {
        printf("one succeeded\n");
    } else {
        printf("one failed\n");
    }
    void* two = manager.allocate(1024*1024, 1);
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

    manager.deallocate(1);

    res = manager.coordinator.GetPartitionsForProcess(server_id);
    for (auto x : res) {
        for (auto it : x)
            std::cout << it.first << " " << it.second << std::endl;
    }

    return 0;
}
