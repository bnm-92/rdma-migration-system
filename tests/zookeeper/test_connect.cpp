#include <iostream>
#include <map>
#include <zookeeper/zookeeper.hpp>


std::map<std::string, std::string> ZStoMAP(std::string input) {
    int len = input.size();
    int start = 0;
    int end = 0;
    std::map<std::string, std::string> res;
    int i = 0;
    while(i < len) {
        //find a key
        std::string key;
        start = i;
        while(input.at(i) != ',') {
            i++;
        }
        end = i;

        key = input.substr(start, end - start);

        //find the value
        i++;
        std::string val;
        start = i;
        while(input.at(i) != ',') {
            i++;
        }
        end = i;

        val = input.substr(start, end - start);

        //add to map
        res[key] = val;
    }
    return res;
}

std::string MAPtoZS(std::map<std::string, std::string> map) {
    std::string res;
    for(auto it = map.begin(); it != map.end(); it++) {
        res.append(it.first.c_str());
        res.append(",");
        res.append(it.second.c_str());
        res.append(",");
    }
    return res;
}

int main(int argc, char** argv) {
    ZooKeeper zk("127.0.0.1:2181", 1000, nullptr);
    printf("established connection\n");
    // check timeout
    int timeout = zk.getSessionTimeout();
    printf("negotiated timeout with zookeeper was: %d\n", timeout);
    
    //check if node exists
    Stat stat;
    std::string node = "/test_node";
    int rc = zk.exists(node, &stat);
    printf("return code was: %d\n", rc);
    if(rc == ZOK) {
        printf("\"/test node\" exists\n");
        int rc_remove = zk.remove(node, stat.version);
        printf("return code for remove on existence was: %s\n", zk.toString(rc_remove).c_str());
    } else if(rc == ZNONODE) {
        printf("\"test node\" does not exists\n");
    }

    std::string *result;
    
    rc = zk.create(node, "node1:val1", ZOO_OPEN_ACL_UNSAFE, 0, result);

    if(rc == ZOK) {
        printf("created node %s\n", node.c_str());
    } else {
        printf("error in node creation %s\n", zk.toString(rc));
    }

    // typedef void (*watcher_fn)(zhandle_t *zh, int type, int state, const char *path,void *watcherCtx);
    
    void (*fn) (zhandle_t *zzh, int type, int state, const char *path, void* context) = ([](zhandle_t *zzh, int type, int state, const char *path, void* context) -> void {
        std::string* str = (std::string*)context;
        printf("call back on wexists %s\n", str->c_str());
    });

    rc = zk.wexists(node, &stat, fn, (void*)(&node));
    printf("return code was: %s\n", zk.toString(rc).c_str());
    fflush(stdout);

    rc = zk.set(node,"new data", stat.version);
    printf("return code was: %s\n", zk.toString(rc).c_str());
    fflush(stdout);

    // while(true){}
    zk.Close();
    return 0;
}