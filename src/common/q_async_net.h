/*
===============================================================================
Asynchronous Network Operations for id Tech 3

Non-blocking I/O system for improved network performance and responsiveness.
===============================================================================
*/

#ifndef __Q_ASYNC_NET_H__
#define __Q_ASYNC_NET_H__

#include "q_shared.h"

// Async operation types
typedef enum {
    ASYNC_OP_SEND,
    ASYNC_OP_RECEIVE,
    ASYNC_OP_CONNECT,
    ASYNC_OP_DISCONNECT,
    ASYNC_OP_DNS_RESOLVE
} async_op_type_t;

// Async operation status
typedef enum {
    ASYNC_STATUS_PENDING,
    ASYNC_STATUS_IN_PROGRESS,
    ASYNC_STATUS_COMPLETED,
    ASYNC_STATUS_FAILED,
    ASYNC_STATUS_CANCELLED
} async_status_t;

// Async operation result
typedef struct {
    async_status_t status;
    int error_code;
    size_t bytes_transferred;
    void *user_data;
    double completion_time;
} async_result_t;

// Async operation callback
typedef void (*async_callback_t)(const async_result_t *result, void *user_data);

// Async operation descriptor
typedef struct async_operation_s {
    int id;
    async_op_type_t type;
    netadr_t address;
    void *buffer;
    size_t buffer_size;
    async_callback_t callback;
    void *user_data;
    double start_time;
    async_status_t status;
    int error_code;
    size_t bytes_transferred;

    // Internal use
    struct async_operation_s *next;
    struct async_operation_s *prev;
} async_operation_t;

// Async network context
typedef struct {
    qboolean initialized;
    int max_operations;
    int active_operations;
    async_operation_t *operation_pool;
    async_operation_t *pending_queue;
    async_operation_t *completed_queue;

    // Threading (if available)
    qboolean use_threads;
    void *worker_thread;
    volatile qboolean shutdown_requested;

    // Statistics
    int total_operations;
    int successful_operations;
    int failed_operations;
    double average_latency;
    int queue_depth;
} async_net_context_t;

// Global async network context
extern async_net_context_t async_net;

// Initialize async network system
qboolean AsyncNet_Init(int max_operations);

// Shutdown async network system
void AsyncNet_Shutdown(void);

// Create async operation
async_operation_t *AsyncNet_CreateOperation(async_op_type_t type, const netadr_t *address);

// Send data asynchronously
async_operation_t *AsyncNet_SendTo(const void *data, size_t size, const netadr_t *address,
                                  async_callback_t callback, void *user_data);

// Receive data asynchronously
async_operation_t *AsyncNet_RecvFrom(void *buffer, size_t max_size,
                                    async_callback_t callback, void *user_data);

// Connect asynchronously
async_operation_t *AsyncNet_Connect(const netadr_t *address,
                                   async_callback_t callback, void *user_data);

// Resolve hostname asynchronously
async_operation_t *AsyncNet_ResolveHost(const char *hostname, int default_port,
                                       async_callback_t callback, void *user_data);

// Cancel async operation
qboolean AsyncNet_CancelOperation(async_operation_t *op);

// Process completed operations (call in main loop)
void AsyncNet_ProcessCompleted(void);

// Get operation status
async_status_t AsyncNet_GetOperationStatus(async_operation_t *op);

// Wait for operation completion (blocking)
qboolean AsyncNet_WaitForOperation(async_operation_t *op, int timeout_ms);

// Statistics and monitoring
void AsyncNet_PrintStats(void);
int AsyncNet_GetActiveOperationCount(void);
int AsyncNet_GetQueueDepth(void);
double AsyncNet_GetAverageLatency(void);

// Performance tuning
void AsyncNet_SetMaxOperations(int max_ops);
void AsyncNet_EnableThreading(qboolean enable);

// Low-level socket operations (for advanced users)
int AsyncNet_CreateSocket(int domain, int type, int protocol);
qboolean AsyncNet_SetNonBlocking(int socket_fd);
qboolean AsyncNet_BindSocket(int socket_fd, const netadr_t *address);

// Integration with existing network system
void NET_AsyncSendPacket(NS_e net_socket, int length, const void *data, const netadr_t *to);
qboolean NET_AsyncGetPacket(NS_e net_socket, netadr_t *net_from, msg_t *net_message);

// Compatibility layer (maps async calls to sync where threading not available)
#define NET_SendPacket(net_socket, length, data, to) \
    AsyncNet_SendTo(data, length, to, NULL, NULL)

#define NET_GetPacket(net_socket, net_from, net_message) \
    AsyncNet_RecvFrom(net_message->data, net_message->maxsize, NULL, NULL)

#endif // __Q_ASYNC_NET_H__