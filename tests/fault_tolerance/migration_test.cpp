#include "distributed-allocator/RDMAMemory.hpp"
#include "zookeeper/zookeeper.hpp"

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("./migration_test config.txt server_id\n");
        return 1;
    }

    int server_id = atoi(argv[2]);
    RDMAMemoryManager manager(argv[1], server_id);
    
    printf("starting experiments\n");

    printf("connecting to zookeeper for leader election algorithm\n");
    ZooKeeper zk("10.30.0.14:2181" , 2000 , nullptr);

    // set up failure detection
    Stat stat;
    std::string node = "/faults";
    int rc = zk.exists(node, &stat);
    if(rc == ZNONODE) {
        std::string** res = (std::string**)malloc(sizeof(std::string*));
        zk.create(node, "", ZOO_OPEN_ACL_UNSAFE, 0, res);
    }

    // node exists so you may simply add your node to it
    std::string** res = (std::string**)malloc(sizeof(std::string*));;
    std::string path = node + "/" +std::to_string(server_id);
    
    rc = zk.create(path, "", ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, res);

    if(rc == ZOK) {
        std::cout << "created node " << path << std::endl; 
    }

    //make callback watcher function
        void (*fn) (zhandle_t *zzh, int type, int state, const char *path, void* context) = ([](zhandle_t *zzh, int type, int state, const char *path, void* context) -> void {
        printf("in call back at server\n");
        
        ZooKeeper *zk = (ZooKeeper*)context;
        
        std::vector<std::string> results;
        std::string node = "/faults";
        
        int rc = zk->getChildren(node, &results);
        if(rc != ZOK) {
            LogError("somethings wrong");
            exit(1);
        }

        for (std::string x : results) {
            std::cout << x << std::endl;
        }

    });
    
    std::vector<std::string> vec;
    zk.wgetChildren(node, &vec, fn, (void*)&zk);

    // zk.wexists(node, &stat, fn, (void*)&zk);
    for (std::string x : vec) {
        std::cout << x << std::endl;
    }

    while(1){}

    // void* one = manager.allocate(1024*1024, 0);
    // if(one != nullptr) {
    //     printf("one succeeded\n");
    // } else {
    //     printf("one failed\n");
    // }
    // void* two = manager.allocate(1024*1024, 1);
    // if(two != nullptr) {
    //     printf("two succeeded\n");
    // } else {
    //     printf("two failed\n");
    // }

    // std::vector<std::map<std::string, std::string>> res = manager.coordinator.GetPartitionsForProcess(server_id);
    // for (auto x : res) {
    //     for (auto it : x)
    //         std::cout << it.first << " " << it.second << std::endl;
    // }

    // manager.deallocate(1);

    // res = manager.coordinator.GetPartitionsForProcess(server_id);
    // for (auto x : res) {
    //     for (auto it : x)
    //         std::cout << it.first << " " << it.second << std::endl;
    // }

    return 0;
}
