// test-client.cpp

/*
The client half of a basic demonstration/test of rdma-made-easy.
Connects to a test-server and RDMA reads a message from their memory.
*/

#include <iostream>
#include <string>

#include "utils/miscutils.hpp"

#include "rdma-network/rdma_client.hpp"

int main (int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <IP address of test-server>";
        std::cerr << std::endl;
        return 1;
    }
    std::cerr << std::endl << ASCII_STARS << std::endl;

    const char* addr = argv[1];
    const char* port = "29435";

    // For this test, we're going to RDMA read a region of memory
    // from the test-server.

    // Spin up our RDMA client, and connect to the server.
    RDMAClient* client = new RDMAClient();
    uintptr_t conn_id = client->connect(addr, port);

    // Receive a message from the test-server.
    // This will be the address we are supposed to read.
    std::cerr << "Performing RDMA-receive from test-server... ";
    std::pair<void*, size_t> recvd = client->receive(conn_id);
    void* remote_addr = *(void**)recvd.first;
    std::cerr << "completed. Received: " << remote_addr << std::endl;
    std::cerr << "This will be the address we will read on test-server.";
    std::cerr << std::endl << std::endl;
    // Receive another message. This will be the size of the data to read.
    std::cerr << "Performing RDMA-receive from test-server... ";
    recvd = client->receive(conn_id);
    size_t remote_msg_size = *(size_t*)recvd.first;
    std::cerr << "completed. Received: " << remote_msg_size << std::endl;
    std::cerr << "This will be the size of the message to read from test-server.";
    std::cerr << std::endl << std::endl;

    // Make a buffer to store the RDMA-read data in.
    void* read_buffer = malloc(remote_msg_size);

    // Register the read buffer with RDMA, so we can put our read result in it.
    client->register_memory(conn_id, read_buffer, remote_msg_size, false);

    // RDMA-read the memory on test-server at that address.
    std::cerr << "RDMA-reading address " << remote_addr << " on test-server... ";
    client->rdma_read(conn_id, read_buffer, remote_addr, remote_msg_size);
    std::cerr << "completed. Read: \"" << (char*) read_buffer << "\"" << std::endl;

    // We're done with the connection, so call done().
    client->done(conn_id);
    // And clean up the client.
    client->destroy();

    delete client;
    return 0;
}
