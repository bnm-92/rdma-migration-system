// rdma_client.tpp

#include <netdb.h>

#include <cstring>
#include <iostream>

#include "rdma-network/util.hpp"
#include "rdma-network/rdma_server_prototype.hpp"
#include "rdma-network/rdma_client.hpp"


RDMAClient::RDMAClient()
: RDMAServerPrototype(), connection(NULL) {
    stop_after_last_conn = true;
}


RDMAClient::~RDMAClient() {}


uintptr_t RDMAClient::connect(
    const char* addr, const char* port
) {
    struct sockaddr* src_addr = NULL;
    void* context = NULL;

    // Flag for reliable communication.
    rdma_port_space port_space = RDMA_PS_TCP;

    // Convert the address and port into an addrinfo struct.
    struct addrinfo* addr_info;
    const struct addrinfo* hints = NULL;

    started = true;

    // Create the event channel;
    ASSERT_NONZERO(event_channel = rdma_create_event_channel());

    // Create the RDMA socket, and register the event channel with it.
    ASSERT_ZERO(rdma_create_id(event_channel, &client_socket, context, port_space));
    ASSERT_ZERO(getaddrinfo(addr, port, hints, &addr_info));

    // Resolve the address, which also binds the socket to an actual device.
    // (This will put an event on the event channel when it's done.)
    ASSERT_ZERO(rdma_resolve_addr(client_socket, src_addr, addr_info->ai_addr, TIMEOUT_MS));

    freeaddrinfo(addr_info);

    // Create the semaphore:
    // We'd like to block until the connection is established before returning.
    // However, the connection is established by the event loop thread,
    // not by us. Thus we'll use a semaphore.
    ASSERT_ZERO(sem_init(&connect_semaphore, 0, 0));

    // Spin up the event loop.
    event_thread = std::thread(&RDMAClient::event_loop, this);
    LogInfo("waiting on semaphore");

    sem_wait(&connect_semaphore);

    sem_destroy(&connect_semaphore);

    // We should now have an rdma_connection object for this connection.
    // Return it as the connection ID.
    // Destroy the semaphore.
    // connection_done:
    uintptr_t conn_id = (uintptr_t) client_socket->context;

    return conn_id;
}


int RDMAClient::on_addr_resolved(struct rdma_cm_id* rdma_socket) {
    LogInfo("RDMAClient: Address resolved.");

    if(!error) {
        // Build device resources.
        // (Assert that they have not been built yet.)
        if (resources != NULL) {
            throw std::logic_error(
                "Resources already built upon call to on_addr_resolved!");
        }
        build_resources_for_device(rdma_socket->verbs);
        build_queue_pair(rdma_socket);

        // Create an object for this connection and its resources.
        // (Assert we haven't built it yet.)
        if (connection != NULL) {
            throw std::logic_error(
                "Connection object already built upon on_addr_resolved!");
        }
        struct rdma_connection* conn = build_connection_object(rdma_socket);
        // Store it in the socket context, and the RDMA client object too.
        rdma_socket->context = conn;
        connection = conn;

        // Arm the receive functionality.
        post_rdma_receive(conn);

        // continue on to the next step in the connection:
        // Resolve the route.

    }
    ASSERT_ZERO(rdma_resolve_route(rdma_socket, TIMEOUT_MS));

    return 0;
}


int RDMAClient::on_route_resolved(struct rdma_cm_id* rdma_socket) {
    LogInfo("Route Resolved");
    // Establish the connection.
    struct rdma_conn_param conn_param;
    build_conn_param(&conn_param);
    ASSERT_ZERO(rdma_connect(rdma_socket, &conn_param));

    return 0;
}


int RDMAClient::on_connection(struct rdma_cm_id* rdma_socket) {
    LogInfo("Connection established for rdma socket %p ", rdma_socket);
    error = false;
    // Smash dat semaphore. Let em know this connection is gucci.
    sem_post(&connect_semaphore);

    return 0;
}


void RDMAClient::on_completion(struct ibv_wc* work_completion) {
    if (work_completion->status != IBV_WC_SUCCESS) {
        std::cout << "Case IBV_WC_SUCCESS error, it should be handled" << std::endl;
        return;
    }

    if (work_completion->opcode & IBV_WC_RECV) {
        std::cerr << "Received message: "
                  << (char*) ((struct work_context*) work_completion->wr_id)->addr
                  << std::endl;
    }
}


void RDMAClient::event_loop() {
    struct rdma_cm_event* event;
    while (rdma_get_cm_event(event_channel, &event) == 0) {

        struct rdma_cm_event event_copy;
        memcpy(&event_copy, event, sizeof(*event));

        // Acknowledge the event.
        rdma_ack_cm_event(event);

        // For cleanliness, handle the event in a different function.
        int res = this->on_event(&event_copy);

        if (res) { break; }
    }

    LogInfo("out of the loop in event loop, destroying connection");
    rdma_destroy_event_channel(event_channel);

    return;
}


int RDMAClient::on_event(struct rdma_cm_event* event) {
    int res = 1;

    if (event->event == RDMA_CM_EVENT_ADDR_RESOLVED) {
        res = on_addr_resolved(event->id);
    } else if (event->event == RDMA_CM_EVENT_ROUTE_RESOLVED) {
        res = on_route_resolved(event->id);
    } else if (event->event == RDMA_CM_EVENT_CONNECT_REQUEST) {
        res = on_connect_request(event->id);
    } else if (event->event == RDMA_CM_EVENT_ESTABLISHED) {
        res = on_connection(event->id);
    } else if (event->event == RDMA_CM_EVENT_DISCONNECTED) {
        res = on_disconnect(event->id);
        if (stop_after_last_conn and connections.empty()) {
            return -1;
        }
    }
    // else if (event->event == RDMA_CM_EVENT_REJECTED) {
    //     // other server was not present
    //     if(debug_level < 2) {
    //         std::cout << "handling case of RDMA_CM_EVENT_REJECTED, sleeping for 1 sec and waking up semaphore\n";
    //     }
    //     std::this_thread::sleep_for(std::chrono::seconds(1));
    //     error = true;

    //     if(sem_post(&connect_semaphore) != 0) {
    //         std::cout << "sem error " << errno << std::endl;
    //     }

    //     res = 1;
    // }
    else {
        std::cout << toRDMAErrorString(event->event) << "\n";
        // throw std::runtime_error("on_event received unknown event "
            // + toRDMAErrorString(event->event));
    }
    return res;
}
