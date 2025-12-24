/*
=================
Lock-Free Data Structures Implementation
=================
*/

#include "q_lockfree.h"
#include "qcommon.h"
#include <stdlib.h>

qboolean LF_Queue_Init(lf_queue_t *queue, size_t size) {
    if (!queue || (size & (size - 1)) != 0) {
        return qfalse; // Size must be power of 2
    }

    queue->buffer = (lf_cell_t*)Z_Malloc(sizeof(lf_cell_t) * size);
    if (!queue->buffer) {
        return qfalse;
    }

    queue->buffer_mask = size - 1;
    for (size_t i = 0; i != size; i++) {
        atomic_init(&queue->buffer[i].sequence, (int)i);
    }

    atomic_init(&queue->enqueue_pos, 0);
    atomic_init(&queue->dequeue_pos, 0);

    return qtrue;
}

void LF_Queue_Shutdown(lf_queue_t *queue) {
    if (!queue || !queue->buffer) {
        return;
    }

    Z_Free(queue->buffer);
    queue->buffer = NULL;
}

qboolean LF_Queue_Enqueue(lf_queue_t *queue, void* data) {
    lf_cell_t* cell;
    int pos = atomic_load_explicit(&queue->enqueue_pos, memory_order_relaxed);

    for (;;) {
        cell = &queue->buffer[pos & queue->buffer_mask];
        int seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        int dif = seq - pos;

        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(&queue->enqueue_pos, &pos, pos + 1, memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        } else if (dif < 0) {
            return qfalse; // Queue full
        } else {
            pos = atomic_load_explicit(&queue->enqueue_pos, memory_order_relaxed);
        }
    }

    cell->data = data;
    atomic_store_explicit(&cell->sequence, pos + 1, memory_order_release);

    return qtrue;
}

qboolean LF_Queue_Dequeue(lf_queue_t *queue, void** data) {
    lf_cell_t* cell;
    int pos = atomic_load_explicit(&queue->dequeue_pos, memory_order_relaxed);

    for (;;) {
        cell = &queue->buffer[pos & queue->buffer_mask];
        int seq = atomic_load_explicit(&cell->sequence, memory_order_acquire);
        int dif = seq - (pos + 1);

        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(&queue->dequeue_pos, &pos, pos + 1, memory_order_relaxed, memory_order_relaxed)) {
                break;
            }
        } else if (dif < 0) {
            return qfalse; // Queue empty
        } else {
            pos = atomic_load_explicit(&queue->dequeue_pos, memory_order_relaxed);
        }
    }

    *data = cell->data;
    atomic_store_explicit(&cell->sequence, (int)(pos + queue->buffer_mask + 1), memory_order_release);

    return qtrue;
}

size_t LF_Queue_GetCount(lf_queue_t *queue) {
    int enq = atomic_load_explicit(&queue->enqueue_pos, memory_order_relaxed);
    int deq = atomic_load_explicit(&queue->dequeue_pos, memory_order_relaxed);
    if (enq >= deq) return (size_t)(enq - deq);
    return 0; // Should not happen with unsigned indices usually, but here they are signed
}

