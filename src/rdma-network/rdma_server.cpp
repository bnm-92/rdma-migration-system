// rdma_server.tpp

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

#include "rdma-network/util.hpp"
#include "rdma-network/rdma_server.hpp"



RDMAServer::RDMAServer() : RDMAServerPrototype() {
    // Create the connection queue semaphore.
    ASSERT_ZERO(sem_init(&conn_queue_sem, 0, 0));
}


RDMAServer::~RDMAServer() {
    // Note: This is never called right now because RDMAServer
    // stopping has not been implemented.
    // Also, ths is just for mirroring constructor.
    // Resources acquired in start or the event loop
    // should be released either at the end of the event loop, or stop().
    sem_destroy(&conn_queue_sem);
}


void RDMAServer::start(int port) {
    // Make sure the server hasn't already been started,
    // and set the started flag.
    // (Obviously this should be mutexed for full concurrency protection
    // but we're just doing this to catch programming errors.)
    if (started) {
        throw std::logic_error("run() called a second time on an RDMAServer.");
    }
    started = true;

    // Create the event channel.
    ASSERT_NONZERO(event_channel = rdma_create_event_channel());

    // Create the RDMA socket, and register the event channel with it.
    void* context = NULL;
    // Flag for reliable communication.
    rdma_port_space port_space = RDMA_PS_TCP;
    ASSERT_ZERO(rdma_create_id(
        event_channel, &listener_socket, context, port_space));

    // Bind the RDMA socket to a socket address.
    // This also binds the socket to an actual RDMA device.
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    ASSERT_ZERO(rdma_bind_addr(listener_socket, (struct sockaddr*)&addr));

    // Activate the RDMA socket for listening.
    int backlog = 10; // Arbitrary.
    ASSERT_ZERO(rdma_listen(listener_socket, backlog));
    LogInfo("RDMAServer created. Listening with socket %p on port %d", listener_socket, ntohs(rdma_get_src_port(listener_socket)));

    // Spin up the event loop.
    event_thread = std::thread(&RDMAServer::event_loop, this);

    return;
}


void RDMAServer::stop() {
    // Implementation: Set the stop flag, and try to disconnect every socket.
    // The event loop will exit when the last socket has been disconnected
    // and the stop flag has been set.

    // TODO: implement the above l o l

    // and then clean up
    rdma_destroy_id(listener_socket);
    // if(resources)
    //     delete resources;
    return;
}


uintptr_t RDMAServer::accept() {
    // Signal the semaphore that someone's ready to accept a connection.
    sem_post(&conn_queue_sem);
    // And grab a connection from the queue.
    struct rdma_cm_id* rdma_socket = conn_queue.dequeue();

    return (uintptr_t) rdma_socket->context;
}


int RDMAServer::get_port() {
    return ntohs(rdma_get_src_port(listener_socket));
}


int RDMAServer::on_connect_request(struct rdma_cm_id* rdma_socket) {
    LogInfo("Received connection request");

    // If we haven't built the device resources for this RDMA server,
    // build them now.
    // See comments in header for more details.
    if (resources == NULL) {
        build_resources_for_device(rdma_socket->verbs);
    } else {
        // Just to be safe, we actually want to assert that we're using
        // the same device.
        if (resources->device_context != rdma_socket->verbs) {
            throw std::runtime_error(
                "on_connect_request received a different device context "
                "than the one we already registered with this RDMAServer!");
        }
    }

    // Build the queue pair for this new socket.
    build_queue_pair(rdma_socket);

    // Create the context for this connection.
    struct rdma_connection* conn = build_connection_object(rdma_socket);
    // We'll just store it in the context pointer for the rdma_socket
    // for now; I'm not sure if it's necessary to track all connection
    // contexts in device resources.
    rdma_socket->context = conn;

    // Great. Now let's arm the RDMA receive functionality on this socket.
    post_rdma_receive(conn);

    // And finally, accept the connection.
    struct rdma_conn_param conn_param;
    build_conn_param(&conn_param);
    ASSERT_ZERO(rdma_accept(rdma_socket, &conn_param));

    return 0;
}


int RDMAServer::on_connection(struct rdma_cm_id* rdma_socket) {
    LogInfo("Connection established for rdma socket %p", rdma_socket);

    // First, block until a user is available to accept this connection.
    sem_wait(&conn_queue_sem);
    // Then pass this connection to an accepter.
    conn_queue.enqueue(rdma_socket);
    return 0;
}
