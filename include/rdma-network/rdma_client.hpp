#ifndef __RDMA_CLIENT_HPP__
#define __RDMA_CLIENT_HPP__

/*
 * An RDMAClient is very similar to an RDMAServer.
 * However, it is different in two important ways:
 * 1. It only encapsulates a single socket.
 * 2. The client is only alive for the lifetime of the connection;
 *    it does not persist like the server.
 *
 * These two changes actually require a vastly different implementation and
 * interface from the RDMAServer, so the client is its own separate class.
 */

#include <semaphore.h>
#include <thread>

#include "rdma_server_prototype.hpp"

// A class for making outgoing RDMA connections. Encapsulates a client RDMA
// socket and all resources associated with it.
class RDMAClient : public RDMAServerPrototype {
public:
    RDMAClient();
    ~RDMAClient();

    // Disable copy construction and copy assignment.
    RDMAClient(const RDMAClient&) = delete;
    RDMAClient& operator=(const RDMAClient&) = delete;

    // Spin up the client and connect to a server.
    // Clients can only ever connect to one server.
    // Returns the id of this connection.
    uintptr_t connect(const char* addr, const char* port);

protected:
    // Boilerplate for handling event and delegating to approprate method.
    int on_event(struct rdma_cm_event*);

    // This is called on all work completions in this RDMAClient.
    void on_completion(struct ibv_wc*);
    void event_loop();

    // The semaphore for connect(), moving this from derived class to base for easier error management
    // See implementation for details.
    sem_t connect_semaphore;
    bool error;
private:
    // Handlers for each RDMA event we can receive.
    // Returns 0 for success.
    int on_addr_resolved(struct rdma_cm_id*);
    int on_route_resolved(struct rdma_cm_id*);
    int on_connection(struct rdma_cm_id*);

public:
    // The client socket.
    struct rdma_cm_id* client_socket;

    // The single connection for this socket,
    // available only after connect() returns.
    struct rdma_connection* connection;

    // Timeout to use for RDMA connection establishment operations.
    static const int TIMEOUT_MS = 500;
};

#include "rdma_client.tpp"

#endif // __RDMA_CLIENT_HPP__
