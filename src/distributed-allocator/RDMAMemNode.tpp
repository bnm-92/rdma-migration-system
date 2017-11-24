
inline
RDMAMemNode::RDMAMemNode(std::string config_path, int server_id): 
cfg(), server(nullptr) {
    //initialize vars
    this->server_id = server_id;
    server = new RDMAServer();
    
    //parse config
    cfg.parse(config_path);
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
