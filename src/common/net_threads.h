/*
=============================================================================
Network Thread System

Dedicated networking with lock-free message queues for high-performance
low-latency network processing.
=============================================================================
*/

#ifndef __NET_THREADS_H__
#define __NET_THREADS_H__

#include "q_shared.h"
#include "q_lockfree.h"
#include "thread_platform.h"

// Network thread types
typedef enum {
    NET_THREAD_SEND = 0,    // Outgoing message processing and sending
    NET_THREAD_RECV,        // Incoming message processing and parsing
    NET_THREAD_RELIABLE,    // Reliable message handling and acknowledgments
    NET_THREAD_FRAGMENT,    // Message fragmentation and reassembly
    NET_THREAD_ENCRYPT,     // Message encryption/decryption
    NET_THREAD_COMPRESS,    // Message compression/decompression
    NET_THREAD_MAX
} net_thread_type_t;

// Network work item
typedef struct {
    void (*work_function)(void* data);
    void* work_data;
    uint64_t submit_time;
    net_thread_type_t thread_type;
    int priority; // 0 = highest, higher numbers = lower priority
    uint32_t sequence; // For ordering within priority levels
} net_work_item_t;

// Network message queue entry
typedef struct {
    netadr_t address;
    msg_t message;
    int flags;
    uint64_t timestamp;
    int retry_count;
} net_message_t;

// Network thread data
typedef struct {
    net_thread_type_t thread_type;
    thread_handle_t handle;
    condition_t work_available;
    mutex_t work_mutex;
    spinlock_t queue_lock;
    qboolean should_exit;

    // Work queue (lock-free)
    lf_queue_t work_queue;

    // Thread-specific data
    void* thread_context;
    uint64_t total_work_items_processed;
    uint64_t total_execution_time_ns;
    float average_work_time_ms;
    uint64_t last_activity_time;
} net_thread_data_t;

// Network thread system
typedef struct {
    qboolean enabled;
    qboolean initialized;

    // Thread management
    net_thread_data_t threads[NET_THREAD_MAX];
    qboolean thread_enabled[NET_THREAD_MAX];

    // Message queues (lock-free)
    lf_queue_t send_queue;
    lf_queue_t recv_queue;
    lf_queue_t reliable_queue;

    // Statistics
    atomic_uint64_t total_messages_sent;
    atomic_uint64_t total_messages_received;
    atomic_uint64_t total_bytes_sent;
    atomic_uint64_t total_bytes_received;
    atomic_uint64_t dropped_messages;
    atomic_uint64_t fragmented_messages;
    atomic_uint64_t compressed_messages;
    atomic_uint64_t encrypted_messages;

    // Performance tracking
    uint64_t frame_start_time;
    float average_frame_time_ms;
    uint32_t max_queue_depth;
} net_thread_system_t;

extern net_thread_system_t net_thread_system;

// Network Thread API
qboolean NetThread_Init(void);
void NetThread_Shutdown(void);

qboolean NetThread_EnableThread(net_thread_type_t threadType);
void NetThread_DisableThread(net_thread_type_t threadType);
qboolean NetThread_IsThreadEnabled(net_thread_type_t threadType);

// Message queuing (lock-free)
qboolean NetThread_QueueSendMessage(const netadr_t* address, const msg_t* message, int flags);
qboolean NetThread_QueueRecvMessage(const netadr_t* address, const msg_t* message);
qboolean NetThread_QueueReliableMessage(const netadr_t* address, const msg_t* message);

// Work submission
void NetThread_SubmitSendWork(void* workData);
void NetThread_SubmitRecvWork(void* workData);
void NetThread_SubmitReliableWork(void* workData);
void NetThread_SubmitFragmentWork(void* workData);
void NetThread_SubmitEncryptWork(void* workData);
void NetThread_SubmitCompressWork(void* workData);

// Synchronization
void NetThread_WaitForAllThreads(void);
void NetThread_WaitForThread(net_thread_type_t threadType);
void NetThread_FlushQueues(void);

// Performance monitoring
void NetThread_GetStats(net_thread_type_t threadType,
                        uint64_t* processedItems,
                        float* avgTimeMs,
                        uint64_t* totalTimeNs);
void NetThread_GetGlobalStats(uint64_t* sent, uint64_t* received,
                             uint64_t* sentBytes, uint64_t* recvBytes,
                             uint64_t* dropped);

// Network processing functions (called by work items)
void NetThread_ProcessSend(void* data);
void NetThread_ProcessReceive(void* data);
void NetThread_ProcessReliable(void* data);
void NetThread_ProcessFragment(void* data);
void NetThread_ProcessEncrypt(void* data);
void NetThread_ProcessCompress(void* data);

#endif // __NET_THREADS_H__
