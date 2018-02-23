#include <algorithm>

inline
RDMAMemNode::RDMAMemNode(std::string config_path, int server_id): 
cfg(), server(nullptr), zk(nullptr) {
    //initialize vars
    this->server_id = server_id;
    server = new RDMAServer();
    
    //parse config
    cfg.parse(config_path);
    //connect with zookeeper
    #if FAULT_TOLERANT
    LogInfo("connecting to zookeeper");
    if (cfg.getZKLogFileHandle() == nullptr)
        zk = new ZooKeeper(cfg.getZKServerList(), cfg.getZKHeartbeatTimeout(), stderr);
    else
        zk = new ZooKeeper(cfg.getZKServerList(), cfg.getZKHeartbeatTimeout(), cfg.getZKLogFileHandle());

    // lets create basic zookeeper paths
    std::string **result = (std::string**)malloc(sizeof(std::string*));
    Stat stat;

    if(zk->exists(memory_segment_node, &stat) == ZNONODE) {
        int rc = zk->create(memory_segment_node, "", ZOO_OPEN_ACL_UNSAFE, 0, result);
        if(rc != ZOK || rc != ZNODEEXISTS) {
            LogError("Could not create %s in zookeeper", memory_segment_node.c_str());
        }
    }
    free(*result);

    if(zk->exists(process_node, &stat) == ZNONODE) {
        int rc = zk->create(process_node, "", ZOO_OPEN_ACL_UNSAFE, 0, result);
        if(rc != ZOK || rc != ZNODEEXISTS) {
            LogError("Could not create %s in zookeeper", process_node.c_str());
        }
    }
    free(*result);

    uintptr_t start_address = ALLOCATABLE_RANGE_START;
    std::string start_address_string = std::to_string(start_address);
    if(zk->exists(allocator_node, &stat) == ZNONODE) {
        int rc = zk->create(allocator_node, start_address_string, ZOO_OPEN_ACL_UNSAFE, 0, result);
        if(rc != ZOK || rc != ZNODEEXISTS) {
            LogError("Could not create %s in zookeeper", allocator_node.c_str());
        }
    }
    free(*result);

    //create a process for yourself
    std::string process_path = process_node + std::to_string(this->server_id);
    if(zk->exists(process_path, &stat) == ZNONODE) {
        int rc = zk->create(process_path, "", ZOO_OPEN_ACL_UNSAFE, 0, result);
        if(rc != ZOK) {
            LogError("Could not create %s in zookeeper", process_path.c_str());
        }
    } else {
        //overwrite the data
        LogWarning("node may not have been cleaned");
        int rc = zk->set(process_path, "", stat.version);
        if(rc != ZOK) {
            LogError("could not reset %s in zookeeper", process_path.c_str());
        }
    }
    free(*result);
    free(result);
    #endif
    // starting server
    server->start(cfg.getNode(this->server_id)->port);
}

inline
RDMAMemNode::~RDMAMemNode(){
    server->stop();
    this->clients.clear();
    this->connections.clear();
}

inline
int RDMAMemNode::connect_mesh(){
    /*
        - connect to all servers with id lesser than self
        - then wait until you get connections from all ids greater than yours
    */
    int id_to_connect = this->server_id - 1;
    LogInfo("Connecting to all servers with lower ids");
    while (id_to_connect >= 0) {
        if (id_to_connect == this->server_id) {
            id_to_connect--;
            continue;
        }
        RNode* node = this->cfg.getNode(id_to_connect);
        RDMAClient* client = new RDMAClient();
        uintptr_t connection = client->connect(node->ip.c_str(), std::to_string(node->port).c_str());
        if(connection == 0) {
            LogError("Could not connect to specified address");
            return -1;
        }
            
        clients.insert(std::make_pair(connection, client));
        connections.insert(std::make_pair(id_to_connect, connection));
        client->send(connection, &this->server_id, sizeof(this->server_id));
        id_to_connect--;
    }

    unsigned int total_servers = cfg.getNumServers() - 1;
    
    while(total_servers > connections.size()) {
        // Accept the connection.
        LogInfo("Waiting for connection");
        uintptr_t conn_id = server->accept();
        LogInfo("got connection");

        LogInfo("waiting for id");
        std::pair<void*, size_t> message = server->receive(conn_id);
        int id;
        memcpy(&id, message.first, message.second);
        LogInfo("got connection from server %d", id);


        //add to connections
        connections.insert(std::make_pair(id, conn_id));
    }

    return 0;
}

#if FAULT_TOLERANT

void* RDMAMemNode::getAllocationAddress(size_t size){
    LogInfo("getting an allocation address");
    failed_zk_write:
    Stat stat;
    std::string *result = nullptr;
    int rc = zk->get(allocator_node, &result, &stat);
    
    if(rc != ZOK) {
        LogError("could not read zookeeper node at allocation, MAYDAY");
    }
    LogInfo("rc was %s", this->zk->toString(rc).c_str());
    LogInfo("got address %s", (*result).c_str());

    size_t sz = 0;
    uintptr_t res = std::stoul(*result, &sz, 0);

    uintptr_t addr = res + size;

    rc = zk->set(allocator_node, std::to_string(addr), stat.version);
    if(rc != ZOK) {
        goto failed_zk_write;
    }

    return (void*)res;
}

int RDMAMemNode::createMemorySegmentNode(void* address, size_t size, int source, int destination, int64_t clientSpecifiedID) {
    std::map<std::string, std::string> segment_map;
    segment_map["address"] = std::to_string((uintptr_t)address);
    segment_map["size"] = std::to_string(size);
    segment_map["source"] = std::to_string(source);
    segment_map["destination"] = std::to_string(destination);
    segment_map["clientID"] = std::to_string(clientSpecifiedID);

    std::string segment_zk = MAPtoZS(segment_map);
    std::string **result = (std::string**)malloc(sizeof(std::string*));
    std::string path = memory_segment_node + std::to_string(clientSpecifiedID);
    int rc = zk->create(path, segment_zk, ZOO_OPEN_ACL_UNSAFE, 0, result);
    if(rc != ZOK) {
        LogWarning("Could not create memory segment meta data on zookeeper because %s", zk->toString(rc).c_str());
        return -1;
    }

    return 0;
}

int RDMAMemNode::updateSegmentDestination(int64_t app_id, int destination) {
    Stat stat;
    std::string **result = (std::string**)malloc(sizeof(std::string*));
    std::string path = memory_segment_node + std::to_string(app_id);
    int rc = zk->get(path, result, &stat);
    if(rc != ZOK) {
        LogError("zk reading problem for segment");
    }

    std::map<std::string, std::string> segment_map = ZStoMAP(*(*result));
    segment_map["destination"] = std::to_string(destination);

    std::string new_segment_str = MAPtoZS(segment_map);

    rc = zk->set(path, new_segment_str, stat.version);

    if(rc != ZOK) {
        LogWarning("could not update dest on ZK");
        return -1;
    }

    return 0;
}

int RDMAMemNode::cleanMemorySegment(int64_t app_id) {
    Stat stat;
    std::string **result = (std::string**)malloc(sizeof(std::string*));
    std::string path = memory_segment_node + std::to_string(app_id);
    int rc = zk->get(path, result, &stat);
    if(rc != ZOK) {
        LogError("zk reading problem for segment");
    }

    std::map<std::string, std::string> segment_map = ZStoMAP(*(*result));
    segment_map["source"] = std::to_string(this->server_id);
    segment_map["destination"] = "";

    std::string new_segment_str = MAPtoZS(segment_map);

    rc = zk->set(path, new_segment_str, stat.version);
    if(rc != ZOK) {
        LogWarning("could not update dest on ZK");
        return -1;
    }

    return 0;
}

int RDMAMemNode::deleteMemorySegment(int64_t app_id, int source) {
    Stat stat;
    std::string **result = (std::string**)malloc(sizeof(std::string*));
    std::string path = memory_segment_node + std::to_string(app_id);
    int rc = zk->get(path, result, &stat);
    if(rc != ZOK) {
        LogError("zk reading problem for segment");
    }

    auto map = ZStoMAP(*(*result));
    int64_t source_zk = atoi((map["source"]).c_str());
    if(source != source_zk) {
        LogWarning("rouge process tried to delete segment it did not recognise/understand");
        return -1;
    }

    rc = zk->remove(path, stat.version);
    if(rc != ZOK) {
        LogError("could not remove segment");
        return -1;
    }

    return 0;
}

int RDMAMemNode::addToProcessList(int64_t id) {
    LogInfo("adding id to process %ld", id);
    Stat stat;
    std::string **result = (std::string**)malloc(sizeof(std::string*));
    std::string path = process_node + std::to_string(this->server_id);
    int rc = zk->get(path, result, &stat);
    if(rc != ZOK) {
        LogError("zookeeper reading issue for process list %d", this->server_id);
        return -1;
    }

    rc = zk->set(path, (*(*result)) + std::to_string(id)+ ",", stat.version);
    
    if(rc != ZOK) {
        LogError("zookeeper set issue for process list %d", this->server_id);
        return -1;
    }

    return 0;
}

int RDMAMemNode::removeFromProcessList(int64_t id) {
    LogInfo("removing from process list");
    
    Stat stat;
    std::string **result = (std::string**)malloc(sizeof(std::string*));
    std::string path = process_node + std::to_string(this->server_id);
    int rc = zk->get(path, result, &stat);
    if(rc != ZOK) {
        LogError("zk reading problem for process list");
    }

    LogInfo("current list %s, removing %ld", (*(*result)).c_str(), id);

    std::vector<int64_t> vec = ZStoProcessList(*(*result));
    
    vec.erase(std::remove(vec.begin(), vec.end(), id), vec.end());
    
    std::string new_list = ProcessListtoZS(vec);
    LogInfo("new list is : %s", new_list.c_str());
    zk->set(path, new_list, stat.version);
    
    if(rc != ZOK) {
        LogError("could not update process list");
        return -1;
    }

    return 0;
}

std::vector<std::map<std::string, std::string>> RDMAMemNode::GetPartitionsForProcess(int64_t process_id) {
    LogInfo("getting partitions for process");
    Stat stat;
    std::string **result = (std::string**)malloc(sizeof(std::string*));
    std::string path = process_node + std::to_string(process_id);
    int rc = zk->get(path, result, &stat);
    if(rc != ZOK) {
        LogError("Could not get partition list for process id %ld", process_id);
    }

    std::vector<int64_t> list = ZStoProcessList(*(*result));
    std::vector<std::map<std::string, std::string>> segment_info;
    
    for(int64_t id : list) {
        path = memory_segment_node + std::to_string(id);
        rc = zk->get(path, result, &stat);
        if(rc != ZOK) {
            LogError("Could not get memory segment for id %ld", id);
        }

        auto map = ZStoMAP(*(*result));
        segment_info.push_back(map);
    }

    return segment_info;
}

std::vector<int64_t> RDMAMemNode::GetLivePartitionsForProcess(int64_t process_id) {
    uintptr_t conn_id = this->connections[process_id];
    ASSERT_ZERO(sem_init(&sem_get_partition_list, 0, 0));
    LogInfo("posting get partitions request");
    this->getServer(process_id, conn_id)->getPartitionList(conn_id);
    

    // And wait for the read to finish.
    sem_wait(&sem_get_partition_list);
    sem_destroy(&sem_get_partition_list);

    return ZStoProcessList(this->partition_list);
}

std::map<std::string, std::string> RDMAMemNode::getMemorySegment(int64_t app_id) {
    Stat stat;
    std::string path = memory_segment_node + std::to_string(app_id);
    std::string **result = (std::string**)malloc(sizeof(std::string*));

    int rc = zk->get(path, result, &stat);
    if(rc != ZOK) {
        LogError("Cannot read zookeeper for memory segment with user id %ld", app_id);
    }

    return ZStoMAP(*(*result));
}

#endif