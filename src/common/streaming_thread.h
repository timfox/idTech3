/*
=============================================================================
Streaming Thread System

Background asset streaming with priority management for seamless loading.
=============================================================================
*/

#ifndef __STREAMING_THREAD_H__
#define __STREAMING_THREAD_H__

#include "q_shared.h"
#include "q_lockfree.h"
#include "thread_platform.h"
#include "q_asset_loaders.h"

// Streaming thread types
typedef enum {
    STREAM_THREAD_GENERAL = 0,    // General asset loading
    STREAM_THREAD_TEXTURE,        // Texture streaming
    STREAM_THREAD_MODEL,          // Model streaming
    STREAM_THREAD_SOUND,          // Sound streaming
    STREAM_THREAD_SHADER,         // Shader streaming
    STREAM_THREAD_MAX
} stream_thread_type_t;

// Asset load priority levels
typedef enum {
    ASSET_PRIORITY_CRITICAL = 0,  // Must load immediately (blocking)
    ASSET_PRIORITY_HIGH,          // Load as soon as possible
    ASSET_PRIORITY_NORMAL,        // Standard loading priority
    ASSET_PRIORITY_LOW,           // Background loading
    ASSET_PRIORITY_IDLE,          // Load only when system idle
    ASSET_PRIORITY_MAX
} asset_priority_t;

// Asset load request
typedef struct {
    char assetName[MAX_QPATH];        // Asset name/path
    assetType_t assetType;           // Type of asset
    asset_priority_t priority;       // Loading priority
    uint64_t requestTime;            // When request was made
    uint32_t sequence;               // Sequence number for ordering

    // Asset-specific parameters
    union {
        struct {
            int flags;               // Texture flags
        } texture;
        struct {
            int pointSize;          // Font point size
            fontInfo_t *font;       // Font info structure
        } font;
        struct {
            sfxHandle_t *handle;    // Sound handle output
        } sound;
        struct {
            void *buffer;           // Config buffer
            int bufferSize;         // Buffer size
        } config;
    } params;

    // Callback on completion (optional)
    void (*completionCallback)(qhandle_t handle, void* userData);
    void* callbackUserData;

    // Result storage
    qhandle_t resultHandle;
    qboolean completed;
    qboolean success;
} asset_load_request_t;

// Streaming work item
typedef struct {
    void (*work_function)(void* data);
    void* work_data;
    uint64_t submit_time;
    stream_thread_type_t thread_type;
    asset_priority_t priority;
    uint32_t sequence;
} stream_work_item_t;

// Streaming thread data
typedef struct {
    stream_thread_type_t thread_type;
    thread_handle_t handle;
    condition_t work_available;
    mutex_t work_mutex;
    spinlock_t queue_lock;
    qboolean should_exit;

    // Work queues (one per priority level)
    lf_queue_t work_queues[ASSET_PRIORITY_MAX];

    // Thread-specific data
    void* thread_context;
    uint64_t total_work_items_processed;
    uint64_t total_execution_time_ns;
    float average_work_time_ms;
    uint64_t last_activity_time;

    // Statistics per priority
    uint64_t priority_processed[ASSET_PRIORITY_MAX];
    uint64_t priority_wait_time[ASSET_PRIORITY_MAX];
} stream_thread_data_t;

// Streaming thread system
typedef struct {
    qboolean enabled;
    qboolean initialized;

    // Thread management
    stream_thread_data_t threads[STREAM_THREAD_MAX];
    qboolean thread_enabled[STREAM_THREAD_MAX];

    // Global request queues (lock-free)
    lf_queue_t request_queues[ASSET_PRIORITY_MAX];

    // Asset cache and management
    qboolean cache_enabled;
    uint32_t max_cache_size;        // Max cached assets
    uint32_t current_cache_size;

    // Statistics
    atomic_uint64_t total_requests_submitted;
    atomic_uint64_t total_requests_completed;
    atomic_uint64_t total_requests_failed;
    atomic_uint64_t total_bytes_loaded;
    atomic_uint64_t cache_hits;
    atomic_uint64_t cache_misses;

    // Performance tracking
    uint64_t frame_start_time;
    float average_frame_time_ms;
    uint32_t max_queue_depth;
} stream_thread_system_t;

extern stream_thread_system_t stream_thread_system;

// Streaming Thread API
qboolean StreamThread_Init(void);
void StreamThread_Shutdown(void);

qboolean StreamThread_EnableThread(stream_thread_type_t threadType);
void StreamThread_DisableThread(stream_thread_type_t threadType);
qboolean StreamThread_IsThreadEnabled(stream_thread_type_t threadType);

// Asset loading requests
qboolean StreamThread_RequestAssetLoad(const char* assetName, assetType_t assetType,
                                      asset_priority_t priority, void* params,
                                      void (*callback)(qhandle_t, void*), void* userData);
qboolean StreamThread_RequestTextureLoad(const char* textureName, int flags, asset_priority_t priority);
qboolean StreamThread_RequestModelLoad(const char* modelName, asset_priority_t priority);
qboolean StreamThread_RequestSoundLoad(const char* soundName, sfxHandle_t* handle, asset_priority_t priority);
qboolean StreamThread_RequestShaderLoad(const char* shaderName, asset_priority_t priority);

// Synchronous loading (for critical assets)
qhandle_t StreamThread_LoadAssetSync(const char* assetName, assetType_t assetType, void* params);
qhandle_t StreamThread_LoadTextureSync(const char* textureName, int flags);
qhandle_t StreamThread_LoadModelSync(const char* modelName);

// Cache management
void StreamThread_EnableCache(qboolean enable);
void StreamThread_SetMaxCacheSize(uint32_t maxSize);
void StreamThread_ClearCache(void);
qboolean StreamThread_IsAssetCached(const char* assetName, assetType_t assetType);

// Work submission
void StreamThread_SubmitGeneralWork(void* workData, asset_priority_t priority);
void StreamThread_SubmitTextureWork(void* workData, asset_priority_t priority);
void StreamThread_SubmitModelWork(void* workData, asset_priority_t priority);
void StreamThread_SubmitSoundWork(void* workData, asset_priority_t priority);
void StreamThread_SubmitShaderWork(void* workData, asset_priority_t priority);

// Synchronization
void StreamThread_WaitForAllThreads(void);
void StreamThread_WaitForThread(stream_thread_type_t threadType);
void StreamThread_WaitForAsset(const char* assetName, assetType_t assetType);
void StreamThread_FlushQueues(asset_priority_t minPriority);

// Performance monitoring
void StreamThread_GetStats(stream_thread_type_t threadType,
                          uint64_t* processedItems,
                          float* avgTimeMs,
                          uint64_t* totalTimeNs);
void StreamThread_GetGlobalStats(uint64_t* submitted, uint64_t* completed,
                                uint64_t* failed, uint64_t* bytesLoaded,
                                uint64_t* cacheHits, uint64_t* cacheMisses);

// Asset processing functions (called by work items)
void StreamThread_ProcessGeneralLoad(void* data);
void StreamThread_ProcessTextureLoad(void* data);
void StreamThread_ProcessModelLoad(void* data);
void StreamThread_ProcessSoundLoad(void* data);
void StreamThread_ProcessShaderLoad(void* data);

#endif // __STREAMING_THREAD_H__
