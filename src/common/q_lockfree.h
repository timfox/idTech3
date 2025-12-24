/*
=================
Lock-Free Data Structures
=================
*/

#ifndef __Q_LOCKFREE_H__
#define __Q_LOCKFREE_H__

#include "q_shared.h"
#include "thread_platform.h"

/*
===========================================================================
Hazard Pointers
Memory-safe concurrent data reclamation.
===========================================================================
*/

#define MAX_HP_THREADS 64
#define MAX_HP_COUNT   4
#define HP_THRESHOLD   (MAX_HP_THREADS * MAX_HP_COUNT * 2)

typedef void (*hp_free_func_t)(void*);

typedef struct hp_retired_node_s {
    void* ptr;
    hp_free_func_t free_func;
    struct hp_retired_node_s* next;
} hp_retired_node_t;

typedef struct {
    atomic_uintptr_t pointers[MAX_HP_COUNT];
    atomic_int_t active;
    unsigned long thread_id;
    
    // Local retired list
    hp_retired_node_t* retired_list;
    int retired_count;
} hp_thread_state_t;

typedef struct {
    hp_thread_state_t states[MAX_HP_THREADS];
    atomic_int_t thread_count;
    qboolean initialized;
} hp_system_t;

/*
=================
HP_Init
Initialize hazard pointer system
=================
*/
void HP_Init(void);

/*
=================
HP_Shutdown
Shutdown hazard pointer system
=================
*/
void HP_Shutdown(void);

/*
=================
HP_RegisterThread
Register current thread with HP system. Returns thread index.
=================
*/
int HP_RegisterThread(void);

/*
=================
HP_UnregisterThread
Unregister current thread
=================
*/
void HP_UnregisterThread(int thread_idx);

/*
=================
HP_Acquire
Safely acquire a pointer from an atomic source
=================
*/
void* HP_Acquire(int thread_idx, int hp_idx, atomic_uintptr_t* source);

/*
=================
HP_Release
Release a hazard pointer
=================
*/
void HP_Release(int thread_idx, int hp_idx);

/*
=================
HP_Retire
Retire a pointer for later reclamation
=================
*/
void HP_Retire(int thread_idx, void* ptr, hp_free_func_t free_func);

/*
=================
HP_Scan
Scan all hazard pointers and reclaim safe memory
=================
*/
void HP_Scan(int thread_idx);

/*
===========================================================================
MPMC Lock-Free Queue (Multi-Producer, Multi-Consumer)
Based on Dmitry Vyukov's bounded MPMC queue.
===========================================================================
*/

typedef struct {
    atomic_int_t sequence;
    void* data;
} lf_cell_t;

typedef struct {
    lf_cell_t* buffer;
    size_t buffer_mask;
    char pad0[64]; // Padding to prevent false sharing
    atomic_int_t enqueue_pos;
    char pad1[64];
    atomic_int_t dequeue_pos;
    char pad2[64];
} lf_queue_t;

/*
=================
LF_Queue_Init
Initialize a lock-free queue
size: Must be a power of 2
=================
*/
qboolean LF_Queue_Init(lf_queue_t *queue, size_t size);

/*
=================
LF_Queue_Shutdown
Shutdown a lock-free queue
=================
*/
void LF_Queue_Shutdown(lf_queue_t *queue);

/*
=================
LF_Queue_Enqueue
Add an item to the queue (thread-safe, lock-free)
=================
*/
qboolean LF_Queue_Enqueue(lf_queue_t *queue, void* data);

/*
=================
LF_Queue_Dequeue
Remove an item from the queue (thread-safe, lock-free)
=================
*/
qboolean LF_Queue_Dequeue(lf_queue_t *queue, void** data);

/*
=================
LF_Queue_GetCount
Get approximate number of items in queue
=================
*/
size_t LF_Queue_GetCount(lf_queue_t *queue);

/*
===========================================================================
Lock-Free Stack (LIFO)
Uses Hazard Pointers for safe memory reclamation.
===========================================================================
*/

typedef struct lf_stack_node_s {
    void* data;
    struct lf_stack_node_s* next;
} lf_stack_node_t;

typedef struct {
    atomic_uintptr_t head;
} lf_stack_t;

/*
=================
LF_Stack_Init
Initialize a lock-free stack
=================
*/
void LF_Stack_Init(lf_stack_t *stack);

/*
=================
LF_Stack_Push
Push an item onto the stack
=================
*/
void LF_Stack_Push(lf_stack_t *stack, void* data);

/*
=================
LF_Stack_Pop
Pop an item from the stack
thread_idx: Current thread index from HP_RegisterThread()
=================
*/
void* LF_Stack_Pop(lf_stack_t *stack, int thread_idx);

/*
===========================================================================
Read-Copy-Update (RCU)
Efficient concurrent read-heavy operations.
===========================================================================
*/

#define MAX_RCU_THREADS 64

typedef struct {
    atomic_int_t epoch;
    atomic_int_t active;
    unsigned long thread_id;
} rcu_thread_state_t;

typedef struct {
    rcu_thread_state_t threads[MAX_RCU_THREADS];
    atomic_int_t global_epoch;
    qboolean initialized;
} rcu_system_t;

/*
=================
RCU_Init
Initialize RCU system
=================
*/
void RCU_Init(void);

/*
=================
RCU_RegisterThread
Register current thread for RCU. Returns thread index.
=================
*/
int RCU_RegisterThread(void);

/*
=================
RCU_ReadLock
Enter RCU read-side critical section
=================
*/
void RCU_ReadLock(int thread_idx);

/*
=================
RCU_ReadUnlock
Exit RCU read-side critical section
=================
*/
void RCU_ReadUnlock(int thread_idx);

/*
=================
RCU_Synchronize
Wait for all current readers to finish (grace period)
=================
*/
void RCU_Synchronize(void);

/*
=================
RCU_AssignPointer
Safely assign a new value to an RCU-protected pointer
=================
*/
#define RCU_AssignPointer(ptr, val) \
    atomic_store_explicit(&(ptr), (uintptr_t)(val), memory_order_release)

/*
=================
RCU_Dereference
Safely read an RCU-protected pointer
=================
*/
#define RCU_Dereference(ptr) \
    (void*)atomic_load_explicit(&(ptr), memory_order_consume)

#endif // __Q_LOCKFREE_H__

