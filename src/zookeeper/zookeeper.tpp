#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <zookeeper/zookeeper.hpp>

ZooKeeper::ZooKeeper(const std::string& servers,
const int64_t& sessionTimeout, 
FILE * file) : 
    connected(0),
    zh(nullptr),  
    sessionTimeout(2000) {
    
    LogAssert(servers.compare("") != 0, "input list to zookeeper was empty");
    LogAssert(sessionTimeout > 0, "session timeout to zookeer was <= 0");

    this->list_zservers = servers;
    this->sessionTimeout = sessionTimeout;

    if(file != nullptr) {
        //default is stderr
        zoo_set_log_stream(file);
    }
    zoo_set_debug_level(ZOO_LOG_LEVEL_WARN);
    
    zh = zookeeper_init(list_zservers.c_str(), synch_connection_CB, this->sessionTimeout, &z_cid, (void*)this, 0);
    if(!zh) {
        LogError("couldn't create zookeeper handle");
    }

    //wait for zookeeper connection
    while(!this->connected.load());        
}

ZooKeeper::~ZooKeeper() {}     

void ZooKeeper::Close() {
    zookeeper_close(zh);
    zh = nullptr;
}
