#ifndef __RDMA_SERVER_HPP__
#define __RDMA_SERVER_HPP__

#include <cstdio>
#include <cstdlib>
#include <semaphore.h>

#include <unordered_set>

#include "rdma_server_prototype.hpp"
#include "util.hpp"

class RDMAServer : public RDMAServerPrototype {
/*
A RDMA server class for handling incoming RDMA connections.
Encapsulates a server RDMA socket, all sockets it spawns,
and all resources associated with these sockets.
*/

public:
    RDMAServer();
    ~RDMAServer();

    // Copy construction and copy assignment disabled for now.
    RDMAServer(const RDMAServer&) = delete;
    RDMAServer& operator=(const RDMAServer&) = delete;

    // Starts the server, listening on the port provided.
    void start(int port);

    // Gracefully stop the server.
    void stop();

    // Accept a connection on this server.
    // Returns an identifier for this connection that you pass when
    // asking this RDMAServer to do work on this connection in the future
    // (see methods taking a conn_id in the parent class).
    uintptr_t accept();

    // Get the port for this server.
    int get_port();

protected:
    // Boilerplate for handling event and delegating to approprate method.
    int on_event(struct rdma_cm_event*);

    // This is called on all work completions in this RDMAServer.
    void on_completion(struct ibv_wc*);
private:
    // Handler for receipt of connection request.
    // NOTE: The rdma socket passed into here is NOT the same as
    // listener_socket. Much like TCP/UDP sockets, the listener will create a
    // new socket to service each connection request; this is the socket that
    // is passed in here.
    int on_connect_request(struct rdma_cm_id*);
    // Handler for establishment of connection.
    int on_connection(struct rdma_cm_id*);

private:
    // This is basically an RDMA socket. For more details, see:
    // man 3 rdma_create_id, which makes these things
    // Definition for rdma_cm_id should be in /usr/include/rdma/rdma_cma.h
    struct rdma_cm_id* listener_socket;

    // All sockets spawned by the listener.
    std::unordered_set<struct rdma_cm_id*> child_sockets;

    // The queue of connections pending acceptance by the user.
    ThreadsafeQueue<struct rdma_cm_id*> conn_queue;
    // A semaphore for this queue, since we currently want to block the server
    // when a connection is made until someone comes around to accept it.
    sem_t conn_queue_sem;
};

#include "rdma_server.tpp"

#endif // __RDMA_SERVER_HPP__
