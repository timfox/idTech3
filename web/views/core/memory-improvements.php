<?php
/**
 * Memory System Improvements
 */
$title = 'Memory System Improvements - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/memory-improvements' => 'Memory System Improvements'
];
?>

<h1>Memory System Improvements</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 engine has received significant memory system improvements, including a reworked Zone memory allocator and enhanced memory management capabilities.</p>
</div>

<div class="section">
    <h2>Reworked Zone Memory Allocator</h2>
    
    <h3>Overview</h3>
    <p>The Zone memory allocator has been completely reworked to eliminate out-of-memory errors and provide better memory management.</p>
    
    <h3>Key Improvements</h3>
    <ul>
        <li><strong>No More Out-of-Memory Errors:</strong> Improved allocation strategies prevent OOM conditions</li>
        <li><strong>Better Fragmentation Handling:</strong> Reduced memory fragmentation</li>
        <li><strong>Improved Allocation:</strong> More efficient memory allocation algorithms</li>
        <li><strong>Better Cleanup:</strong> Improved memory deallocation and cleanup</li>
    </ul>
    
    <h3>Technical Details</h3>
    <p>The new Zone allocator features:</p>
    <ul>
        <li>Improved allocation strategies</li>
        <li>Better memory pool management</li>
        <li>Reduced fragmentation</li>
        <li>More robust error handling</li>
        <li>Better memory tracking</li>
    </ul>
    
    <h3>Benefits</h3>
    <ul>
        <li>More stable gameplay</li>
        <li>Better performance in memory-constrained situations</li>
        <li>Reduced crashes from memory issues</li>
        <li>Better support for large maps and mods</li>
    </ul>
</div>

<div class="section">
    <h2>Memory Type Tracking</h2>
    
    <h3>Per-Type Statistics</h3>
    <p>The memory system tracks usage by type:</p>
    <ul>
        <li><strong>MEMTYPE_HUNK:</strong> Temporary allocations</li>
        <li><strong>MEMTYPE_ZONE:</strong> Game state memory</li>
        <li><strong>MEMTYPE_RENDERER:</strong> Graphics memory</li>
        <li><strong>MEMTYPE_SOUND:</strong> Audio buffers</li>
        <li><strong>MEMTYPE_MISC:</strong> Other allocations</li>
    </ul>
    
    <h3>Memory Statistics</h3>
    <p>View memory statistics via:</p>
    <div class="code-block">
        <pre><code># Enable memory tracking
/set memtrack_enable 1

# View statistics in ImGui overlay
/set cl_imgui 1
/set cl_imgui_debug_memory 1</code></pre>
    </div>
</div>

<div class="section">
    <h2>Memory Safety Tools</h2>
    
    <h3>AddressSanitizer Support</h3>
    <p>Build with AddressSanitizer to detect memory errors:</p>
    <div class="code-block">
        <pre><code>cd build
cmake .. -DENABLE_ASAN=ON
make</code></pre>
    </div>
    
    <h3>UndefinedBehaviorSanitizer</h3>
    <p>Detect undefined behavior:</p>
    <div class="code-block">
        <pre><code>cmake .. -DENABLE_UBSAN=ON</code></pre>
    </div>
    
    <h3>Leak Detection</h3>
    <p>Automatic leak detection and reporting:</p>
    <div class="code-block">
        <pre><code># Enable leak reporting
/set memtrack_report_leaks 1

# Log leaks to file
/set memtrack_log_leaks 1</code></pre>
    </div>
</div>

<div class="section">
    <h2>Server-Side Memory Improvements</h2>
    
    <h3>Reduced Memory Usage</h3>
    <p>Server-side memory usage has been significantly reduced:</p>
    <ul>
        <li>More efficient client state management</li>
        <li>Better memory pooling</li>
        <li>Reduced per-client overhead</li>
        <li>Improved cleanup of disconnected clients</li>
    </ul>
    
    <h3>DoS Protection</h3>
    <p>Improved server-side DoS protection includes:</p>
    <ul>
        <li>Memory usage limits</li>
        <li>Rate limiting</li>
        <li>Connection limits</li>
        <li>Resource protection</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Impact</h2>
    
    <h3>Memory Tracking Overhead</h3>
    <ul>
        <li>Minimal overhead (~5-10%) when enabled</li>
        <li>Can be disabled in release builds</li>
        <li>Optional feature - no impact if disabled</li>
    </ul>
    
    <h3>Allocator Performance</h3>
    <ul>
        <li>Faster allocation/deallocation</li>
        <li>Reduced fragmentation overhead</li>
        <li>Better cache locality</li>
    </ul>
</div>

<div class="section">
    <h2>Best Practices</h2>
    <ul>
        <li>Enable memory tracking during development</li>
        <li>Use memory statistics to identify hotspots</li>
        <li>Monitor memory usage in production</li>
        <li>Use sanitizers for debugging</li>
        <li>Check for leaks regularly</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="core/memory-safety">Memory Safety & Profiling</a> - Complete memory safety documentation</li>
        <li><a href="core/memory-management">Memory Management</a> - Memory system architecture</li>
        <li><a href="tutorials/memory-profiling">Memory Profiling Tutorial</a> - How to use memory tools</li>
        <li><a href="imgui">ImGui Debug Overlays</a> - Visual memory statistics</li>
    </ul>
</div>

