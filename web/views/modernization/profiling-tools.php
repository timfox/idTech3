<?php
/**
 * Performance Profiling Tools for id Tech 3
 */
$title = 'Profiling Tools - id Tech 3 Documentation';
$breadcrumbs = [
    '/modernization' => 'Modernization',
    '/modernization/profiling-tools' => 'Performance Profiling Tools'
];
?>

<h1>Performance Profiling Tools for id Tech 3</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Modern profiling tools and techniques for optimizing id Tech 3 performance. This guide covers integration of contemporary profilers, performance analysis workflows, and optimization strategies for maintaining high frame rates in modernized engines.</p>
    
    <div class="feature-list">
        <h3>Profiling Benefits</h3>
        <ul>
            <li><strong>Real-time Analysis:</strong> Live performance monitoring during gameplay</li>
            <li><strong>Bottleneck Identification:</strong> CPU, GPU, and memory hotspot detection</li>
            <li><strong>Frame Analysis:</strong> Per-frame breakdown of rendering costs</li>
            <li><strong>Optimization Validation:</strong> Measure improvements from code changes</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Tracy Profiler Integration</h2>
    
    <h3>Setup and Configuration</h3>
    <div class="code-block">
        <pre><code># CMakeLists.txt - Tracy integration
option(USE_TRACY "Enable Tracy profiler" OFF)

if(USE_TRACY)
    find_package(Tracy REQUIRED)
    target_link_libraries(quake3e PRIVATE Tracy::TracyClient)
    target_compile_definitions(quake3e PRIVATE 
        TRACY_ENABLE
        TRACY_ON_DEMAND
        TRACY_CALLSTACK=10
    )
endif()

# Conditional compilation header
// tracy_config.h
#ifdef USE_TRACY
    #include "tracy/Tracy.hpp"
    #define PROFILE_SCOPE(name) ZoneScoped
    #define PROFILE_FUNCTION() ZoneScopedN(__FUNCTION__)
    #define PROFILE_FRAME() FrameMark
    #define PROFILE_ALLOC(ptr, size) TracyAlloc(ptr, size)
    #define PROFILE_FREE(ptr) TracyFree(ptr)
    #define PROFILE_MESSAGE(text) TracyMessage(text, strlen(text))
    #define PROFILE_GPU_ZONE(name) TracyVkZone(tr.tracyCtx, tr.commandBuffer, name)
#else
    #define PROFILE_SCOPE(name)
    #define PROFILE_FUNCTION()
    #define PROFILE_FRAME()
    #define PROFILE_ALLOC(ptr, size)
    #define PROFILE_FREE(ptr)
    #define PROFILE_MESSAGE(text)
    #define PROFILE_GPU_ZONE(name)
#endif</code></pre>
    </div>
    
    <h3>Engine Integration Points</h3>
    <div class="code-block">
        <pre><code>// Main loop profiling
void Com_Frame(void) {
    PROFILE_FUNCTION();
    
    {
        PROFILE_SCOPE("Input Processing");
        IN_ProcessEvents();
    }
    
    {
        PROFILE_SCOPE("Game Logic");
        SV_Frame();
        CL_Frame();
    }
    
    {
        PROFILE_SCOPE("Rendering");
        SCR_UpdateScreen();
    }
    
    PROFILE_FRAME(); // Mark end of frame
}

// Renderer profiling
void R_RenderView(void) {
    PROFILE_FUNCTION();
    
    {
        PROFILE_SCOPE("World Culling");
        PROFILE_GPU_ZONE("GPU Culling");
        R_CullWorld();
    }
    
    {
        PROFILE_SCOPE("Shadow Maps");
        PROFILE_GPU_ZONE("Shadow Rendering");
        R_RenderShadowMaps();
    }
    
    {
        PROFILE_SCOPE("Opaque Pass");
        PROFILE_GPU_ZONE("Opaque Geometry");
        R_DrawOpaqueObjects();
    }
    
    {
        PROFILE_SCOPE("Transparent Pass");
        PROFILE_GPU_ZONE("Transparent Geometry");
        R_DrawTransparentObjects();
    }
}</code></pre>
    </div>
    
    <h3>Memory Profiling</h3>
    <div class="code-block">
        <pre><code>// Custom allocator with Tracy integration
void* Z_MallocDebug(int size, int tag, const char* file, int line) {
    void* ptr = malloc(size);
    
#ifdef USE_TRACY
    TracyAllocS(ptr, size, 10); // Track with 10-frame callstack
#endif
    
    // Log allocation for debugging
    if (developer->integer) {
        Com_DPrintf("Alloc: %p, size: %d, tag: %d (%s:%d)\n", 
                   ptr, size, tag, file, line);
    }
    
    return ptr;
}

void Z_FreeDebug(void* ptr, const char* file, int line) {
    if (!ptr) return;
    
#ifdef USE_TRACY
    TracyFreeS(ptr, 10);
#endif
    
    if (developer->integer) {
        Com_DPrintf("Free: %p (%s:%d)\n", ptr, file, line);
    }
    
    free(ptr);
}

// Macro wrappers
#define Z_Malloc(size, tag) Z_MallocDebug(size, tag, __FILE__, __LINE__)
#define Z_Free(ptr) Z_FreeDebug(ptr, __FILE__, __LINE__)</code></pre>
    </div>
</div>

<div class="section">
    <h2>GPU Profiling</h2>
    
    <h3>Vulkan GPU Timing</h3>
    <div class="code-block">
        <pre><code>// GPU profiling context setup
typedef struct {
    VkQueryPool queryPool;
    VkCommandBuffer cmdBuffer;
    uint32_t queryIndex;
    bool enabled;
} gpuProfiler_t;

static gpuProfiler_t gpuProfiler;

void R_InitGPUProfiler(void) {
    if (!USE_TRACY) return;
    
    VkQueryPoolCreateInfo queryPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .queryType = VK_QUERY_TYPE_TIMESTAMP,
        .queryCount = 1024 // Max queries per frame
    };
    
    vkCreateQueryPool(tr.device, &queryPoolInfo, NULL, &gpuProfiler.queryPool);
    gpuProfiler.enabled = true;
}

void R_BeginGPUZone(const char* name) {
    if (!gpuProfiler.enabled) return;
    
    // Write timestamp at start
    vkCmdWriteTimestamp(gpuProfiler.cmdBuffer, 
                       VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                       gpuProfiler.queryPool, gpuProfiler.queryIndex++);
    
#ifdef USE_TRACY
    TracyVkZone(tr.tracyCtx, gpuProfiler.cmdBuffer, name);
#endif
}

void R_EndGPUZone(void) {
    if (!gpuProfiler.enabled) return;
    
    // Write timestamp at end
    vkCmdWriteTimestamp(gpuProfiler.cmdBuffer,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                       gpuProfiler.queryPool, gpuProfiler.queryIndex++);
}</code></pre>
    </div>
    
    <h3>GPU Memory Profiling</h3>
    <div class="code-block">
        <pre><code>// GPU memory usage tracking
typedef struct {
    size_t textureMemory;
    size_t bufferMemory;
    size_t totalAllocated;
    uint32_t allocationCount;
} gpuMemoryStats_t;

static gpuMemoryStats_t gpuStats;

void R_TrackGPUAllocation(VkDeviceSize size, VkMemoryPropertyFlags flags) {
    gpuStats.totalAllocated += size;
    gpuStats.allocationCount++;
    
    // Categorize allocation type
    if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
        gpuStats.textureMemory += size; // Assume textures for device-local
    } else {
        gpuStats.bufferMemory += size; // Staging/uniform buffers
    }
    
#ifdef USE_TRACY
    TracyAllocS((void*)(uintptr_t)gpuStats.allocationCount, size, 5);
    
    // Send GPU memory stats to Tracy
    TracyPlotI("GPU Memory (MB)", gpuStats.totalAllocated / (1024*1024));
    TracyPlotI("GPU Allocations", gpuStats.allocationCount);
#endif
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Dear ImGui Profiler Integration</h2>
    
    <h3>Real-time Performance Dashboard</h3>
    <div class="code-block">
        <pre><code>void ImGui_ShowPerformanceWindow() {
    if (!ImGui::Begin("Performance Profiler")) {
        ImGui::End();
        return;
    }
    
    // Frame time graph
    static float frameTimeHistory[120] = {};
    static int frameIndex = 0;
    static float maxFrameTime = 16.67f; // 60 FPS baseline
    
    float currentFrameTime = ImGui::GetIO().DeltaTime * 1000.0f;
    frameTimeHistory[frameIndex] = currentFrameTime;
    frameIndex = (frameIndex + 1) % 120;
    
    if (currentFrameTime > maxFrameTime) {
        maxFrameTime = currentFrameTime;
    }
    
    ImGui::Text("Frame Time: %.2f ms (%.1f FPS)", 
               currentFrameTime, 1000.0f / currentFrameTime);
    ImGui::PlotLines("Frame Time History", frameTimeHistory, 120, 
                    frameIndex, nullptr, 0.0f, maxFrameTime, ImVec2(0, 80));
    
    // CPU profiling
    if (ImGui::CollapsingHeader("CPU Profiling")) {
        ImGui::Text("Game Logic: %.2f ms", gameLogicTime);
        ImGui::Text("Rendering: %.2f ms", renderTime);
        ImGui::Text("Audio: %.2f ms", audioTime);
        ImGui::Text("Input: %.2f ms", inputTime);
        
        // CPU usage breakdown pie chart (if available)
        if (ImGui::Button("CPU Breakdown")) {
            ImGui::OpenPopup("CPU Usage");
        }
    }
    
    // GPU profiling
    if (ImGui::CollapsingHeader("GPU Profiling")) {
        ImGui::Text("GPU Time: %.2f ms", gpuFrameTime);
        ImGui::Text("Draw Calls: %d", tr.stats.drawCalls);
        ImGui::Text("Triangles: %d", tr.stats.triangles);
        ImGui::Text("Texture Switches: %d", tr.stats.textureSwitches);
        
        ImGui::Separator();
        ImGui::Text("GPU Memory Usage:");
        ImGui::Text("  Textures: %zu MB", gpuStats.textureMemory / (1024*1024));
        ImGui::Text("  Buffers: %zu MB", gpuStats.bufferMemory / (1024*1024));
        ImGui::Text("  Total: %zu MB", gpuStats.totalAllocated / (1024*1024));
    }
    
    // Memory profiling  
    if (ImGui::CollapsingHeader("Memory Profiling")) {
        ImGui::Text("Heap Usage: %zu MB", Z_GetHeapSize() / (1024*1024));
        ImGui::Text("Stack Usage: %zu KB", Z_GetStackSize() / 1024);
        ImGui::Text("Active Allocations: %d", Z_GetAllocationCount());
        
        if (ImGui::Button("Force GC")) {
            // Trigger any cleanup
            Z_ForceGarbageCollection();
        }
    }
    
    ImGui::End();
}</code></pre>
    </div>
    
    <h3>Hierarchical Profiler</h3>
    <div class="code-block">
        <pre><code>// Simple hierarchical profiler for detailed analysis
typedef struct profileNode_s {
    char name[64];
    double startTime;
    double totalTime;
    double maxTime;
    int callCount;
    struct profileNode_s* parent;
    struct profileNode_s* child;
    struct profileNode_s* sibling;
} profileNode_t;

static profileNode_t* currentProfileNode = NULL;
static profileNode_t* rootProfileNode = NULL;

void Profile_BeginBlock(const char* name) {
    profileNode_t* node = Profile_FindNode(name, currentProfileNode);
    if (!node) {
        node = Profile_CreateNode(name, currentProfileNode);
    }
    
    node->startTime = Sys_Milliseconds();
    node->callCount++;
    currentProfileNode = node;
}

void Profile_EndBlock(void) {
    if (!currentProfileNode) return;
    
    double endTime = Sys_Milliseconds();
    double blockTime = endTime - currentProfileNode->startTime;
    
    currentProfileNode->totalTime += blockTime;
    if (blockTime > currentProfileNode->maxTime) {
        currentProfileNode->maxTime = blockTime;
    }
    
    currentProfileNode = currentProfileNode->parent;
}

void ImGui_ShowHierarchicalProfiler() {
    if (!ImGui::Begin("Hierarchical Profiler")) {
        ImGui::End();
        return;
    }
    
    if (ImGui::Button("Reset Stats")) {
        Profile_Reset();
    }
    
    ImGui::Columns(4, "ProfilerColumns");
    ImGui::Text("Function");
    ImGui::NextColumn();
    ImGui::Text("Total Time");
    ImGui::NextColumn();
    ImGui::Text("Max Time");
    ImGui::NextColumn();
    ImGui::Text("Call Count");
    ImGui::NextColumn();
    ImGui::Separator();
    
    Profile_DisplayNode(rootProfileNode, 0);
    
    ImGui::End();
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Platform-Specific Profilers</h2>
    
    <h3>Windows - PIX Integration</h3>
    <div class="code-block">
        <pre><code>// PIX for Windows integration
#ifdef _WIN32
#include <pix3.h>

void R_BeginPIXEvent(const char* name) {
    PIXBeginEvent(PIX_COLOR_DEFAULT, name);
}

void R_EndPIXEvent(void) {
    PIXEndEvent();
}

void R_SetPIXMarker(const char* name) {
    PIXSetMarker(PIX_COLOR_DEFAULT, name);
}

// GPU captures
void R_TriggerPIXCapture(void) {
    if (PIXIsAttachedForGpuCapture()) {
        PIXGpuCaptureNextFrames(L"quake3e_capture.wpix", 1);
    }
}

#else
#define R_BeginPIXEvent(name)
#define R_EndPIXEvent()
#define R_SetPIXMarker(name)
#define R_TriggerPIXCapture()
#endif</code></pre>
    </div>
    
    <h3>Linux - perf Integration</h3>
    <div class="code-block">
        <pre><code>// Linux perf profiling support
#ifdef __linux__
#include <sys/syscall.h>
#include <linux/perf_event.h>

typedef struct {
    int fd;
    bool enabled;
} perfProfiler_t;

static perfProfiler_t perfProfiler;

void Perf_Init(void) {
    struct perf_event_attr pe = {};
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(struct perf_event_attr);
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.disabled = 1;
    pe.exclude_kernel = 1;
    pe.exclude_hv = 1;
    
    perfProfiler.fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    perfProfiler.enabled = (perfProfiler.fd >= 0);
}

void Perf_BeginSample(void) {
    if (perfProfiler.enabled) {
        ioctl(perfProfiler.fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(perfProfiler.fd, PERF_EVENT_IOC_ENABLE, 0);
    }
}

uint64_t Perf_EndSample(void) {
    uint64_t count = 0;
    if (perfProfiler.enabled) {
        ioctl(perfProfiler.fd, PERF_EVENT_IOC_DISABLE, 0);
        read(perfProfiler.fd, &count, sizeof(count));
    }
    return count;
}
#endif</code></pre>
    </div>
</div>

<div class="section">
    <h2>Automated Performance Testing</h2>
    
    <h3>Benchmark Framework</h3>
    <div class="code-block">
        <pre><code>// Automated benchmark system
typedef struct {
    char name[64];
    char mapName[64];
    vec3_t cameraPos;
    vec3_t cameraAngles;
    int duration; // seconds
    int expectedFPS;
} benchmark_t;

static benchmark_t benchmarks[] = {
    {"q3dm1_spawn", "q3dm1", {1056, -3608, 112}, {0, 90, 0}, 30, 60},
    {"q3dm6_courtyard", "q3dm6", {-384, 1280, 64}, {0, 180, 0}, 30, 60},
    {"q3tourney2_mid", "q3tourney2", {0, 0, 64}, {0, 0, 0}, 30, 60}
};

typedef struct {
    float avgFPS;
    float minFPS;
    float maxFPS;
    float frameTimeVariance;
    int totalFrames;
} benchmarkResult_t;

benchmarkResult_t Benchmark_Run(const benchmark_t* bench) {
    benchmarkResult_t result = {};
    
    // Load map and set camera
    Cbuf_AddText(va("map %s\n", bench->mapName));
    Cbuf_Execute();
    
    VectorCopy(bench->cameraPos, cl.snap.ps.origin);
    VectorCopy(bench->cameraAngles, cl.snap.ps.viewangles);
    
    // Disable input
    cl_freezeDemo->integer = 1;
    
    // Run benchmark
    int startTime = Sys_Milliseconds();
    int frameCount = 0;
    float frameTimes[1000];
    
    while ((Sys_Milliseconds() - startTime) < (bench->duration * 1000)) {
        Com_Frame();
        
        float frameTime = 1000.0f / tr.frameRate;
        if (frameCount < 1000) {
            frameTimes[frameCount] = frameTime;
        }
        frameCount++;
        
        result.avgFPS += tr.frameRate;
        if (tr.frameRate < result.minFPS || result.minFPS == 0) {
            result.minFPS = tr.frameRate;
        }
        if (tr.frameRate > result.maxFPS) {
            result.maxFPS = tr.frameRate;
        }
    }
    
    result.avgFPS /= frameCount;
    result.totalFrames = frameCount;
    
    // Calculate variance
    float variance = 0;
    int samples = min(frameCount, 1000);
    for (int i = 0; i < samples; i++) {
        float diff = frameTimes[i] - (1000.0f / result.avgFPS);
        variance += diff * diff;
    }
    result.frameTimeVariance = variance / samples;
    
    return result;
}

void Benchmark_RunAll(void) {
    Com_Printf("Running automated benchmarks...\n");
    
    for (int i = 0; i < ARRAY_LEN(benchmarks); i++) {
        benchmarkResult_t result = Benchmark_Run(&benchmarks[i]);
        
        Com_Printf("Benchmark: %s\n", benchmarks[i].name);
        Com_Printf("  Avg FPS: %.1f\n", result.avgFPS);
        Com_Printf("  Min FPS: %.1f\n", result.minFPS);
        Com_Printf("  Max FPS: %.1f\n", result.maxFPS);
        Com_Printf("  Variance: %.2f\n", result.frameTimeVariance);
        
        if (result.avgFPS < benchmarks[i].expectedFPS) {
            Com_Printf("  WARNING: Performance below expected %.1f FPS\n", 
                      benchmarks[i].expectedFPS);
        }
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Optimization Workflow</h2>
    
    <h3>Performance Analysis Pipeline</h3>
    <div class="troubleshooting">
        <h4>1. Baseline Measurement</h4>
        <ul>
            <li>Run standardized benchmarks on target hardware</li>
            <li>Establish performance baselines for each map/scenario</li>
            <li>Document hardware specifications and driver versions</li>
        </ul>
        
        <h4>2. Bottleneck Identification</h4>
        <ul>
            <li>Use Tracy to identify CPU hotspots</li>
            <li>Analyze GPU timing with vendor tools (NSight, RenderDoc)</li>
            <li>Check memory allocation patterns and cache misses</li>
        </ul>
        
        <h4>3. Targeted Optimization</h4>
        <ul>
            <li>Focus on largest time consumers first</li>
            <li>Measure impact of each optimization</li>
            <li>Validate improvements with automated benchmarks</li>
        </ul>
        
        <h4>4. Regression Testing</h4>
        <ul>
            <li>Run full benchmark suite after changes</li>
            <li>Compare results with baseline measurements</li>
            <li>Monitor for performance regressions in CI/CD</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/external/imgui-integration">ImGui Integration</a></li>
        <li><a href="/modernization/modern-cpp">Modern C++ Features</a></li>
        <li><a href="/modernization/build-systems">Build Systems</a></li>
        <li><a href="/development/debugging">Debugging Tools</a></li>
        <li><a href="/rendering/vulkan">Vulkan Renderer</a></li>
    </ul>
</div>