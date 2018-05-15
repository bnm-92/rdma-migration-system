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
#include <math.h>
/*
    simple client server test between two nodes to ensure transfer mechanism is working
*/

int main(int argc, char** argv) {
    if(argc < 2) {
        std::cerr << "./memory_pinning server_id sizeofmemory" << std::endl; 
        return 1;
    }

    int server_id = atoi(argv[1]);

    if(server_id == 0) {
        RDMAServer *rdma_server = nullptr;
        rdma_server = new RDMAServer();
        rdma_server->start(5000);
        uintptr_t conn_id = rdma_server->accept();
        size_t data_size = atoi(argv[2]);
        
        printf("memory_size, registration time, deregistration time\n");
        while(data_size < 1073741824) {
            data_size = data_size*2;
            MultiTimer t = MultiTimer();
            MultiTimer t2 = MultiTimer();

            for (int i=0; i<10; i++) {
                void* data_addr = mmap(0, data_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,-1,0); 

                if(data_addr == MAP_FAILED)
                    throw std::runtime_error("Could not MMAP memory location");

                    // printf("MMAPed at %p\n", data_addr); 

                t.start();
                rdma_server->register_memory(conn_id, data_addr, data_size, false);
                t.stop();

                t2.start();
                rdma_server->deregister_memory(conn_id, data_addr);
                t2.stop();
                munmap(data_addr, data_size);
            }

            std::vector<double> vec = t.getTime();
            double sum = 0;
            double var = 0;
            double sd = 0;
            for(unsigned int i=0; i<vec.size(); i++) {
                sum += vec.at(i);
            }
            
            double mean = ((double)sum/(double)vec.size());
            for(unsigned int i=0; i<vec.size(); i++) {
                var += (vec.at(i) - mean) * (vec.at(i) - mean);
            }

            var /= vec.size();
            sd = sqrt(var);

            std::vector<double> vec2 = t2.getTime();
            double sum2 = 0;
            double var2 = 0;
            double sd2 = 0;
            for(unsigned int i=0; i<vec2.size(); i++) {
                sum2 += vec2.at(i);
            }
            
            double mean2 = ((double)sum2/(double)vec2.size());
            for(unsigned int i=0; i<vec2.size(); i++) {
                var2 += (vec2.at(i) - mean2) * (vec2.at(i) - mean2);
            }

            var2 /= vec2.size();
            sd2 = sqrt(var2);

            printf("%lu, %f, %f, %f, %f\n",data_size, mean, sd, mean2, sd2);
            

            fflush(stdout);
            
        }
        rdma_server->done(conn_id);
    } else {
        const char* addr = "10.30.0.12";
        const char* port = "5000";


        RDMAClient* client = new RDMAClient();
        uintptr_t conn_id = client->connect(addr, port);
        // We're done with the connection, so call done().
         client->done(conn_id);
        // And clean up the client.
        client->destroy();

        delete client;       
    }

    return 0;
}
