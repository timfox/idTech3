/*
=============================================================================
Audio Thread System Implementation

Isolated audio processing with minimal latency for high-performance sound.
=============================================================================
*/

#include "snd_audio_thread.h"
#include "client.h"
#include "snd_local.h"
#include "../common/qcommon.h"
#include <string.h>

// Global audio thread system instance
audio_thread_system_t audio_thread_system = {0};

/*
=============================================================================
Audio Thread Worker Functions
=============================================================================
*/

// Priority comparison for work items (higher priority = lower number)
static int AudioWorkItemCompare(const void* a, const void* b) {
    const audio_work_item_t* itemA = (const audio_work_item_t*)a;
    const audio_work_item_t* itemB = (const audio_work_item_t*)b;
    return itemA->priority - itemB->priority;
}

// Main audio thread worker function
static THREAD_RETURN THREAD_CALL AudioThreadWorker(void* arg) {
    audio_thread_data_t* thread = (audio_thread_data_t*)arg;
    char thread_name[32];

    Com_sprintf(thread_name, sizeof(thread_name), "Audio_%d", thread->thread_type);
    Thread_SetCurrentAffinity(1ULL << (thread->thread_type % Sys_GetCPUCount()));

    while (!thread->should_exit) {
        // Wait for work with timeout
        MUTEX_LOCK(thread->work_mutex);
        while (atomic_load_explicit(&thread->work_available_count, memory_order_relaxed) == 0 &&
               !thread->should_exit) {
            // Wait with 1ms timeout for low latency
            struct timespec timeout;
            timeout.tv_sec = 0;
            timeout.tv_nsec = 1000000; // 1ms

            CONDITION_TIMED_WAIT(thread->work_available, thread->work_mutex, &timeout);
        }
        MUTEX_UNLOCK(thread->work_mutex);

        if (thread->should_exit) break;

        // Process work items (drain queue)
        while (1) {
            SpinLock_Lock(&thread->queue_lock);

            int available = atomic_load_explicit(&thread->work_available_count, memory_order_relaxed);
            if (available == 0) {
                SpinLock_Unlock(&thread->queue_lock);
                break;
            }

            // Get next work item (priority-based selection would go here)
            int head = atomic_load_explicit(&thread->work_queue_head, memory_order_relaxed);
            audio_work_item_t* work = &thread->work_queue[head];

            atomic_store_explicit(&thread->work_queue_head,
                                (head + 1) % thread->work_queue_size, memory_order_relaxed);
            atomic_fetch_sub_explicit(&thread->work_available_count, 1, memory_order_relaxed);

            SpinLock_Unlock(&thread->queue_lock);

            if (work && work->work_function) {
                // Execute work with timing
                uint64_t start_time = ri.Microseconds() * 1000;
                work->work_function(work->work_data);
                uint64_t end_time = ri.Microseconds() * 1000;

                // Update statistics
                thread->total_work_items_processed++;
                uint64_t execution_time = end_time - start_time;
                thread->total_execution_time_ns += execution_time;
                thread->average_work_time_ms = (float)thread->total_execution_time_ns /
                                             (float)thread->total_work_items_processed / 1000000.0f;
                thread->last_activity_time = end_time;
            }
        }

        // Yield to prevent busy-waiting
        Thread_Yield();
    }

    return 0;
}

/*
=============================================================================
Audio Processing Functions
=============================================================================
*/

typedef struct {
    int start_time;
    int end_time;
    channel_t* channels;
    int num_channels;
} mixing_work_data_t;

void AudioThread_ProcessMixing(void* data) {
    mixing_work_data_t* mix_data = (mixing_work_data_t*)data;

    // Process audio mixing for the specified time range
    S_PaintChannels(mix_data->end_time);

    // Update global statistics
    atomic_fetch_add_explicit(&audio_thread_system.total_audio_frames_processed, 1, memory_order_relaxed);
}

typedef struct {
    channel_t* channel;
    int entity_num;
} spatial_work_data_t;

void AudioThread_ProcessSpatialization(void* data) {
    spatial_work_data_t* spatial_data = (spatial_work_data_t*)data;

    // Perform 3D spatialization calculations
    S_Spatialize(spatial_data->channel);

    // Update entity position if needed
    if (spatial_data->entity_num >= 0) {
        // Mark channel as spatially processed
        spatial_data->channel->doppler = qtrue; // Reuse flag for processing state
    }
}

typedef struct {
    sfx_t* sfx;
    int offset;
    int count;
} streaming_work_data_t;

void AudioThread_ProcessStreaming(void* data) {
    streaming_work_data_t* stream_data = (streaming_work_data_t*)data;

    // Process audio streaming/decoding
    if (stream_data->sfx && !stream_data->sfx->inMemory) {
        // Load/decode audio data
        S_LoadSound(stream_data->sfx);
    }
}

typedef struct {
    channel_t* channel;
    int effect_type;
    float* parameters;
} effects_work_data_t;

void AudioThread_ProcessEffects(void* data) {
    effects_work_data_t* effect_data = (effects_work_data_t*)data;

    // Apply DSP effects (reverb, echo, etc.)
    // Implementation would depend on specific effects system
    // For now, this is a placeholder for future DSP work

    Q_UNUSED(effect_data);
}

/*
=============================================================================
Audio Thread System API
=============================================================================
*/

qboolean AudioThread_Init(void) {
    if (audio_thread_system.initialized) {
        return qtrue;
    }

    memset(&audio_thread_system, 0, sizeof(audio_thread_system_t));

    // Initialize atomic counters
    atomic_init(&audio_thread_system.active_threads, 0);

    // Default configuration - enable mixing thread by default
    audio_thread_system.enabled = qtrue;

    // Initialize all threads as disabled
    for (int i = 0; i < AUDIO_THREAD_MAX; i++) {
        audio_thread_system.thread_enabled[i] = qfalse;
    }

    audio_thread_system.initialized = qtrue;

    ri.Printf(PRINT_ALL, "Audio thread system initialized\n");
    return qtrue;
}

void AudioThread_Shutdown(void) {
    if (!audio_thread_system.initialized) {
        return;
    }

    // Disable all threads
    for (int i = 0; i < AUDIO_THREAD_MAX; i++) {
        AudioThread_DisableThread((audio_thread_type_t)i);
    }

    audio_thread_system.initialized = qfalse;
    ri.Printf(PRINT_ALL, "Audio thread system shutdown\n");
}

qboolean AudioThread_EnableThread(audio_thread_type_t threadType) {
    if (threadType >= AUDIO_THREAD_MAX || !audio_thread_system.initialized) {
        return qfalse;
    }

    if (audio_thread_system.thread_enabled[threadType]) {
        return qtrue; // Already enabled
    }

    audio_thread_data_t* thread = &audio_thread_system.threads[threadType];
    memset(thread, 0, sizeof(audio_thread_data_t));

    thread->thread_type = threadType;
    thread->should_exit = qfalse;
    thread->work_queue_size = 512; // Larger queue for audio processing
    atomic_init(&thread->work_available_count, 0);

    // Initialize synchronization primitives
    SpinLock_Init(&thread->queue_lock);
    MUTEX_INIT(thread->work_mutex);
    CONDITION_INIT(thread->work_available);

    // Allocate work queue
    thread->work_queue = (audio_work_item_t*)ri.Hunk_AllocateTempMemory(
        thread->work_queue_size * sizeof(audio_work_item_t));
    memset(thread->work_queue, 0, thread->work_queue_size * sizeof(audio_work_item_t));

    // Start thread with real-time priority for minimal latency
    if (!Thread_Create(&thread->handle, AudioThreadWorker, thread, "AudioWorker",
                      THREAD_PRIORITY_CRITICAL)) {
        ri.Printf(PRINT_ERROR, "Failed to create audio thread %d\n", threadType);
        ri.Hunk_FreeTempMemory(thread->work_queue);
        return qfalse;
    }

    audio_thread_system.thread_enabled[threadType] = qtrue;
    atomic_fetch_add_explicit(&audio_thread_system.active_threads, 1, memory_order_relaxed);

    ri.Printf(PRINT_ALL, "Enabled audio thread: %d\n", threadType);
    return qtrue;
}

void AudioThread_DisableThread(audio_thread_type_t threadType) {
    if (threadType >= AUDIO_THREAD_MAX ||
        !audio_thread_system.thread_enabled[threadType]) {
        return;
    }

    audio_thread_data_t* thread = &audio_thread_system.threads[threadType];

    // Signal thread to exit
    MUTEX_LOCK(thread->work_mutex);
    thread->should_exit = qtrue;
    CONDITION_SIGNAL(thread->work_available);
    MUTEX_UNLOCK(thread->work_mutex);

    // Wait for thread to finish
    Thread_Join(thread->handle);

    // Cleanup resources
    if (thread->work_queue) {
        ri.Hunk_FreeTempMemory(thread->work_queue);
        thread->work_queue = NULL;
    }

    audio_thread_system.thread_enabled[threadType] = qfalse;
    atomic_fetch_sub_explicit(&audio_thread_system.active_threads, 1, memory_order_relaxed);

    ri.Printf(PRINT_ALL, "Disabled audio thread: %d\n", threadType);
}

qboolean AudioThread_IsThreadEnabled(audio_thread_type_t threadType) {
    if (threadType >= AUDIO_THREAD_MAX) return qfalse;
    return audio_thread_system.thread_enabled[threadType];
}

/*
=============================================================================
Work Submission Functions
=============================================================================
*/

static void SubmitAudioWork(audio_thread_type_t threadType, void* workData,
                           void (*workFunction)(void*), int priority) {
    if (threadType >= AUDIO_THREAD_MAX ||
        !audio_thread_system.thread_enabled[threadType]) {
        // Execute immediately if thread not available
        workFunction(workData);
        return;
    }

    audio_thread_data_t* thread = &audio_thread_system.threads[threadType];

    // Create work item
    audio_work_item_t* workItem = (audio_work_item_t*)ri.Hunk_AllocateTempMemory(
        sizeof(audio_work_item_t));
    workItem->work_function = workFunction;
    workItem->work_data = workData;
    workItem->submit_time = ri.Microseconds() * 1000;
    workItem->thread_type = threadType;
    workItem->priority = priority;

    // Add to queue
    SpinLock_Lock(&thread->queue_lock);

    int available_count = atomic_load_explicit(&thread->work_available_count, memory_order_relaxed);
    if (available_count >= thread->work_queue_size) {
        // Queue full - execute immediately as fallback
        SpinLock_Unlock(&thread->queue_lock);
        workFunction(workData);
        ri.Hunk_FreeTempMemory(workItem);
        return;
    }

    int tail = atomic_load_explicit(&thread->work_queue_tail, memory_order_relaxed);
    memcpy(&thread->work_queue[tail], workItem, sizeof(audio_work_item_t));
    atomic_store_explicit(&thread->work_queue_tail,
                        (tail + 1) % thread->work_queue_size, memory_order_relaxed);
    atomic_fetch_add_explicit(&thread->work_available_count, 1, memory_order_relaxed);

    SpinLock_Unlock(&thread->queue_lock);

    // Signal thread
    MUTEX_LOCK(thread->work_mutex);
    CONDITION_SIGNAL(thread->work_available);
    MUTEX_UNLOCK(thread->work_mutex);

    ri.Hunk_FreeTempMemory(workItem); // Work item was copied to queue
}

void AudioThread_SubmitMixingWork(void* workData) {
    SubmitAudioWork(AUDIO_THREAD_MIXING, workData, AudioThread_ProcessMixing, 0); // Highest priority
}

void AudioThread_SubmitSpatialWork(void* workData) {
    SubmitAudioWork(AUDIO_THREAD_SPATIAL, workData, AudioThread_ProcessSpatialization, 1);
}

void AudioThread_SubmitStreamingWork(void* workData) {
    SubmitAudioWork(AUDIO_THREAD_STREAMING, workData, AudioThread_ProcessStreaming, 2);
}

void AudioThread_SubmitEffectsWork(void* workData) {
    SubmitAudioWork(AUDIO_THREAD_EFFECTS, workData, AudioThread_ProcessEffects, 1);
}

/*
=============================================================================
Synchronization Functions
=============================================================================
*/

void AudioThread_WaitForAllThreads(void) {
    for (int i = 0; i < AUDIO_THREAD_MAX; i++) {
        if (audio_thread_system.thread_enabled[i]) {
            AudioThread_WaitForThread((audio_thread_type_t)i);
        }
    }
}

void AudioThread_WaitForThread(audio_thread_type_t threadType) {
    if (threadType >= AUDIO_THREAD_MAX ||
        !audio_thread_system.thread_enabled[threadType]) {
        return;
    }

    audio_thread_data_t* thread = &audio_thread_system.threads[threadType];

    // Wait until work queue is empty (with timeout for safety)
    uint64_t start_wait = ri.Microseconds() * 1000;
    while (atomic_load_explicit(&thread->work_available_count, memory_order_relaxed) > 0) {
        // Timeout after 100ms to prevent infinite waiting
        if ((ri.Microseconds() * 1000 - start_wait) > 100000000ULL) {
            ri.Printf(PRINT_WARNING, "Audio thread %d wait timeout\n", threadType);
            break;
        }
        Thread_Sleep(1); // Small sleep to avoid busy waiting
    }
}

/*
=============================================================================
Performance Monitoring
=============================================================================
*/

void AudioThread_GetStats(audio_thread_type_t threadType,
                         uint64_t* processedItems,
                         float* avgTimeMs,
                         uint64_t* totalTimeNs) {
    if (threadType >= AUDIO_THREAD_MAX ||
        !audio_thread_system.thread_enabled[threadType]) {
        if (processedItems) *processedItems = 0;
        if (avgTimeMs) *avgTimeMs = 0.0f;
        if (totalTimeNs) *totalTimeNs = 0;
        return;
    }

    audio_thread_data_t* thread = &audio_thread_system.threads[threadType];
    if (processedItems) *processedItems = thread->total_work_items_processed;
    if (avgTimeMs) *avgTimeMs = thread->average_work_time_ms;
    if (totalTimeNs) *totalTimeNs = thread->total_execution_time_ns;
}
