#include "distributed-allocator/RDMAMemory.hpp"
#include "zookeeper/zookeeper.hpp"

//make callback watcher function
void (*fn) (zhandle_t *zzh, int type, int state, const char *path, void* context) = ([](zhandle_t *zzh, int type, int state, const char *path, void* context) -> void {
    printf("something failed, lets check\n");
    
    ZooKeeper *zk = (ZooKeeper*)context;
    std::vector<int> failed_process;
    
    //add all processes, this is a hack for testing, 
    //in real application, we would track a live list and compare against that only
    int max_servers = 2;
    for (int i=0; i<max_servers; i++) {
        failed_process.push_back(i);
    }

    std::vector<std::string> results;
    std::string node = "/faults";
    
    int rc = zk->getChildren(node, &results);
    if(rc != ZOK) {
        LogError("somethings wrong");
        exit(1);
    }

    for (std::string x : results) {
        failed_process.erase(
            std::remove(failed_process.begin(), failed_process.end(), atoi(x.c_str())), 
            failed_process.end());
    }

    for(int fail : failed_process) {
        // for each currently failing process
        // get partitions affected
        LogInfo("cleaning partitions at process %d\n", fail);
        auto partitions = manager->coordinator.GetPartitionsForProcess(fail);
        for (auto map : partitions) {
            if(atoi(map["source"].c_str()) == fail) {
                //check destination
                if(map["destination"] != "") {
                    //destination is set, get local process list from destination
                    int destination = atoi(map["destination"].c_str());
                    std::vector<int64_t> live_list = manager->coordinator.GetLivePartitionsForProcess(destination);
                    std::string list = "";
                    for (int x : live_list) {
                        list.append(std::to_string(x));
                        list.append(",");
                    }

                    LogInfo("live list is %s", list.c_str());
                    int app_id = atoi(map["clientID"].c_str());
                    auto it = std::find(live_list.begin(), live_list.end(), app_id);
                    if(it != live_list.end()) {
                        //found it
                        printf("do nothing\n");
                    } else {
                        //deallocate
                        LogInfo("did not find it so, deallocating\n");
                        manager->deallocate(atoi(map["clientID"].c_str()));
                    }
                } else {
                    //simply deallocate
                    LogInfo("destination not set so deallocating\n");
                    manager->deallocate(atoi(map["clientID"].c_str()));
                }

            } else if (atoi(map["destination"].c_str()) == fail) {
                //check source
                LogInfo("potential migration failure, lets check source\n");
                int source = atoi(map["source"].c_str());
                LogInfo("source is %d", source);
                
                std::vector<int64_t> live_list = manager->coordinator.GetLivePartitionsForProcess(source);
                std::string list = "";
                for (int x : live_list) {
                    list.append(std::to_string(x));
                    list.append(",");
                }

                LogInfo("live list is %s", list.c_str());
                int app_id = atoi(map["clientID"].c_str());
                auto it = std::find(live_list.begin(), live_list.end(), app_id);
                if(it != live_list.end()) {
                    //found it
                    printf("do nothing\n");
                } else {
                    //deallocate
                    LogInfo("deallocating\n");
                    manager->deallocate(atoi(map["clientID"].c_str()));
                }

            }
        }
        LogInfo("process node list will be created or refreshed when it is restarted so it doesnt need deletion");

    }

});

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("./migration_test config.txt server_id\n");
        return 1;
    }

    int server_id = atoi(argv[2]);
    manager = new RDMAMemoryManager (argv[1], server_id);
    // initialize();

    printf("starting experiments\n");

    printf("connecting to zookeeper for leader election algorithm\n");
    ZooKeeper zk("10.10.0.7:2181" , 2000 , nullptr);

    // set up failure detection
    Stat stat;
    std::string node = "/faults";
    int rc = zk.exists(node, &stat);
    if(rc == ZNONODE) {
        std::string res = "";
        zk.create(node, "", ZOO_OPEN_ACL_UNSAFE, 0, &res);
    }

    // node exists so you may simply add your node to it
    std::string res = "";
    std::string path = node + "/" +std::to_string(server_id);
    
    rc = zk.create(path, "", ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, &res);

    if(rc == ZOK) {
        std::cout << "created node " << path << std::endl; 
    }
    
    std::vector<std::string> vec;
    zk.wgetChildren(node, &vec, fn, (void*)&zk);

    // zk.wexists(node, &stat, fn, (void*)&zk);
    for (std::string x : vec) {
        std::cout << x << std::endl;
    }

    if(server_id == 0) {
        // source
        size_t size = 1024*1024*1024;
        char* memory = (char*)manager->allocate(size, 25);
        LogInfo("allocated memory at %lu", (uintptr_t)memory);

        /*
            Prepare
        */
        manager->Prepare((void*)memory, size, 1);
        RDMAMemory* rdma_memory  = nullptr;
        while((rdma_memory = manager->PollForAccept()) == nullptr);
        LogInfo("Transfering memory: %p of size %zu", (void*)memory, size);

        /*
            Transfer
        */
        manager->Transfer(rdma_memory->vaddr, rdma_memory->size, rdma_memory->pair);
        manager->PollForClose();

    
    } else {
        // destination
        RDMAMemory* rdma_memory  = nullptr;
        while((rdma_memory = manager->PollForTransfer()) == nullptr) {
        }
        LogInfo("got memory %p", rdma_memory->vaddr);
        manager->close(rdma_memory->vaddr, rdma_memory->size, rdma_memory->pair);
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
