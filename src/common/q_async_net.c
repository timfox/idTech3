/*
===============================================================================
Asynchronous Network Operations Implementation

Non-blocking network I/O with callback-based completion.
===============================================================================
*/

#include "q_async_net.h"
#include "q_shared.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Platform-specific includes
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define NET_EWOULDBLOCK WSAEWOULDBLOCK
#define NET_EINPROGRESS WSAEINPROGRESS
typedef SOCKET net_socket_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#define NET_EWOULDBLOCK EWOULDBLOCK
#define NET_EINPROGRESS EINPROGRESS
typedef int net_socket_t;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

// Global async network context
async_net_context_t async_net;

// Operation ID counter
static int next_operation_id = 1;

//============================================================================
// Internal Functions
//============================================================================

static int AsyncNet_GetError(void) {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

static void AsyncNet_SetNonBlockingInternal(net_socket_t socket_fd) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(socket_fd, FIONBIO, &mode);
#else
    fcntl(socket_fd, F_SETFL, fcntl(socket_fd, F_GETFL, 0) | O_NONBLOCK);
#endif
}

static void AsyncNet_AddToQueue(async_operation_t **queue, async_operation_t *op) {
    op->next = *queue;
    if (*queue) {
        (*queue)->prev = op;
    }
    *queue = op;
    op->prev = NULL;
}

static void AsyncNet_RemoveFromQueue(async_operation_t **queue, async_operation_t *op) {
    if (op->prev) {
        op->prev->next = op->next;
    } else {
        *queue = op->next;
    }
    if (op->next) {
        op->next->prev = op->prev;
    }
    op->next = op->prev = NULL;
}

static async_operation_t *AsyncNet_FindOperation(int id) {
    async_operation_t *op;

    // Check pending queue
    for (op = async_net.pending_queue; op; op = op->next) {
        if (op->id == id) return op;
    }

    // Check completed queue
    for (op = async_net.completed_queue; op; op = op->next) {
        if (op->id == id) return op;
    }

    return NULL;
}

//============================================================================
// Public API Implementation
//============================================================================

qboolean AsyncNet_Init(int max_operations) {
    if (async_net.initialized) {
        return qtrue;
    }

    memset(&async_net, 0, sizeof(async_net));

    async_net.max_operations = max_operations;
    async_net.operation_pool = (async_operation_t *)malloc(sizeof(async_operation_t) * max_operations);

    if (!async_net.operation_pool) {
        Com_Printf("AsyncNet_Init: Failed to allocate operation pool\n");
        return qfalse;
    }

    // Initialize operation pool
    memset(async_net.operation_pool, 0, sizeof(async_operation_t) * max_operations);
    for (int i = 0; i < max_operations; i++) {
        async_net.operation_pool[i].id = -1; // Mark as free
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        Com_Printf("AsyncNet_Init: WSAStartup failed\n");
        free(async_net.operation_pool);
        return qfalse;
    }
#endif

    async_net.initialized = qtrue;
    async_net.use_threads = qfalse; // Default to non-threaded for compatibility

    Com_Printf("Async network system initialized (%d max operations)\n", max_operations);
    return qtrue;
}

void AsyncNet_Shutdown(void) {
    if (!async_net.initialized) {
        return;
    }

    // Cancel all pending operations
    async_operation_t *op, *next;
    for (op = async_net.pending_queue; op; op = next) {
        next = op->next;
        op->status = ASYNC_STATUS_CANCELLED;
        if (op->callback) {
            async_result_t result = {
                .status = ASYNC_STATUS_CANCELLED,
                .error_code = 0,
                .bytes_transferred = 0,
                .user_data = op->user_data,
                .completion_time = Sys_Milliseconds() * 0.001
            };
            op->callback(&result, op->user_data);
        }
    }

    free(async_net.operation_pool);
    memset(&async_net, 0, sizeof(async_net));

#ifdef _WIN32
    WSACleanup();
#endif

    Com_Printf("Async network system shut down\n");
}

async_operation_t *AsyncNet_CreateOperation(async_op_type_t type, const netadr_t *address) {
    if (!async_net.initialized || async_net.active_operations >= async_net.max_operations) {
        return NULL;
    }

    // Find free operation slot
    for (int i = 0; i < async_net.max_operations; i++) {
        if (async_net.operation_pool[i].id == -1) {
            async_operation_t *op = &async_net.operation_pool[i];

            memset(op, 0, sizeof(*op));
            op->id = next_operation_id++;
            op->type = type;
            op->status = ASYNC_STATUS_PENDING;
            op->start_time = Sys_Milliseconds() * 0.001;

            if (address) {
                op->address = *address;
            }

            async_net.active_operations++;
            return op;
        }
    }

    return NULL;
}

async_operation_t *AsyncNet_SendTo(const void *data, size_t size, const netadr_t *address,
                                  async_callback_t callback, void *user_data) {
    async_operation_t *op = AsyncNet_CreateOperation(ASYNC_OP_SEND, address);
    if (!op) return NULL;

    op->buffer = (void *)data;
    op->buffer_size = size;
    op->callback = callback;
    op->user_data = user_data;

    // For now, execute synchronously (can be made async with threading)
    op->status = ASYNC_STATUS_IN_PROGRESS;

    // Execute the send
    if (NET_SendPacket(NS_CLIENT, size, data, *address)) {
        op->status = ASYNC_STATUS_COMPLETED;
        op->bytes_transferred = size;
        async_net.successful_operations++;

        AsyncNet_AddToQueue(&async_net.completed_queue, op);
    } else {
        op->status = ASYNC_STATUS_FAILED;
        op->error_code = AsyncNet_GetError();
        async_net.failed_operations++;

        AsyncNet_AddToQueue(&async_net.completed_queue, op);
    }

    return op;
}

async_operation_t *AsyncNet_RecvFrom(void *buffer, size_t max_size,
                                    async_callback_t callback, void *user_data) {
    async_operation_t *op = AsyncNet_CreateOperation(ASYNC_OP_RECEIVE, NULL);
    if (!op) return NULL;

    op->buffer = buffer;
    op->buffer_size = max_size;
    op->callback = callback;
    op->user_data = user_data;

    // For now, execute synchronously
    op->status = ASYNC_STATUS_IN_PROGRESS;

    netadr_t from;
    msg_t msg;
    MSG_Init(&msg, buffer, max_size);

    if (NET_GetPacket(NS_CLIENT, &from, &msg)) {
        op->status = ASYNC_STATUS_COMPLETED;
        op->bytes_transferred = msg.cursize;
        op->address = from;
        async_net.successful_operations++;

        AsyncNet_AddToQueue(&async_net.completed_queue, op);
    } else {
        op->status = ASYNC_STATUS_FAILED;
        op->error_code = AsyncNet_GetError();

        // Don't add to completed queue for failed receives (they're normal)
        op->status = ASYNC_STATUS_PENDING; // Keep pending for retry
        AsyncNet_AddToQueue(&async_net.pending_queue, op);
    }

    return op;
}

void AsyncNet_ProcessCompleted(void) {
    async_operation_t *op, *next;

    for (op = async_net.completed_queue; op; op = next) {
        next = op->next;

        if (op->callback) {
            async_result_t result = {
                .status = op->status,
                .error_code = op->error_code,
                .bytes_transferred = op->bytes_transferred,
                .user_data = op->user_data,
                .completion_time = Sys_Milliseconds() * 0.001
            };
            op->callback(&result, op->user_data);
        }

        // Update statistics
        double latency = result.completion_time - op->start_time;
        async_net.average_latency = (async_net.average_latency * async_net.total_operations + latency) /
                                   (async_net.total_operations + 1);
        async_net.total_operations++;

        // Free operation
        op->id = -1; // Mark as free
        async_net.active_operations--;
    }

    async_net.completed_queue = NULL;
}

async_status_t AsyncNet_GetOperationStatus(async_operation_t *op) {
    return op ? op->status : ASYNC_STATUS_FAILED;
}

void AsyncNet_PrintStats(void) {
    Com_Printf("Async Network Statistics:\n");
    Com_Printf("========================\n");
    Com_Printf("Active operations: %d/%d\n", async_net.active_operations, async_net.max_operations);
    Com_Printf("Total operations: %d\n", async_net.total_operations);
    Com_Printf("Successful: %d\n", async_net.successful_operations);
    Com_Printf("Failed: %d\n", async_net.failed_operations);
    Com_Printf("Average latency: %.2f ms\n", async_net.average_latency * 1000.0);
    Com_Printf("Queue depth: %d\n", async_net.queue_depth);
    Com_Printf("Threading: %s\n", async_net.use_threads ? "enabled" : "disabled");
}

int AsyncNet_GetActiveOperationCount(void) {
    return async_net.active_operations;
}

void AsyncNet_SetMaxOperations(int max_ops) {
    // Note: In a full implementation, this would resize the pool
    async_net.max_operations = max_ops;
}

// Compatibility functions
void NET_AsyncSendPacket(NS_e net_socket, int length, const void *data, const netadr_t *to) {
    AsyncNet_SendTo(data, length, to, NULL, NULL);
}

qboolean NET_AsyncGetPacket(NS_e net_socket, netadr_t *net_from, msg_t *net_message) {
    return NET_GetPacket(net_socket, net_from, net_message);
}