#include <iostream>
#include <map>
#include <vector>

#include <zookeeper/zookeeper.hpp>


int test_serialization_deserialization() {
    printf("starting serialization/deserialization tests\n");
    std::map<std::string, std::string> map;
    map["hello"] = "world";
    int64_t id = 1024;
    map["number"] = std::to_string(id);

    void* addr = malloc(1024);
    uintptr_t addr_ = (uintptr_t)addr;
    std::string x = std::to_string(addr_);
    // size_t sz = 0;
    // uintptr_t addr_2 = std::stoul(x, &sz, 0);
    // void* addr2 = (void*)addr_2;

    map["address"] = std::to_string(addr_);

    std::string output = MAPtoZS(map);
    std::cout << output << std::endl;
    std::map<std::string, std::string> output_map = ZStoMAP(output);

    if(output_map["hello"] != "world") {
        printf("string test failed\n");
        return 1;
    } else {
        printf("string test passed\n");
    }

    if(atoi(output_map["number"].c_str()) != id) {
        printf("number test failed\n");
        return 1;
    } else {
        printf("number test passed\n");
    }

    size_t sz = 0;
    std::string input = output_map["address"];
    uintptr_t res = std::stoul(input, &sz, 0);

    if( res != addr_) {
        printf("address test failed\n");
        return 1;
    } else {
        printf("address test passed\n");
    }
    printf("serialization/deserialization tests passed \n");
    return 0;
}


int test_serialization_deserialization2() {
    printf("starting serialization/deserialization tests 2\n");
    std::vector<int64_t> process_list;

    process_list.push_back(1);
    process_list.push_back(2);
    process_list.push_back(3);
    process_list.push_back(4);

    std::string output = ProcessListtoZS(process_list);
    std::cout << output << std::endl;

    std::vector<int64_t> output_list = ZStoProcessList(output);
    for (int x : output_list) {
        std::cout << x;
    }
    std::cout << std::endl;

    return 0;
}

int test_zookeeper() {
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
        printf("error in node creation %s\n", zk.toString(rc).c_str());
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

int main(int argc, char** argv) {
 test_serialization_deserialization2();
 return 0;
}
