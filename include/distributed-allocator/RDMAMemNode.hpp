#ifndef RDMAMEMNODE
#define RDMAMEMNODE

#include "rdma_server.hpp"
#include "rdma_client.hpp"
#include "miscutils.hpp"

#include <cstdint>
#include <stdio.h>
#include <stdlib.h>

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

};

#include "RDMAMemNode.tpp"

#endif //RDMAMEMNODE