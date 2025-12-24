/*
=================
Lock-Free Data Structures Implementation
=================
*/

#include "q_lockfree.h"
#include "qcommon.h"
#include <stdlib.h>
#include <string.h>

/*
===========================================================================
Hazard Pointers Implementation
===========================================================================
*/

static hp_system_t s_hp_system = {0};

void HP_Init(void) {
    if (s_hp_system.initialized) return;
    
    memset(&s_hp_system, 0, sizeof(s_hp_system));
    atomic_init(&s_hp_system.thread_count, 0);
    
    for (int i = 0; i < MAX_HP_THREADS; i++) {
        atomic_init(&s_hp_system.states[i].active, 0);
        for (int j = 0; j < MAX_HP_COUNT; j++) {
            atomic_init(&s_hp_system.states[i].pointers[j], 0);
        }
    }
    
    s_hp_system.initialized = qtrue;
}

void HP_Shutdown(void) {
    if (!s_hp_system.initialized) return;
    
    for (int i = 0; i < MAX_HP_THREADS; i++) {
        HP_Scan(i);
        // Force cleanup of any remaining retired nodes
        hp_retired_node_t* node = s_hp_system.states[i].retired_list;
        while (node) {
            hp_retired_node_t* next = node->next;
            if (node->free_func) node->free_func(node->ptr);
            Z_Free(node);
            node = next;
        }
    }
    
    s_hp_system.initialized = qfalse;
}

int HP_RegisterThread(void) {
    unsigned long id = Thread_GetCurrentID();
    
    // First, try to find an existing entry for this thread
    for (int i = 0; i < MAX_HP_THREADS; i++) {
        if (atomic_load_explicit(&s_hp_system.states[i].active, memory_order_relaxed) && 
            s_hp_system.states[i].thread_id == id) {
            return i;
        }
    }
    
    // Find an empty slot
    for (int i = 0; i < MAX_HP_THREADS; i++) {
        int expected = 0;
        if (atomic_compare_exchange_weak_explicit(&s_hp_system.states[i].active, &expected, 1, 
                                                memory_order_relaxed, memory_order_relaxed)) {
            s_hp_system.states[i].thread_id = id;
            atomic_fetch_add_explicit(&s_hp_system.thread_count, 1, memory_order_relaxed);
            return i;
        }
    }
    
    return -1; // Max threads reached
}

void HP_UnregisterThread(int thread_idx) {
    if (thread_idx < 0 || thread_idx >= MAX_HP_THREADS) return;
    
    HP_Scan(thread_idx);
    
    for (int j = 0; j < MAX_HP_COUNT; j++) {
        atomic_store_explicit(&s_hp_system.states[thread_idx].pointers[j], 0, memory_order_relaxed);
    }
    
    atomic_store_explicit(&s_hp_system.states[thread_idx].active, 0, memory_order_release);
    atomic_fetch_sub_explicit(&s_hp_system.thread_count, 1, memory_order_relaxed);
}

void* HP_Acquire(int thread_idx, int hp_idx, atomic_uintptr_t* source) {
    if (thread_idx < 0 || thread_idx >= MAX_HP_THREADS || hp_idx < 0 || hp_idx >= MAX_HP_COUNT) return NULL;
    
    uintptr_t ptr;
    do {
        ptr = atomic_load_explicit(source, memory_order_relaxed);
        atomic_store_explicit(&s_hp_system.states[thread_idx].pointers[hp_idx], ptr, memory_order_seq_cst);
    } while (ptr != atomic_load_explicit(source, memory_order_acquire));
    
    return (void*)ptr;
}

void HP_Release(int thread_idx, int hp_idx) {
    if (thread_idx < 0 || thread_idx >= MAX_HP_THREADS || hp_idx < 0 || hp_idx >= MAX_HP_COUNT) return;
    atomic_store_explicit(&s_hp_system.states[thread_idx].pointers[hp_idx], 0, memory_order_release);
}

void HP_Retire(int thread_idx, void* ptr, hp_free_func_t free_func) {
    if (!ptr || thread_idx < 0 || thread_idx >= MAX_HP_THREADS) return;
    
    hp_retired_node_t* node = (hp_retired_node_t*)Z_Malloc(sizeof(hp_retired_node_t));
    if (!node) {
        if (free_func) free_func(ptr); // Emergency cleanup
        return;
    }
    
    node->ptr = ptr;
    node->free_func = free_func;
    node->next = s_hp_system.states[thread_idx].retired_list;
    s_hp_system.states[thread_idx].retired_list = node;
    s_hp_system.states[thread_idx].retired_count++;
    
    if (s_hp_system.states[thread_idx].retired_count >= HP_THRESHOLD) {
        HP_Scan(thread_idx);
    }
}

static int compare_ptr(const void* a, const void* b) {
    uintptr_t pa = *(uintptr_t*)a;
    uintptr_t pb = *(uintptr_t*)b;
    return (pa < pb) ? -1 : (pa > pb ? 1 : 0);
}

void HP_Scan(int thread_idx) {
    if (thread_idx < 0 || thread_idx >= MAX_HP_THREADS) return;
    
    uintptr_t active_hps[MAX_HP_THREADS * MAX_HP_COUNT];
    int hp_count = 0;
    
    // Collect all active hazard pointers
    for (int i = 0; i < MAX_HP_THREADS; i++) {
        if (atomic_load_explicit(&s_hp_system.states[i].active, memory_order_acquire)) {
            for (int j = 0; j < MAX_HP_COUNT; j++) {
                uintptr_t p = atomic_load_explicit(&s_hp_system.states[i].pointers[j], memory_order_acquire);
                if (p) active_hps[hp_count++] = p;
            }
        }
    }
    
    // Sort for faster lookup
    if (hp_count > 1) {
        qsort(active_hps, hp_count, sizeof(uintptr_t), compare_ptr);
    }
    
    // Reclaim safe memory
    hp_retired_node_t* current = s_hp_system.states[thread_idx].retired_list;
    s_hp_system.states[thread_idx].retired_list = NULL;
    s_hp_system.states[thread_idx].retired_count = 0;
    
    while (current) {
        hp_retired_node_t* next = current->next;
        uintptr_t ptr = (uintptr_t)current->ptr;
        
        qboolean in_use = qfalse;
        if (hp_count > 0) {
            uintptr_t* found = (uintptr_t*)bsearch(&ptr, active_hps, hp_count, sizeof(uintptr_t), compare_ptr);
            if (found) in_use = qtrue;
        }
        
        if (in_use) {
            // Keep in retired list for next scan
            current->next = s_hp_system.states[thread_idx].retired_list;
            s_hp_system.states[thread_idx].retired_list = current;
            s_hp_system.states[thread_idx].retired_count++;
        } else {
            // Safe to free
            if (current->free_func) current->free_func(current->ptr);
            Z_Free(current);
        }
        current = next;
    }
}

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

/*
===========================================================================
Lock-Free Stack Implementation
===========================================================================
*/

void LF_Stack_Init(lf_stack_t *stack) {
    if (!stack) return;
    atomic_init(&stack->head, 0);
}

void LF_Stack_Push(lf_stack_t *stack, void* data) {
    if (!stack) return;
    
    lf_stack_node_t* node = (lf_stack_node_t*)Z_Malloc(sizeof(lf_stack_node_t));
    if (!node) return;
    
    node->data = data;
    uintptr_t head = atomic_load_explicit(&stack->head, memory_order_relaxed);
    
    do {
        node->next = (lf_stack_node_t*)head;
    } while (!atomic_compare_exchange_weak_explicit(&stack->head, &head, (uintptr_t)node,
                                                   memory_order_release, memory_order_relaxed));
}

static void free_stack_node(void* ptr) {
    Z_Free(ptr);
}

void* LF_Stack_Pop(lf_stack_t *stack, int thread_idx) {
    if (!stack || thread_idx < 0) return NULL;
    
    lf_stack_node_t* node;
    uintptr_t head;
    
    do {
        node = (lf_stack_node_t*)HP_Acquire(thread_idx, 0, &stack->head);
        if (!node) return NULL;
        
        head = (uintptr_t)node;
        uintptr_t next = (uintptr_t)node->next;
        
        if (atomic_compare_exchange_weak_explicit(&stack->head, &head, next,
                                                memory_order_acquire, memory_order_relaxed)) {
            break;
        }
    } while (1);
    
    HP_Release(thread_idx, 0);
    void* data = node->data;
    HP_Retire(thread_idx, node, free_stack_node);
    
    return data;
}

/*
===========================================================================
Read-Copy-Update (RCU) Implementation
===========================================================================
*/

static rcu_system_t s_rcu_system = {0};

void RCU_Init(void) {
    if (s_rcu_system.initialized) return;
    
    memset(&s_rcu_system, 0, sizeof(s_rcu_system));
    atomic_init(&s_rcu_system.global_epoch, 1);
    
    for (int i = 0; i < MAX_RCU_THREADS; i++) {
        atomic_init(&s_rcu_system.threads[i].active, 0);
        atomic_init(&s_rcu_system.threads[i].epoch, 0);
    }
    
    s_rcu_system.initialized = qtrue;
}

int RCU_RegisterThread(void) {
    unsigned long id = Thread_GetCurrentID();
    
    // Check if already registered
    for (int i = 0; i < MAX_RCU_THREADS; i++) {
        if (atomic_load_explicit(&s_rcu_system.threads[i].active, memory_order_relaxed) && 
            s_rcu_system.threads[i].thread_id == id) {
            return i;
        }
    }
    
    // Find empty slot
    for (int i = 0; i < MAX_RCU_THREADS; i++) {
        int expected = 0;
        if (atomic_compare_exchange_weak_explicit(&s_rcu_system.threads[i].active, &expected, 1, 
                                                memory_order_relaxed, memory_order_relaxed)) {
            s_rcu_system.threads[i].thread_id = id;
            return i;
        }
    }
    
    return -1;
}

void RCU_ReadLock(int thread_idx) {
    if (thread_idx < 0 || thread_idx >= MAX_RCU_THREADS) return;
    
    // Set thread epoch to current global epoch to mark entry into critical section
    int epoch = atomic_load_explicit(&s_rcu_system.global_epoch, memory_order_relaxed);
    atomic_store_explicit(&s_rcu_system.threads[thread_idx].epoch, epoch, memory_order_relaxed);
    
    // Full barrier to ensure RCU_ReadLock precedes any subsequent reads
    atomic_thread_fence(memory_order_seq_cst);
}

void RCU_ReadUnlock(int thread_idx) {
    if (thread_idx < 0 || thread_idx >= MAX_RCU_THREADS) return;
    
    // Full barrier to ensure all reads complete before clearing epoch
    atomic_thread_fence(memory_order_seq_cst);
    atomic_store_explicit(&s_rcu_system.threads[thread_idx].epoch, 0, memory_order_relaxed);
}

void RCU_Synchronize(void) {
    if (!s_rcu_system.initialized) return;
    
    // 1. Snapshot the current epoch and increment global epoch
    int target_epoch = atomic_fetch_add_explicit(&s_rcu_system.global_epoch, 1, memory_order_seq_cst);
    
    // 2. Wait for all threads to transition out of the target_epoch
    for (int i = 0; i < MAX_RCU_THREADS; i++) {
        if (atomic_load_explicit(&s_rcu_system.threads[i].active, memory_order_acquire)) {
            // Spin until thread is either inactive or in a newer epoch
            while (1) {
                int thread_epoch = atomic_load_explicit(&s_rcu_system.threads[i].epoch, memory_order_acquire);
                if (thread_epoch == 0 || thread_epoch > target_epoch) {
                    break;
                }
                Thread_Yield();
            }
        }
    }
}

