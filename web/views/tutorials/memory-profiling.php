<?php
/**
 * Memory Profiling Tutorial
 */
$title = 'Memory Profiling Tutorial - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/memory-profiling' => 'Memory Profiling Tutorial'
];
?>

<h1>Memory Profiling Tutorial</h1>

<div class="section">
    <h2>Introduction</h2>
    <p>This tutorial will guide you through using memory safety tools and profiling capabilities in id Tech 3. You'll learn how to detect memory leaks, find memory errors, and optimize memory usage.</p>
    
    <div class="feature-list">
        <h3>What You'll Learn</h3>
        <ul>
            <li>Building with memory tracking enabled</li>
            <li>Using AddressSanitizer to find memory errors</li>
            <li>Using UndefinedBehaviorSanitizer to find bugs</li>
            <li>Tracking memory usage by type</li>
            <li>Detecting and fixing memory leaks</li>
            <li>Using Valgrind for detailed analysis</li>
            <li>Interpreting memory statistics</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Prerequisites</h2>
    <ul>
        <li>id Tech 3 source code</li>
        <li>CMake build system</li>
        <li>GCC or Clang compiler (for sanitizers)</li>
        <li>Linux system (for Valgrind, optional)</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Building with Memory Tracking</h2>
    
    <h3>Step 1: Configure CMake</h3>
    <p>Enable memory tracking in your build:</p>
    <div class="code-block">
        <pre><code>cd build
cmake .. -DENABLE_MEMORY_TRACKING=ON
make</code></pre>
    </div>
    
    <h3>Step 2: Enable Memory Tracking at Runtime</h3>
    <p>Memory tracking is enabled by default, but you can verify:</p>
    <div class="code-block">
        <pre><code># Check if memory tracking is enabled
/memtrack_enable

# Enable if disabled
/set memtrack_enable 1</code></pre>
    </div>
    
    <h3>Step 3: View Memory Statistics</h3>
    <p>Check memory usage statistics:</p>
    <div class="code-block">
        <pre><code># View memory statistics in console
# (Statistics are automatically displayed when tracking is enabled)

# Or use ImGui overlay for visual statistics
/set cl_imgui 1
/set cl_imgui_debug_memory 1</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Using AddressSanitizer (ASan)</h2>
    
    <h3>Step 1: Build with AddressSanitizer</h3>
    <p>ASan detects memory errors like use-after-free and buffer overflows:</p>
    <div class="code-block">
        <pre><code>cd build
cmake .. -DENABLE_ASAN=ON
make</code></pre>
    </div>
    
    <h3>Step 2: Run Your Application</h3>
    <p>ASan will automatically detect memory errors:</p>
    <div class="code-block">
        <pre><code>./idtech3.x86_64</code></pre>
    </div>
    
    <h3>Step 3: Interpret ASan Output</h3>
    <p>When ASan detects an error, it prints detailed information:</p>
    <div class="code-block">
        <pre><code>==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x60300000f1d0
    #0 0x7f8b2c3d4f2a in MyFunction /path/to/file.c:123
    #1 0x7f8b2c1d82bf in main /path/to/main.c:45
    #2 0x7f8b2c1d82bf in __libc_start_main

0x60300000f1d0 is located 0 bytes inside of 16-byte region [0x60300000f1d0,0x60300000f1e0)
freed by thread T0 here:
    #0 0x7f8b2c4a5b2a in free
    #1 0x7f8b2c3d4e10 in MyFunction /path/to/file.c:120</code></pre>
    </div>
    
    <p><strong>What to look for:</strong></p>
    <ul>
        <li>Error type (heap-use-after-free, heap-buffer-overflow, etc.)</li>
        <li>Stack trace showing where the error occurred</li>
        <li>Memory address and region information</li>
        <li>Where the memory was allocated/freed</li>
    </ul>
    
    <h3>Step 4: Fix the Error</h3>
    <p>Use the stack trace to locate the problematic code and fix it:</p>
    <ol>
        <li>Find the file and line number from the stack trace</li>
        <li>Review the code at that location</li>
        <li>Fix the memory error (e.g., don't use freed memory)</li>
        <li>Rebuild and test again</li>
    </ol>
</div>

<div class="section">
    <h2>Tutorial: Using UndefinedBehaviorSanitizer (UBSan)</h2>
    
    <h3>Step 1: Build with UBSan</h3>
    <div class="code-block">
        <pre><code>cd build
cmake .. -DENABLE_UBSAN=ON
make</code></pre>
    </div>
    
    <h3>Step 2: Run and Check for Warnings</h3>
    <p>UBSan will print warnings when undefined behavior is detected:</p>
    <div class="code-block">
        <pre><code>./idtech3.x86_64</code></pre>
    </div>
    
    <h3>Step 3: Interpret UBSan Output</h3>
    <p>Example UBSan warning:</p>
    <div class="code-block">
        <pre><code>file.c:123:5: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
file.c:456:10: runtime error: null pointer passed as argument 1, which is declared to never be null</code></pre>
    </div>
    
    <h3>Step 4: Fix Undefined Behavior</h3>
    <p>Common fixes:</p>
    <ul>
        <li><strong>Integer Overflow:</strong> Use larger types or check for overflow before operations</li>
        <li><strong>Null Pointer:</strong> Add null checks before dereferencing</li>
        <li><strong>Invalid Shift:</strong> Check shift amount is within valid range</li>
        <li><strong>Out of Bounds:</strong> Validate array indices before access</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Detecting Memory Leaks</h2>
    
    <h3>Step 1: Enable Leak Detection</h3>
    <div class="code-block">
        <pre><code># Enable leak reporting on shutdown
/set memtrack_report_leaks 1

# Also log leaks to file
/set memtrack_log_leaks 1</code></pre>
    </div>
    
    <h3>Step 2: Run Your Application</h3>
    <p>Play the game or run your test scenario:</p>
    <div class="code-block">
        <pre><code>./idtech3.x86_64 +set memtrack_report_leaks 1</code></pre>
    </div>
    
    <h3>Step 3: Check for Leaks on Shutdown</h3>
    <p>When the application exits, check the console or log file for leak reports:</p>
    <div class="code-block">
        <pre><code>=== Memory Leak Report ===
MEMTYPE_RENDERER: 3 leaks detected
  Leak #1: 1024 bytes allocated at tr_image.c:123 (LoadTexture)
  Leak #2: 2048 bytes allocated at tr_shader.c:456 (LoadShader)
  Leak #3: 512 bytes allocated at tr_model.c:789 (LoadModel)

Total leaks: 3
Total leaked memory: 3584 bytes</code></pre>
    </div>
    
    <h3>Step 4: Fix Leaks</h3>
    <p>For each leak:</p>
    <ol>
        <li>Find the allocation location (file and line)</li>
        <li>Ensure there's a corresponding free/deallocation</li>
        <li>Check that deallocation is actually reached</li>
        <li>Verify cleanup happens in all code paths</li>
    </ol>
</div>

<div class="section">
    <h2>Tutorial: Using Valgrind (Linux)</h2>
    
    <h3>Step 1: Build with Valgrind Support</h3>
    <div class="code-block">
        <pre><code>cd build
cmake .. -DENABLE_VALGRIND=ON
make</code></pre>
    </div>
    
    <h3>Step 2: Run with Valgrind</h3>
    <div class="code-block">
        <pre><code>valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes ./idtech3.x86_64</code></pre>
    </div>
    
    <h3>Step 3: Analyze Valgrind Output</h3>
    <p>Valgrind provides detailed information about:</p>
    <ul>
        <li>Memory leaks with stack traces</li>
        <li>Use of uninitialized memory</li>
        <li>Invalid memory access</li>
        <li>Memory errors</li>
    </ul>
    
    <h3>Step 4: Use Suppression File</h3>
    <p>Create a suppression file for known false positives:</p>
    <div class="code-block">
        <pre><code># valgrind.supp
{
   ignore_libc_malloc
   Memcheck:Leak
   match-leak-kinds: reachable
   ...
   fun:malloc
}</code></pre>
    </div>
    
    <p>Run with suppression file:</p>
    <div class="code-block">
        <pre><code>valgrind --suppressions=valgrind.supp \
         --leak-check=full ./idtech3.x86_64</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Memory Usage Optimization</h2>
    
    <h3>Step 1: Identify Memory Hotspots</h3>
    <p>Use memory statistics to find where memory is used:</p>
    <div class="code-block">
        <pre><code># Enable memory overlay in ImGui
/set cl_imgui 1
/set cl_imgui_debug_memory 1</code></pre>
    </div>
    
    <h3>Step 2: Analyze Per-Type Statistics</h3>
    <p>Check which memory types use the most memory:</p>
    <ul>
        <li>MEMTYPE_RENDERER - Texture and model memory</li>
        <li>MEMTYPE_HUNK - Temporary allocations</li>
        <li>MEMTYPE_ZONE - Game state memory</li>
        <li>MEMTYPE_SOUND - Audio buffers</li>
    </ul>
    
    <h3>Step 3: Optimize High-Usage Areas</h3>
    <p>Common optimizations:</p>
    <ul>
        <li><strong>Texture Compression:</strong> Use compressed texture formats</li>
        <li><strong>Model LOD:</strong> Use lower detail models at distance</li>
        <li><strong>Streaming:</strong> Load assets on-demand rather than all at once</li>
        <li><strong>Pooling:</strong> Reuse memory allocations instead of allocating/freeing</li>
        <li><strong>Cleanup:</strong> Free unused resources promptly</li>
    </ul>
    
    <h3>Step 4: Monitor Peak Usage</h3>
    <p>Track peak memory usage to identify worst-case scenarios:</p>
    <div class="code-block">
        <pre><code># Peak usage is tracked automatically
# Check peak values in memory statistics
# Compare peak vs current to identify growth patterns</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Complete Workflow Example</h2>
    
    <h3>Scenario: Finding and Fixing a Memory Leak</h3>
    
    <h4>Step 1: Build with Memory Tracking</h4>
    <div class="code-block">
        <pre><code>cd build
cmake .. -DENABLE_MEMORY_TRACKING=ON
make</code></pre>
    </div>
    
    <h4>Step 2: Enable Leak Detection</h4>
    <div class="code-block">
        <pre><code>./idtech3.x86_64 +set memtrack_report_leaks 1 +set memtrack_log_leaks 1</code></pre>
    </div>
    
    <h4>Step 3: Reproduce the Issue</h4>
    <p>Run through the scenario that causes the leak (e.g., load/unload maps multiple times)</p>
    
    <h4>Step 4: Check Leak Report</h4>
    <p>Review <code>memleaks.log</code> or console output for leak information</p>
    
    <h4>Step 5: Use ASan for Detailed Analysis</h4>
    <div class="code-block">
        <pre><code>cd build
cmake .. -DENABLE_ASAN=ON
make
./idtech3.x86_64</code></pre>
    </div>
    
    <h4>Step 6: Fix the Leak</h4>
    <p>Based on leak report, add proper cleanup code</p>
    
    <h4>Step 7: Verify Fix</h4>
    <div class="code-block">
        <pre><code># Run again and verify no leaks reported
./idtech3.x86_64 +set memtrack_report_leaks 1</code></pre>
    </div>
</div>

<div class="section">
    <h2>Best Practices</h2>
    <ul>
        <li><strong>Enable Tracking in Development:</strong> Always use memory tracking during development</li>
        <li><strong>Check for Leaks Regularly:</strong> Run with leak detection enabled frequently</li>
        <li><strong>Use Appropriate Memory Types:</strong> Choose correct type for better tracking</li>
        <li><strong>Run Sanitizers in CI/CD:</strong> Automate memory error detection</li>
        <li><strong>Profile Before Optimizing:</strong> Measure first, optimize based on data</li>
        <li><strong>Monitor Production:</strong> Track memory usage in production builds</li>
        <li><strong>Document Memory Patterns:</strong> Note expected memory usage patterns</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Considerations</h2>
    <p>Understanding the overhead of memory tools:</p>
    <ul>
        <li><strong>Memory Tracking:</strong> ~5-10% overhead - acceptable for development</li>
        <li><strong>AddressSanitizer:</strong> ~2x slowdown - use only for debugging</li>
        <li><strong>UndefinedBehaviorSanitizer:</strong> ~1.5x slowdown - use for testing</li>
        <li><strong>Valgrind:</strong> ~10-50x slowdown - use only for deep analysis</li>
    </ul>
    <p><strong>Recommendation:</strong> Use memory tracking in development builds, sanitizers in test builds, and disable in release builds for performance.</p>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="core/memory-safety">Memory Safety & Profiling Documentation</a> - Complete reference</li>
        <li><a href="imgui">ImGui Debug Overlays</a> - Visual memory statistics</li>
        <li><a href="development/debugging">Debugging Guide</a> - General debugging techniques</li>
        <li><a href="core/structured-logging">Structured Logging</a> - Logging memory-related events</li>
    </ul>
</div>

