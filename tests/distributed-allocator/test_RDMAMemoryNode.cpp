#include "distributed-allocator/RDMAMemNode.hpp"

/*
    tests to check for n connection mesh
    - please note, reconnect is not working without bugs therefore
    the processes have to be started in order
*/

int main(int argc, char** argv) {
    if(argc < 3) {
        std::cerr << "improper usage, please see README " << std::endl; 
        return 1;
    }

    int server_id = std::stoi(argv[2]);
    RDMAMemNode node(argv[1], server_id);
    LogInfo("connecting mesh");
    node.connect_mesh();
    while(1){}
    return 0;
}