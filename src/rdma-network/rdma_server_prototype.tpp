// rdma_server_prototype.tpp

#include <cstring>

#include <iostream>
#include <string>

#include "rdma-network/util.hpp"
#include "rdma-network/rdma_server_prototype.hpp"
#include <sys/mman.h>

inline
RDMAServerPrototype::RDMAServerPrototype()
: resources(NULL), started(false), run(true) {}

inline
RDMAServerPrototype::~RDMAServerPrototype() {}

inline
void RDMAServerPrototype::register_memory(
    uintptr_t conn_id, void* addr, size_t len, bool remote_access
) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;

    // Register this memory locally., CHANGING INFO TEMP AS A HACK
    int access_flags = IBV_ACCESS_LOCAL_WRITE;
    if (remote_access) access_flags =
        access_flags | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

        struct ibv_mr* meminfo =
        register_memory_with_conn(conn, addr, len, access_flags);

    // If remote access is set, we must notify the other side as well
    // (specifically, the other side needs to have the rkey
    // before it can perform remote RDMA operations on this memory).
    if (remote_access) {
        send_meminfo(conn, meminfo);
    }

    return;
}

inline
void RDMAServerPrototype::register_memory(
    uintptr_t conn_id, void* addr, size_t len) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;

    // Register this memory locally., CHANGING INFO TEMP AS A HACK
    int access_flags = IBV_ACCESS_LOCAL_WRITE;
    access_flags =
        access_flags | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;

    register_memory_with_conn(conn, addr, len, access_flags);

    return;
}

inline
void RDMAServerPrototype::deregister_memory(uintptr_t conn_id, void* addr) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;

    struct ibv_mr* registration = conn->registrations[addr];
    conn->registrations.erase(addr);
    //ibv_dereg_mr returns an int for success or fail, TODO, add check
    ibv_dereg_mr(registration);
    
    return;
}

inline
void RDMAServerPrototype::send(
    uintptr_t conn_id, const void* msg_buffer, size_t len
) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;

    // Assert that this message is not too big *gasping emoji*
    if (len > rdma_message::MAX_DATA_SIZE) {
        throw std::logic_error(
            "send() called with size "
            + std::to_string(len)
            + " which is larger than the maximum message size "
            + std::to_string(rdma_message::MAX_DATA_SIZE));
    }

    // Package up the message into an rdma_message.
    struct rdma_message rdma_msg;
    memset(&rdma_msg, 0, sizeof(rdma_msg));
    rdma_msg.message_type = rdma_message::MessageType::MSG_USER;
    memcpy(rdma_msg.data, msg_buffer, len);
    rdma_msg.data_size = len;

    // A semaphore for us to block on.
    sem_t completion_sem;
    ASSERT_ZERO(sem_init(&completion_sem, 0, 0));

    // Send the message.
    post_rdma_send(conn, &rdma_msg, &completion_sem);

    // Block until the send has completed.
    sem_wait(&completion_sem);
    sem_destroy(&completion_sem);

    LogInfo("RDMAServerPrototype::send completed");
}

inline
void RDMAServerPrototype::send_prepare(
    uintptr_t conn_id, void* start_addr, size_t len) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;

    // skipping length check since only sending a message of standard size
    // Package up the message into an rdma_message.
    struct rdma_message rdma_msg;
    memset(&rdma_msg, 0, sizeof(rdma_msg));
    rdma_msg.message_type = rdma_message::MessageType::MSG_PREPARE;
    rdma_msg.region_info.addr = start_addr;
    rdma_msg.region_info.length = len;
    rdma_msg.region_info.rkey = conn->registrations[start_addr]->rkey;
    rdma_msg.data_size = 0;

    // Send the message.
    post_rdma_send(conn, &rdma_msg, NULL);

    LogInfo("RDMAServerPrototype::send prepare message to client");
}

#if FAULT_TOLERANT
inline
int RDMAServerPrototype::send_prepare(uintptr_t conn_id, 
    void* start_addr, size_t len, char* client_id, size_t client_id_size) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;

    // skipping length check since only sending a message of standard size
    // Package up the message into an rdma_message.
    struct rdma_message rdma_msg;
    memset(&rdma_msg, 0, sizeof(rdma_msg));
    rdma_msg.message_type = rdma_message::MessageType::MSG_PREPARE;
    rdma_msg.region_info.addr = start_addr;
    rdma_msg.region_info.length = len;
    rdma_msg.region_info.rkey = conn->registrations[start_addr]->rkey;
    memcpy(rdma_msg.data, client_id, client_id_size);
    rdma_msg.data_size = client_id_size;
    LogInfo("RDMAServerPrototype::send prepare message to client");
    // Send the message.
    return post_rdma_send(conn, &rdma_msg, NULL);
}


void RDMAServerPrototype::getPartitionList(uintptr_t conn_id) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;

    struct rdma_message rdma_msg;
    memset(&rdma_msg, 0, sizeof(rdma_msg));
    rdma_msg.message_type = rdma_message::MessageType::MSG_GET_PARTITIONS;
    // Send the message.
    post_rdma_send(conn, &rdma_msg, NULL);

    LogInfo("RDMAServerPrototype::send get partition list message to pair");
}

void RDMAServerPrototype::sendPartitionList(uintptr_t conn_id, std::string str) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;

    struct rdma_message rdma_msg;
    memset(&rdma_msg, 0, sizeof(rdma_msg));
    rdma_msg.message_type = rdma_message::MessageType::MSG_SENT_PARTITIONS;
    LogAssert(str.size() <= rdma_message::MAX_DATA_SIZE, "max size exceeding");
    std::copy(str.begin(), str.end(), rdma_msg.data);
    rdma_msg.data[str.size()] = '\0';
    rdma_msg.data_size = str.size();
    // Send the message.
    post_rdma_send(conn, &rdma_msg, NULL);

    LogInfo("RDMAServerPrototype::send get partition list message to pair");
}

#endif

inline
void RDMAServerPrototype::send_decline(
    uintptr_t conn_id, void* start_addr, size_t len) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;

    // skipping length check since only sending a message of standard size
    // Package up the message into an rdma_message.
    struct rdma_message rdma_msg;
    memset(&rdma_msg, 0, sizeof(rdma_msg));
    rdma_msg.message_type = rdma_message::MessageType::MSG_DECLINE;
    rdma_msg.region_info.addr = start_addr;
    rdma_msg.region_info.length = len;
    rdma_msg.data_size = 0;

    // Send the message.
    post_rdma_send(conn, &rdma_msg, NULL);
    LogInfo("RDMAServerPrototype::send decline message to client");
}

inline
int RDMAServerPrototype::send_accept(
    uintptr_t conn_id, void* start_addr, size_t len) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;

    // skipping length check since only sending a message of standard size
    // Package up the message into an rdma_message.
    struct rdma_message rdma_msg;
    memset(&rdma_msg, 0, sizeof(rdma_msg));
    rdma_msg.message_type = rdma_message::MessageType::MSG_ACCEPT;
    rdma_msg.region_info.addr = start_addr;
    rdma_msg.region_info.length = len;
    rdma_msg.data_size = 0;

    // Send the message.
    LogInfo("RDMAServerPrototype::send accept message to client");
    return post_rdma_send(conn, &rdma_msg, NULL);
}

inline
void RDMAServerPrototype::send_transfer(
    uintptr_t conn_id, void* start_addr, size_t len) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;

    // skipping length check since only sending a message of standard size
    // Package up the message into an rdma_message.
    struct rdma_message rdma_msg;
    memset(&rdma_msg, 0, sizeof(rdma_msg));
    rdma_msg.message_type = rdma_message::MessageType::MSG_TRANSFER;
    rdma_msg.region_info.addr = start_addr;
    rdma_msg.region_info.length = len;
    rdma_msg.region_info.rkey = conn->registrations[start_addr]->rkey;
    rdma_msg.data_size = 0;

    // Send the message.
    post_rdma_send(conn, &rdma_msg, NULL);
    LogInfo("RDMAServerPrototype::send transfer message to client");
}

inline
void RDMAServerPrototype::send_close(
    uintptr_t conn_id, void* addr, size_t len) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;
    // conn->remote_registrations.erase(addr);
    struct rdma_message rdma_msg;
    memset(&rdma_msg, 0, sizeof(rdma_msg));
    rdma_msg.message_type = rdma_message::MessageType::MSG_DONE_TRANSFER;
    rdma_msg.region_info.addr = addr;
    rdma_msg.region_info.length = len;
    rdma_msg.data_size = 0;

    // Send the message.
    post_rdma_send(conn, &rdma_msg, NULL);
    LogInfo("RDMAServerPrototype::send done message to client");
}

inline
std::pair<void*, size_t> RDMAServerPrototype::receive(uintptr_t conn_id) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;

    std::pair<void*, size_t> res = conn->recv_queue.dequeue();

    // Implementation detail:
    // You might have noticed we pretty much do nothing here compared
    // to the other user interface methods.
    // This is because the completion thread automatically handles
    // reposting of receives to the work queue, since they are used for more
    // than just user-level sends.
    return res;
}

inline
bool RDMAServerPrototype::checkForMessage(uintptr_t conn_id) {
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;
    return !conn->recv_queue.empty();
}

inline
void RDMAServerPrototype::rdma_read_async(
    uintptr_t conn_id, void* local_addr, void* remote_addr, size_t len, void (*callback)(void*), void* data
) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*) conn_id;

    // First, we have to fetch the lkey and rkey for these regions
    // from our registration information.
    // Since we can, we'll do a lot of error checking here,
    // and make sure that the region is indeed registered.
    // Also, I know this is runs in time linear wrt number of registrations,
    // but we expect that number to be very small :)
    uint32_t lkey = 0;
    bool lkey_found = false;
    void* local_addr_end = (void*) ((char*)local_addr + len);
    for (const auto& it : conn->registrations) {
        void* addr = it.first;
        void* end_addr = (void*) ((char*)addr + it.second->length);
        if (local_addr >= addr and local_addr_end <= end_addr) {
            lkey_found = true;
            lkey = it.second->lkey;
            break;
        }
    }
    if (not lkey_found) {
        throw std::logic_error(
            "rdma_read called on locally unregistered memory!");
    }

    uint32_t rkey = 0;
    bool rkey_found = false;
    void* remote_addr_end = (void*) ((char*)remote_addr + len);
    for (const auto& it : conn->remote_registrations) {
        void* addr = it.first;
        void* end_addr = (void*) ((char*)addr + it.second.length);
        if (remote_addr >= addr and remote_addr_end <= end_addr) {
            rkey_found = true;
            rkey = it.second.rkey;
            break;
        }
    }
    if (not rkey_found) {
        throw std::logic_error(
            "rdma_read called on remotely unregistered memory!");
    }

    LogInfo("posting read to rdma queue");
    // Do the read.
    post_rdma_read(conn, local_addr, lkey, remote_addr, rkey, len, callback, data);
    
    // And wait for the read to finish.
    LogInfo("read posted, will complete async");
    return;
}

inline
int RDMAServerPrototype::rdma_read(
    uintptr_t conn_id, void* local_addr, void* remote_addr, size_t len
) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*) conn_id;

    // First, we have to fetch the lkey and rkey for these regions
    // from our registration information.
    // Since we can, we'll do a lot of error checking here,
    // and make sure that the region is indeed registered.
    // Also, I know this is runs in time linear wrt number of registrations,
    // but we expect that number to be very small :)
    uint32_t lkey = 0;
    bool lkey_found = false;
    void* local_addr_end = (void*) ((char*)local_addr + len);
    for (const auto& it : conn->registrations) {
        void* addr = it.first;
        void* end_addr = (void*) ((char*)addr + it.second->length);
        if (local_addr >= addr and local_addr_end <= end_addr) {
            lkey_found = true;
            lkey = it.second->lkey;
            break;
        }
    }
    if (not lkey_found) {
        return -1;
        // throw std::logic_error(
        //     "rdma_read called on locally unregistered memory!");
    }

    uint32_t rkey = 0;
    bool rkey_found = false;
    void* remote_addr_end = (void*) ((char*)remote_addr + len);
    for (const auto& it : conn->remote_registrations) {
        void* addr = it.first;
        void* end_addr = (void*) ((char*)addr + it.second.length);
        if (remote_addr >= addr and remote_addr_end <= end_addr) {
            rkey_found = true;
            rkey = it.second.rkey;
            break;
        }
    }
    if (not rkey_found) {
        return -1;
        // throw std::logic_error(
        //     "rdma_read called on remotely unregistered memory!");
    }

    // Now we can actually execute the read.
    // Create the semaphore to block on.
    sem_t sem;
    ASSERT_ZERO(sem_init(&sem, 0, 0));
    LogInfo("posting read to rdma queue");
    // Do the read.
    post_rdma_read(conn, local_addr, lkey, remote_addr, rkey, len, &sem);
    
    // And wait for the read to finish.
    sem_wait(&sem);
    sem_destroy(&sem);
    LogInfo("read completed");
    return 0;
}

inline
void RDMAServerPrototype::rdma_write(
    uintptr_t conn_id, void* local_addr, void* remote_addr, size_t len
) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*) conn_id;

    uint32_t lkey = 0;
    ibv_mr* mr_local = conn->registrations[local_addr];
    lkey = mr_local->lkey;



    uint32_t rkey = 0;
    remote_region mr_remote = conn->remote_registrations[remote_addr];
    rkey = mr_remote.rkey;
    
    // Now we can actually execute the read.
    // Create the semaphore to block on.
    sem_t sem;
    ASSERT_ZERO(sem_init(&sem, 0, 0));

    // Do the read.
    post_rdma_write(conn, local_addr, lkey, remote_addr, rkey, len, &sem);

    // And wait for the read to finish.
    sem_wait(&sem);
    sem_destroy(&sem);

    return;
}

inline
void RDMAServerPrototype::done(uintptr_t conn_id) {
    std::lock_guard<std::mutex> guard(user_mutex);
    struct rdma_connection* conn = (struct rdma_connection*)conn_id;

    LogInfo("Closing RDMA connection %p ",(void*) conn_id);

    // To close this connection, we'll send a MSG_DONE
    // to the other side. Let's create this message.
    struct rdma_message rdma_msg;
    memset(&rdma_msg, 0, sizeof(rdma_msg));
    rdma_msg.message_type = rdma_message::MessageType::MSG_DONE;

    // The semaphore for us to block on.
    sem_t done_sem;
    ASSERT_ZERO(sem_init(&done_sem, 0, 0));
    conn->disconnection_sem = &done_sem;

    // Send the close message.
    post_rdma_send(conn, &rdma_msg, NULL);

    // Actually disconnecting will be handled by the completion thread
    // for this connection, as well as the event thread for this server.
    // All we do now is wait.
    sem_wait(&done_sem);
    sem_destroy(&done_sem);

    LogInfo("Connection %p successfully closed", (void*) conn_id);
    return;
}

inline
void RDMAServerPrototype::destroy() {
    event_thread.join();
}

inline
void RDMAServerPrototype::event_loop() {
    // Pull from the event channel in a loop and process the events.
    struct rdma_cm_event* event;
    while (rdma_get_cm_event(event_channel, &event) == 0) {
        // Copy the event out before we process it. (for safety?)
        struct rdma_cm_event event_copy;
        memcpy(&event_copy, event, sizeof(*event));

        // Acknowledge the event.
        rdma_ack_cm_event(event);

        // For cleanliness, handle the event in a different function.
        int res = this->on_event(&event_copy);

        // Break out of the loop if the handler returned an error.
        // (NB: I don't think any handler returns errors right now.)
        // (Also, we'll want an additional way to gracefully shut down
        // the server I think...)
        if (res) { break; }
    }

    LogInfo("out of the loop in event loop, destroying connection");
    rdma_destroy_event_channel(event_channel);
    return;
}

inline
std::string toRDMAErrorString(int event) {
    if (event == RDMA_CM_EVENT_ADDR_RESOLVED) {
        return "RDMA_CM_EVENT_ADDR_RESOLVED";
    } else if (event == RDMA_CM_EVENT_TIMEWAIT_EXIT) {
        return "RDMA_CM_EVENT_TIMEWAIT_EXIT";
    } else if (event == RDMA_CM_EVENT_ADDR_CHANGE) {
        return "RDMA_CM_EVENT_ADDR_CHANGE";
    } else if (event == RDMA_CM_EVENT_MULTICAST_ERROR) {
        return "RDMA_CM_EVENT_MULTICAST_ERROR";
    } else if (event == RDMA_CM_EVENT_MULTICAST_JOIN) {
        return "RDMA_CM_EVENT_MULTICAST_JOIN";
    } else if (event == RDMA_CM_EVENT_DEVICE_REMOVAL) {
        return "RDMA_CM_EVENT_DEVICE_REMOVAL";
    } else if (event == RDMA_CM_EVENT_DISCONNECTED) {
        return "RDMA_CM_EVENT_DISCONNECTED";
    } else if (event == RDMA_CM_EVENT_ADDR_ERROR) {
        return "RDMA_CM_EVENT_ADDR_ERROR";
    } else if (event == RDMA_CM_EVENT_ESTABLISHED) {
        return "RDMA_CM_EVENT_ESTABLISHED";
    } else if (event == RDMA_CM_EVENT_ROUTE_RESOLVED) {
        return "RDMA_CM_EVENT_ROUTE_RESOLVED";
    } else if (event == RDMA_CM_EVENT_ROUTE_ERROR) {
        return "RDMA_CM_EVENT_ROUTE_ERROR";
    } else if (event == RDMA_CM_EVENT_CONNECT_REQUEST) {
        return "RDMA_CM_EVENT_CONNECT_REQUEST";
    } else if (event == RDMA_CM_EVENT_CONNECT_RESPONSE) {
        return "RDMA_CM_EVENT_CONNECT_RESPONSE";
    } else if (event == RDMA_CM_EVENT_CONNECT_ERROR) {
        return "RDMA_CM_EVENT_CONNECT_ERROR";
    } else if (event == RDMA_CM_EVENT_UNREACHABLE) {
        return "RDMA_CM_EVENT_UNREACHABLE";
    } else if (event == RDMA_CM_EVENT_REJECTED) {
        return "RDMA_CM_EVENT_REJECTED";
    }    else {
        return "unknown event";
    }
}

inline
int RDMAServerPrototype::on_event(struct rdma_cm_event* event) {
    // FYI:
    // The event flow for the server and client are different from each other.
    // Servers listen for connections, and thus undergo:
    // -> RDMA_CM_EVENT_CONNECT_REQUEST
    // -> RDMA_CM_EVENT_ESTABLISHED
    // -> RDMA_CM_EVENT_DISCONNECTED
    // whereas clients initiate connections, and thus undergo:
    // -> RDMA_CM_EVENT_ADDR_RESOLVED
    // -> RDMA_CM_EVENT_ROUTE_RESOLVED
    // -> RDMA_CM_EVENT_ESTABLISHED
    // -> RDMA_CM_EVENT_DISCONNECTED
    // Thus not all of the handlers will be valid for each connection.
    // We leave it to the implementer to check that only valid handlers
    // are called;
    // currently, we'll do this by having most handlers raise exceptions by
    // default, and you have to explicitly override the ones you want in the
    // child classes for your server type.
    int res = -1;
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
    } else {
        std::cout << toRDMAErrorString(event->event) << "\n";
        // throw std::runtime_error("on_event received unknown event + toRDMAErrorString(event->event));
    }
    return res;
}

// Context specific events to be implemented by subclasses.
//
// (Not all of these need to be implemented by subclasses, so to avoid
// pure virtual errors, we'll give them all an implementation right now.)
inline
int RDMAServerPrototype::on_addr_resolved(struct rdma_cm_id* rdma_socket) {
    throw std::exception();
}
inline
int RDMAServerPrototype::on_route_resolved(struct rdma_cm_id* rdma_socket) {
    throw std::exception();
}
inline
int RDMAServerPrototype::on_connect_request(struct rdma_cm_id* rdma_socket) {
    throw std::exception();
}
inline
int RDMAServerPrototype::on_connection(struct rdma_cm_id* rdma_socket) {
    throw std::exception();
}

inline
int RDMAServerPrototype::on_disconnect(struct rdma_cm_id* rdma_socket) {
    // The socket's connection.
    struct rdma_connection* conn = (struct rdma_connection*) rdma_socket->context;

    LogInfo("Socket %p disconnected", rdma_socket);
    LogInfo("Connection ID was : %p", conn);

    // Smash the semaphore.
    // sem_post(conn->disconnection_sem);

    // Delete this connection from our list of connections.
    connections.erase(conn);

    // Destroy the queue pair.
    rdma_destroy_qp(rdma_socket);

    // Deregister all memory regions.
    for (const auto& it : conn->registrations) {
        ibv_dereg_mr(it.second);
    }

    // Since we manually allocated our send and receive buffer ourselves,
    // deallocate them.
    free(conn->send_buffer);
    free(conn->recv_buffer);

    // Delete our connection context structure.
    delete conn;

    // And finally, destroy the socket.
    rdma_destroy_id(rdma_socket);

    return 0;
}

inline
void* RDMAServerPrototype::poll_cq() {
    struct ibv_cq *cq;
    struct ibv_wc wc;
    void* cq_context;
    while (run) {
        // Wait until the completion channel notifies us.
        // It will fill in the completion queue and context.
        ASSERT_ZERO(ibv_get_cq_event(resources->completion_channel, &cq, &cq_context));
        // Acknowledge the notification by acknowledging one event.
        ibv_ack_cq_events(cq, 1);
        // Rearm the completion queue.
        int solicited_only = 0;
        ASSERT_ZERO(ibv_req_notify_cq(cq, solicited_only));

        // Empty the completion queue and process completions.
        while (ibv_poll_cq(cq, 1, &wc)) {
            on_completion(&wc);
        }
    }

    return NULL;
}

inline
void RDMAServerPrototype::on_completion(struct ibv_wc* work_completion) {
    // TODO: handle this more verbosely. Just for debugging though --
    // we don't intend to allow errors in proper operation.
    if (work_completion->status != IBV_WC_SUCCESS) {
        LogError("Case IBV_WC_SUCCESS error, it should be handled");
        run = false;
        return;
    }

    // Call the appropriate handler depending on opcode.
    if (work_completion->opcode == IBV_WC_RECV) {
        on_recv_finish(work_completion);
    } else if (work_completion->opcode == IBV_WC_SEND) {
        on_send_finish(work_completion);
    } else if (work_completion->opcode == IBV_WC_RDMA_READ) {
        on_rdma_read_finish(work_completion);
    } else if (work_completion->opcode == IBV_WC_RDMA_WRITE) {
        on_rdma_write_finish(work_completion);
    } else if (work_completion->opcode == IBV_WC_COMP_SWAP) {
        on_comp_swap_finish(work_completion);
    } else if (work_completion->opcode == IBV_WC_FETCH_ADD) {
        on_fetch_add_finish(work_completion);
    } else if (work_completion->opcode == IBV_WC_BIND_MW) {
        on_bind_mw_finish(work_completion);
    } else if (work_completion->opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
        on_imm_recv_finish(work_completion);
    }

    // Signal the semaphore if one was provided in the work context.
    // Then destroy the work context.
    struct work_context* work_ctx = (struct work_context*) work_completion->wr_id;
    if (work_ctx->sem != NULL) {
        sem_post(work_ctx->sem);
    }

    if (work_ctx->call_back != NULL) {
        work_ctx->call_back(work_ctx->data);
        // free(work_ctx->data);
    }
    delete work_ctx;

    return;
}

inline
void RDMAServerPrototype::on_recv_finish(struct ibv_wc* wc) {
    // unsafe for receiving lots of messages in a quick burst
    // Grab the work context.
    struct work_context* work_ctx = (struct work_context*) wc->wr_id;
    struct rdma_connection* conn = work_ctx->conn;

    // An RDMA receive operation has been completed.
    // We've standardized the message that can be sent,
    // so extract the message into a struct.
    struct rdma_message* msg = (struct rdma_message*) work_ctx->addr;

    // Check the message type.
    if (msg->message_type == msg->MessageType::MSG_USER) {
        // Rearm the RDMA receive queue as soon as possible.
        post_rdma_receive(conn);

        // Copy the user message into some new memory.
        void* msg_for_user = malloc(msg->data_size);
        memcpy(msg_for_user, msg->data, msg->data_size);
        // And pass it back up to the user.
        conn->recv_queue.enqueue(std::pair<void*, size_t>(msg_for_user, msg->data_size));

    } else if (msg->message_type == msg->MessageType::MSG_MEMINFO) {
        // Rearm the RDMA receive queue as soon as possible.
        post_rdma_receive(conn);
        // posting a receive on the same mem location is not safe here as the NIC
        //can write to the same location


        // Save this memory region into our list of remote registrations.
        void* addr = msg->region_info.addr;
        conn->remote_registrations[addr] = msg->region_info;

        // Send an ack.
        struct rdma_message rdma_msg;
        memset(&rdma_msg, 0, sizeof(rdma_msg));
        rdma_msg.message_type = rdma_message::MessageType::MSG_ACK_MEMINFO;
        post_rdma_send(conn, &rdma_msg, NULL);

    } else if (msg->message_type == msg->MessageType::MSG_ACK_MEMINFO) {
        // Rearm the RDMA receive queue as soon as possible.
        post_rdma_receive(conn);

        // We've received an ack for a MSG_MEMINFO we sent out earlier.
        // Hit the semaphore to let the user thread know it's done.
        sem_post(conn->register_memory_sem);

    } else if (msg->message_type == msg->MessageType::MSG_DONE) {
        conn->recv_done = true;
        // If both sides have sent DONE, then we can begin disconnecting.
        if (conn->sent_done) {
            rdma_disconnect(conn->rdma_socket);
        }
    
    } else if(msg->message_type == msg->MessageType::MSG_PREPARE) {
        // Rearm the RDMA receive queue as soon as possible.
        post_rdma_receive(conn);

        //alternately we can send this to the user and let them decide or add checks here for acceptance
        // And pass it back up to the user.
        struct rdma_message* msg_for_user = (struct rdma_message*)malloc(sizeof(struct rdma_message));
        memcpy(msg_for_user, msg, sizeof(struct rdma_message));
        conn->recv_queue.enqueue(
            std::pair<void*, size_t>((void*)msg_for_user, sizeof(struct rdma_message))); 

        //register memory and accept
        // void* addr = msg->region_info.addr;
        // std::cout << "got prepare at memory at locaiton : " << addr << std::endl;
        // // conn->remote_registrations[addr] = msg->region_info;
        
        // size_t size = msg->region_info.length;
        // addr = mmap(addr, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        
        // std::cout << "allocating memory at locaiton : " << addr << std::endl;
        
        // if(addr == MAP_FAILED) {
        //     struct rdma_message rdma_msg;
        //     memset(&rdma_msg, 0, sizeof(rdma_msg));
        //     rdma_msg.message_type = rdma_message::MessageType::MSG_DECLINE;
        //     post_rdma_send(conn, &rdma_msg, NULL);
        // } 
        // //lets register the memory as well
        // this->register_memory((uintptr_t)conn->rdma_socket, addr, size, false);

        // // Send an ack.
        // struct rdma_message rdma_msg;
        // memset(&rdma_msg, 0, sizeof(rdma_msg));
        // rdma_msg.message_type = rdma_message::MessageType::MSG_ACCEPT;
        // post_rdma_send(conn, &rdma_msg, NULL);

    } else if (msg->message_type == msg->MessageType::MSG_ACCEPT) {
        // Rearm the RDMA receive queue as soon as possible.
        post_rdma_receive(conn);
        // pass it to user to determine when to send  MSG_TRANSFER
        struct rdma_message* msg_for_user = (struct rdma_message*)malloc(sizeof(struct rdma_message));
        memcpy(msg_for_user, msg, sizeof(struct rdma_message));
        conn->recv_queue.enqueue(
            std::pair<void*, size_t>((void*)msg_for_user, sizeof(struct rdma_message))); 

        
        // throw std::runtime_error("not implemented receiveing accept");  
    } else if (msg->message_type == msg->MessageType::MSG_TRANSFER) {
        // Rearm the RDMA receive queue as soon as possible.
        post_rdma_receive(conn);
        struct rdma_message* msg_for_user = (struct rdma_message*)malloc(sizeof(struct rdma_message));
        memcpy(msg_for_user, msg, sizeof(struct rdma_message));
        
        void* addr = msg_for_user->region_info.addr;
        conn->remote_registrations[addr] = msg_for_user->region_info;
        
        conn->recv_queue.enqueue(
            std::pair<void*, size_t>((void*)msg_for_user, sizeof(struct rdma_message))); 

        // // we have alread registered the memory so we will simply read the memory location
        // this->rdma_read((uintptr_t)conn->rdma_socket, 
        // msg->region_info.addr, msg->region_info.addr, msg->region_info.length);

        // // pass it to user to notify the transfer has completed
        // conn->recv_queue.enqueue(
        //     std::pair<void*, size_t>((void*)msg, sizeof(struct rdma_message)));
    } else if (msg->message_type == msg->MessageType::MSG_DONE_TRANSFER) {
        // Rearm the RDMA receive queue as soon as possible.
        post_rdma_receive(conn);
        struct rdma_message* msg_for_user = (struct rdma_message*)malloc(sizeof(struct rdma_message));
        memcpy(msg_for_user, msg, sizeof(struct rdma_message));
        
        // conn->registrations.erase(addr);
        
        conn->recv_queue.enqueue(
            std::pair<void*, size_t>((void*)msg_for_user, sizeof(struct rdma_message))); 

        // // we have alread registered the memory so we will simply read the memory location
        // this->rdma_read((uintptr_t)conn->rdma_socket, 
        // msg->region_info.addr, msg->region_info.addr, msg->region_info.length);

        // // pass it to user to notify the transfer has completed
        // conn->recv_queue.enqueue(
        //     std::pair<void*, size_t>((void*)msg, sizeof(struct rdma_message)));
    } else if(msg->message_type == msg->MessageType::MSG_DECLINE) {
        
        post_rdma_receive(conn);
        struct rdma_message* msg_for_user = (struct rdma_message*)malloc(sizeof(struct rdma_message));
        memcpy(msg_for_user, msg, sizeof(struct rdma_message));
        conn->recv_queue.enqueue(
            std::pair<void*, size_t>((void*)msg_for_user, sizeof(struct rdma_message)));
    } else if(msg->message_type == msg->MessageType::MSG_GET_PARTITIONS) {
        
        post_rdma_receive(conn);
        struct rdma_message* msg_for_user = (struct rdma_message*)malloc(sizeof(struct rdma_message));
        memcpy(msg_for_user, msg, sizeof(struct rdma_message));
        conn->recv_queue.enqueue(
            std::pair<void*, size_t>((void*)msg_for_user, sizeof(struct rdma_message)));

    } else if(msg->message_type == msg->MessageType::MSG_SENT_PARTITIONS) {
        
        post_rdma_receive(conn);
        struct rdma_message* msg_for_user = (struct rdma_message*)malloc(sizeof(struct rdma_message));
        memcpy(msg_for_user, msg, sizeof(struct rdma_message));
        conn->recv_queue.enqueue(
            std::pair<void*, size_t>((void*)msg_for_user, sizeof(struct rdma_message)));

    } else {
        throw std::runtime_error("Invalid enum for MessageType!");
    }
}

inline
void RDMAServerPrototype::on_send_finish(struct ibv_wc* wc) {
    // Grab the work context.
    struct work_context* work_ctx = (struct work_context*) wc->wr_id;
    struct rdma_connection* conn = work_ctx->conn;

    // Do specific things depending on the message type.
    struct rdma_message* msg = (struct rdma_message*) work_ctx->addr;
    if (msg->message_type == msg->MessageType::MSG_USER) {
        // Nothing to do; boilerplate in on_completion will
        // hit the semaphore to signal the calling thread.

    } else if (msg->message_type == msg->MessageType::MSG_MEMINFO) {
        // We've successfully sent memory region information.
        // Again, nothing to do here.

    } else if (msg->message_type == msg->MessageType::MSG_ACK_MEMINFO) {
        // Nothing to do.

    } else if (msg->message_type == msg->MessageType::MSG_DONE) {
        // The remote side has called done().
        // Update our connection state to remember this.
        // And if we've also called done(), do the disconnect.
        conn->sent_done = true;
        if (conn->recv_done) {
            rdma_disconnect(conn->rdma_socket);
        }

    } else if(msg->message_type == msg->MessageType::MSG_PREPARE) {
        LogInfo("completed send on prepare");
    } else if (msg->message_type == msg->MessageType::MSG_ACCEPT) {
        LogInfo("completed send on accept");
    } else if (msg->message_type == msg->MessageType::MSG_TRANSFER) {
        LogInfo("completed send on transfer");
    } else if (msg->message_type == msg->MessageType::MSG_DONE_TRANSFER){
        LogInfo("completed send on done");
    } else if (msg->message_type == msg->MessageType::MSG_GET_PARTITIONS){
        LogInfo("completed send on MSG_GET_PARTITIONS");
    } else if (msg->message_type == msg->MessageType::MSG_SENT_PARTITIONS){
        LogInfo("completed send on MSG_SENT_PARTITIONS");
    } else {
        throw std::runtime_error("Invalid enum for MessageType on_send_finish!");
    }
}

inline
void RDMAServerPrototype::on_rdma_read_finish(struct ibv_wc* wc) {
    LogInfo("read completed");
}

inline
void RDMAServerPrototype::on_rdma_write_finish(struct ibv_wc* wc) {
    throw std::logic_error(
        "Support for this opcode has not yet been implemented!");
}

inline
void RDMAServerPrototype::on_comp_swap_finish(struct ibv_wc* wc) {
    throw std::logic_error(
        "Support for this opcode has not yet been implemented!");
}

inline
void RDMAServerPrototype::on_fetch_add_finish(struct ibv_wc* wc) {
    throw std::logic_error(
        "Support for this opcode has not yet been implemented!");
}

inline
void RDMAServerPrototype::on_bind_mw_finish(struct ibv_wc* wc) {
    throw std::logic_error(
        "Support for this opcode has not yet been implemented!");
}

inline
void RDMAServerPrototype::on_imm_recv_finish(struct ibv_wc* wc) {
    throw std::logic_error(
        "Support for this opcode has not yet been implemented!");
}

inline
void RDMAServerPrototype::build_resources_for_device(
    struct ibv_context* dev_ctx
) {
    // Build the resources object and fill it out.
    resources = new device_resources();

    // Save the device context.
    resources->device_context = dev_ctx;
    // Create the protection domain.
    ASSERT_NONZERO(resources->protection_domain = ibv_alloc_pd(dev_ctx));

    // Create the completion channel.
    ASSERT_NONZERO(resources->completion_channel = ibv_create_comp_channel(dev_ctx));

    // Create the completion queue, and register the channel with it.
    int num_entries = 256; // Arbitrary.
    // The completion queue allows you to attach an arbitrary context object
    // to it, but we won't be using that right now.
    void* cq_context = NULL;
    // Not going to pretend to know what this does.
    // See http://www.rdmamojo.com/2012/11/03/ibv_create_cq/
    int comp_vector = 0;
    ASSERT_NONZERO(resources->completion_queue = ibv_create_cq(
        dev_ctx, num_entries, cq_context,
        resources->completion_channel, comp_vector));

    // Arm the completion queue (request a notification via the completion
    // channel the next time an event is added to the completion queue).
    // See man 3 ibv_req_notify_cq for details.
    int solicited_only = 0;
    ASSERT_ZERO(ibv_req_notify_cq(resources->completion_queue, solicited_only));

    // Spin up the completion queue poller thread.
    resources->cq_poller_thread = std::thread(&RDMAServerPrototype::poll_cq, this);

    return;
}

inline
void RDMAServerPrototype::build_queue_pair(struct rdma_cm_id* rdma_socket) {
    // Create the parameter struct.
    struct ibv_qp_init_attr qp_attr;
    memset(&qp_attr, 0, sizeof(qp_attr));
    // Attach the completion queue to the queue pair.
    qp_attr.send_cq = resources->completion_queue;
    qp_attr.recv_cq = resources->completion_queue;
    // Flag for a reliable connection.
    qp_attr.qp_type = IBV_QPT_RC;
    // Size of the queues (arbitrary I think?)
    qp_attr.cap.max_send_wr = 1024;
    qp_attr.cap.max_recv_wr = 1024;
    // Max number of SGEs per work request (also arbitrary?)
    qp_attr.cap.max_send_sge = 1;
    qp_attr.cap.max_recv_sge = 1;

    // Create the queue pair.
    ASSERT_ZERO(rdma_create_qp(rdma_socket, resources->protection_domain, &qp_attr));
}

inline
struct rdma_connection* RDMAServerPrototype::build_connection_object(
    struct rdma_cm_id* rdma_socket
) {
    struct rdma_connection* conn = new rdma_connection();

    // Fill in the upwards pointer.
    conn->rdma_socket = rdma_socket;

    // Allocate our send and receive buffers.
    conn->send_buffer = malloc(sizeof(struct rdma_message));
    conn->recv_buffer = malloc(sizeof(struct rdma_message));

    // Register this memory with our connection.
    int access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE;
    register_memory_with_conn(conn, conn->send_buffer, sizeof(struct rdma_message), access_flags);
    register_memory_with_conn(conn, conn->recv_buffer, sizeof(struct rdma_message), access_flags);

    conn->recv_done = false;
    conn->sent_done = false;

    // Add to list of connections.
    connections.insert(conn);

    return conn;
}

inline
struct ibv_mr* RDMAServerPrototype::register_memory_with_conn(
    struct rdma_connection* conn, void* addr, int size, int access_flags
) {
    struct ibv_mr* registration;

    // Register the memory.
    ASSERT_NONZERO(registration = ibv_reg_mr(resources->protection_domain, addr, size, access_flags));

    // Add the registration to our map of registrations.
    conn->registrations[addr] = registration;

    // For convenience, return the registration.
    return registration;
}

inline
void RDMAServerPrototype::send_meminfo(
    struct rdma_connection* conn, struct ibv_mr* meminfo
) {
    // Create the rdma_message object that we're going to send.
    struct rdma_message rdma_msg;
    memset(&rdma_msg, 0, sizeof(rdma_msg));
    // Fill it out.
    rdma_msg.message_type = rdma_message::MessageType::MSG_MEMINFO;
    rdma_msg.region_info.addr = meminfo->addr;
    rdma_msg.region_info.length = meminfo->length;
    rdma_msg.region_info.rkey = meminfo->rkey;

    // Make a semaphore for us to wait on.
    sem_t done_sem;
    ASSERT_ZERO(sem_init(&done_sem, 0, 0));
    conn->register_memory_sem = &done_sem;

    // Send this message and wait for it to finish.
    post_rdma_send(conn, &rdma_msg, NULL);
    sem_wait(&done_sem);
    conn->register_memory_sem = NULL;
    sem_destroy(&done_sem);
}

inline
void RDMAServerPrototype::post_rdma_receive(struct rdma_connection* conn) {
    // Create the work context.
    struct work_context* work_ctx = new work_context();
    work_ctx->addr = conn->recv_buffer;
    work_ctx->conn = conn;

    // Create the SGE.
    struct ibv_sge sge;
    sge.addr = (uintptr_t) conn->recv_buffer;
    sge.length = sizeof(struct rdma_message);
    sge.lkey = conn->registrations[(void*)sge.addr]->lkey;

    // Create the work request for the RDMA receive.
    struct ibv_recv_wr receive_request;
    // Attach the work context.
    receive_request.wr_id = (uintptr_t)work_ctx;
    // We only want one work request, so set next to NULL.
    receive_request.next = NULL;
    // Attach the SGE.
    receive_request.sg_list = &sge;
    receive_request.num_sge = 1;

    // If a work request fails, it will be returned here.
    struct ibv_recv_wr* bad_wr;

    ASSERT_ZERO(ibv_post_recv(conn->rdma_socket->qp, &receive_request, &bad_wr));
}

inline
int RDMAServerPrototype::post_rdma_send(
    struct rdma_connection* conn, struct rdma_message* msg, sem_t* sem
) {
    // Create the work context.
    struct work_context* work_ctx = new work_context();
    work_ctx->conn = conn;
    work_ctx->addr = conn->send_buffer;
    work_ctx->sem = sem;

    // Copy the message into our send buffer.
    memcpy(conn->send_buffer, msg, sizeof(struct rdma_message));

    // Create the sge.
    struct ibv_sge sge;
    memset(&sge, 0, sizeof(sge));
    sge.addr = (uintptr_t) conn->send_buffer;
    sge.length = sizeof(struct rdma_message);
    sge.lkey = conn->registrations[(void*)sge.addr]->lkey;

    // Create the work request.
    struct ibv_send_wr send_request;
    // (Tutorial zeros this out and I'm too lazy to verify this is necessary)
    memset(&send_request, 0, sizeof(send_request));
    // Set opcode and flags; see rdmamojo ibv_post_send for details.
    send_request.opcode = IBV_WR_SEND;
    send_request.send_flags = IBV_SEND_SIGNALED;
    // Attach the work context.
    send_request.wr_id = (uintptr_t)work_ctx;
    // This is a single work request, so set next to NULL.
    send_request.next = NULL;
    // Attach the SGE.
    send_request.sg_list = &sge;
    send_request.num_sge = 1;

    // If a work request fails, it will be returned here.
    struct ibv_send_wr* bad_wr;

    return ibv_post_send(conn->rdma_socket->qp, &send_request, &bad_wr);
}

inline
void RDMAServerPrototype::post_rdma_read(
    struct rdma_connection* conn,
    void* local_addr, uint32_t lkey,
    void* remote_addr, uint32_t rkey,
    size_t length, void (*callback)(void*), void* data
) {
    // Create the work context.
    struct work_context* work_ctx = new work_context();
    work_ctx->conn = conn;
    work_ctx->addr = local_addr;
    work_ctx->sem = NULL;
    work_ctx->call_back = callback;
    work_ctx->data = data;

    // Create the sge.
    struct ibv_sge sge;
    memset(&sge, 0, sizeof(sge));
    // And attach local memory info.
    sge.addr = (uintptr_t)local_addr;
    sge.length = length;
    sge.lkey = lkey;

    // Create the work request.
    struct ibv_send_wr send_request;
    memset(&send_request, 0, sizeof(send_request));
    // Attach remote memory info.
    send_request.wr.rdma.remote_addr = (uintptr_t)remote_addr;
    send_request.wr.rdma.rkey = rkey;
    // Boilerplate: set opcode and flags; attach sge; attach work context.
    send_request.opcode = IBV_WR_RDMA_READ;
    send_request.send_flags = IBV_SEND_SIGNALED;
    send_request.sg_list = &sge;
    send_request.num_sge = 1;
    send_request.next = NULL;
    send_request.wr_id = (uintptr_t)work_ctx;

    struct ibv_send_wr* bad_wr;
    int rc = ibv_post_send(conn->rdma_socket->qp, &send_request, &bad_wr);
    LogAssert(rc == 0, "async posting failure bcz %s", strerror(rc));
}

inline
void RDMAServerPrototype::post_rdma_read(
    struct rdma_connection* conn,
    void* local_addr, uint32_t lkey,
    void* remote_addr, uint32_t rkey,
    size_t length, sem_t* sem
) {
    // Create the work context.
    struct work_context* work_ctx = new work_context();
    work_ctx->conn = conn;
    work_ctx->addr = local_addr;
    work_ctx->sem = sem;

    // Create the sge.
    struct ibv_sge sge;
    memset(&sge, 0, sizeof(sge));
    // And attach local memory info.
    sge.addr = (uintptr_t)local_addr;
    sge.length = length;
    sge.lkey = lkey;

    // Create the work request.
    struct ibv_send_wr send_request;
    memset(&send_request, 0, sizeof(send_request));
    // Attach remote memory info.
    send_request.wr.rdma.remote_addr = (uintptr_t)remote_addr;
    send_request.wr.rdma.rkey = rkey;
    // Boilerplate: set opcode and flags; attach sge; attach work context.
    send_request.opcode = IBV_WR_RDMA_READ;
    send_request.send_flags = IBV_SEND_SIGNALED;
    send_request.sg_list = &sge;
    send_request.num_sge = 1;
    send_request.next = NULL;
    send_request.wr_id = (uintptr_t)work_ctx;

    struct ibv_send_wr* bad_wr;

    ASSERT_ZERO(ibv_post_send(
        conn->rdma_socket->qp, &send_request, &bad_wr));
}

inline
void RDMAServerPrototype::post_rdma_write(
    struct rdma_connection* conn,
    void* local_addr, uint32_t lkey,
    void* remote_addr, uint32_t rkey,
    size_t length, sem_t* sem
) {
    // Create the work context.
    struct work_context* work_ctx = new work_context();
    work_ctx->conn = conn;
    work_ctx->addr = local_addr;
    work_ctx->sem = sem;

    // Create the sge.
    struct ibv_sge sge;
    memset(&sge, 0, sizeof(sge));
    // And attach local memory info.
    sge.addr = (uintptr_t)remote_addr;
    sge.length = length;
    sge.lkey = lkey;

    // Create the work request.
    struct ibv_send_wr send_request;
    memset(&send_request, 0, sizeof(send_request));
    // Attach remote memory info.
    send_request.wr.rdma.remote_addr = (uintptr_t)remote_addr;
    send_request.wr.rdma.rkey = rkey;
    // Boilerplate: set opcode and flags; attach sge; attach work context.
    send_request.opcode = IBV_WR_RDMA_WRITE;
    send_request.send_flags = IBV_SEND_SIGNALED;
    send_request.sg_list = &sge;
    send_request.num_sge = 1;
    send_request.next = NULL;
    send_request.wr_id = (uintptr_t)work_ctx;

    struct ibv_send_wr* bad_wr;

    ASSERT_ZERO(ibv_post_send(
        conn->rdma_socket->qp, &send_request, &bad_wr));
}

inline
void RDMAServerPrototype::build_conn_param(struct rdma_conn_param* param) {
    // See man 3 rdma_accept for more details about rdma_conn_param.
    memset(param, 0, sizeof(*param));
    param->initiator_depth = param->responder_resources = 1;
    param->rnr_retry_count = 7;  // Infinite retry.
}
