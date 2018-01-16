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

int ZooKeeper::getState() {
    if(zh != nullptr)
        return zoo_state(zh);
    return 0;
}

int64_t ZooKeeper::getSessionId() {
    return zoo_client_id(zh)->client_id;
}

int64_t ZooKeeper::getSessionTimeout() const {
    // ZooKeeper server uses int representation of milliseconds for
    // session timeouts.
    // See:
    // http://zookeeper.apache.org/doc/trunk/zookeeperProgrammers.html
    return zoo_recv_timeout(zh);
}

int ZooKeeper::create(
    const std::string& path,
    const std::string& data,
    const ACL_vector& acl,
    int flags,
    std::string* result) {
    /*
        add a buffer to support sequential nodes
    */
    return zoo_create(this->zh, path.c_str(), data.data(),
        static_cast<int>(data.size()),
        &acl,
        flags,
        NULL, 
        0);
}

int ZooKeeper::remove(const std::string& path, int version) {
    return zoo_delete(this->zh, path.c_str(), version);
}

int ZooKeeper::exists(const std::string& path, bool watch, Stat* stat) {
    return zoo_exists(this->zh, path.c_str(), watch, stat);
}

int ZooKeeper::get(
    const std::string& path,
    bool watch,
    std::string* result,
    Stat* stat) {
        int buflen = BUFLEN;
        result = new std::string(buflen, '\0');
        int rc = zoo_get(this->zh, path.c_str(), watch, &(*result)[0], &buflen, stat);
        if(buflen != -1)
            result->resize(strlen(result->c_str()));
        return rc;
}

int ZooKeeper::getChildren(
    const std::string& path,
    bool watch,
    std::vector<std::string>* results) {
    String_vector sv;
    int rc = zoo_get_children(this->zh, path.c_str(), watch, &sv);
    int entries = sv.count;
    for (int i=0; i<entries; i++) {
        std::string s((*sv.data) + i);
        results->push_back(s);
    }
    return rc;
}

int ZooKeeper::set(const std::string& path, const std::string& data, int version) {
    return zoo_set(this->zh, path.c_str(), 
                    data.data(),
                    static_cast<int>(data.size()),  
                    version);
}

std::string ZooKeeper::message(int code) const {
      return std::string(zerror(code));
}