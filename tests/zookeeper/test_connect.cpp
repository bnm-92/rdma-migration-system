#include <iostream>
#include <zookeeper/zookeeper.hpp>

int main(int argc, char** argv) {
    ZooKeeper zk("127.0.0.1:5000", 2000, nullptr);
    // printf("established connection\n"); 
    // zk.Close();
    return 0;
}