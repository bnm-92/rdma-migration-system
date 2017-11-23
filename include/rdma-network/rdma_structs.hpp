// #ifndef __RDMA_STRUCTS_HPP__
// #define __RDMA_STRUCTS_HPP__

// #include <map>
// #include "miscutils.hpp"

// // This holds all of the resources that we create that are tightly coupled
// // to the RDMA device we are using for all sockets on an RDMA server.
// //
// // This struct is just for cleanliness so it's clear what the resources are
// // that are "created by" the RDMA socket.
// // However, we also do this because we may also eventually pass a context
// // pointer around.
// //
// // For further details on any terms you're unfamiliar with here, see this link:
// // https://blog.zhaw.ch/icclab/infiniband-an-introduction-simple-ib-verbs-program-with-rdma-write/
// //
// // Note: In the tutorial, this is split into a global static `context` struct
// // as well as a local `connection` struct that they also call a "context".
// // The tutorial uses the word "context" to refer to at least three
// // completely different things, so in my code I will try to avoid using it
// // whenever an alternative sensible word exists.
// struct device_resources {
//     // ibv_context holds information about the RDMA device itself.
//     struct ibv_context* device_context;

//     // The protection domain. A struct you use to register memory with RDMA.
//     struct ibv_pd* protection_domain;

//     // The completion queue, for completions from ALL send+receive queue pairs.
//     struct ibv_cq* completion_queue;

//     // The completion channel. A notification channel for the completion queue.
//     struct ibv_comp_channel* completion_channel;

//     // The thread that is polling on the completion channel.
//     std::thread cq_poller_thread;

//     // TODO: we may wish to allow the user to register memory regions outside
//     // of specific connections as well (i.e. usable by any connection on this
//     // server). To do that, we would keep a registrations map here too.
// };

// // Information about a memory region on a remote server.
// // Basically just a tuple (addr, length, rkey).
// struct remote_region {
//     void* addr;
//     size_t length;
//     uint32_t rkey;
// };

// // This holds all of the information and resources pertaining to a single RDMA
// // connection. Multiple connections may exist simultaneously; each of these
// // have their own socket.
// struct rdma_connection {
//     // The socket for this connection.
//     // (We don't own this, it owns us; this is an upwards reference.)
//     struct rdma_cm_id* rdma_socket;

//     // All memory regions registered with this RDMA connection.
//     // A map of address to registration information.
//     std::map<void*, struct ibv_mr*> registrations;

//     // The addresses of the buffers to use for RDMA sends and receives
//     // (not RDMA reads and writes).
//     // RDMA sends and receives will be our way of emulating TCP-style
//     // functionality within RDMA. We want the results of these to get exposed
//     // to the user of this object in a way similar to send() and recv()
//     // on a TCP socket.
//     // (We'll have separate function calls to do RDMA reads and writes.)
//     void* send_buffer;
//     void* recv_buffer;

//     // Remote memory registration info.
//     std::map<void*, struct remote_region> remote_registrations;

//     // Queue of received messages.
//     ThreadsafeQueue<std::pair<void*, size_t>> recv_queue;

//     // Information about whether we're ready to disconnect.
//     // We can only call rdma_disconnect after we've both sent and received
//     // msg_done, so we'll track that here.
//     bool sent_done;
//     bool recv_done;

//     // Semaphores that user methods use for synchronization.
//     // We don't own these, but they sit here because this connection object
//     // is often the only thing available across both the event
//     // and completion threads.
//     sem_t* register_memory_sem;
//     sem_t* disconnection_sem;
// };

// // This is a struct that we can attach to a work request and it will be
// // attached to the corresponding work completion that is provided back to us.
// struct work_context {
//     // The connection under which this work request happened.
//     struct rdma_connection* conn;
//     // Optionally, an address of the local memory region associated with this
//     // work request. (What this actually is depends on the context.)
//     void* addr;
//     // An optional semaphore to smash when the work request is completed.
//     sem_t* sem;
// };

// // This will be the standard top-level struct we pass around in RDMA sends
// // and receives. As such, it should be properly abstracted to handle both
// // RDMA server state coordination logic as well as application logic.
// //
// // ANOTHER THING TO KEEP IN MIND: it's almost certainly best to use
// // size-standardized types in this struct so that each field is the same size
// // across machines. Think about this later.
// struct rdma_message {
//     static const int MAX_DATA_SIZE = 1024 * 4; // 4 KB. Arbitrary.
//     // Specifies what kind of message this is to the RDMA server.
//     enum class MessageType{
//         MSG_USER,
//             // This message is for the user.
//             // The data field and data_size will be populated.
//         MSG_MEMINFO,
//             // This message is to communicate memory region information.
//             // region_info will be populated.
//         MSG_ACK_MEMINFO,
//             // An ack reply for the above.
//             // Using an ack allows us to guarantee the postcondition that
//             // once the owner has finished calling register_memory,
//             // the remote side is actually ready to conduct RDMA operations.
//         MSG_DONE,
//             // Signals that we want to disconnect.
//             // No fields will be populated.
//          MSG_PREPARE,
//             //used to signal a server to prepare to accept a partition
//             //similar to MSG_MEMINFO except it will illicit a registration and a accept response
//         MSG_ACCEPT,
//             //used for servers to indicate that they are willing to accept the partition source for as their own
//             //after they have registered the required memory
//         MSG_TRANSFER,
//             //message is sent once the source server reliquishes its command over the partition
//             //a reply is not required as we are using RDMA_RC connections
//     } message_type;

//     // See MessageType for details.
//     struct remote_region region_info;
//     char data[MAX_DATA_SIZE];
//     size_t data_size;
// };

// #endif
