// test-server.cpp

/*
The server half of a basic demonstration/test of rdma-made-easy.
Give it a message, and it when clients connect, clients will RDMA-read the message.
*/

#include <unistd.h>
#include <cstring>

#include <iostream>
#include <utility>

#include "miscutils.hpp"

#include "rdma_server.hpp"

int main (int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " \"message for clients to read>\"" << std::endl;
        return 1;
    }
    std::cerr << std::endl << ASCII_STARS << std::endl;

    std::string secret = argv[1];

    // Spin up our RDMA server.
    RDMAServer* rdma_server = new RDMAServer();
    rdma_server->start(29435);

    // Allocate some memory on the heap, and write the message into it.
    size_t data_size = secret.length() + 1;
    void* data_addr = malloc(data_size);
    memcpy(data_addr, secret.c_str(), data_size);
    std::cerr << "Wrote message into local memory at " << data_addr;
    std::cerr << " that says: \"" << secret << "\"" << std::endl;

    // Continuously accept RDMA connections and do the demonstration.
    while (true) {
        // Accept the connection.
        std::cerr << "Waiting for incoming RDMA connection... ";
        uintptr_t conn_id = rdma_server->accept();
        std::cerr << "Connection accepted." << std::endl;

        // The client is going to RDMA-read the message we stored at `data_addr`.
        // Register the memory at `data_addr` with RDMA.
        rdma_server->register_memory(conn_id, data_addr, data_size, true);

        // Send over the address of the data_addr.
        // (In case this is confusing: we are sending over the pointer
        // `data_addr`, and we do this by calling send() with the address and
        // size of the data_addr variable.)
        std::cerr << "Sending address of message (" << data_addr;
        std::cerr << ") to the client... ";
        rdma_server->send(conn_id, (void*)&data_addr, sizeof(void*));
        std::cerr << "Done." << std::endl;
        // Send over the size we've allocated too.
        std::cerr << "Sending size of message (" << data_size;
        std::cerr << ") to the client... ";
        rdma_server->send(conn_id, (void*)&data_size, sizeof(size_t));
        std::cerr << "Done." << std::endl;

        // The client will now do the RDMA-read, but on the server side we are
        // all done, so call done() on the connection.
        std::cerr << "Closing connection." << std::endl;
        rdma_server->done(conn_id);
    }

    // delete rdma_server;
    return 0;
}
