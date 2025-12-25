/*
=============================================================================
Network Thread System Implementation

Dedicated networking with lock-free message queues for high-performance
low-latency network processing.
=============================================================================
*/

#include "net_threads.h"
#include "qcommon.h"
#include <string.h>

// Global network thread system instance
net_thread_system_t net_thread_system = {0};

/*
=============================================================================
Network Thread Worker Functions
=============================================================================
*/

// Priority comparison for work items (higher priority = lower number)
static int NetWorkItemCompare(const void* a, const void* b) {
    const net_work_item_t* itemA = (const net_work_item_t*)a;
    const net_work_item_t* itemB = (const net_work_item_t*)b;

    // First compare by priority
    if (itemA->priority != itemB->priority) {
        return itemA->priority - itemB->priority;
    }

    // Then by sequence number for ordering within priority
    return (int)(itemA->sequence - itemB->sequence);
}

// Main network thread worker function
static THREAD_RETURN THREAD_CALL NetThreadWorker(void* arg) {
    net_thread_data_t* thread = (net_thread_data_t*)arg;

    // Set thread affinity for network processing (prefer specific cores)
    Thread_SetCurrentAffinity(1ULL << (thread->thread_type % Sys_GetCPUCount()));

    while (!thread->should_exit) {
        // Wait for work with timeout
        MUTEX_LOCK(thread->work_mutex);
        while (LF_Queue_IsEmpty(&thread->work_queue) && !thread->should_exit) {
            // Wait with 1ms timeout for low latency
            struct timespec timeout;
            timeout.tv_sec = 0;
            timeout.tv_nsec = 1000000; // 1ms

            CONDITION_TIMED_WAIT(thread->work_available, thread->work_mutex, &timeout);
        }
        MUTEX_UNLOCK(thread->work_mutex);

        if (thread->should_exit) break;

        // Process work items (drain queue)
        while (!LF_Queue_IsEmpty(&thread->work_queue)) {
            net_work_item_t* work = (net_work_item_t*)LF_Queue_Dequeue(&thread->work_queue);
            if (!work) continue;

            // Execute work with timing
            uint64_t start_time = Sys_Milliseconds() * 1000000ULL;
            work->work_function(work->work_data);
            uint64_t end_time = Sys_Milliseconds() * 1000000ULL;

            // Update statistics
            thread->total_work_items_processed++;
            uint64_t execution_time = end_time - start_time;
            thread->total_execution_time_ns += execution_time;
            thread->average_work_time_ms = (float)thread->total_execution_time_ns /
                                         (float)thread->total_work_items_processed / 1000000.0f;
            thread->last_activity_time = end_time;
        }

        // Yield to prevent busy-waiting
        Thread_Yield();
    }

    return 0;
}

/*
=============================================================================
Network Processing Functions
=============================================================================
*/

typedef struct {
    netadr_t address;
    msg_t message;
    int flags;
} send_work_data_t;

void NetThread_ProcessSend(void* data) {
    send_work_data_t* send_data = (send_work_data_t*)data;

    // Send the message using the network system
    NET_SendPacket(send_data->flags, send_data->message.cursize, send_data->message.data, &send_data->address);

    // Update statistics
    atomic_fetch_add_explicit(&net_thread_system.total_messages_sent, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&net_thread_system.total_bytes_sent, send_data->message.cursize, memory_order_relaxed);
}

typedef struct {
    netadr_t address;
    msg_t message;
} recv_work_data_t;

void NetThread_ProcessReceive(void* data) {
    recv_work_data_t* recv_data = (const recv_work_data_t*)data;

    // Process received message
    // This would integrate with the existing network message parsing
    // For now, just update statistics
    atomic_fetch_add_explicit(&net_thread_system.total_messages_received, 1, memory_order_relaxed);
    atomic_fetch_add_explicit(&net_thread_system.total_bytes_received, recv_data->message.cursize, memory_order_relaxed);

    // Forward to appropriate handler based on message type
    // This would call into the client/server network processing
}

typedef struct {
    netadr_t address;
    msg_t message;
    int sequence;
} reliable_work_data_t;

void NetThread_ProcessReliable(void* data) {
    reliable_work_data_t* reliable_data = (reliable_work_data_t*)data;

    // Handle reliable message delivery and acknowledgments
    // This would implement the reliable message protocol
    // For now, just pass through to regular processing
    NetThread_ProcessReceive(data);
}

typedef struct {
    msg_t* fragments;
    int fragment_count;
    int total_size;
    netadr_t address;
} fragment_work_data_t;

void NetThread_ProcessFragment(void* data) {
    fragment_work_data_t* frag_data = (fragment_work_data_t*)data;

    // Reassemble message fragments
    // This would implement message fragmentation/reassembly
    msg_t reassembled_msg;
    byte reassembled_data[MAX_MSGLEN];

    MSG_Init(&reassembled_msg, reassembled_data, sizeof(reassembled_data));

    // Reassemble logic here
    for (int i = 0; i < frag_data->fragment_count; i++) {
        // Copy fragment data
        if (reassembled_msg.cursize + frag_data->fragments[i].cursize > reassembled_msg.maxsize) {
            atomic_fetch_add_explicit(&net_thread_system.dropped_messages, 1, memory_order_relaxed);
            return;
        }
        Com_Memcpy(reassembled_msg.data + reassembled_msg.cursize,
                  frag_data->fragments[i].data,
                  frag_data->fragments[i].cursize);
        reassembled_msg.cursize += frag_data->fragments[i].cursize;
    }

    // Process reassembled message
    recv_work_data_t recv_data = { frag_data->address, reassembled_msg };
    NetThread_ProcessReceive(&recv_data);

    atomic_fetch_add_explicit(&net_thread_system.fragmented_messages, 1, memory_order_relaxed);
}

typedef struct {
    msg_t* message;
    const char* key;
    qboolean encrypt;
} encrypt_work_data_t;

void NetThread_ProcessEncrypt(void* data) {
    encrypt_work_data_t* encrypt_data = (encrypt_work_data_t*)data;

    // Handle message encryption/decryption
    // This would implement secure message transmission
    if (encrypt_data->encrypt) {
        // Encrypt message
        // Implementation would use appropriate encryption algorithm
    } else {
        // Decrypt message
        // Implementation would use appropriate decryption algorithm
    }

    atomic_fetch_add_explicit(&net_thread_system.encrypted_messages, 1, memory_order_relaxed);
}

typedef struct {
    msg_t* message;
    qboolean compress;
} compress_work_data_t;

void NetThread_ProcessCompress(void* data) {
    compress_work_data_t* compress_data = (compress_work_data_t*)data;

    // Handle message compression/decompression
    // This would implement message compression for bandwidth optimization
    if (compress_data->compress) {
        // Compress message
        // Implementation would use compression algorithm (LZ, etc.)
    } else {
        // Decompress message
        // Implementation would use decompression algorithm
    }

    atomic_fetch_add_explicit(&net_thread_system.compressed_messages, 1, memory_order_relaxed);
}

/*
=============================================================================
Network Thread System API
=============================================================================
*/

qboolean NetThread_Init(void) {
    if (net_thread_system.initialized) {
        return qtrue;
    }

    memset(&net_thread_system, 0, sizeof(net_thread_system_t));

    // Initialize atomic counters
    atomic_init(&net_thread_system.total_messages_sent, 0);
    atomic_init(&net_thread_system.total_messages_received, 0);
    atomic_init(&net_thread_system.total_bytes_sent, 0);
    atomic_init(&net_thread_system.total_bytes_received, 0);
    atomic_init(&net_thread_system.dropped_messages, 0);
    atomic_init(&net_thread_system.fragmented_messages, 0);
    atomic_init(&net_thread_system.compressed_messages, 0);
    atomic_init(&net_thread_system.encrypted_messages, 0);

    // Initialize lock-free queues
    if (!LF_Queue_Init(&net_thread_system.send_queue, 1024)) {
        Com_Printf("Failed to initialize send queue\n");
        return qfalse;
    }

    if (!LF_Queue_Init(&net_thread_system.recv_queue, 1024)) {
        Com_Printf("Failed to initialize receive queue\n");
        LF_Queue_Shutdown(&net_thread_system.send_queue);
        return qfalse;
    }

    if (!LF_Queue_Init(&net_thread_system.reliable_queue, 512)) {
        Com_Printf("Failed to initialize reliable queue\n");
        LF_Queue_Shutdown(&net_thread_system.send_queue);
        LF_Queue_Shutdown(&net_thread_system.recv_queue);
        return qfalse;
    }

    // Initialize all threads as disabled
    for (int i = 0; i < NET_THREAD_MAX; i++) {
        net_thread_system.thread_enabled[i] = qfalse;
    }

    net_thread_system.enabled = qtrue;
    net_thread_system.initialized = qtrue;

    Com_Printf("Network thread system initialized\n");
    return qtrue;
}

void NetThread_Shutdown(void) {
    if (!net_thread_system.initialized) {
        return;
    }

    // Disable all threads
    for (int i = 0; i < NET_THREAD_MAX; i++) {
        NetThread_DisableThread((net_thread_type_t)i);
    }

    // Shutdown queues
    LF_Queue_Shutdown(&net_thread_system.send_queue);
    LF_Queue_Shutdown(&net_thread_system.recv_queue);
    LF_Queue_Shutdown(&net_thread_system.reliable_queue);

    net_thread_system.initialized = qfalse;
    Com_Printf("Network thread system shutdown\n");
}

qboolean NetThread_EnableThread(net_thread_type_t threadType) {
    if (threadType >= NET_THREAD_MAX || !net_thread_system.initialized) {
        return qfalse;
    }

    if (net_thread_system.thread_enabled[threadType]) {
        return qtrue; // Already enabled
    }

    net_thread_data_t* thread = &net_thread_system.threads[threadType];
    memset(thread, 0, sizeof(net_thread_data_t));

    thread->thread_type = threadType;
    thread->should_exit = qfalse;

    // Initialize lock-free work queue for this thread
    if (!LF_Queue_Init(&thread->work_queue, 256)) {
        Com_Printf("Failed to initialize work queue for thread %d\n", threadType);
        return qfalse;
    }

    // Initialize synchronization primitives
    MUTEX_INIT(thread->work_mutex);
    CONDITION_INIT(thread->work_available);

    // Start thread with high priority for network processing
    const char* threadNames[NET_THREAD_MAX] = {
        "NetSend", "NetRecv", "NetReliable", "NetFragment", "NetEncrypt", "NetCompress"
    };

    if (!Thread_Create(&thread->handle, NetThreadWorker, thread, threadNames[threadType],
                      THREAD_PRIORITY_HIGH)) {
        Com_Printf("Failed to create network thread %d\n", threadType);
        LF_Queue_Shutdown(&thread->work_queue);
        return qfalse;
    }

    net_thread_system.thread_enabled[threadType] = qtrue;

    Com_Printf("Enabled network thread: %s\n", threadNames[threadType]);
    return qtrue;
}

void NetThread_DisableThread(net_thread_type_t threadType) {
    if (threadType >= NET_THREAD_MAX ||
        !net_thread_system.thread_enabled[threadType]) {
        return;
    }

    net_thread_data_t* thread = &net_thread_system.threads[threadType];

    // Signal thread to exit
    MUTEX_LOCK(thread->work_mutex);
    thread->should_exit = qtrue;
    CONDITION_SIGNAL(thread->work_available);
    MUTEX_UNLOCK(thread->work_mutex);

    // Wait for thread to finish
    Thread_Join(thread->handle);

    // Cleanup resources
    LF_Queue_Shutdown(&thread->work_queue);

    net_thread_system.thread_enabled[threadType] = qfalse;

    const char* threadNames[NET_THREAD_MAX] = {
        "NetSend", "NetRecv", "NetReliable", "NetFragment", "NetEncrypt", "NetCompress"
    };
    Com_Printf("Disabled network thread: %s\n", threadNames[threadType]);
}

qboolean NetThread_IsThreadEnabled(net_thread_type_t threadType) {
    if (threadType >= NET_THREAD_MAX) return qfalse;
    return net_thread_system.thread_enabled[threadType];
}

/*
=============================================================================
Message Queuing Functions
=============================================================================
*/

qboolean NetThread_QueueSendMessage(const netadr_t* address, const msg_t* message, int flags) {
    if (!net_thread_system.initialized) {
        return qfalse;
    }

    send_work_data_t* send_data = (send_work_data_t*)malloc(sizeof(send_work_data_t));
    if (!send_data) return qfalse;

    send_data->address = *address;
    MSG_Copy(&send_data->message, send_data->message.data, sizeof(send_data->message.data), message);
    send_data->flags = flags;

    if (!LF_Queue_Enqueue(&net_thread_system.send_queue, send_data)) {
        free(send_data);
        atomic_fetch_add_explicit(&net_thread_system.dropped_messages, 1, memory_order_relaxed);
        return qfalse;
    }

    return qtrue;
}

qboolean NetThread_QueueRecvMessage(const netadr_t* address, const msg_t* message) {
    if (!net_thread_system.initialized) {
        return qfalse;
    }

    recv_work_data_t* recv_data = (recv_work_data_t*)malloc(sizeof(recv_work_data_t));
    if (!recv_data) return qfalse;

    recv_data->address = *address;
    MSG_Copy(&recv_data->message, recv_data->message.data, sizeof(recv_data->message.data), message);

    if (!LF_Queue_Enqueue(&net_thread_system.recv_queue, recv_data)) {
        free(recv_data);
        atomic_fetch_add_explicit(&net_thread_system.dropped_messages, 1, memory_order_relaxed);
        return qfalse;
    }

    return qtrue;
}

qboolean NetThread_QueueReliableMessage(const netadr_t* address, const msg_t* message) {
    if (!net_thread_system.initialized) {
        return qfalse;
    }

    reliable_work_data_t* reliable_data = (reliable_work_data_t*)malloc(sizeof(reliable_work_data_t));
    if (!reliable_data) return qfalse;

    reliable_data->address = *address;
    MSG_Copy(&reliable_data->message, reliable_data->message.data, sizeof(reliable_data->message.data), message);
    reliable_data->sequence = 0; // Would be set by caller

    if (!LF_Queue_Enqueue(&net_thread_system.reliable_queue, reliable_data)) {
        free(reliable_data);
        atomic_fetch_add_explicit(&net_thread_system.dropped_messages, 1, memory_order_relaxed);
        return qfalse;
    }

    return qtrue;
}

/*
=============================================================================
Work Submission Functions
=============================================================================
*/

static void SubmitNetWork(net_thread_type_t threadType, void* workData,
                         void (*workFunction)(void*), int priority) {
    if (threadType >= NET_THREAD_MAX ||
        !net_thread_system.thread_enabled[threadType]) {
        // Execute immediately if thread not available
        workFunction(workData);
        return;
    }

    net_thread_data_t* thread = &net_thread_system.threads[threadType];

    // Create work item
    net_work_item_t* workItem = (net_work_item_t*)malloc(sizeof(net_work_item_t));
    if (!workItem) {
        workFunction(workData); // Fallback
        return;
    }

    workItem->work_function = workFunction;
    workItem->work_data = workData;
    workItem->submit_time = Sys_Milliseconds() * 1000000ULL;
    workItem->thread_type = threadType;
    workItem->priority = priority;
    workItem->sequence = 0; // Could be incremented atomically

    // Add to queue
    if (!LF_Queue_Enqueue(&thread->work_queue, workItem)) {
        // Queue full - execute immediately as fallback
        free(workItem);
        workFunction(workData);
        return;
    }

    // Signal thread
    MUTEX_LOCK(thread->work_mutex);
    CONDITION_SIGNAL(thread->work_available);
    MUTEX_UNLOCK(thread->work_mutex);
}

void NetThread_SubmitSendWork(void* workData) {
    SubmitNetWork(NET_THREAD_SEND, workData, NetThread_ProcessSend, 0);
}

void NetThread_SubmitRecvWork(void* workData) {
    SubmitNetWork(NET_THREAD_RECV, workData, NetThread_ProcessReceive, 0);
}

void NetThread_SubmitReliableWork(void* workData) {
    SubmitNetWork(NET_THREAD_RELIABLE, workData, NetThread_ProcessReliable, 1);
}

void NetThread_SubmitFragmentWork(void* workData) {
    SubmitNetWork(NET_THREAD_FRAGMENT, workData, NetThread_ProcessFragment, 2);
}

void NetThread_SubmitEncryptWork(void* workData) {
    SubmitNetWork(NET_THREAD_ENCRYPT, workData, NetThread_ProcessEncrypt, 1);
}

void NetThread_SubmitCompressWork(void* workData) {
    SubmitNetWork(NET_THREAD_COMPRESS, workData, NetThread_ProcessCompress, 1);
}

/*
=============================================================================
Synchronization Functions
=============================================================================
*/

void NetThread_WaitForAllThreads(void) {
    for (int i = 0; i < NET_THREAD_MAX; i++) {
        if (net_thread_system.thread_enabled[i]) {
            NetThread_WaitForThread((net_thread_type_t)i);
        }
    }
}

void NetThread_WaitForThread(net_thread_type_t threadType) {
    if (threadType >= NET_THREAD_MAX ||
        !net_thread_system.thread_enabled[threadType]) {
        return;
    }

    net_thread_data_t* thread = &net_thread_system.threads[threadType];

    // Wait until work queue is empty
    uint64_t start_wait = Sys_Milliseconds() * 1000000ULL;
    while (!LF_Queue_IsEmpty(&thread->work_queue)) {
        // Timeout after 100ms to prevent infinite waiting
        if ((Sys_Milliseconds() * 1000000ULL - start_wait) > 100000000ULL) {
            Com_Printf("Network thread %d wait timeout\n", threadType);
            break;
        }
        Thread_Sleep(1);
    }
}

void NetThread_FlushQueues(void) {
    // Process all queued messages immediately
    void* item;

    while ((item = LF_Queue_Dequeue(&net_thread_system.send_queue)) != NULL) {
        send_work_data_t* send_data = (send_work_data_t*)item;
        NetThread_ProcessSend(send_data);
        free(send_data);
    }

    while ((item = LF_Queue_Dequeue(&net_thread_system.recv_queue)) != NULL) {
        recv_work_data_t* recv_data = (recv_work_data_t*)item;
        NetThread_ProcessReceive(recv_data);
        free(recv_data);
    }

    while ((item = LF_Queue_Dequeue(&net_thread_system.reliable_queue)) != NULL) {
        reliable_work_data_t* reliable_data = (reliable_work_data_t*)item;
        NetThread_ProcessReliable(reliable_data);
        free(reliable_data);
    }
}

/*
=============================================================================
Performance Monitoring
=============================================================================
*/

void NetThread_GetStats(net_thread_type_t threadType,
                        uint64_t* processedItems,
                        float* avgTimeMs,
                        uint64_t* totalTimeNs) {
    if (threadType >= NET_THREAD_MAX ||
        !net_thread_system.thread_enabled[threadType]) {
        if (processedItems) *processedItems = 0;
        if (avgTimeMs) *avgTimeMs = 0.0f;
        if (totalTimeNs) *totalTimeNs = 0;
        return;
    }

    net_thread_data_t* thread = &net_thread_system.threads[threadType];
    if (processedItems) *processedItems = thread->total_work_items_processed;
    if (avgTimeMs) *avgTimeMs = thread->average_work_time_ms;
    if (totalTimeNs) *totalTimeNs = thread->total_execution_time_ns;
}

void NetThread_GetGlobalStats(uint64_t* sent, uint64_t* received,
                             uint64_t* sentBytes, uint64_t* recvBytes,
                             uint64_t* dropped) {
    if (sent) *sent = atomic_load_explicit(&net_thread_system.total_messages_sent, memory_order_relaxed);
    if (received) *received = atomic_load_explicit(&net_thread_system.total_messages_received, memory_order_relaxed);
    if (sentBytes) *sentBytes = atomic_load_explicit(&net_thread_system.total_bytes_sent, memory_order_relaxed);
    if (recvBytes) *recvBytes = atomic_load_explicit(&net_thread_system.total_bytes_received, memory_order_relaxed);
    if (dropped) *dropped = atomic_load_explicit(&net_thread_system.dropped_messages, memory_order_relaxed);
}
