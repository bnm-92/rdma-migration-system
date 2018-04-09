#ifndef RDMAMEMNODE
#define RDMAMEMNODE

#include "rdma-network/rdma_server.hpp"
#include "rdma-network/rdma_client.hpp"
#include "utils/miscutils.hpp"
#if FAULT_TOLERANT
#include "zookeeper/zookeeper.hpp"
#endif
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include <utils/miscutils.hpp>


const std::string memory_segment_node = "/memory_segments";
const std::string process_node = "/process";
const std::string allocator_node = "/allocator";


class RDMAMemNode{
public:
    /*
        Sets up an RDMA node by reading the config file specified in the path, the config specifies the following:
        - range of shared contigious virtual addresses
        - number of servers and details of the servers
    */
    RDMAMemNode(std::string config_path, int server_id);
    ~RDMAMemNode();

    int connect_mesh();
    ConfigParser cfg;

    int server_id;

    RDMAServerPrototype* getServer(int id, uintptr_t conn_id) {
        return (id < server_id) ? (RDMAServerPrototype*)clients[conn_id] : (RDMAServerPrototype*)this->server; 
    }

    RDMAServer *server;
    
    std::unordered_map<uintptr_t, RDMAClient*> clients;

    std::unordered_map<int, uintptr_t> connections;
    #if FAULT_TOLERANT
    
    ZooKeeper *zk;
    
    sem_t sem_get_partition_list;
    std::string partition_list;


    void* getAllocationAddress(size_t size);
    int createMemorySegmentNode(void* address, size_t size, int source, int destination, int64_t clientSpecifiedID);
    std::map<std::string, std::string> getMemorySegment(int64_t app_id);
    int updateSegmentDestination(int64_t applicaiton_id, int destination);
    
    int deleteMemorySegment(int64_t app_id, int source);
    
    int addToProcessList(int64_t id);
    int removeFromProcessList(int64_t id);

    std::vector<std::map<std::string, std::string>> GetPartitionsForProcess(int64_t process_id);
    /**
     * Can only issue max one request at a time, not allocated extra resources for parallelism
    */
    std::vector<int64_t> GetLivePartitionsForProcess(int64_t process_id);

    int cleanMemorySegment(int64_t app_id);
    #endif
};

#endif //RDMAMEMNODE