<?php
/**
 * Threading and Concurrency - Modern Multi-threading for id Tech 3
 */
$title = 'Threading and Concurrency - id Tech 3 Documentation';
$breadcrumbs = [
    '/platform' => 'Platform and Deployment',
    '/platform/threading-concurrency' => 'Threading and Concurrency'
];
?>

<h1>Threading and Concurrency - Modern Multi-threading Approaches</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Modern id Tech 3 with PBR and Vulkan requires sophisticated threading to fully utilize multi-core processors and modern GPU architectures. This guide covers practical threading implementations used in JKSunny's PBR port for maximum performance across platforms.</p>
    
    <div class="feature-list">
        <h3>Threading Architecture Goals</h3>
        <ul>
            <li><strong>Vulkan Command Buffer Recording:</strong> Multi-threaded rendering submission</li>
            <li><strong>Asset Streaming:</strong> Background loading of textures and models</li>
            <li><strong>PBR Computations:</strong> Parallel IBL and lighting calculations</li>
            <li><strong>Frame Pipelining:</strong> CPU/GPU work overlap for higher throughput</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Cross-Platform Threading Foundation</h2>
    
    <h3>Platform Thread Abstraction</h3>
    <div class="code-block">
        <pre><code>// thread_platform.h - Cross-platform threading for PBR port
#ifndef THREAD_PLATFORM_H
#define THREAD_PLATFORM_H

#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    #include <process.h>
    
    typedef HANDLE thread_t;
    typedef CRITICAL_SECTION mutex_t;
    typedef CONDITION_VARIABLE condition_t;
    typedef SRWLOCK rwlock_t;
    typedef volatile LONG atomic_int_t;
    
    #define THREAD_CALL WINAPI
    #define THREAD_RETURN DWORD
    
#else
    #include <pthread.h>
    #include <unistd.h>
    #include <sched.h>
    
    typedef pthread_t thread_t;
    typedef pthread_mutex_t mutex_t;
    typedef pthread_cond_t condition_t;
    typedef pthread_rwlock_t rwlock_t;
    typedef volatile int atomic_int_t;
    
    #define THREAD_CALL
    #define THREAD_RETURN void*
#endif

// Thread priority levels
typedef enum {
    THREAD_PRIORITY_LOW,
    THREAD_PRIORITY_NORMAL,
    THREAD_PRIORITY_HIGH,
    THREAD_PRIORITY_CRITICAL
} threadPriority_t;

// Thread affinity for CPU core binding
typedef struct {
    int coreCount;
    int coreMask;
} threadAffinity_t;

// Cross-platform thread creation
typedef struct {
    thread_t handle;
    const char* name;
    threadPriority_t priority;
    threadAffinity_t affinity;
    qboolean running;
    atomic_int_t shouldStop;
} platformThread_t;

// Thread management functions
qboolean Thread_Create(platformThread_t* thread, THREAD_RETURN (THREAD_CALL *func)(void*), 
                      void* param, const char* name, threadPriority_t priority);
void Thread_Join(platformThread_t* thread);
void Thread_SetPriority(platformThread_t* thread, threadPriority_t priority);
void Thread_SetAffinity(platformThread_t* thread, int coreMask);
void Thread_Yield(void);
void Thread_Sleep(int milliseconds);

// Atomic operations
int Atomic_Increment(atomic_int_t* value);
int Atomic_Decrement(atomic_int_t* value);
int Atomic_Add(atomic_int_t* value, int amount);
int Atomic_CompareExchange(atomic_int_t* dest, int exchange, int compare);

// Implementation
qboolean Thread_Create(platformThread_t* thread, THREAD_RETURN (THREAD_CALL *func)(void*),
                      void* param, const char* name, threadPriority_t priority) {
    thread->name = name;
    thread->priority = priority;
    thread->running = qtrue;
    thread->shouldStop = 0;
    
#ifdef PLATFORM_WINDOWS
    thread->handle = (HANDLE)_beginthreadex(NULL, 0, func, param, 0, NULL);
    
    if (!thread->handle) {
        Com_Printf("^1Failed to create thread %s\n", name);
        return qfalse;
    }
    
    // Set thread priority
    int winPriority = THREAD_PRIORITY_NORMAL;
    switch (priority) {
    case THREAD_PRIORITY_LOW: winPriority = THREAD_PRIORITY_BELOW_NORMAL; break;
    case THREAD_PRIORITY_HIGH: winPriority = THREAD_PRIORITY_ABOVE_NORMAL; break;
    case THREAD_PRIORITY_CRITICAL: winPriority = THREAD_PRIORITY_TIME_CRITICAL; break;
    }
    SetThreadPriority(thread->handle, winPriority);
    
#else
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    
    // Set scheduling policy and priority
    struct sched_param param_sched;
    param_sched.sched_priority = sched_get_priority_min(SCHED_OTHER);
    
    switch (priority) {
    case THREAD_PRIORITY_HIGH:
        param_sched.sched_priority = sched_get_priority_max(SCHED_OTHER);
        break;
    case THREAD_PRIORITY_CRITICAL:
        pthread_attr_setschedpolicy(&attr, SCHED_RR);
        param_sched.sched_priority = sched_get_priority_max(SCHED_RR);
        break;
    }
    
    pthread_attr_setschedparam(&attr, &param_sched);
    
    if (pthread_create(&thread->handle, &attr, func, param) != 0) {
        Com_Printf("^1Failed to create thread %s\n", name);
        pthread_attr_destroy(&attr);
        return qfalse;
    }
    
    pthread_attr_destroy(&attr);
    
    // Set thread name for debugging
#ifdef __linux__
    pthread_setname_np(thread->handle, name);
#elif defined(__APPLE__)
    pthread_setname_np(name);
#endif
#endif
    
    return qtrue;
}</code></pre>
    </div>
    
    <h3>Lock-Free Data Structures</h3>
    <div class="code-block">
        <pre><code>// lockfree.h - Lock-free data structures for high-performance threading
#include <stdatomic.h>

// Lock-free ring buffer for command queues
typedef struct {
    _Atomic(int) head;
    _Atomic(int) tail;
    int capacity;
    int mask;  // capacity - 1 (capacity must be power of 2)
    void** data;
} lockfreeQueue_t;

// Lock-free stack for memory pools
typedef struct lfNode_s {
    struct lfNode_s* next;
    void* data;
} lfNode_t;

typedef struct {
    _Atomic(lfNode_t*) head;
    _Atomic(int) count;
} lockfreeStack_t;

// Ring buffer operations
qboolean LockfreeQueue_Init(lockfreeQueue_t* queue, int capacity) {
    // Ensure capacity is power of 2
    if ((capacity & (capacity - 1)) != 0) {
        Com_Printf("^1LockfreeQueue capacity must be power of 2\n");
        return qfalse;
    }
    
    queue->capacity = capacity;
    queue->mask = capacity - 1;
    queue->data = Z_Malloc(sizeof(void*) * capacity);
    atomic_store(&queue->head, 0);
    atomic_store(&queue->tail, 0);
    
    return qtrue;
}

qboolean LockfreeQueue_Enqueue(lockfreeQueue_t* queue, void* item) {
    int tail = atomic_load(&queue->tail);
    int next_tail = (tail + 1) & queue->mask;
    
    // Check if queue is full
    if (next_tail == atomic_load(&queue->head)) {
        return qfalse; // Queue full
    }
    
    queue->data[tail] = item;
    atomic_store(&queue->tail, next_tail);
    
    return qtrue;
}

void* LockfreeQueue_Dequeue(lockfreeQueue_t* queue) {
    int head = atomic_load(&queue->head);
    
    // Check if queue is empty
    if (head == atomic_load(&queue->tail)) {
        return NULL; // Queue empty
    }
    
    void* item = queue->data[head];
    atomic_store(&queue->head, (head + 1) & queue->mask);
    
    return item;
}

// Lock-free stack operations for memory management
void LockfreeStack_Init(lockfreeStack_t* stack) {
    atomic_store(&stack->head, NULL);
    atomic_store(&stack->count, 0);
}

void LockfreeStack_Push(lockfreeStack_t* stack, lfNode_t* node) {
    lfNode_t* old_head;
    
    do {
        old_head = atomic_load(&stack->head);
        node->next = old_head;
    } while (!atomic_compare_exchange_weak(&stack->head, &old_head, node));
    
    atomic_fetch_add(&stack->count, 1);
}

lfNode_t* LockfreeStack_Pop(lockfreeStack_t* stack) {
    lfNode_t* head;
    lfNode_t* next;
    
    do {
        head = atomic_load(&stack->head);
        if (!head) {
            return NULL; // Stack empty
        }
        next = head->next;
    } while (!atomic_compare_exchange_weak(&stack->head, &head, next));
    
    atomic_fetch_sub(&stack->count, 1);
    return head;
}

// Work-stealing queue for load balancing
typedef struct {
    _Atomic(int) top;    // Only owner thread modifies
    _Atomic(int) bottom; // Only owner thread modifies  
    _Atomic(void**) buffer;
    int capacity;
    int mask;
} workStealQueue_t;

qboolean WorkStealQueue_Init(workStealQueue_t* queue, int capacity) {
    if ((capacity & (capacity - 1)) != 0) {
        return qfalse; // Must be power of 2
    }
    
    queue->capacity = capacity;
    queue->mask = capacity - 1;
    
    void** buffer = Z_Malloc(sizeof(void*) * capacity);
    atomic_store(&queue->buffer, buffer);
    atomic_store(&queue->top, 0);
    atomic_store(&queue->bottom, 0);
    
    return qtrue;
}

// Owner thread pushes work (LIFO order for cache locality)
qboolean WorkStealQueue_Push(workStealQueue_t* queue, void* work) {
    int bottom = atomic_load_explicit(&queue->bottom, memory_order_relaxed);
    int top = atomic_load_explicit(&queue->top, memory_order_acquire);
    
    void** buffer = atomic_load_explicit(&queue->buffer, memory_order_relaxed);
    
    if (bottom - top >= queue->capacity) {
        return qfalse; // Queue full
    }
    
    buffer[bottom & queue->mask] = work;
    atomic_store_explicit(&queue->bottom, bottom + 1, memory_order_release);
    
    return qtrue;
}

// Owner thread pops work (LIFO)
void* WorkStealQueue_Pop(workStealQueue_t* queue) {
    int bottom = atomic_load_explicit(&queue->bottom, memory_order_relaxed) - 1;
    void** buffer = atomic_load_explicit(&queue->buffer, memory_order_relaxed);
    
    atomic_store_explicit(&queue->bottom, bottom, memory_order_relaxed);
    
    int top = atomic_load_explicit(&queue->top, memory_order_relaxed);
    
    if (top <= bottom) {
        void* work = buffer[bottom & queue->mask];
        
        if (top == bottom) {
            // Last element - need synchronization
            if (!atomic_compare_exchange_strong_explicit(&queue->top, &top, top + 1,
                                                        memory_order_seq_cst,
                                                        memory_order_relaxed)) {
                work = NULL; // Lost race
            }
            atomic_store_explicit(&queue->bottom, bottom + 1, memory_order_relaxed);
        }
        
        return work;
    } else {
        // Queue empty
        atomic_store_explicit(&queue->bottom, bottom + 1, memory_order_relaxed);
        return NULL;
    }
}

// Other threads steal work (FIFO order)
void* WorkStealQueue_Steal(workStealQueue_t* queue) {
    int top = atomic_load_explicit(&queue->top, memory_order_acquire);
    int bottom = atomic_load_explicit(&queue->bottom, memory_order_acquire);
    
    if (top < bottom) {
        void** buffer = atomic_load_explicit(&queue->buffer, memory_order_consume);
        void* work = buffer[top & queue->mask];
        
        if (!atomic_compare_exchange_strong_explicit(&queue->top, &top, top + 1,
                                                    memory_order_seq_cst,
                                                    memory_order_relaxed)) {
            return NULL; // Lost race
        }
        
        return work;
    }
    
    return NULL; // Queue empty
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Vulkan Multi-threaded Rendering</h2>
    
    <h3>Command Buffer Recording Architecture</h3>
    <div class="code-block">
        <pre><code>// vk_threading.h - Multi-threaded Vulkan command recording for PBR
#define MAX_RENDER_THREADS 8
#define MAX_COMMAND_BUFFERS_PER_THREAD 4

typedef struct renderThread_s {
    platformThread_t thread;
    int threadIndex;
    
    // Vulkan objects per thread
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffers[MAX_COMMAND_BUFFERS_PER_THREAD];
    VkCommandBuffer currentCommandBuffer;
    
    // Thread-local state
    qboolean recording;
    int currentFrame;
    
    // Work queue
    workStealQueue_t workQueue;
    
    // Synchronization
    mutex_t stateMutex;
    condition_t workAvailable;
    
} renderThread_t;

typedef struct renderWork_s {
    enum {
        WORK_DRAW_ENTITIES,
        WORK_DRAW_WORLD,
        WORK_DRAW_PARTICLES,
        WORK_COMPUTE_LIGHTING,
        WORK_UPDATE_UNIFORMS
    } type;
    
    union {
        struct {
            refEntity_t* entities;
            int entityCount;
            frustum_t frustum;
        } drawEntities;
        
        struct {
            msurface_t* surfaces;
            int surfaceCount;
            vec3_t viewOrigin;
        } drawWorld;
        
        struct {
            particle_t* particles;
            int particleCount;
            matrix_t viewMatrix;
        } drawParticles;
        
        struct {
            light_t* lights;
            int lightCount;
            vec3_t viewOrigin;
        } computeLighting;
    } data;
    
} renderWork_t;

// Global rendering thread pool
static struct {
    renderThread_t threads[MAX_RENDER_THREADS];
    int threadCount;
    qboolean initialized;
    
    // Frame synchronization
    VkSemaphore frameStartSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore frameEndSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence frameFences[MAX_FRAMES_IN_FLIGHT];
    
    // Global work distribution
    lockfreeQueue_t globalWorkQueue;
    atomic_int_t activeThreads;
    
} renderThreadPool;

qboolean RenderThreads_Init(void) {
    if (renderThreadPool.initialized) {
        return qtrue;
    }
    
    // Determine optimal thread count
    renderThreadPool.threadCount = min(platform.cpuThreads - 1, MAX_RENDER_THREADS);
    renderThreadPool.threadCount = max(renderThreadPool.threadCount, 1);
    
    Com_Printf("Initializing %d render threads\n", renderThreadPool.threadCount);
    
    // Initialize global work queue
    if (!LockfreeQueue_Init(&renderThreadPool.globalWorkQueue, 1024)) {
        Com_Printf("^1Failed to initialize global work queue\n");
        return qfalse;
    }
    
    // Create Vulkan synchronization objects
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkSemaphoreCreateInfo semaphoreInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        
        VK_CHECK(vkCreateSemaphore(vk.device, &semaphoreInfo, NULL, 
                                  &renderThreadPool.frameStartSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(vk.device, &semaphoreInfo, NULL, 
                                  &renderThreadPool.frameEndSemaphores[i]));
        
        VkFenceCreateInfo fenceInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        
        VK_CHECK(vkCreateFence(vk.device, &fenceInfo, NULL, 
                              &renderThreadPool.frameFences[i]));
    }
    
    // Initialize render threads
    for (int i = 0; i < renderThreadPool.threadCount; i++) {
        if (!RenderThread_Init(&renderThreadPool.threads[i], i)) {
            Com_Printf("^1Failed to initialize render thread %d\n", i);
            return qfalse;
        }
    }
    
    atomic_store(&renderThreadPool.activeThreads, 0);
    renderThreadPool.initialized = qtrue;
    
    return qtrue;
}

qboolean RenderThread_Init(renderThread_t* thread, int index) {
    thread->threadIndex = index;
    thread->recording = qfalse;
    thread->currentFrame = 0;
    
    // Create thread-specific command pool
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vk.queueFamilyIndex
    };
    
    VK_CHECK(vkCreateCommandPool(vk.device, &poolInfo, NULL, &thread->commandPool));
    
    // Allocate command buffers
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = thread->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
        .commandBufferCount = MAX_COMMAND_BUFFERS_PER_THREAD
    };
    
    VK_CHECK(vkAllocateCommandBuffers(vk.device, &allocInfo, thread->commandBuffers));
    
    // Initialize work queue
    if (!WorkStealQueue_Init(&thread->workQueue, 256)) {
        Com_Printf("^1Failed to initialize thread work queue\n");
        return qfalse;
    }
    
    // Initialize synchronization
    Mutex_Init(&thread->stateMutex);
    Condition_Init(&thread->workAvailable);
    
    // Start thread
    char threadName[32];
    Com_sprintf(threadName, sizeof(threadName), "RenderThread_%d", index);
    
    if (!Thread_Create(&thread->thread, RenderThread_Main, thread, 
                      threadName, THREAD_PRIORITY_HIGH)) {
        Com_Printf("^1Failed to create render thread %d\n", index);
        return qfalse;
    }
    
    // Set thread affinity to specific cores for better cache performance
    int coreMask = 1 << (index + 1); // Skip core 0 (main thread)
    Thread_SetAffinity(&thread->thread, coreMask);
    
    return qtrue;
}

THREAD_RETURN THREAD_CALL RenderThread_Main(void* param) {
    renderThread_t* thread = (renderThread_t*)param;
    
    Com_DPrintf("Render thread %d started\n", thread->threadIndex);
    
    while (!atomic_load(&thread->thread.shouldStop)) {
        // Try to get work from local queue first
        renderWork_t* work = WorkStealQueue_Pop(&thread->workQueue);
        
        if (!work) {
            // Try to steal work from global queue
            work = LockfreeQueue_Dequeue(&renderThreadPool.globalWorkQueue);
        }
        
        if (!work) {
            // Try to steal from other threads
            for (int i = 0; i < renderThreadPool.threadCount; i++) {
                if (i == thread->threadIndex) continue;
                
                work = WorkStealQueue_Steal(&renderThreadPool.threads[i].workQueue);
                if (work) break;
            }
        }
        
        if (work) {
            // Process work item
            RenderThread_ProcessWork(thread, work);
            
            // Free work item
            Z_Free(work);
        } else {
            // No work available, wait briefly
            Thread_Sleep(1);
        }
    }
    
    Com_DPrintf("Render thread %d stopped\n", thread->threadIndex);
    
#ifdef PLATFORM_WINDOWS
    return 0;
#else
    return NULL;
#endif
}</code></pre>
    </div>
    
    <h3>PBR Lighting Multi-threading</h3>
    <div class="code-block">
        <pre><code>// pbr_threading.h - Multi-threaded PBR lighting calculations
typedef struct pbrWorkItem_s {
    enum {
        PBR_WORK_IBL_SPECULAR,     // Image-based lighting specular
        PBR_WORK_IBL_DIFFUSE,      // Image-based lighting diffuse
        PBR_WORK_SHADOW_MAPS,      // Shadow map generation
        PBR_WORK_LIGHT_PROBES,     // Light probe updates
        PBR_WORK_MATERIAL_PARAMS   // Material parameter updates
    } type;
    
    union {
        struct {
            vec3_t viewPos;
            vec3_t normal;
            float roughness;
            float metallic;
            vec3_t baseColor;
            vec3_t* result;
        } iblSpecular;
        
        struct {
            vec3_t normal;
            vec3_t baseColor;
            vec3_t* result;
        } iblDiffuse;
        
        struct {
            light_t* light;
            msurface_t* surfaces;
            int surfaceCount;
            matrix_t lightMatrix;
        } shadowMaps;
        
        struct {
            lightProbe_t* probe;
            vec3_t position;
            float influence;
        } lightProbes;
    } data;
} pbrWorkItem_t;

// PBR thread pool for compute-intensive operations
static struct {
    platformThread_t threads[MAX_RENDER_THREADS];
    int threadCount;
    lockfreeQueue_t workQueue;
    atomic_int_t workItemsRemaining;
    condition_t allWorkComplete;
    mutex_t completionMutex;
} pbrThreadPool;

// IBL (Image-Based Lighting) precomputation
void PBR_ComputeIBLSpecular_MT(vec3_t viewPos, vec3_t normal, float roughness, 
                               float metallic, vec3_t baseColor, vec3_t result) {
    // Create work item
    pbrWorkItem_t* work = Z_Malloc(sizeof(pbrWorkItem_t));
    work->type = PBR_WORK_IBL_SPECULAR;
    
    VectorCopy(viewPos, work->data.iblSpecular.viewPos);
    VectorCopy(normal, work->data.iblSpecular.normal);
    work->data.iblSpecular.roughness = roughness;
    work->data.iblSpecular.metallic = metallic;
    VectorCopy(baseColor, work->data.iblSpecular.baseColor);
    work->data.iblSpecular.result = result;
    
    // Submit to queue
    LockfreeQueue_Enqueue(&pbrThreadPool.workQueue, work);
    atomic_fetch_add(&pbrThreadPool.workItemsRemaining, 1);
}

// Environment map convolution for diffuse IBL
void PBR_ConvolveEnvironmentMap_MT(const float* envMap, int envMapSize, 
                                  float* diffuseMap, int diffuseMapSize) {
    const int totalPixels = diffuseMapSize * diffuseMapSize * 6; // Cubemap faces
    const int pixelsPerThread = totalPixels / pbrThreadPool.threadCount;
    
    for (int t = 0; t < pbrThreadPool.threadCount; t++) {
        pbrWorkItem_t* work = Z_Malloc(sizeof(pbrWorkItem_t));
        work->type = PBR_WORK_IBL_DIFFUSE;
        
        int startPixel = t * pixelsPerThread;
        int endPixel = (t == pbrThreadPool.threadCount - 1) ? 
                       totalPixels : (t + 1) * pixelsPerThread;
        
        // Calculate work range
        work->data.iblDiffuse.startPixel = startPixel;
        work->data.iblDiffuse.endPixel = endPixel;
        work->data.iblDiffuse.envMap = envMap;
        work->data.iblDiffuse.envMapSize = envMapSize;
        work->data.iblDiffuse.diffuseMap = diffuseMap;
        work->data.iblDiffuse.diffuseMapSize = diffuseMapSize;
        
        LockfreeQueue_Enqueue(&pbrThreadPool.workQueue, work);
        atomic_fetch_add(&pbrThreadPool.workItemsRemaining, 1);
    }
}

// Parallel shadow map cascade generation
void PBR_GenerateShadowCascades_MT(directionalLight_t* light, frustum_t viewFrustum,
                                  int cascadeCount) {
    for (int cascade = 0; cascade < cascadeCount; cascade++) {
        pbrWorkItem_t* work = Z_Malloc(sizeof(pbrWorkItem_t));
        work->type = PBR_WORK_SHADOW_MAPS;
        
        // Calculate cascade frustum
        float cascadeStart = (cascade == 0) ? 0.1f : 
                            light->cascadeDistances[cascade - 1];
        float cascadeEnd = light->cascadeDistances[cascade];
        
        frustum_t cascadeFrustum;
        Frustum_CreateCascade(&viewFrustum, cascadeStart, cascadeEnd, &cascadeFrustum);
        
        work->data.shadowMaps.light = light;
        work->data.shadowMaps.cascade = cascade;
        work->data.shadowMaps.frustum = cascadeFrustum;
        
        LockfreeQueue_Enqueue(&pbrThreadPool.workQueue, work);
        atomic_fetch_add(&pbrThreadPool.workItemsRemaining, 1);
    }
}

// Wait for all PBR work to complete
void PBR_WaitForCompletion(void) {
    Mutex_Lock(&pbrThreadPool.completionMutex);
    
    while (atomic_load(&pbrThreadPool.workItemsRemaining) > 0) {
        Condition_Wait(&pbrThreadPool.allWorkComplete, &pbrThreadPool.completionMutex);
    }
    
    Mutex_Unlock(&pbrThreadPool.completionMutex);
}

// PBR worker thread main function
THREAD_RETURN THREAD_CALL PBR_WorkerThread(void* param) {
    int threadIndex = *(int*)param;
    
    while (!atomic_load(&pbrThreadPool.threads[threadIndex].thread.shouldStop)) {
        pbrWorkItem_t* work = LockfreeQueue_Dequeue(&pbrThreadPool.workQueue);
        
        if (work) {
            // Process work based on type
            switch (work->type) {
            case PBR_WORK_IBL_SPECULAR:
                PBR_ProcessIBLSpecular(&work->data.iblSpecular);
                break;
                
            case PBR_WORK_IBL_DIFFUSE:
                PBR_ProcessIBLDiffuse(&work->data.iblDiffuse);
                break;
                
            case PBR_WORK_SHADOW_MAPS:
                PBR_ProcessShadowMap(&work->data.shadowMaps);
                break;
                
            case PBR_WORK_LIGHT_PROBES:
                PBR_ProcessLightProbe(&work->data.lightProbes);
                break;
            }
            
            Z_Free(work);
            
            // Signal completion if this was the last work item
            if (atomic_fetch_sub(&pbrThreadPool.workItemsRemaining, 1) == 1) {
                Mutex_Lock(&pbrThreadPool.completionMutex);
                Condition_Broadcast(&pbrThreadPool.allWorkComplete);
                Mutex_Unlock(&pbrThreadPool.completionMutex);
            }
        } else {
            // No work available, sleep briefly
            Thread_Sleep(1);
        }
    }
    
#ifdef PLATFORM_WINDOWS
    return 0;
#else
    return NULL;
#endif
}

// Specific PBR processing functions
void PBR_ProcessIBLSpecular(const pbrIBLSpecularData_t* data) {
    // Monte Carlo integration for specular IBL
    const int sampleCount = 1024;
    vec3_t color = {0, 0, 0};
    float totalWeight = 0.0f;
    
    for (int i = 0; i < sampleCount; i++) {
        // Generate random sample direction
        vec2_t xi = Hammersley(i, sampleCount);
        vec3_t H = ImportanceSampleGGX(xi, data->normal, data->roughness);
        vec3_t L = reflect(-data->viewPos, H);
        
        float NdotL = max(dot(data->normal, L), 0.0f);
        
        if (NdotL > 0.0f) {
            // Sample environment map
            vec3_t envColor;
            Environment_Sample(L, envColor);
            
            // Apply BRDF
            vec3_t F0;
            PBR_CalculateF0(data->baseColor, data->metallic, F0);
            
            vec3_t brdf;
            PBR_EvaluateBRDF(data->viewPos, L, data->normal, data->roughness, 
                           data->metallic, data->baseColor, F0, brdf);
            
            VectorMA(color, NdotL, brdf, color);
            VectorScale(color, envColor[0] * envColor[1] * envColor[2], color);
            
            totalWeight += NdotL;
        }
    }
    
    if (totalWeight > 0.0f) {
        VectorScale(color, 1.0f / totalWeight, data->result);
    } else {
        VectorClear(data->result);
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Asset Streaming System</h2>
    
    <h3>Background Asset Loading</h3>
    <div class="code-block">
        <pre><code>// asset_streaming.h - Multi-threaded asset loading for large worlds
#define MAX_STREAMING_THREADS 4
#define MAX_STREAMING_REQUESTS 256

typedef enum {
    ASSET_PRIORITY_LOW,
    ASSET_PRIORITY_NORMAL,
    ASSET_PRIORITY_HIGH,
    ASSET_PRIORITY_CRITICAL
} assetPriority_t;

typedef enum {
    ASSET_STATE_PENDING,
    ASSET_STATE_LOADING,
    ASSET_STATE_LOADED,
    ASSET_STATE_ERROR
} assetState_t;

typedef struct streamingRequest_s {
    char path[MAX_QPATH];
    assetType_t type;
    assetPriority_t priority;
    assetState_t state;
    
    // Loading progress
    atomic_int_t bytesLoaded;
    int totalBytes;
    
    // Result data
    void* data;
    size_t dataSize;
    
    // Callback when complete
    void (*onComplete)(struct streamingRequest_s* request);
    void* userData;
    
    // Timing
    int submitTime;
    int startTime;
    int completeTime;
    
} streamingRequest_t;

typedef struct streamingThread_s {
    platformThread_t thread;
    int threadIndex;
    
    // Work queue
    lockfreeQueue_t requestQueue;
    
    // Current request
    streamingRequest_t* currentRequest;
    
    // Statistics
    int requestsProcessed;
    int totalBytesLoaded;
    int averageLoadTime;
    
} streamingThread_t;

// Global streaming system
static struct {
    streamingThread_t threads[MAX_STREAMING_THREADS];
    int threadCount;
    qboolean initialized;
    
    // Request management
    streamingRequest_t requests[MAX_STREAMING_REQUESTS];
    lockfreeStack_t freeRequests;
    
    // Priority queues
    lockfreeQueue_t criticalQueue;
    lockfreeQueue_t highQueue;
    lockfreeQueue_t normalQueue;
    lockfreeQueue_t lowQueue;
    
    // Statistics
    atomic_int_t activeRequests;
    atomic_int_t completedRequests;
    atomic_int_t failedRequests;
    
} streamingSystem;

qboolean Streaming_Init(void) {
    if (streamingSystem.initialized) {
        return qtrue;
    }
    
    // Determine thread count (usually 2-4 threads for IO)
    streamingSystem.threadCount = min(platform.cpuThreads / 2, MAX_STREAMING_THREADS);
    streamingSystem.threadCount = max(streamingSystem.threadCount, 1);
    
    Com_Printf("Initializing %d asset streaming threads\n", streamingSystem.threadCount);
    
    // Initialize request pool
    LockfreeStack_Init(&streamingSystem.freeRequests);
    
    for (int i = 0; i < MAX_STREAMING_REQUESTS; i++) {
        lfNode_t* node = Z_Malloc(sizeof(lfNode_t));
        node->data = &streamingSystem.requests[i];
        LockfreeStack_Push(&streamingSystem.freeRequests, node);
    }
    
    // Initialize priority queues
    LockfreeQueue_Init(&streamingSystem.criticalQueue, 64);
    LockfreeQueue_Init(&streamingSystem.highQueue, 128);
    LockfreeQueue_Init(&streamingSystem.normalQueue, 256);
    LockfreeQueue_Init(&streamingSystem.lowQueue, 512);
    
    // Initialize streaming threads
    for (int i = 0; i < streamingSystem.threadCount; i++) {
        if (!StreamingThread_Init(&streamingSystem.threads[i], i)) {
            Com_Printf("^1Failed to initialize streaming thread %d\n", i);
            return qfalse;
        }
    }
    
    streamingSystem.initialized = qtrue;
    return qtrue;
}

streamingRequest_t* Streaming_LoadAsset(const char* path, assetType_t type, 
                                       assetPriority_t priority, 
                                       void (*onComplete)(streamingRequest_t*),
                                       void* userData) {
    // Get free request
    lfNode_t* node = LockfreeStack_Pop(&streamingSystem.freeRequests);
    if (!node) {
        Com_Printf("^3Warning: No free streaming requests available\n");
        return NULL;
    }
    
    streamingRequest_t* request = (streamingRequest_t*)node->data;
    Z_Free(node);
    
    // Initialize request
    Q_strncpyz(request->path, path, sizeof(request->path));
    request->type = type;
    request->priority = priority;
    request->state = ASSET_STATE_PENDING;
    request->onComplete = onComplete;
    request->userData = userData;
    request->submitTime = Sys_Milliseconds();
    
    atomic_store(&request->bytesLoaded, 0);
    
    // Add to appropriate priority queue
    lockfreeQueue_t* queue = &streamingSystem.normalQueue;
    
    switch (priority) {
    case ASSET_PRIORITY_CRITICAL:
        queue = &streamingSystem.criticalQueue;
        break;
    case ASSET_PRIORITY_HIGH:
        queue = &streamingSystem.highQueue;
        break;
    case ASSET_PRIORITY_LOW:
        queue = &streamingSystem.lowQueue;
        break;
    }
    
    LockfreeQueue_Enqueue(queue, request);
    atomic_fetch_add(&streamingSystem.activeRequests, 1);
    
    return request;
}

THREAD_RETURN THREAD_CALL StreamingThread_Main(void* param) {
    streamingThread_t* thread = (streamingThread_t*)param;
    
    Com_DPrintf("Streaming thread %d started\n", thread->threadIndex);
    
    while (!atomic_load(&thread->thread.shouldStop)) {
        streamingRequest_t* request = StreamingThread_GetNextRequest(thread);
        
        if (request) {
            thread->currentRequest = request;
            request->state = ASSET_STATE_LOADING;
            request->startTime = Sys_Milliseconds();
            
            // Load the asset
            qboolean success = StreamingThread_LoadAsset(thread, request);
            
            request->completeTime = Sys_Milliseconds();
            request->state = success ? ASSET_STATE_LOADED : ASSET_STATE_ERROR;
            
            // Call completion callback
            if (request->onComplete) {
                request->onComplete(request);
            }
            
            // Update statistics
            thread->requestsProcessed++;
            thread->totalBytesLoaded += request->dataSize;
            thread->averageLoadTime = (thread->averageLoadTime + 
                                     (request->completeTime - request->startTime)) / 2;
            
            // Update global statistics
            if (success) {
                atomic_fetch_add(&streamingSystem.completedRequests, 1);
            } else {
                atomic_fetch_add(&streamingSystem.failedRequests, 1);
            }
            
            atomic_fetch_sub(&streamingSystem.activeRequests, 1);
            thread->currentRequest = NULL;
            
            // Return request to free pool
            lfNode_t* node = Z_Malloc(sizeof(lfNode_t));
            node->data = request;
            LockfreeStack_Push(&streamingSystem.freeRequests, node);
        } else {
            // No work available, sleep briefly
            Thread_Sleep(5);
        }
    }
    
    Com_DPrintf("Streaming thread %d stopped\n", thread->threadIndex);
    
#ifdef PLATFORM_WINDOWS
    return 0;
#else
    return NULL;
#endif
}

streamingRequest_t* StreamingThread_GetNextRequest(streamingThread_t* thread) {
    streamingRequest_t* request;
    
    // Check queues in priority order
    request = LockfreeQueue_Dequeue(&streamingSystem.criticalQueue);
    if (request) return request;
    
    request = LockfreeQueue_Dequeue(&streamingSystem.highQueue);
    if (request) return request;
    
    request = LockfreeQueue_Dequeue(&streamingSystem.normalQueue);
    if (request) return request;
    
    request = LockfreeQueue_Dequeue(&streamingSystem.lowQueue);
    if (request) return request;
    
    return NULL;
}

qboolean StreamingThread_LoadAsset(streamingThread_t* thread, streamingRequest_t* request) {
    switch (request->type) {
    case ASSET_TYPE_TEXTURE:
        return StreamingThread_LoadTexture(thread, request);
    case ASSET_TYPE_MODEL:
        return StreamingThread_LoadModel(thread, request);
    case ASSET_TYPE_SOUND:
        return StreamingThread_LoadSound(thread, request);
    case ASSET_TYPE_SHADER:
        return StreamingThread_LoadShader(thread, request);
    default:
        Com_Printf("^1Unknown asset type: %d\n", request->type);
        return qfalse;
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/platform/cross-platform">Cross-Platform Development</a></li>
        <li><a href="/platform/mobile-console">Mobile and Console Ports</a></li>
        <li><a href="/rendering/vulkan">Vulkan Renderer</a></li>
        <li><a href="/rendering/pbr">PBR Implementation</a></li>
        <li><a href="/modernization/profiling-tools">Performance Profiling</a></li>
    </ul>
</div>