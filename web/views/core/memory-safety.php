<?php
/**
 * Memory Safety and Profiling Documentation
 */
$title = 'Memory Safety & Profiling - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/memory-safety' => 'Memory Safety & Profiling'
];
?>

<h1>Memory Safety and Profiling</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 engine now includes comprehensive memory safety tools and profiling capabilities to help detect memory errors, leaks, and performance issues. These tools include AddressSanitizer (ASan), UndefinedBehaviorSanitizer (UBSan), memory tracking system, and integration with Valgrind and Dr. Memory.</p>
    
    <div class="feature-list">
        <h3>Key Features</h3>
        <ul>
            <li><strong>AddressSanitizer (ASan):</strong> Detects memory errors (use-after-free, buffer overflows, leaks)</li>
            <li><strong>UndefinedBehaviorSanitizer (UBSan):</strong> Detects undefined behavior (integer overflow, null pointer dereference)</li>
            <li><strong>Memory Tracking:</strong> Per-type tracking with leak detection and statistics</li>
            <li><strong>Valgrind Integration:</strong> Full Valgrind support for Linux</li>
            <li><strong>Dr. Memory Integration:</strong> Memory debugging for Windows</li>
            <li><strong>Comprehensive Statistics:</strong> Track allocated, freed, current, and peak memory usage</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>AddressSanitizer (ASan)</h2>
    <p>AddressSanitizer detects memory errors such as:</p>
    <ul>
        <li>Use-after-free</li>
        <li>Heap buffer overflow</li>
        <li>Stack buffer overflow</li>
        <li>Use-after-return</li>
        <li>Memory leaks</li>
    </ul>
    
    <h3>Usage</h3>
    <div class="code-block">
        <pre><code>cd build
cmake .. -DENABLE_ASAN=ON
make
./idtech3.x86_64</code></pre>
    </div>
    
    <h3>Output Example</h3>
    <div class="code-block">
        <pre><code>==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x60300000f1d0
    #0 0x7f8b2c3d4f2a in main /path/to/file.c:123
    #1 0x7f8b2c1d82bf in __libc_start_main</code></pre>
    </div>
</div>

<div class="section">
    <h2>UndefinedBehaviorSanitizer (UBSan)</h2>
    <p>UndefinedBehaviorSanitizer detects undefined behavior such as:</p>
    <ul>
        <li>Integer overflow</li>
        <li>Null pointer dereference</li>
        <li>Invalid shift operations</li>
        <li>Out-of-bounds array access</li>
    </ul>
    
    <h3>Usage</h3>
    <div class="code-block">
        <pre><code>cd build
cmake .. -DENABLE_UBSAN=ON
make
./idtech3.x86_64</code></pre>
    </div>
    
    <h3>Output Example</h3>
    <div class="code-block">
        <pre><code>file.c:123:5: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'</code></pre>
    </div>
</div>

<div class="section">
    <h2>Memory Tracking System</h2>
    <p>The memory tracking system provides detailed statistics and leak detection for all memory allocations.</p>
    
    <h3>Features</h3>
    <ul>
        <li><strong>Per-type tracking:</strong> Track memory by type (HUNK, ZONE, TEMP, SOUND, RENDERER, etc.)</li>
        <li><strong>Leak detection:</strong> Automatically detect memory leaks on shutdown</li>
        <li><strong>Statistics:</strong> Track allocated, freed, current, and peak memory usage</li>
        <li><strong>Detailed reports:</strong> Generate detailed leak reports with file/line information</li>
    </ul>
    
    <h3>CVars</h3>
    <table class="settings-table">
        <tr>
            <th>CVar</th>
            <th>Default</th>
            <th>Description</th>
        </tr>
        <tr>
            <td><code>memtrack_enable</code></td>
            <td>1</td>
            <td>Enable memory tracking</td>
        </tr>
        <tr>
            <td><code>memtrack_report_leaks</code></td>
            <td>0</td>
            <td>Report leaks on shutdown</td>
        </tr>
        <tr>
            <td><code>memtrack_log_leaks</code></td>
            <td>0</td>
            <td>Log leaks to memleaks.log</td>
        </tr>
    </table>
    
    <h3>Usage</h3>
    <div class="code-block">
        <pre><code>#include "q_memtrack.h"

// Track allocation
void *ptr = Q_MemTrack_Malloc(1024, MEMTYPE_RENDERER);

// Track reallocation
ptr = Q_MemTrack_Realloc(ptr, 2048, MEMTYPE_RENDERER);

// Track free
Q_MemTrack_Free(ptr);

// Get statistics
memstats_t stats;
Q_MemTrack_GetStats(MEMTYPE_RENDERER, &stats);
Com_Printf("Renderer memory: %lld bytes allocated, %lld bytes current\n",
    (long long)stats.allocated, (long long)stats.current);

// Report leaks
Q_MemTrack_ReportLeaks();</code></pre>
    </div>
    
    <h3>Memory Types</h3>
    <ul>
        <li><code>MEMTYPE_HUNK</code>: Hunk allocator memory</li>
        <li><code>MEMTYPE_ZONE</code>: Zone allocator memory</li>
        <li><code>MEMTYPE_TEMP</code>: Temporary allocations</li>
        <li><code>MEMTYPE_SOUND</code>: Sound system memory</li>
        <li><code>MEMTYPE_RENDERER</code>: Renderer memory</li>
        <li><code>MEMTYPE_NETWORK</code>: Network buffers</li>
        <li><code>MEMTYPE_FILESYSTEM</code>: File system buffers</li>
        <li><code>MEMTYPE_SCRIPT</code>: Scripting memory</li>
        <li><code>MEMTYPE_BOTLIB</code>: Bot library memory</li>
        <li><code>MEMTYPE_OTHER</code>: Other allocations</li>
    </ul>
</div>

<div class="section">
    <h2>Valgrind Integration</h2>
    <p>Valgrind is a powerful memory debugging tool for Linux. The engine includes hooks for Valgrind integration.</p>
    
    <h3>Usage</h3>
    <div class="code-block">
        <pre><code># Build with Valgrind support
cd build
cmake .. -DENABLE_VALGRIND=ON
make

# Run with Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./idtech3.x86_64</code></pre>
    </div>
    
    <h3>Valgrind Options</h3>
    <ul>
        <li><code>--leak-check=full</code>: Full leak checking</li>
        <li><code>--show-leak-kinds=all</code>: Show all leak kinds</li>
        <li><code>--track-origins=yes</code>: Track origins of uninitialized values</li>
        <li><code>--suppressions=valgrind.supp</code>: Use suppression file</li>
    </ul>
</div>

<div class="section">
    <h2>Dr. Memory Integration (Windows)</h2>
    <p>Dr. Memory is a memory debugging tool for Windows similar to Valgrind.</p>
    
    <h3>Usage</h3>
    <div class="code-block">
        <pre><code># Build with Dr. Memory support
cd build
cmake .. -DENABLE_DRMEMORY=ON
make

# Run with Dr. Memory
drmemory.exe idtech3.x86_64.exe</code></pre>
    </div>
</div>

<div class="section">
    <h2>Memory Statistics</h2>
    <p>The engine tracks comprehensive memory statistics:</p>
    
    <h3>Per-Type Statistics</h3>
    <ul>
        <li><strong>allocated:</strong> Total bytes allocated</li>
        <li><strong>freed:</strong> Total bytes freed</li>
        <li><strong>current:</strong> Current bytes in use</li>
        <li><strong>peak:</strong> Peak bytes in use</li>
        <li><strong>count:</strong> Number of allocations</li>
        <li><strong>free_count:</strong> Number of frees</li>
        <li><strong>leak_count:</strong> Number of leaks detected</li>
    </ul>
</div>

<div class="section">
    <h2>Best Practices</h2>
    <ul>
        <li><strong>Enable tracking in development:</strong> Use <code>ENABLE_MEMORY_TRACKING=ON</code> during development</li>
        <li><strong>Check for leaks regularly:</strong> Set <code>memtrack_report_leaks 1</code> to catch leaks early</li>
        <li><strong>Use appropriate memory types:</strong> Choose the correct memory type for better tracking</li>
        <li><strong>Run with sanitizers:</strong> Use ASan/UBSan in CI/CD pipelines</li>
        <li><strong>Profile regularly:</strong> Check memory statistics to identify memory hotspots</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Impact</h2>
    <ul>
        <li><strong>Memory Tracking:</strong> ~5-10% overhead, mainly from tracking overhead</li>
        <li><strong>ASan:</strong> ~2x slowdown, significant memory overhead</li>
        <li><strong>UBSan:</strong> ~1.5x slowdown, minimal memory overhead</li>
        <li><strong>Valgrind:</strong> ~10-50x slowdown, use only for debugging</li>
    </ul>
</div>

<div class="section">
    <h2>Example Workflow</h2>
    <div class="code-block">
        <pre><code># 1. Build with memory tracking
cmake .. -DENABLE_MEMORY_TRACKING=ON
make

# 2. Run and check for leaks
./idtech3.x86_64
# Check console for leak reports

# 3. Enable leak logging
/set memtrack_report_leaks 1
/set memtrack_log_leaks 1

# 4. Run with ASan for error detection
cmake .. -DENABLE_ASAN=ON
make
./idtech3.x86_64

# 5. Run with Valgrind for detailed analysis
valgrind --leak-check=full ./idtech3.x86_64</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="core/memory-management">Memory Management</a></li>
        <li><a href="core/structured-logging">Structured Logging</a></li>
        <li><a href="development/debugging">Debugging Tools</a></li>
        <li><a href="imgui">ImGui Debug Overlays</a></li>
    </ul>
</div>

