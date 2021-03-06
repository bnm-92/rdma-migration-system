#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <zookeeper/zookeeper.hpp>

inline
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
    // zoo_set_debug_level(ZOO_LOG_LEVEL_DEBUG);
    
    zh = zookeeper_init(list_zservers.c_str(), global_watch_CB, this->sessionTimeout, 0, (void*)this, 0);
    if(!zh) {
        LogError("couldn't create zookeeper handle");
    }

    //wait for zookeeper connection
    while(!this->connected.load());        
}

inline
ZooKeeper::~ZooKeeper() {}     

inline
void ZooKeeper::Close() {
    zookeeper_close(zh);
    zh = nullptr;
}

inline
void ZooKeeper::SetDebugLevel(ZooLogLevel logLevel) {
    zoo_set_debug_level(logLevel);
}

inline
int ZooKeeper::getState() {
    if(zh != nullptr)
        return zoo_state(zh);
    return 0;
}

inline
int64_t ZooKeeper::getSessionId() {
    return zoo_client_id(zh)->client_id;
}

inline
int64_t ZooKeeper::getSessionTimeout() const {
    // ZooKeeper server uses int representation of milliseconds for
    // session timeouts.
    // See:
    // http://zookeeper.apache.org/doc/trunk/zookeeperProgrammers.html
    return zoo_recv_timeout(zh);
}

inline
int ZooKeeper::create(
    const std::string& path,
    const std::string& data,
    const ACL_vector& acl,
    int flags,
    std::string* result) {

    int buflen = BUFLEN;
    (*result) = std::string(buflen, '\0');
    
    int rc =  zoo_create(this->zh, path.c_str(), data.data(),
        static_cast<int>(data.size()),
        &acl,
        flags,
        &((*result))[0], 
        buflen);

    if(buflen != -1)
        (result)->resize(strlen((result)->c_str()));
    
    return rc;
}

inline
int ZooKeeper::remove(const std::string& path, int version) {
    return zoo_delete(this->zh, path.c_str(), version);
}

inline
int ZooKeeper::exists(const std::string& path, Stat* stat) {
    return zoo_exists(this->zh, path.c_str(), false, stat);
}

inline
int ZooKeeper::wexists(const std::string& path, Stat* stat, watcher_fn watcher, void* watcherCtx){
    return zoo_wexists(this->zh, path.c_str(), watcher, watcherCtx, stat);
}

inline
int ZooKeeper::get(
    const std::string& path,
    std::string* result,
    Stat* stat) {
        int buflen = BUFLEN;
        *result = std::string(buflen, '\0');
        int rc = zoo_get(this->zh, path.c_str(), false, &(*(result))[0], &buflen, stat);
        
        if(buflen != -1)
            (result)->resize(strlen((result)->c_str()));
        return rc;
}

inline
int ZooKeeper::wget(const std::string& path, std::string* result, Stat* stat, watcher_fn watcher, void* watcherCtx) {
    int buflen = BUFLEN;
    *result = std::string(buflen, '\0');
    int rc = zoo_wget(this->zh, path.c_str(), watcher, watcherCtx, &(*(result))[0], &buflen, stat);
    if(buflen != -1)
        (result)->resize(strlen((result)->c_str()));
    return rc;
}

inline
int ZooKeeper::getChildren(
    const std::string& path,
    std::vector<std::string>* results) {
    String_vector sv;
    int rc = zoo_get_children(this->zh, path.c_str(), false, &sv);
    int entries = sv.count;
    char** res = sv.data;
    for (int i=0; i<entries; i++) {
        std::string s((*res));
        results->push_back(s);
        res++;
    }
    return rc;
}

inline
int ZooKeeper::wgetChildren(const std::string& path, std::vector<std::string>* results, watcher_fn watcher, void* watcherCtx) {
    String_vector sv;
    int rc = zoo_wget_children(this->zh, path.c_str(), watcher, watcherCtx, &sv);
    int entries = sv.count;
    LogInfo("number of entries %d", entries);
    char** res = sv.data;
    for (int i=0; i<entries; i++) {
        std::string s((*res));
        results->push_back(s);
        res++;
    }
    return rc;
}

inline
int ZooKeeper::set(const std::string& path, const std::string& data, int version) {
    return zoo_set(this->zh, path.c_str(), 
                    data.data(),
                    static_cast<int>(data.size()),  
                    version);
}

inline
std::string ZooKeeper::message(int code) const {
      return std::string(zerror(code));
}

inline
bool ZooKeeper::retryable(int code) {
  switch (code) {
    case ZCONNECTIONLOSS:
    case ZOPERATIONTIMEOUT:
    case ZSESSIONEXPIRED:
    case ZSESSIONMOVED:
      return true;

    case ZOK: // No need to retry!

    case ZSYSTEMERROR: // Should not be encountered, here for completeness.
    case ZRUNTIMEINCONSISTENCY:
    case ZDATAINCONSISTENCY:
    case ZMARSHALLINGERROR:
    case ZUNIMPLEMENTED:
    case ZBADARGUMENTS:
    case ZINVALIDSTATE:

    case ZAPIERROR: // Should not be encountered, here for completeness.
    case ZNONODE:
    case ZNOAUTH:
    case ZBADVERSION:
    case ZNOCHILDRENFOREPHEMERALS:
    case ZNODEEXISTS:
    case ZNOTEMPTY:
    case ZINVALIDCALLBACK:
    case ZINVALIDACL:
    case ZAUTHFAILED:
    case ZCLOSING:
    case ZNOTHING: // Is this used? It's not exposed in the Java API.
      return false;

    default:
      LogError("Unknown ZooKeeper code:%d", code);
      return false;
    //   UNREACHABLE(); // Make compiler happy.
  }
}

inline
std::string ZooKeeper::toString(int code) {
  switch (code) {
    case ZCONNECTIONLOSS:
        return "ZCONNECTIONLOSS";
    case ZOPERATIONTIMEOUT:
        return "ZOPERATIONTIMEOUT";
    case ZSESSIONEXPIRED:
        return "ZSESSIONEXPIRED";
    case ZSESSIONMOVED:
        return "ZSESSIONMOVED";
    case ZOK:
        return "ZOK";
    case ZSYSTEMERROR: // Should not be encountered, here for completeness.
        return "ZSYSTEMERROR";
    case ZRUNTIMEINCONSISTENCY:
        return "ZRUNTIMEINCONSISTENCY";
    case ZDATAINCONSISTENCY:
        return "ZDATAINCONSISTENCY";
    case ZMARSHALLINGERROR:
        return "ZMARSHALLINGERROR";
    case ZUNIMPLEMENTED:
        return "ZUNIMPLEMENTED";
    case ZBADARGUMENTS:
        return "ZBADARGUMENTS";
    case ZINVALIDSTATE:
        return "ZINVALIDSTATE";
    case ZAPIERROR:
        return "ZAPIERROR";
    case ZNONODE:
        return "ZNONODE";
    case ZNOAUTH:
        return "ZNOAUTH";
    case ZBADVERSION:
        return "ZBADVERSION";
    case ZNOCHILDRENFOREPHEMERALS:
        return "ZNOCHILDRENFOREPHEMERALS";
    case ZNODEEXISTS:
        return "ZNODEEXISTS";
    case ZNOTEMPTY:
        return "ZNOTEMPTY";    
    case ZINVALIDCALLBACK:
        return "ZINVALIDCALLBACK";    
    case ZINVALIDACL:
        return "ZINVALIDACL";    
    case ZAUTHFAILED:
        return "ZAUTHFAILED";    
    case ZCLOSING:
        return "ZCLOSING";        
    case ZNOTHING:
        return "ZNOTHING";
    
    default:
      return "Unknown Error";
    
  } 
}
