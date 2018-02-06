// test-transfer.cpp

/*
The server half of a basic demonstration/test of rdma-made-easy.
Give it a message, and it when clients connect, clients will RDMA-read the message.
*/

#include <unistd.h>
#include <cstring>

#include <iostream>
#include <utility>

#include "rdma-network/rdma_server.hpp"
#include "rdma-network/rdma_client.hpp"
#include "utils/miscutils.hpp"
#include <sys/mman.h>

#include <errno.h>

/*
    simple client server test between two nodes to ensure transfer mechanism is working
*/

int main(int argc, char** argv) {
    if(argc < 3) {
        std::cerr << "./test-transfer config.txt id " << std::endl; 
        return 1;
    }

    int server_id = std::stoi(argv[2]);
    ConfigParser cfp;
    std::unordered_map<int, RDMAClient*> peers;
    cfp.parse(argv[1]);
    
    RDMAServer *rdma_server = nullptr;
    RDMAClient* client = nullptr;

    if(server_id == 0) {
        //set up self
        rdma_server = new RDMAServer();
        rdma_server->start(cfp.getNode(server_id)->port);

    } else {
        int num_servers = cfp.getNumServers();
            std::cout << "trying to connect to " << num_servers << "\n";
        
        int connections = 0;
        int id_to_connect = 0;
    
        while(connections < num_servers - 1) {
            if(id_to_connect == server_id)
                id_to_connect++;
    
            //keep trying to make the mesh
            client = nullptr;
            auto search = peers.find(id_to_connect);
            if(search != peers.end()) {
                client = search->second;
                // delete client;
                peers.erase(id_to_connect);
            }
        
            client = new RDMAClient();
        
            assert(client != nullptr);
            //client object exists
            if (client->connect(cfp.getNode(id_to_connect)->ip.c_str(), 
                std::to_string(cfp.getNode(id_to_connect)->port).c_str())) {
                //connected
                peers.insert({id_to_connect, client});
                id_to_connect++;
                connections++;
            } else {
                std::cout << "could not connect, trying again\n";
            }
        }
    }

    std::cout << "starting experiment\n";
    std::string secret = "this is a secret";

    if(server_id == 0) {
        // what the server will do
        // Allocate some memory on the heap, and write the message into it.
        size_t data_size = secret.length() + 1;
        void* data_addr = mmap(0, data_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,-1,0); 
        if(data_addr == MAP_FAILED)
            throw std::runtime_error("Could not MMAP memory location");
        printf("MMAPed at %p\n", data_addr); 
        memcpy(data_addr, secret.c_str(), data_size);
        std::cerr << "Wrote message into local memory at " << data_addr;
        std::cerr << " that says: \"" << secret << "\"" << std::endl;

        // Accept the connection.
        std::cerr << "Waiting for incoming RDMA connection... ";
        uintptr_t conn_id = rdma_server->accept();
        std::cerr << "Connection accepted." << std::endl;

        // The client is going to RDMA-read the message we stored at `data_addr`.
        // Register the memory at `data_addr` with RDMA.
        std::cerr << "adding memory location: " << data_addr << std::endl;
        rdma_server->register_memory(conn_id, data_addr, data_size, false);

        std::cerr << "Preparing the client to take over mem address" << data_addr;
        
        rdma_server->send_prepare(conn_id, (void*)data_addr, data_size);
        std::cerr << "sent prepare" << std::endl;
        
        //wait or change data until you get an accept message
        //while queue is empty
        while(rdma_server->checkForMessage(conn_id)) {}

        std::pair<void*, size_t> recvd = rdma_server->receive(conn_id);
        struct rdma_message* msg = (rdma_message*)recvd.first;

        if(msg->message_type == rdma_message::MessageType::MSG_ACCEPT) {
            std::cerr << "got rdma accept message" << std::endl;
        } else {
            throw std::runtime_error("did not receive a message accept");
        }

        //now lets send informaiton to transfer after finishing our remaining work
        rdma_server->send_transfer(conn_id, (void*)data_addr, data_size);
        
        // The client will now do the RDMA-read, but on the server side we are
        // all done, so call done() on the connection.
        std::cerr << "Closing connection." << std::endl;
        rdma_server->done(conn_id);
    } else {

        if(client == NULL) {
            throw std::runtime_error("client was null fool\n");
        }
        bool run = true;
        while (run) {
            // doing some work

            if (!client->checkForMessage((uintptr_t)client->client_socket->context)) {
                std::pair<void*, size_t> recvd = client->receive((uintptr_t)client->client_socket->context);
                std::cerr << "got message\n";
                uintptr_t conn_id = (uintptr_t)client->client_socket->context;

                struct rdma_message* rdma_msg = (rdma_message*)recvd.first;
                
                if(rdma_msg->message_type == rdma_message::MessageType::MSG_PREPARE) {
                    void* addr = rdma_msg->region_info.addr;
                    size_t size = rdma_msg->region_info.length;
                    
                    //allocate memory
                    void* data_addr = mmap(addr, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,-1,0); 
                    if(data_addr == MAP_FAILED || data_addr != addr) {
                        client->send_decline(conn_id, addr, size);
                        throw std::runtime_error("Could not MMAP memory location");
                    }
                        
                    printf("MMAPed at %p\n", data_addr); 
                                
                    //register memory
                    client->register_memory(conn_id, addr, size);
                    //accept
                    client->send_accept(conn_id, addr, size);

                } else if(rdma_msg->message_type == rdma_message::MessageType::MSG_TRANSFER) {
                    // you know you have the addr to transfer ownership to yourself and make edits if you want
                    // // RDMA-read the memory on test-server at that address.
                    std::cerr << "completed. Read: \"" << (char*) rdma_msg->region_info.addr << "\"" << std::endl;
                    run = false;                    
                }
            }
        }
        
        // We're done with the connection, so call done().
        client->done((uintptr_t)client->client_socket->context);
        // And clean up the client.
        client->destroy();

    }

    return 0;
}