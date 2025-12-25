/*
=============================================================================
Audio Thread System

Isolated audio processing with minimal latency for high-performance sound.
=============================================================================
*/

#ifndef __SND_AUDIO_THREAD_H__
#define __SND_AUDIO_THREAD_H__

#include "../common/q_shared.h"
#include "../common/thread_platform.h"
#include "snd_local.h"

// Audio thread types
typedef enum {
    AUDIO_THREAD_MIXING = 0,    // Main audio mixing and processing
    AUDIO_THREAD_SPATIAL,       // 3D spatialization calculations
    AUDIO_THREAD_STREAMING,     // Audio streaming and buffering
    AUDIO_THREAD_EFFECTS,       // DSP effects processing
    AUDIO_THREAD_MAX
} audio_thread_type_t;

// Audio work item
typedef struct {
    void (*work_function)(void* data);
    void* work_data;
    uint64_t submit_time;
} audio_work_item_t;

// Mixing work data structure
typedef struct {
    uint64_t start_time;
    uint64_t end_time;
    channel_t* channels;
    int num_channels;
} mixing_work_data_t;

// Function declarations
qboolean AudioThread_IsThreadEnabled(audio_thread_type_t type);
void AudioThread_SubmitMixingWork(mixing_work_data_t* mix_data);
void AudioThread_WaitForThread(audio_thread_type_t type);
    audio_thread_type_t thread_type;
    int priority; // 0 = highest, higher numbers = lower priority
} audio_work_item_t;

// Audio thread data
typedef struct {
    audio_thread_type_t thread_type;
    thread_handle_t handle;
    condition_t work_available;
    mutex_t work_mutex;
    spinlock_t queue_lock;
    qboolean should_exit;

    // Work queue
    audio_work_item_t* work_queue;
    int work_queue_size;
    atomic_int_t work_queue_head;
    atomic_int_t work_queue_tail;
    atomic_int_t work_available_count;

    // Performance tracking
    uint64_t total_work_items_processed;
    uint64_t total_execution_time_ns;
    float average_work_time_ms;
    uint64_t last_activity_time;

    // Thread-specific data
    void* thread_context;
} audio_thread_data_t;

// Audio thread system
typedef struct {
    qboolean enabled;
    qboolean initialized;

    // Thread management
    audio_thread_data_t threads[AUDIO_THREAD_MAX];
    qboolean thread_enabled[AUDIO_THREAD_MAX];

    // Global statistics
    uint64_t total_audio_frames_processed;
    uint64_t total_latency_ns;
    float average_frame_time_ms;

    // Synchronization
    uint64_t frame_start_time;
    atomic_int_t active_threads;
} audio_thread_system_t;

extern audio_thread_system_t audio_thread_system;

// Audio Thread API
qboolean AudioThread_Init(void);
void AudioThread_Shutdown(void);

qboolean AudioThread_EnableThread(audio_thread_type_t threadType);
void AudioThread_DisableThread(audio_thread_type_t threadType);
qboolean AudioThread_IsThreadEnabled(audio_thread_type_t threadType);

// Work submission
void AudioThread_SubmitMixingWork(void* workData);
void AudioThread_SubmitSpatialWork(void* workData);
void AudioThread_SubmitStreamingWork(void* workData);
void AudioThread_SubmitEffectsWork(void* workData);

// Synchronization
void AudioThread_WaitForAllThreads(void);
void AudioThread_WaitForThread(audio_thread_type_t threadType);

// Performance monitoring
void AudioThread_GetStats(audio_thread_type_t threadType,
                         uint64_t* processedItems,
                         float* avgTimeMs,
                         uint64_t* totalTimeNs);

// Audio processing functions (called by work items)
void AudioThread_ProcessMixing(void* data);
void AudioThread_ProcessSpatialization(void* data);
void AudioThread_ProcessStreaming(void* data);
void AudioThread_ProcessEffects(void* data);

#endif // __SND_AUDIO_THREAD_H__
