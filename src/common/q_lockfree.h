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

#endif // __Q_LOCKFREE_H__

