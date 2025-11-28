<?php
/**
 * Performance Optimization Documentation
 */
$title = 'Performance Optimization - id Tech 3 Documentation';
$breadcrumbs = [
    '/performance' => 'Performance',
    '/performance/optimization' => 'Optimization'
];
?>

<h1>Performance Optimization</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Optimizing id Tech 3 engines for maximum performance requires understanding both engine limitations and modern hardware capabilities. This guide covers CPU, GPU, memory, and network optimizations for better frame rates and reduced latency.</p>
    
    <div class="feature-list">
        <h3>Optimization Areas</h3>
        <ul>
            <li><strong>CPU Optimization:</strong> Reduce processing overhead and improve threading</li>
            <li><strong>GPU Optimization:</strong> Efficient rendering and shader performance</li>
            <li><strong>Memory Management:</strong> Reduce allocations and improve cache usage</li>
            <li><strong>Network Optimization:</strong> Minimize latency and bandwidth usage</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>CPU Optimization</h2>
    
    <h3>Frame Rate Configuration</h3>
    <div class="code-block">
        <pre><code># Optimal frame rate settings
seta com_maxfps "125"            // 125 FPS for optimal physics
seta r_displayRefresh "144"      // Match monitor refresh rate
seta r_swapInterval "0"          // Disable V-Sync for competitive play

# Physics timing
seta pmove_fixed "1"             // Fixed physics timestep
seta pmove_msec "8"              // 8ms physics updates (125 FPS)

# CPU-specific optimizations
seta r_smp "1"                   // Enable multi-processor support
seta r_primitives "0"            // Use optimal primitive rendering
seta r_fastsky "1"               // Fast sky rendering (if available)</code></pre>
    </div>
    
    <h3>Thread Optimization</h3>
    <div class="code-block">
        <pre><code>// Multi-threading improvements in modern engines
void R_InitThreads(void) {
    int numCores = Sys_GetProcessorCount();
    
    // Create worker threads for parallel tasks
    r_numWorkerThreads = min(numCores - 1, MAX_WORKER_THREADS);
    
    for (int i = 0; i < r_numWorkerThreads; i++) {
        Sys_CreateThread(R_WorkerThread, &threadData[i]);
    }
    
    Com_Printf("Initialized %d worker threads\n", r_numWorkerThreads);
}

// Parallel command buffer submission
void R_SubmitCommands(void) {
    if (r_numWorkerThreads > 0) {
        // Distribute commands across worker threads
        R_ParallelSubmit(backEndData);
    } else {
        // Single-threaded submission
        R_SingleThreadSubmit(backEndData);
    }
}</code></pre>
    </div>
    
    <h3>Culling Optimizations</h3>
    <div class="code-block">
        <pre><code># Visibility and culling settings
seta r_nocull "0"                // Enable backface culling
seta r_norefresh "0"             // Enable screen refresh
seta r_drawworld "1"             // Draw world geometry
seta r_speeds "0"                // Disable debug timing (performance hit)

# LOD (Level of Detail) settings
seta r_lodbias "0"               // LOD bias (negative = higher detail)
seta r_lodCurveError "250"       // Curve tessellation error threshold
seta cg_fov "90"                 // Optimal FOV for performance vs visibility

// Frustum culling optimization
qboolean R_CullBox(vec3_t bounds[2]) {
    int i;
    cplane_t *frust;
    int r, aggregateMask = 0;
    
    for (i = 0; i < 4; i++) {
        frust = &tr.viewParms.frustum[i];
        r = BoxOnPlaneSide(bounds[0], bounds[1], frust);
        if (r == 2) {
            return qtrue; // Completely outside
        }
        if (r == 1) {
            aggregateMask |= (1 << i);
        }
    }
    
    if (aggregateMask == 15) {
        return qfalse; // Completely inside
    }
    
    return qfalse; // Partially inside, don't cull
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>GPU Optimization</h2>
    
    <h3>Rendering Settings</h3>
    <div class="code-block">
        <pre><code># GPU performance settings
seta r_texturebits "32"          // 32-bit textures for quality
seta r_colorbits "32"            // 32-bit color depth
seta r_depthbits "24"            // 24-bit depth buffer
seta r_stencilbits "8"           // 8-bit stencil buffer

# Texture settings
seta r_picmip "0"                // Texture detail (0 = highest)
seta r_textureMode "GL_LINEAR_MIPMAP_LINEAR" // Trilinear filtering
seta r_ext_texture_filter_anisotropic "8"   // Anisotropic filtering

# Modern GPU features
seta r_ext_compiled_vertex_array "1"  // Use compiled vertex arrays
seta r_ext_multitexture "1"           // Enable multitexturing
seta r_ext_gamma_control "1"          // Hardware gamma control</code></pre>
    </div>
    
    <h3>Vulkan Optimizations</h3>
    <div class="code-block">
        <pre><code># Vulkan-specific optimizations (Quake3e)
seta r_backend "vk"              // Use Vulkan renderer
seta r_device "0"                // Use primary GPU
seta r_renderScale "1.0"         // Render scale (supersampling)

# Vulkan memory management
seta r_vkDeviceLocalMemory "1"   // Use device local memory
seta r_vkStagingBuffer "64"      // Staging buffer size (MB)
seta r_vkDescriptorPoolSize "1024" // Descriptor pool size

# Pipeline optimization
seta r_pipelineCache "1"         // Enable pipeline caching
seta r_shaderCache "1"           // Enable shader caching
seta r_asyncShaders "1"          // Asynchronous shader compilation

// GPU memory allocation optimization
VkResult AllocateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                       VmaMemoryUsage memUsage, VkBuffer *buffer, 
                       VmaAllocation *allocation) {
    VkBufferCreateInfo bufferInfo = {0};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    
    VmaAllocationCreateInfo allocInfo = {0};
    allocInfo.usage = memUsage;
    
    // Use dedicated allocation for large buffers
    if (size > 64 * 1024 * 1024) { // 64MB threshold
        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }
    
    return vmaCreateBuffer(vmaAllocator, &bufferInfo, &allocInfo, 
                          buffer, allocation, NULL);
}</code></pre>
    </div>
    
    <h3>Shader Optimization</h3>
    <div class="code-block">
        <pre><code>// Optimized fragment shader
#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texDiffuse;
layout(binding = 2) uniform sampler2D texNormal;

layout(binding = 0) uniform UBO {
    mat4 mvp;
    vec3 lightPos;
    vec3 viewPos;
} ubo;

void main() {
    // Early depth test optimization
    if (gl_FragCoord.z > 0.99999) discard;
    
    // Use fast texture sampling
    vec3 albedo = texture(texDiffuse, fragTexCoord).rgb;
    
    // Simplified lighting calculation
    vec3 lightDir = normalize(ubo.lightPos - fragWorldPos);
    float NdotL = max(dot(fragNormal, lightDir), 0.0);
    
    // Avoid expensive calculations when possible
    vec3 color = albedo * (0.1 + 0.9 * NdotL); // Simple Lambert
    
    outColor = vec4(color, 1.0);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Memory Optimization</h2>
    
    <h3>Memory Pool Configuration</h3>
    <div class="code-block">
        <pre><code># Memory allocation settings
seta com_hunkmegs "128"          // Main memory pool (MB)
seta com_zonemegs "32"           // Zone memory for small allocations
seta com_soundmegs "64"          // Sound system memory

# Asset streaming
seta r_textureStreaming "1"      // Enable texture streaming
seta r_textureStreamingMemory "512" // Streaming memory (MB)
seta r_geometryStreaming "1"     // Enable geometry streaming

// Optimized memory allocator
typedef struct memblock_s {
    int size;                    // Size including header
    int tag;                     // For group allocations
    int id;                      // For debugging
    struct memblock_s *next, *prev;
    int pad;                     // Pad to 64-bit boundary
} memblock_t;

void *Z_Malloc(int size) {
    memblock_t *block;
    int extra;
    
    // Align to cache line boundary (64 bytes)
    extra = sizeof(memblock_t);
    size = (size + 63) & ~63;
    
    // Try to allocate from memory pool
    block = Z_AllocateFromPool(size + extra);
    if (!block) {
        // Fall back to system allocation
        block = malloc(size + extra);
        if (!block) {
            Com_Error(ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes", size);
        }
    }
    
    // Initialize block header
    block->size = size + extra;
    block->tag = TAG_GENERAL;
    block->id = Hunk_MallocDebug();
    
    return (void *)((byte *)block + extra);
}</code></pre>
    </div>
    
    <h3>Cache Optimization</h3>
    <div class="code-block">
        <pre><code>// Structure packing for cache efficiency
typedef struct {
    vec3_t origin;               // 12 bytes - frequently accessed
    int entityNum;               // 4 bytes - frequently accessed
    // Total: 16 bytes (cache line friendly)
} entityCache_t;

// Avoid this - poor cache usage
typedef struct {
    vec3_t origin;               // 12 bytes
    char padding[52];            // 52 bytes - wasted space
    int entityNum;               // 4 bytes
    // Total: 68 bytes - spans multiple cache lines
} badEntityCache_t;

// Data-oriented design for better performance
void UpdateEntities(void) {
    // Process positions in batch (better cache usage)
    for (int i = 0; i < numEntities; i++) {
        VectorCopy(entities[i].origin, positions[i]);
    }
    
    // Process visibility in batch
    for (int i = 0; i < numEntities; i++) {
        visibility[i] = R_CullSphere(positions[i], entities[i].radius);
    }
    
    // Process rendering for visible entities only
    for (int i = 0; i < numEntities; i++) {
        if (visibility[i]) {
            R_RenderEntity(&entities[i]);
        }
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Network Optimization</h2>
    
    <h3>Connection Settings</h3>
    <div class="code-block">
        <pre><code># Network performance settings
seta rate "25000"               // Network rate (bytes/sec)
seta snaps "40"                 // Snapshot rate (updates/sec)
seta cl_maxpackets "100"        // Max packets sent per second
seta cl_packetdup "1"           // Packet duplication for reliability

# Lag compensation
seta cl_timenudge "0"           // Time nudging (usually 0)
seta cg_smoothTime "100"        // Smooth interpolation time
seta cg_lagometer "1"           // Show lag meter

// Optimized packet compression
int MSG_WriteDeltaEntity(msg_t *msg, struct entityState_s *from, 
                        struct entityState_s *to, qboolean force) {
    int i, lc;
    int numFields;
    netField_t *field;
    int trunc;
    float fullFloat;
    int *fromF, *toF;
    
    numFields = sizeof(entityStateFields) / sizeof(entityStateFields[0]);
    lc = 0;
    
    // Build a delta
    for (i = 0, field = entityStateFields; i < numFields; i++, field++) {
        fromF = (int *)((byte *)from + field->offset);
        toF = (int *)((byte *)to + field->offset);
        
        if (*fromF != *toF) {
            lc = i + 1;
        }
    }
    
    if (lc == 0) {
        MSG_WriteBits(msg, 0, 1); // No delta
        return 0;
    }
    
    MSG_WriteBits(msg, 1, 1); // Delta present
    MSG_WriteByte(msg, lc);   // Number of fields
    
    // Write delta fields with compression
    for (i = 0, field = entityStateFields; i < lc; i++, field++) {
        fromF = (int *)((byte *)from + field->offset);
        toF = (int *)((byte *)to + field->offset);
        
        if (*fromF == *toF) {
            MSG_WriteBits(msg, 0, 1); // No change
            continue;
        }
        
        MSG_WriteBits(msg, 1, 1); // Field changed
        
        // Apply field-specific compression
        if (field->bits == 0) {
            // Float compression
            fullFloat = *(float *)toF;
            trunc = (int)fullFloat;
            
            if (trunc == fullFloat && trunc + FLOAT_INT_BIAS >= 0 && 
                trunc + FLOAT_INT_BIAS < (1 << FLOAT_INT_BITS)) {
                // Send as truncated integer
                MSG_WriteBits(msg, 0, 1);
                MSG_WriteBits(msg, trunc + FLOAT_INT_BIAS, FLOAT_INT_BITS);
            } else {
                // Send as full float
                MSG_WriteBits(msg, 1, 1);
                MSG_WriteBits(msg, *(int *)toF, 32);
            }
        } else {
            // Integer compression
            MSG_WriteBits(msg, *toF, field->bits);
        }
    }
    
    return lc;
}</code></pre>
    </div>
    
    <h3>Prediction Optimization</h3>
    <div class="code-block">
        <pre><code># Client prediction settings
seta cl_predict "1"             // Enable client prediction
seta cl_showmiss "0"            // Show prediction misses (debug only)
seta cg_predict "1"             // Client-side prediction

// Optimized prediction system
void CL_PredictMovement(void) {
    int ack, current;
    int frame;
    int oldFrame;
    usercmd_t oldestCmd;
    usercmd_t latestCmd;
    
    if (cls.state != CA_ACTIVE) return;
    if (cl_predict->integer == 0) return;
    if (clc.demoplaying) return;
    
    // Get the most recent command
    current = cls.netchan.outgoingSequence;
    
    // Get the acknowledged command
    ack = clc.serverCommandSequence;
    
    // Don't predict too far ahead
    if (current - ack >= MAX_PREDICT_COMMANDS) {
        return;
    }
    
    // Copy current state
    predictedPlayerState = cl.snap.ps;
    predictedPlayerState.pm_flags &= ~PMF_RESPAWNED;
    
    // Run prediction for unacknowledged commands
    for (frame = ack + 1; frame <= current; frame++) {
        Pmove(&pmove);
        
        // Check for prediction errors periodically
        if ((frame & 7) == 0) {
            CL_CheckPredictionError();
        }
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Platform-Specific Optimizations</h2>
    
    <h3>Windows Optimizations</h3>
    <div class="code-block">
        <pre><code># Windows-specific settings
seta r_allowSoftwareGL "0"      // Force hardware acceleration
seta r_maskMinidriver "0"       // Don't mask mini drivers
seta win_noalttab "1"           // Disable Alt+Tab (fullscreen)

# CPU affinity and priority
void Sys_SetupCPU(void) {
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    
    // Set high priority class
    SetPriorityClass(process, HIGH_PRIORITY_CLASS);
    SetThreadPriority(thread, THREAD_PRIORITY_TIME_CRITICAL);
    
    // Set CPU affinity for consistent timing
    DWORD_PTR processAffinity, systemAffinity;
    GetProcessAffinityMask(process, &processAffinity, &systemAffinity);
    
    // Use only performance cores on hybrid CPUs
    if (systemAffinity != processAffinity) {
        SetProcessAffinityMask(process, processAffinity & 0xAAAAAAAA);
    }
    
    // Enable precise timing
    timeBeginPeriod(1);
}</code></pre>
    </div>
    
    <h3>Linux Optimizations</h3>
    <div class="code-block">
        <pre><code># Linux-specific optimizations
export __GL_THREADED_OPTIMIZATIONS=1    # NVIDIA threaded optimizations
export __GL_SYNC_TO_VBLANK=0            # Disable V-Sync
export MESA_GL_VERSION_OVERRIDE=3.3     # Force OpenGL 3.3 (Mesa)

# CPU scaling and frequency
echo performance > /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
echo 0 > /proc/sys/kernel/numa_balancing  # Disable NUMA balancing

// Thread priority on Linux
void Sys_SetThreadPriority(int priority) {
    struct sched_param param;
    int policy;
    
    if (geteuid() == 0) { // Running as root
        policy = SCHED_FIFO;
        param.sched_priority = priority;
        pthread_setschedparam(pthread_self(), policy, &param);
    } else {
        // Use nice values for non-root
        nice(-10); // Higher priority
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Profiling and Monitoring</h2>
    
    <h3>Performance Metrics</h3>
    <div class="code-block">
        <pre><code># Enable performance monitoring
seta r_speeds "1"               // Show rendering statistics
seta cg_drawFPS "1"             // Show FPS counter
seta cg_lagometer "1"           // Show network lag
seta developer "1"              // Enable developer mode

// Custom performance profiler
typedef struct {
    char name[64];
    int calls;
    double totalTime;
    double minTime;
    double maxTime;
} perfCounter_t;

#define MAX_PERF_COUNTERS 128
perfCounter_t perfCounters[MAX_PERF_COUNTERS];
int numPerfCounters = 0;

void Perf_Begin(const char *name) {
    perfCounter_t *counter = Perf_FindCounter(name);
    counter->startTime = Sys_Milliseconds();
}

void Perf_End(const char *name) {
    perfCounter_t *counter = Perf_FindCounter(name);
    double elapsed = Sys_Milliseconds() - counter->startTime;
    
    counter->calls++;
    counter->totalTime += elapsed;
    counter->minTime = min(counter->minTime, elapsed);
    counter->maxTime = max(counter->maxTime, elapsed);
}</code></pre>
    </div>
    
    <h3>Automated Optimization</h3>
    <div class="code-block">
        <pre><code>// Dynamic quality adjustment
void R_AutoAdjustQuality(void) {
    static int frameHistory[60];
    static int frameIndex = 0;
    static int lastAdjustTime = 0;
    
    int currentTime = Sys_Milliseconds();
    int currentFPS = 1000 / (currentTime - lastFrameTime);
    
    frameHistory[frameIndex] = currentFPS;
    frameIndex = (frameIndex + 1) % 60;
    
    // Adjust quality every 5 seconds
    if (currentTime - lastAdjustTime > 5000) {
        int avgFPS = 0;
        for (int i = 0; i < 60; i++) {
            avgFPS += frameHistory[i];
        }
        avgFPS /= 60;
        
        if (avgFPS < 60 && r_picmip->integer < 3) {
            // Reduce quality
            Cvar_Set("r_picmip", va("%d", r_picmip->integer + 1));
            Com_Printf("Reduced texture quality for better performance\n");
        } else if (avgFPS > 100 && r_picmip->integer > 0) {
            // Increase quality
            Cvar_Set("r_picmip", va("%d", r_picmip->integer - 1));
            Com_Printf("Increased texture quality\n");
        }
        
        lastAdjustTime = currentTime;
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="performance/profiling">Performance Profiling</a></li>
        <li><a href="performance/memory">Memory Management</a></li>
        <li><a href="rendering/vulkan">Vulkan Optimization</a></li>
        <li><a href="networking/prediction">Network Prediction</a></li>
    </ul>
</div> 