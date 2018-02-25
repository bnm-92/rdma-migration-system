#ifndef __RDMA_SERVER_PROTOTYPE_HPP__
#define __RDMA_SERVER_PROTOTYPE_HPP__

#include <rdma/rdma_cma.h>
#include <semaphore.h>

#include <map>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <utility>
#include "utils/miscutils.hpp"

#include "rdma-network/util.hpp"

/*
This file contains definitions and code for the RDMA communication interface
that this library provides via the RDMAServer and RDMAClient.

This file also contains internal logic common to both the RDMAServer
and RDMAClient.

See the public methods for the communication interface available for use.
*/

// An abstract class that handles communication logic and boilerplate
// common to both the server and client sides of RDMA connections,
// Encapsulates all the non-socket resources that an RDMA server manages.
class RDMAServerPrototype {
public:
    // Register local memory for use with all future RDMA operations on this
    // connection. RDMA operations on your memory, both locally and remotely
    // driven ones, can only be conducted on regions that have been registered.
    //
    // Specifically, this will:
    // - Register the memory with RDMA library, which will pin the memory
    // - Register the memory with your side of the connection
    // - If remote_access is set, allow the remote side of this connection
    //   to RDMA operations on this region of memory.
    //
    // NB: Memory is automatically deregistered upon closure of a connection.
    //
    // TODO: It might be useful if we could register memory for use with any
    // connection. This would probably be implemented as a separate method in
    // the RDMAServer subclass, and would involve
    //  - keeping a list of such registrations within the subclass
    //  - upon accepting a new connection, the RDMAServer calls send_meminfo
    //    on every registration in the list before returning
    // Not implementing this right now for lack of time.
    void register_memory(uintptr_t conn_id, void* addr, size_t len, bool remote_access);

    /*
        does the same thing as method register_memory without sending the info/rkey 
        to the remote machine while still making it remotely accesible
    */
    void register_memory(
        uintptr_t conn_id, void* addr, size_t len);


    void deregister_memory(uintptr_t conn_id, void* addr);
    // Sends arbitrary data to the remote side.
    // The other side can receive this with receive().
    //
    // NOTE: Memory addresses of data are not preserved! Use other RDMA
    // operations for finer control.
    // NOTE: There is a current limit to message size (see MAX_DATA_SIZE).
    // FYI: This is a big fat wrapper around an underlying IBV_WR_SEND.
    //
    // conn_id:
    //   the id of this connection, as returned by the method you called
    //   to establish it (connect() or accept()).
    // msg_buffer:
    //   The buffer storing the message you want to send.
    //   This can be any address. Does NOT have to be registered with RDMA.
    // len:
    //   The size of the message, in bytes.
    void send(uintptr_t conn_id, const void* msg_buffer, size_t len);

    void send_prepare(uintptr_t conn_id, void* addr, size_t len);

    #if FAULT_TOLERANT
    void send_prepare(uintptr_t conn_id, void* addr, size_t len, char* client_id, size_t client_id_size);
    void getPartitionList(uintptr_t conn_id);
    void sendPartitionList(uintptr_t conn_id, std::string str);
    #endif

    void send_accept(uintptr_t conn_id, void* addr, size_t len);
    
    void send_decline(uintptr_t conn_id, void* addr, size_t len);

    void send_transfer(uintptr_t conn_id, void* addr, size_t len);
    
    void send_close(uintptr_t conn_id, void* addr, size_t len);
    // Receive a send() on a connection, as identified by the connection ID.
    // Returns the memory address and the size of the message.
    // The caller is responsible for deallocating this memory when they
    // are done with the message.
    std::pair<void*, size_t> receive(uintptr_t conn_id);

    // Reads `len` bytes of memory from the remote server of this connection
    // at remote_addr on that server, and puts it in local_addr here.
    //
    // Precondition: you have called register_meomry on a region that includes
    // local_addr and length, and ditto for the remote server and remote_addr.
    int rdma_read(uintptr_t conn_id, void* local_addr, void* remote_addr, size_t len);
    void rdma_read_async(uintptr_t conn_id, void* local_addr, void* remote_addr, size_t len,void (*callback)(void*), void* data);

    void rdma_write(uintptr_t conn_id, void* local_addr, void* remote_addr, size_t len);

    // Call this when you are finished with this connection.
    // Close the connection indicated by the connection ID.
    // Deregisters all local memory registered with this connection.
    // Blocks until the connection has successfully closed.
    void done(uintptr_t conn_id);

    // Gracefully shut down the server entirely.
    // Precondition: ??? (to design)
    // (Right now we're only implementing this for the client,
    // but this single implementation should work for both client
    // and server.)
    void destroy();


    //to check if there is a pending message on the recv queue
    bool checkForMessage(uintptr_t conn_id);
// Protected internal fields.
protected:
    // An event channel, which should be bound to get events from ALL sockets
    // owned by this RDMA server.
    struct rdma_event_channel* event_channel;

    // This struct will hold all of the resources we create for this device's
    // operation.
    // We assume for now that only one RDMA device will be used by this server
    // (the tutorial isn't clear on this, but manual testing seems to show that
    // this is indeed true.)
    struct device_resources* resources;

    // The thread running the event loop.
    // Some information about the threads and work flow
    // Most work is done in the user thread. When the user calls send(), for
    // instance, the user thread enters this object and does the work required,
    // culminating in calls to the rdma_cm library.
    // Then the user thread blocks and waits for the work to be done.
    //
    // This event_thread is the "main background thread" of this RDMA server,
    // which responds to server connection and disconnection events.
    // You can think of it as being responsible for creating and
    // destroying connections.
    //
    // There is a second thread which handles work completions, which lives
    // under the device resources object, and responds to work completions on
    // all connections. This is responsible for handling the completion
    // and notifying the user thread if necessary.
    std::thread event_thread;

    // A list of all active connections.
    std::unordered_set<struct rdma_connection*> connections;

    // User interface methods are NOT threadsafe. Additionally, due to the
    // implicit state kept by the event-driven nature of the RDMA_CM library,
    // this library is difficult to make threadsafe.
    // So we'll use a mutex to enforce that only one user thread is calling
    // user interface methods at any given time.
    std::mutex user_mutex;

    // Whether the server has already been started or not.
    bool started;
    bool run;
    

    // Whether we should spin down the server as soon as the last connection
    // is finished.
    // (TODO: we should also reject incoming connections if this is true.)
    bool stop_after_last_conn;

// Protected internal methods.
protected:
    // Constructor and destructor.
    // This is an abstract class, so we should only instantiate subclasses.
    RDMAServerPrototype();
    virtual ~RDMAServerPrototype();


    // Methods for the event loop thread.
    // (See event_thread, as well as implementations, for details.)
    void event_loop();
    int on_event(struct rdma_cm_event*);
    virtual int on_addr_resolved(struct rdma_cm_id*);
    virtual int on_route_resolved(struct rdma_cm_id*);
    virtual int on_connect_request(struct rdma_cm_id*);
    virtual int on_connection(struct rdma_cm_id*);
    virtual int on_disconnect(struct rdma_cm_id*);


    // Methods for each connection thread.
    // Boilerplate for polling the completion channel.
    void* poll_cq();
    // Handles completions by updating connection state accordingly,
    // which coordinates synchronization with the user of the connection.
    // See implementation for details.
    // Postcondition: the work context is deallocated and no longer usable.
    void on_completion(struct ibv_wc*);
    // The on_completion handler handles signalling the default semaphore
    // attached to the work context and cleaning up the work context.
    // Everything else should be done in these specific handlers.
    virtual void on_recv_finish(struct ibv_wc*);
    virtual void on_send_finish(struct ibv_wc*);
    virtual void on_rdma_read_finish(struct ibv_wc*);
    virtual void on_rdma_write_finish(struct ibv_wc*);
    virtual void on_comp_swap_finish(struct ibv_wc*);
    virtual void on_fetch_add_finish(struct ibv_wc*);
    virtual void on_bind_mw_finish(struct ibv_wc*);
    virtual void on_imm_recv_finish(struct ibv_wc*);


    // Resource constructors.

    // This builds the connection resources for the RDMA device we're using,
    // as identified by an RDMA device context (the ibv_context passed in).
    // Should only ever be called once, as soon as the device context is
    // available to us (differs between client and server).
    //
    // Note: This is called build_context in the tutorial.
    void build_resources_for_device(struct ibv_context*);

    // Builds a queue pair for an RDMA socket.
    void build_queue_pair(struct rdma_cm_id*);

    // Builds a connection object from a newly spawned RDMA socket.
    struct rdma_connection* build_connection_object(struct rdma_cm_id*);

    // Helpers.

    // Register an address (and size in bytes) for use with RDMA,
    // on the connection as identified by the connection context.
    //
    // access_flags: the local/remote read/write permissions for this region,
    // which will be directly passed into ibv_reg_mr (so see that manual page).
    //
    // Returns a pointer to the memory region info struct for convenience
    // (although this is accessible through the registrations map
    // after this method has returned).
    struct ibv_mr* register_memory_with_conn(struct rdma_connection*, void* addr, int size, int access_flags);

    // Communicate information about a local memory registration
    // to enable the other side to perform operatons on it.
    //
    // Note -- the receiving side will currently process this
    // in on_recv_finish.
    void send_meminfo(struct rdma_connection*, struct ibv_mr*);

    // Arm the RDMA receive functionality on the given connection
    // by posting an RDMA receive. (According to the tutorial,
    // there's no buffer for RDMA sends, so to receive an RDMA send
    // you must have a receive posted.)
    void post_rdma_receive(struct rdma_connection*);

    // Post an RDMA send.
    // sem_t, if not null, will be smashed when the send is done.
    // See struct rdma_message for more details.
    // (The message will be copied out before being sent.)
    void post_rdma_send(struct rdma_connection*, struct rdma_message*, sem_t*);

    // Post an RDMA read.
    // sem_t, if not null, will be signalled when the send is done.
    void post_rdma_read(
        struct rdma_connection* conn,
        void* local_addr, uint32_t lkey,
        void* remote_addr, uint32_t rkey,
        size_t length, sem_t* sem);

    void post_rdma_read(
        struct rdma_connection* conn,
        void* local_addr, uint32_t lkey,
        void* remote_addr, uint32_t rkey,
        size_t length,
        void (*callback)(void*), void* data);

    void post_rdma_write(
        struct rdma_connection* conn,
        void* local_addr, uint32_t lkey,
        void* remote_addr, uint32_t rkey,
        size_t length, sem_t* sem);

    // Helper for creating default RDMA connection parameters.
    void build_conn_param(struct rdma_conn_param*);
};


// This holds all of the resources that we create that are tightly coupled
// to the RDMA device we are using for all sockets on an RDMA server.
//
// This struct is just for cleanliness so it's clear what the resources are
// that are "created by" the RDMA socket.
// However, we also do this because we may also eventually pass a context
// pointer around.
//
// For further details on any terms you're unfamiliar with here, see this link:
// https://blog.zhaw.ch/icclab/infiniband-an-introduction-simple-ib-verbs-program-with-rdma-write/
//
// Note: In the tutorial, this is split into a global static `context` struct
// as well as a local `connection` struct that they also call a "context".
// The tutorial uses the word "context" to refer to at least three
// completely different things, so in my code I will try to avoid using it
// whenever an alternative sensible word exists.
struct device_resources {
    // ibv_context holds information about the RDMA device itself.
    struct ibv_context* device_context;

    // The protection domain. A struct you use to register memory with RDMA.
    struct ibv_pd* protection_domain;

    // The completion queue, for completions from ALL send+receive queue pairs.
    struct ibv_cq* completion_queue;

    // The completion channel. A notification channel for the completion queue.
    struct ibv_comp_channel* completion_channel;

    // The thread that is polling on the completion channel.
    std::thread cq_poller_thread;

    // TODO: we may wish to allow the user to register memory regions outside
    // of specific connections as well (i.e. usable by any connection on this
    // server). To do that, we would keep a registrations map here too.
};

// Information about a memory region on a remote server.
// Basically just a tuple (addr, length, rkey).
struct remote_region {
    void* addr;
    size_t length;
    uint32_t rkey;
};

// This holds all of the information and resources pertaining to a single RDMA
// connection. Multiple connections may exist simultaneously; each of these
// have their own socket.
struct rdma_connection {
    // The socket for this connection.
    // (We don't own this, it owns us; this is an upwards reference.)
    struct rdma_cm_id* rdma_socket;

    // All memory regions registered with this RDMA connection.
    // A map of address to registration information.
    std::map<void*, struct ibv_mr*> registrations;

    // The addresses of the buffers to use for RDMA sends and receives
    // (not RDMA reads and writes).
    // RDMA sends and receives will be our way of emulating TCP-style
    // functionality within RDMA. We want the results of these to get exposed
    // to the user of this object in a way similar to send() and recv()
    // on a TCP socket.
    // (We'll have separate function calls to do RDMA reads and writes.)
    void* send_buffer;
    void* recv_buffer;

    // Remote memory registration info.
    std::map<void*, struct remote_region> remote_registrations;

    // Queue of received messages.
    ThreadsafeQueue<std::pair<void*, size_t>> recv_queue;

    // Information about whether we're ready to disconnect.
    // We can only call rdma_disconnect after we've both sent and received
    // msg_done, so we'll track that here.
    bool sent_done;
    bool recv_done;

    // Semaphores that user methods use for synchronization.
    // We don't own these, but they sit here because this connection object
    // is often the only thing available across both the event
    // and completion threads.
    sem_t* register_memory_sem;
    sem_t* disconnection_sem;

};

// This is a struct that we can attach to a work request and it will be
// attached to the corresponding work completion that is provided back to us.
struct work_context {
    // The connection under which this work request happened.
    struct rdma_connection* conn = NULL;
    // Optionally, an address of the local memory region associated with this
    // work request. (What this actually is depends on the context.)
    void* addr = NULL;
    // An optional semaphore to smash when the work request is completed.
    sem_t* sem = NULL;
    //functional callback
    void (*call_back)(void*) = NULL;
    void* data = NULL;
};

// This will be the standard top-level struct we pass around in RDMA sends
// and receives. As such, it should be properly abstracted to handle both
// RDMA server state coordination logic as well as application logic.
//
// ANOTHER THING TO KEEP IN MIND: it's almost certainly best to use
// size-standardized types in this struct so that each field is the same size
// across machines. Think about this later.
struct rdma_message {
    static const int MAX_DATA_SIZE = 1024 * 2; // 2 KB. Arbitrary.
    // Specifies what kind of message this is to the RDMA server.
    enum class MessageType{
        MSG_GET_PARTITIONS,
            //used to get parition list
        MSG_SENT_PARTITIONS,
            //used to send partiton list
        MSG_USER,
            // This message is for the user.
            // The data field and data_size will be populated.
        MSG_MEMINFO,
            // This message is to communicate memory region information.
            // region_info will be populated.
        MSG_ACK_MEMINFO,
            // An ack reply for the above.
            // Using an ack allows us to guarantee the postcondition that
            // once the owner has finished calling register_memory,
            // the remote side is actually ready to conduct RDMA operations.
        MSG_DONE,
            // Signals that we want to disconnect.
            // No fields will be populated.
        MSG_PREPARE,
            //used to signal a server to prepare to accept a partition
            //similar to MSG_MEMINFO except it will illicit a registration and a accept response
        MSG_ACCEPT,
            //used for servers to indicate that they are willing to accept the partition source for as their own
            //after they have registered the required memory
        MSG_DECLINE,
            //used for servers to indicate that they are not willing to take up the partition
        MSG_TRANSFER,
            //message is sent once the source server reliquishes its command over the partition
            //a reply is not required as we are using RDMA_RC connections
        MSG_DONE_TRANSFER,
    } message_type;

    // See MessageType for details.
    struct remote_region region_info;
    char data[MAX_DATA_SIZE];
    size_t data_size;
};

#include "rdma-network/rdma_server_prototype.tpp"

#endif // __RDMA_SERVER_PROTOTYPE_HPP__
