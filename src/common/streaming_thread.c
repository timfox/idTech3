/*
=============================================================================
Streaming Thread System Implementation

Background asset streaming with priority management for seamless loading.
=============================================================================
*/

#include "streaming_thread.h"
#include "qcommon.h"
#include "q_memory_safety.h"
#include <string.h>

// Global streaming thread system instance
stream_thread_system_t stream_thread_system = {0};

/*
=============================================================================
Streaming Thread Worker Functions
=============================================================================
*/

// Priority comparison for work items (higher priority = lower number)
static int StreamWorkItemCompare(const void* a, const void* b) {
    const stream_work_item_t* itemA = (const stream_work_item_t*)a;
    const stream_work_item_t* itemB = (const stream_work_item_t*)b;

    // First compare by priority
    if (itemA->priority != itemB->priority) {
        return itemA->priority - itemB->priority;
    }

    // Then by sequence number for ordering within priority
    return (int)(itemA->sequence - itemB->sequence);
}

// Select next work item from priority queues (highest priority first)
static stream_work_item_t* StreamThread_SelectNextWork(stream_thread_data_t* thread) {
    // Check queues in priority order
    for (int priority = 0; priority < ASSET_PRIORITY_MAX; priority++) {
        if (!LF_Queue_IsEmpty(&thread->work_queues[priority])) {
            stream_work_item_t* work = (stream_work_item_t*)LF_Queue_Dequeue(&thread->work_queues[priority]);
            if (work) {
                thread->priority_processed[priority]++;
                return work;
            }
        }
    }
    return NULL;
}

// Main streaming thread worker function
static THREAD_RETURN THREAD_CALL StreamThreadWorker(void* arg) {
    stream_thread_data_t* thread = (stream_thread_data_t*)arg;

    // Set thread affinity for streaming (prefer specific cores)
    Thread_SetCurrentAffinity(1ULL << (thread->thread_type % Sys_GetCPUCount()));

    while (!thread->should_exit) {
        // Wait for work with timeout
        MUTEX_LOCK(thread->work_mutex);

        // Check if any work is available
        qboolean hasWork = qfalse;
        for (int i = 0; i < ASSET_PRIORITY_MAX; i++) {
            if (!LF_Queue_IsEmpty(&thread->work_queues[i])) {
                hasWork = qtrue;
                break;
            }
        }

        if (!hasWork && !thread->should_exit) {
            // Wait with 10ms timeout for streaming (less critical than network/audio)
            struct timespec timeout;
            timeout.tv_sec = 0;
            timeout.tv_nsec = 10000000; // 10ms

            CONDITION_TIMED_WAIT(thread->work_available, thread->work_mutex, &timeout);
        }
        MUTEX_UNLOCK(thread->work_mutex);

        if (thread->should_exit) break;

        // Process work items
        stream_work_item_t* work = StreamThread_SelectNextWork(thread);
        if (work) {
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

            // Free work item
            MEMORY_SAFETY_FREE(work);
        } else {
            // Yield to prevent busy-waiting
            Thread_Yield();
        }
    }

    return 0;
}

/*
=============================================================================
Asset Processing Functions
=============================================================================
*/

void StreamThread_ProcessGeneralLoad(void* data) {
    asset_load_request_t* request = (asset_load_request_t*)data;

    if (!request) return;

    request->completed = qtrue;

    // Process based on asset type
    switch (request->assetType) {
        case ASSET_TYPE_MODEL:
            request->resultHandle = Asset_LoadModel(request->assetName);
            request->success = (request->resultHandle != 0);
            break;
        case ASSET_TYPE_SHADER:
            request->resultHandle = Asset_LoadShader(request->assetName);
            request->success = (request->resultHandle != 0);
            break;
        case ASSET_TYPE_CONFIG:
            request->success = Asset_LoadConfig(request->assetName,
                                              request->params.config.buffer,
                                              request->params.config.bufferSize);
            break;
        default:
            request->success = qfalse;
            break;
    }

    // Call completion callback if provided
    if (request->completionCallback) {
        request->completionCallback(request->resultHandle, request->callbackUserData);
    }

    // Update statistics
    if (request->success) {
        atomic_fetch_add_explicit(&stream_thread_system.total_requests_completed, 1, memory_order_relaxed);
        // Estimate bytes loaded (rough approximation)
        atomic_fetch_add_explicit(&stream_thread_system.total_bytes_loaded,
                                strlen(request->assetName) * 100, memory_order_relaxed);
    } else {
        atomic_fetch_add_explicit(&stream_thread_system.total_requests_failed, 1, memory_order_relaxed);
    }
}

void StreamThread_ProcessTextureLoad(void* data) {
    asset_load_request_t* request = (asset_load_request_t*)data;

    if (!request) return;

    request->completed = qtrue;

    // Load texture with specified flags
    request->resultHandle = Asset_LoadTexture(request->assetName, request->params.texture.flags);
    request->success = (request->resultHandle != 0);

    // Call completion callback if provided
    if (request->completionCallback) {
        request->completionCallback(request->resultHandle, request->callbackUserData);
    }

    // Update statistics
    if (request->success) {
        atomic_fetch_add_explicit(&stream_thread_system.total_requests_completed, 1, memory_order_relaxed);
        // Textures typically larger, estimate 64KB per texture
        atomic_fetch_add_explicit(&stream_thread_system.total_bytes_loaded, 65536, memory_order_relaxed);
    } else {
        atomic_fetch_add_explicit(&stream_thread_system.total_requests_failed, 1, memory_order_relaxed);
    }
}

void StreamThread_ProcessModelLoad(void* data) {
    asset_load_request_t* request = (asset_load_request_t*)data;

    if (!request) return;

    request->completed = qtrue;

    // Load model
    request->resultHandle = Asset_LoadModel(request->assetName);
    request->success = (request->resultHandle != 0);

    // Call completion callback if provided
    if (request->completionCallback) {
        request->completionCallback(request->resultHandle, request->callbackUserData);
    }

    // Update statistics
    if (request->success) {
        atomic_fetch_add_explicit(&stream_thread_system.total_requests_completed, 1, memory_order_relaxed);
        // Models can be large, estimate 256KB per model
        atomic_fetch_add_explicit(&stream_thread_system.total_bytes_loaded, 262144, memory_order_relaxed);
    } else {
        atomic_fetch_add_explicit(&stream_thread_system.total_requests_failed, 1, memory_order_relaxed);
    }
}

void StreamThread_ProcessSoundLoad(void* data) {
    asset_load_request_t* request = (asset_load_request_t*)data;

    if (!request) return;

    request->completed = qtrue;

    // Load sound
    request->success = Asset_LoadSound(request->assetName, request->params.sound.handle);

    // Call completion callback if provided
    if (request->completionCallback) {
        request->completionCallback(request->success ? 1 : 0, request->callbackUserData);
    }

    // Update statistics
    if (request->success) {
        atomic_fetch_add_explicit(&stream_thread_system.total_requests_completed, 1, memory_order_relaxed);
        // Sounds vary greatly, estimate 32KB per sound
        atomic_fetch_add_explicit(&stream_thread_system.total_bytes_loaded, 32768, memory_order_relaxed);
    } else {
        atomic_fetch_add_explicit(&stream_thread_system.total_requests_failed, 1, memory_order_relaxed);
    }
}

void StreamThread_ProcessShaderLoad(void* data) {
    asset_load_request_t* request = (asset_load_request_t*)data;

    if (!request) return;

    request->completed = qtrue;

    // Load shader
    request->resultHandle = Asset_LoadShader(request->assetName);
    request->success = (request->resultHandle != 0);

    // Call completion callback if provided
    if (request->completionCallback) {
        request->completionCallback(request->resultHandle, request->callbackUserData);
    }

    // Update statistics
    if (request->success) {
        atomic_fetch_add_explicit(&stream_thread_system.total_requests_completed, 1, memory_order_relaxed);
        // Shaders are small, estimate 4KB per shader
        atomic_fetch_add_explicit(&stream_thread_system.total_bytes_loaded, 4096, memory_order_relaxed);
    } else {
        atomic_fetch_add_explicit(&stream_thread_system.total_requests_failed, 1, memory_order_relaxed);
    }
}

/*
=============================================================================
Streaming Thread System API
=============================================================================
*/

qboolean StreamThread_Init(void) {
    if (stream_thread_system.initialized) {
        return qtrue;
    }

    memset(&stream_thread_system, 0, sizeof(stream_thread_system_t));

    // Initialize atomic counters
    atomic_init(&stream_thread_system.total_requests_submitted, 0);
    atomic_init(&stream_thread_system.total_requests_completed, 0);
    atomic_init(&stream_thread_system.total_requests_failed, 0);
    atomic_init(&stream_thread_system.total_bytes_loaded, 0);
    atomic_init(&stream_thread_system.cache_hits, 0);
    atomic_init(&stream_thread_system.cache_misses, 0);

    // Initialize request queues
    for (int i = 0; i < ASSET_PRIORITY_MAX; i++) {
        if (!LF_Queue_Init(&stream_thread_system.request_queues[i], 256)) {
            Com_Printf("Failed to initialize request queue for priority %d\n", i);
            goto cleanup;
        }
    }

    // Initialize all threads as disabled
    for (int i = 0; i < STREAM_THREAD_MAX; i++) {
        stream_thread_system.thread_enabled[i] = qfalse;
    }

    // Default cache settings
    stream_thread_system.cache_enabled = qtrue;
    stream_thread_system.max_cache_size = 100; // Max 100 cached assets

    stream_thread_system.enabled = qtrue;
    stream_thread_system.initialized = qtrue;

    Com_Printf("Streaming thread system initialized\n");
    return qtrue;

cleanup:
    for (int i = 0; i < ASSET_PRIORITY_MAX; i++) {
        LF_Queue_Shutdown(&stream_thread_system.request_queues[i]);
    }
    return qfalse;
}

void StreamThread_Shutdown(void) {
    if (!stream_thread_system.initialized) {
        return;
    }

    // Disable all threads
    for (int i = 0; i < STREAM_THREAD_MAX; i++) {
        StreamThread_DisableThread((stream_thread_type_t)i);
    }

    // Shutdown request queues
    for (int i = 0; i < ASSET_PRIORITY_MAX; i++) {
        LF_Queue_Shutdown(&stream_thread_system.request_queues[i]);
    }

    stream_thread_system.initialized = qfalse;
    Com_Printf("Streaming thread system shutdown\n");
}

qboolean StreamThread_EnableThread(stream_thread_type_t threadType) {
    if (threadType >= STREAM_THREAD_MAX || !stream_thread_system.initialized) {
        return qfalse;
    }

    if (stream_thread_system.thread_enabled[threadType]) {
        return qtrue; // Already enabled
    }

    stream_thread_data_t* thread = &stream_thread_system.threads[threadType];
    memset(thread, 0, sizeof(stream_thread_data_t));

    thread->thread_type = threadType;
    thread->should_exit = qfalse;

    // Initialize priority work queues
    for (int i = 0; i < ASSET_PRIORITY_MAX; i++) {
        if (!LF_Queue_Init(&thread->work_queues[i], 128)) {
            Com_Printf("Failed to initialize work queue %d for thread %d\n", i, threadType);
            goto cleanup_queues;
        }
    }

    // Initialize synchronization primitives
    MUTEX_INIT(thread->work_mutex);
    CONDITION_INIT(thread->work_available);

    // Start thread
    const char* threadNames[STREAM_THREAD_MAX] = {
        "Stream_General", "Stream_Texture", "Stream_Model", "Stream_Sound", "Stream_Shader"
    };

    if (!Thread_Create(&thread->handle, StreamThreadWorker, thread, threadNames[threadType],
                      THREAD_PRIORITY_NORMAL)) {
        Com_Printf("Failed to create streaming thread %d\n", threadType);
        goto cleanup;
    }

    stream_thread_system.thread_enabled[threadType] = qtrue;

    Com_Printf("Enabled streaming thread: %s\n", threadNames[threadType]);
    return qtrue;

cleanup:
    CONDITION_DESTROY(thread->work_available);
    MUTEX_DESTROY(thread->work_mutex);
cleanup_queues:
    for (int i = 0; i < ASSET_PRIORITY_MAX; i++) {
        LF_Queue_Shutdown(&thread->work_queues[i]);
    }
    return qfalse;
}

void StreamThread_DisableThread(stream_thread_type_t threadType) {
    if (threadType >= STREAM_THREAD_MAX ||
        !stream_thread_system.thread_enabled[threadType]) {
        return;
    }

    stream_thread_data_t* thread = &stream_thread_system.threads[threadType];

    // Signal thread to exit
    MUTEX_LOCK(thread->work_mutex);
    thread->should_exit = qtrue;
    CONDITION_SIGNAL(thread->work_available);
    MUTEX_UNLOCK(thread->work_mutex);

    // Wait for thread to finish
    Thread_Join(thread->handle);

    // Cleanup resources
    for (int i = 0; i < ASSET_PRIORITY_MAX; i++) {
        LF_Queue_Shutdown(&thread->work_queues[i]);
    }

    CONDITION_DESTROY(thread->work_available);
    MUTEX_DESTROY(thread->work_mutex);

    stream_thread_system.thread_enabled[threadType] = qfalse;

    const char* threadNames[STREAM_THREAD_MAX] = {
        "Stream_General", "Stream_Texture", "Stream_Model", "Stream_Sound", "Stream_Shader"
    };
    Com_Printf("Disabled streaming thread: %s\n", threadNames[threadType]);
}

qboolean StreamThread_IsThreadEnabled(stream_thread_type_t threadType) {
    if (threadType >= STREAM_THREAD_MAX) return qfalse;
    return stream_thread_system.thread_enabled[threadType];
}

/*
=============================================================================
Asset Loading Request Functions
=============================================================================
*/

qboolean StreamThread_RequestAssetLoad(const char* assetName, assetType_t assetType,
                                      asset_priority_t priority, void* params,
                                      void (*callback)(qhandle_t, void*), void* userData) {
    if (!stream_thread_system.initialized || !assetName || priority >= ASSET_PRIORITY_MAX) {
        return qfalse;
    }

    // For critical priority, load synchronously
    if (priority == ASSET_PRIORITY_CRITICAL) {
        qhandle_t handle = StreamThread_LoadAssetSync(assetName, assetType, params);
        if (callback) {
            callback(handle, userData);
        }
        return qtrue;
    }

    // Create load request
    asset_load_request_t* request = (asset_load_request_t*)MEMORY_SAFETY_MALLOC(sizeof(asset_load_request_t));
    if (!request) return qfalse;

    memset(request, 0, sizeof(asset_load_request_t));
    Q_strncpyz(request->assetName, assetName, sizeof(request->assetName));
    request->assetType = assetType;
    request->priority = priority;
    request->requestTime = Sys_Milliseconds() * 1000000ULL;
    request->sequence = 0; // Could be atomic increment

    // Copy parameters based on type
    if (params) {
        switch (assetType) {
            case ASSET_TYPE_TEXTURE:
                memcpy(&request->params.texture, params, sizeof(request->params.texture));
                break;
            case ASSET_TYPE_FONT:
                memcpy(&request->params.font, params, sizeof(request->params.font));
                break;
            case ASSET_TYPE_SOUND:
                memcpy(&request->params.sound, params, sizeof(request->params.sound));
                break;
            case ASSET_TYPE_CONFIG:
                memcpy(&request->params.config, params, sizeof(request->params.config));
                break;
            default:
                break;
        }
    }

    request->completionCallback = callback;
    request->callbackUserData = userData;

    // Add to appropriate thread based on asset type
    stream_thread_type_t threadType;
    void (*workFunction)(void*) = NULL;

    switch (assetType) {
        case ASSET_TYPE_TEXTURE:
            threadType = STREAM_THREAD_TEXTURE;
            workFunction = StreamThread_ProcessTextureLoad;
            break;
        case ASSET_TYPE_MODEL:
            threadType = STREAM_THREAD_MODEL;
            workFunction = StreamThread_ProcessModelLoad;
            break;
        case ASSET_TYPE_SOUND:
            threadType = STREAM_THREAD_SOUND;
            workFunction = StreamThread_ProcessSoundLoad;
            break;
        case ASSET_TYPE_SHADER:
            threadType = STREAM_THREAD_SHADER;
            workFunction = StreamThread_ProcessShaderLoad;
            break;
        default:
            threadType = STREAM_THREAD_GENERAL;
            workFunction = StreamThread_ProcessGeneralLoad;
            break;
    }

    // Submit work to thread
    StreamThread_SubmitGeneralWork(request, priority);

    atomic_fetch_add_explicit(&stream_thread_system.total_requests_submitted, 1, memory_order_relaxed);

    return qtrue;
}

qboolean StreamThread_RequestTextureLoad(const char* textureName, int flags, asset_priority_t priority) {
    struct {
        int flags;
    } params = { flags };

    return StreamThread_RequestAssetLoad(textureName, ASSET_TYPE_TEXTURE, priority, &params, NULL, NULL);
}

qboolean StreamThread_RequestModelLoad(const char* modelName, asset_priority_t priority) {
    return StreamThread_RequestAssetLoad(modelName, ASSET_TYPE_MODEL, priority, NULL, NULL, NULL);
}

qboolean StreamThread_RequestSoundLoad(const char* soundName, sfxHandle_t* handle, asset_priority_t priority) {
    struct {
        sfxHandle_t* handle;
    } params = { handle };

    return StreamThread_RequestAssetLoad(soundName, ASSET_TYPE_SOUND, priority, &params, NULL, NULL);
}

qboolean StreamThread_RequestShaderLoad(const char* shaderName, asset_priority_t priority) {
    return StreamThread_RequestAssetLoad(shaderName, ASSET_TYPE_SHADER, priority, NULL, NULL, NULL);
}

/*
=============================================================================
Synchronous Loading Functions
=============================================================================
*/

qhandle_t StreamThread_LoadAssetSync(const char* assetName, assetType_t assetType, void* params) {
    switch (assetType) {
        case ASSET_TYPE_MODEL:
            return Asset_LoadModel(assetName);
        case ASSET_TYPE_SHADER:
            return Asset_LoadShader(assetName);
        case ASSET_TYPE_TEXTURE: {
            int flags = 0;
            if (params) {
                flags = ((struct { int flags; }*)params)->flags;
            }
            return Asset_LoadTexture(assetName, flags);
        }
        case ASSET_TYPE_SOUND: {
            sfxHandle_t handle;
            if (Asset_LoadSound(assetName, &handle)) {
                return handle;
            }
            return 0;
        }
        case ASSET_TYPE_FONT: {
            if (params) {
                struct { int pointSize; fontInfo_t *font; }* fontParams = params;
                return Asset_LoadFont(assetName, fontParams->pointSize, fontParams->font);
            }
            return 0;
        }
        case ASSET_TYPE_CONFIG: {
            if (params) {
                struct { void *buffer; int bufferSize; }* configParams = params;
                Asset_LoadConfig(assetName, configParams->buffer, configParams->bufferSize);
                return 1; // Config loading doesn't return a handle
            }
            return 0;
        }
        default:
            return 0;
    }
}

qhandle_t StreamThread_LoadTextureSync(const char* textureName, int flags) {
    return Asset_LoadTexture(textureName, flags);
}

qhandle_t StreamThread_LoadModelSync(const char* modelName) {
    return Asset_LoadModel(modelName);
}

/*
=============================================================================
Cache Management Functions
=============================================================================
*/

void StreamThread_EnableCache(qboolean enable) {
    stream_thread_system.cache_enabled = enable;
}

void StreamThread_SetMaxCacheSize(uint32_t maxSize) {
    stream_thread_system.max_cache_size = maxSize;
}

void StreamThread_ClearCache(void) {
    // Implementation would clear the asset cache
    // For now, this is a placeholder
    stream_thread_system.current_cache_size = 0;
}

qboolean StreamThread_IsAssetCached(const char* assetName, assetType_t assetType) {
    // Implementation would check if asset is in cache
    // For now, always return false (no caching implemented yet)
    Q_UNUSED(assetName);
    Q_UNUSED(assetType);
    return qfalse;
}

/*
=============================================================================
Work Submission Functions
=============================================================================
*/

static void SubmitStreamWork(stream_thread_type_t threadType, void* workData,
                            void (*workFunction)(void*), asset_priority_t priority) {
    if (threadType >= STREAM_THREAD_MAX ||
        !stream_thread_system.thread_enabled[threadType] ||
        priority >= ASSET_PRIORITY_MAX) {
        // Execute immediately if thread not available
        workFunction(workData);
        return;
    }

    stream_thread_data_t* thread = &stream_thread_system.threads[threadType];

    // Create work item
    stream_work_item_t* workItem = (stream_work_item_t*)MEMORY_SAFETY_MALLOC(sizeof(stream_work_item_t));
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

    // Add to appropriate priority queue
    if (!LF_Queue_Enqueue(&thread->work_queues[priority], workItem)) {
        // Queue full - execute immediately as fallback
        MEMORY_SAFETY_FREE(workItem);
        workFunction(workData);
        return;
    }

    // Signal thread
    MUTEX_LOCK(thread->work_mutex);
    CONDITION_SIGNAL(thread->work_available);
    MUTEX_UNLOCK(thread->work_mutex);
}

void StreamThread_SubmitGeneralWork(void* workData, asset_priority_t priority) {
    SubmitStreamWork(STREAM_THREAD_GENERAL, workData, StreamThread_ProcessGeneralLoad, priority);
}

void StreamThread_SubmitTextureWork(void* workData, asset_priority_t priority) {
    SubmitStreamWork(STREAM_THREAD_TEXTURE, workData, StreamThread_ProcessTextureLoad, priority);
}

void StreamThread_SubmitModelWork(void* workData, asset_priority_t priority) {
    SubmitStreamWork(STREAM_THREAD_MODEL, workData, StreamThread_ProcessModelLoad, priority);
}

void StreamThread_SubmitSoundWork(void* workData, asset_priority_t priority) {
    SubmitStreamWork(STREAM_THREAD_SOUND, workData, StreamThread_ProcessSoundLoad, priority);
}

void StreamThread_SubmitShaderWork(void* workData, asset_priority_t priority) {
    SubmitStreamWork(STREAM_THREAD_SHADER, workData, StreamThread_ProcessShaderLoad, priority);
}

/*
=============================================================================
Synchronization Functions
=============================================================================
*/

void StreamThread_WaitForAllThreads(void) {
    for (int i = 0; i < STREAM_THREAD_MAX; i++) {
        if (stream_thread_system.thread_enabled[i]) {
            StreamThread_WaitForThread((stream_thread_type_t)i);
        }
    }
}

void StreamThread_WaitForThread(stream_thread_type_t threadType) {
    if (threadType >= STREAM_THREAD_MAX ||
        !stream_thread_system.thread_enabled[threadType]) {
        return;
    }

    stream_thread_data_t* thread = &stream_thread_system.threads[threadType];

    // Wait until all work queues are empty
    qboolean hasWork = qtrue;
    while (hasWork) {
        hasWork = qfalse;
        for (int i = 0; i < ASSET_PRIORITY_MAX; i++) {
            if (!LF_Queue_IsEmpty(&thread->work_queues[i])) {
                hasWork = qtrue;
                break;
            }
        }
        if (hasWork) {
            Thread_Sleep(1); // Small sleep to avoid busy waiting
        }
    }
}

void StreamThread_WaitForAsset(const char* assetName, assetType_t assetType) {
    // Implementation would wait for specific asset to complete loading
    // For now, this is a placeholder
    Q_UNUSED(assetName);
    Q_UNUSED(assetType);
}

void StreamThread_FlushQueues(asset_priority_t minPriority) {
    // Process all queued requests at or above minimum priority
    for (int priority = 0; priority <= minPriority; priority++) {
        void* item;
        while ((item = LF_Queue_Dequeue(&stream_thread_system.request_queues[priority])) != NULL) {
            asset_load_request_t* request = (asset_load_request_t*)item;
            // Process immediately
            switch (request->assetType) {
                case ASSET_TYPE_TEXTURE:
                    StreamThread_ProcessTextureLoad(request);
                    break;
                case ASSET_TYPE_MODEL:
                    StreamThread_ProcessModelLoad(request);
                    break;
                case ASSET_TYPE_SOUND:
                    StreamThread_ProcessSoundLoad(request);
                    break;
                case ASSET_TYPE_SHADER:
                    StreamThread_ProcessShaderLoad(request);
                    break;
                default:
                    StreamThread_ProcessGeneralLoad(request);
                    break;
            }
            MEMORY_SAFETY_FREE(request);
        }
    }
}

/*
=============================================================================
Performance Monitoring
=============================================================================
*/

void StreamThread_GetStats(stream_thread_type_t threadType,
                          uint64_t* processedItems,
                          float* avgTimeMs,
                          uint64_t* totalTimeNs) {
    if (threadType >= STREAM_THREAD_MAX ||
        !stream_thread_system.thread_enabled[threadType]) {
        if (processedItems) *processedItems = 0;
        if (avgTimeMs) *avgTimeMs = 0.0f;
        if (totalTimeNs) *totalTimeNs = 0;
        return;
    }

    stream_thread_data_t* thread = &stream_thread_system.threads[threadType];
    if (processedItems) *processedItems = thread->total_work_items_processed;
    if (avgTimeMs) *avgTimeMs = thread->average_work_time_ms;
    if (totalTimeNs) *totalTimeNs = thread->total_execution_time_ns;
}

void StreamThread_GetGlobalStats(uint64_t* submitted, uint64_t* completed,
                                uint64_t* failed, uint64_t* bytesLoaded,
                                uint64_t* cacheHits, uint64_t* cacheMisses) {
    if (submitted) *submitted = atomic_load_explicit(&stream_thread_system.total_requests_submitted, memory_order_relaxed);
    if (completed) *completed = atomic_load_explicit(&stream_thread_system.total_requests_completed, memory_order_relaxed);
    if (failed) *failed = atomic_load_explicit(&stream_thread_system.total_requests_failed, memory_order_relaxed);
    if (bytesLoaded) *bytesLoaded = atomic_load_explicit(&stream_thread_system.total_bytes_loaded, memory_order_relaxed);
    if (cacheHits) *cacheHits = atomic_load_explicit(&stream_thread_system.cache_hits, memory_order_relaxed);
    if (cacheMisses) *cacheMisses = atomic_load_explicit(&stream_thread_system.cache_misses, memory_order_relaxed);
}
