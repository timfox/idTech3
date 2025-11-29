<?php
/**
 * ImGui Debug Overlays Tutorial
 */
$title = 'ImGui Debug Overlays Tutorial - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/imgui-overlays' => 'ImGui Debug Overlays Tutorial'
];
?>

<h1>ImGui Debug Overlays Tutorial</h1>

<div class="section">
    <h2>Introduction</h2>
    <p>This tutorial will guide you through using ImGui debug overlays in id Tech 3. These overlays provide real-time debugging and profiling information directly in-game, making development and optimization much easier.</p>
    
    <div class="feature-list">
        <h3>What You'll Learn</h3>
        <ul>
            <li>Enabling and configuring ImGui overlays</li>
            <li>Using the Performance Overlay for FPS monitoring</li>
            <li>Using the Memory Overlay for leak detection</li>
            <li>Using the Network Overlay for connection monitoring</li>
            <li>Using the CVar Browser for quick configuration</li>
            <li>Using the Console Overlay for debugging</li>
            <li>Customizing overlay layouts</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Prerequisites</h2>
    <ul>
        <li>id Tech 3 engine with ImGui support enabled</li>
        <li>Vulkan or OpenGL renderer (ImGui requires renderer support)</li>
        <li>Basic understanding of console commands</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Initial Setup</h2>
    
    <h3>Step 1: Enable ImGui</h3>
    <p>First, enable ImGui support:</p>
    <div class="code-block">
        <pre><code># Enable ImGui
/set cl_imgui 1</code></pre>
    </div>
    
    <h3>Step 2: Verify ImGui is Working</h3>
    <p>You should see the debug menu bar at the top of the screen. If not:</p>
    <ul>
        <li>Check that your renderer supports ImGui (Vulkan recommended)</li>
        <li>Verify <code>cl_imgui</code> is set to 1</li>
        <li>Check console for any ImGui initialization errors</li>
    </ul>
    
    <h3>Step 3: Access the Debug Menu</h3>
    <p>Click on "Debug" in the menu bar to see available overlays:</p>
    <ul>
        <li>Performance</li>
        <li>Memory</li>
        <li>Network</li>
        <li>Renderer</li>
        <li>CVars</li>
        <li>Console</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Performance Overlay</h2>
    
    <h3>Step 1: Enable Performance Overlay</h3>
    <p>Open the Performance overlay:</p>
    <div class="code-block">
        <pre><code># Via CVar
/set cl_imgui_debug_performance 1

# Or via Debug menu: Debug → Performance</code></pre>
    </div>
    
    <h3>Step 2: Understanding Performance Metrics</h3>
    <p>The Performance overlay shows:</p>
    <ul>
        <li><strong>FPS:</strong> Current frames per second</li>
        <li><strong>Frame Time:</strong> Time per frame in milliseconds</li>
        <li><strong>Frame Time History:</strong> Graph showing last 120 frames</li>
        <li><strong>FPS History:</strong> Graph showing FPS over time</li>
        <li><strong>Client State:</strong> Connection status</li>
    </ul>
    
    <h3>Step 3: Identifying Performance Issues</h3>
    <p>Use the graphs to identify problems:</p>
    <ul>
        <li><strong>Frame Time Spikes:</strong> Look for sudden increases in frame time graph</li>
        <li><strong>Consistent Low FPS:</strong> Check if frame time is consistently high</li>
        <li><strong>FPS Drops:</strong> Correlate with specific game events</li>
    </ul>
    
    <h3>Step 4: Optimizing Based on Data</h3>
    <p>Example workflow:</p>
    <ol>
        <li>Enable performance overlay</li>
        <li>Play through problematic area</li>
        <li>Note frame time spikes in graph</li>
        <li>Correlate spikes with game events</li>
        <li>Optimize the problematic code/rendering</li>
        <li>Verify improvement in graph</li>
    </ol>
</div>

<div class="section">
    <h2>Tutorial: Memory Overlay</h2>
    
    <h3>Step 1: Build with Memory Tracking</h3>
    <p>Memory overlay requires memory tracking to be enabled at build time:</p>
    <div class="code-block">
        <pre><code>cd build
cmake .. -DENABLE_MEMORY_TRACKING=ON
make</code></pre>
    </div>
    
    <h3>Step 2: Enable Memory Overlay</h3>
    <div class="code-block">
        <pre><code># Enable memory tracking at runtime
/set memtrack_enable 1

# Enable memory overlay
/set cl_imgui_debug_memory 1</code></pre>
    </div>
    
    <h3>Step 3: Understanding Memory Statistics</h3>
    <p>The Memory overlay displays:</p>
    <ul>
        <li><strong>Total Memory:</strong> Allocated, freed, current, and peak usage</li>
        <li><strong>Memory by Type:</strong> Breakdown by category (HUNK, ZONE, RENDERER, etc.)</li>
        <li><strong>Leak Count:</strong> Number of detected memory leaks</li>
        <li><strong>Leak Report Button:</strong> Generate detailed leak report</li>
    </ul>
    
    <h3>Step 4: Detecting Memory Leaks</h3>
    <p>Workflow for finding leaks:</p>
    <ol>
        <li>Enable memory overlay</li>
        <li>Play through a scenario (e.g., load/unload maps)</li>
        <li>Watch the "Current" memory value</li>
        <li>If it keeps growing, you likely have a leak</li>
        <li>Click "Report Leaks" button</li>
        <li>Review leak report for allocation locations</li>
    </ol>
    
    <h3>Step 5: Analyzing Memory Usage</h3>
    <p>Use per-type statistics to identify memory hotspots:</p>
    <ul>
        <li>Check which memory type uses the most</li>
        <li>Compare "Current" vs "Peak" to see growth</li>
        <li>Monitor during different game scenarios</li>
        <li>Identify unexpected memory growth</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Network Overlay</h2>
    
    <h3>Step 1: Enable Network Overlay</h3>
    <div class="code-block">
        <pre><code># Enable network overlay
/set cl_imgui_debug_network 1

# Enable network statistics collection
/set cl_net_stats 1</code></pre>
    </div>
    
    <h3>Step 2: Understanding Network Statistics</h3>
    <p>The Network overlay shows:</p>
    <ul>
        <li><strong>Bytes Sent/Received:</strong> Total network traffic</li>
        <li><strong>Request Statistics:</strong> Total, successful, failed requests</li>
        <li><strong>Success Rate:</strong> Percentage of successful requests</li>
        <li><strong>Response Times:</strong> Average and last response time</li>
        <li><strong>Protocol Usage:</strong> HTTP/2 vs HTTP/1.1, IPv6 vs IPv4</li>
        <li><strong>Connection Info:</strong> Server address, ping, packet loss</li>
    </ul>
    
    <h3>Step 3: Monitoring Connection Quality</h3>
    <p>Use the overlay to:</p>
    <ul>
        <li>Monitor ping and packet loss in real-time</li>
        <li>Check if HTTP/2 is being used</li>
        <li>Identify network-related performance issues</li>
        <li>Verify connection pooling is working</li>
    </ul>
    
    <h3>Step 4: Debugging Network Issues</h3>
    <p>Common scenarios:</p>
    <ul>
        <li><strong>High Packet Loss:</strong> Check network connection quality</li>
        <li><strong>Low Success Rate:</strong> Investigate failed requests</li>
        <li><strong>Slow Response Times:</strong> Check server performance or network latency</li>
        <li><strong>HTTP/1.1 Instead of HTTP/2:</strong> Verify server and curl support</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: CVar Browser</h2>
    
    <h3>Step 1: Enable CVar Browser</h3>
    <div class="code-block">
        <pre><code># Enable CVar browser
/set cl_imgui_debug_cvars 1</code></pre>
    </div>
    
    <h3>Step 2: Using the Browser</h3>
    <p>The CVar browser provides:</p>
    <ul>
        <li><strong>Search/Filter:</strong> Type to filter CVars by name</li>
        <li><strong>CVar List:</strong> Scrollable list of all CVars</li>
        <li><strong>Value Editing:</strong> Click values to edit directly</li>
        <li><strong>Reset Button:</strong> Reset CVar to default value</li>
        <li><strong>Flags Display:</strong> Shows CVar flags (Archive, UserInfo, etc.)</li>
    </ul>
    
    <h3>Step 3: Finding CVars</h3>
    <p>Example: Finding renderer settings:</p>
    <ol>
        <li>Open CVar browser</li>
        <li>Type "r_" in the filter</li>
        <li>Browse renderer-related CVars</li>
        <li>Click on a CVar to see its description</li>
        <li>Edit value directly in the browser</li>
    </ol>
    
    <h3>Step 4: Quick Configuration</h3>
    <p>Use the browser for rapid configuration:</p>
    <ul>
        <li>Search for CVars instead of typing commands</li>
        <li>See all available options at once</li>
        <li>Understand CVar relationships</li>
        <li>Reset to defaults easily</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Console Overlay</h2>
    
    <h3>Step 1: Enable Console Overlay</h3>
    <div class="code-block">
        <pre><code># Enable console overlay
/set cl_imgui_debug_console 1</code></pre>
    </div>
    
    <h3>Step 2: Using the Console</h3>
    <p>Features:</p>
    <ul>
        <li><strong>Command Input:</strong> Type commands at the bottom</li>
        <li><strong>History:</strong> Scrollable history of console output</li>
        <li><strong>Filter:</strong> Filter console output by text</li>
        <li><strong>Auto-scroll:</strong> Automatically scrolls to latest output</li>
    </ul>
    
    <h3>Step 3: Filtering Console Output</h3>
    <p>Use the filter to find specific messages:</p>
    <ul>
        <li>Type "error" to see only error messages</li>
        <li>Type "renderer" to see renderer-related output</li>
        <li>Type "network" to see network messages</li>
        <li>Combine with structured logging categories</li>
    </ul>
    
    <h3>Step 4: Executing Commands</h3>
    <p>Execute commands directly from the overlay:</p>
    <ol>
        <li>Click in the command input field</li>
        <li>Type your command</li>
        <li>Press Enter to execute</li>
        <li>See output in the history area</li>
    </ol>
</div>

<div class="section">
    <h2>Tutorial: Renderer Overlay</h2>
    
    <h3>Step 1: Enable Renderer Overlay</h3>
    <div class="code-block">
        <pre><code># Enable renderer overlay
/set cl_imgui_debug_renderer 1

# Also enable renderer performance counters
/set r_speeds 1</code></pre>
    </div>
    
    <h3>Step 2: Understanding Renderer Information</h3>
    <p>The Renderer overlay shows:</p>
    <ul>
        <li><strong>Renderer Info:</strong> Name, vendor, version, extensions</li>
        <li><strong>Display Settings:</strong> Resolution, color/depth/stencil bits</li>
        <li><strong>Performance Counters:</strong> Draw calls, triangles, etc. (when r_speeds enabled)</li>
    </ul>
    
    <h3>Step 3: Using Renderer Stats</h3>
    <p>Monitor renderer performance:</p>
    <ul>
        <li>Check draw call count (lower is better)</li>
        <li>Monitor triangle count</li>
        <li>Verify renderer capabilities</li>
        <li>Check for missing extensions</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Customizing Layout</h2>
    
    <h3>Step 1: Arrange Windows</h3>
    <p>ImGui windows can be moved and resized:</p>
    <ul>
        <li>Click and drag window title bar to move</li>
        <li>Drag window edges to resize</li>
        <li>Windows remember their positions</li>
        <li>Close windows with the X button</li>
    </ul>
    
    <h3>Step 2: Create Your Layout</h3>
    <p>Suggested layouts:</p>
    <ul>
        <li><strong>Development:</strong> Performance top-left, Memory top-right, Console bottom</li>
        <li><strong>Network Debugging:</strong> Network overlay prominent, Console for commands</li>
        <li><strong>Memory Debugging:</strong> Memory overlay large, Performance for correlation</li>
        <li><strong>Configuration:</strong> CVar browser large, other overlays minimized</li>
    </ul>
    
    <h3>Step 3: Save Your Layout</h3>
    <p>Window positions are automatically saved in your config. To reset:</p>
    <div class="code-block">
        <pre><code># Close all overlays
/set cl_imgui_debug_performance 0
/set cl_imgui_debug_memory 0
# ... etc

# Reopen to get default positions</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Complete Debugging Workflow</h2>
    
    <h3>Scenario: Debugging Performance Issues</h3>
    
    <h4>Step 1: Enable All Relevant Overlays</h4>
    <div class="code-block">
        <pre><code>/set cl_imgui 1
/set cl_imgui_debug_performance 1
/set cl_imgui_debug_memory 1
/set cl_imgui_debug_renderer 1
/set r_speeds 1</code></pre>
    </div>
    
    <h4>Step 2: Reproduce the Issue</h4>
    <p>Play through the area with performance problems</p>
    
    <h4>Step 3: Analyze Performance Overlay</h4>
    <ul>
        <li>Check frame time graph for spikes</li>
        <li>Note FPS drops</li>
        <li>Correlate with game events</li>
    </ul>
    
    <h4>Step 4: Check Memory Overlay</h4>
    <ul>
        <li>Verify memory isn't growing unexpectedly</li>
        <li>Check if memory spikes correlate with frame time spikes</li>
        <li>Identify memory-heavy operations</li>
    </ul>
    
    <h4>Step 5: Review Renderer Stats</h4>
    <ul>
        <li>Check draw call count</li>
        <li>Monitor triangle count</li>
        <li>Identify rendering bottlenecks</li>
    </ul>
    
    <h4>Step 6: Use CVar Browser to Experiment</h4>
    <ul>
        <li>Try different quality settings</li>
        <li>Disable features to isolate issues</li>
        <li>Monitor performance changes in real-time</li>
    </ul>
    
    <h4>Step 7: Fix and Verify</h4>
    <p>Make optimizations and verify improvements in the overlays</p>
</div>

<div class="section">
    <h2>Best Practices</h2>
    <ul>
        <li><strong>Keep Performance Overlay Visible:</strong> Always monitor frame times during development</li>
        <li><strong>Use Memory Overlay Regularly:</strong> Check for leaks frequently</li>
        <li><strong>Customize Layout:</strong> Arrange windows for your workflow</li>
        <li><strong>Combine Overlays:</strong> Use multiple overlays together for comprehensive debugging</li>
        <li><strong>Disable When Not Needed:</strong> Turn off overlays in release builds for performance</li>
        <li><strong>Use Filters:</strong> Filter console output to find relevant information quickly</li>
        <li><strong>Save Configurations:</strong> Create different configs for different debugging scenarios</li>
    </ul>
</div>

<div class="section">
    <h2>Keyboard Shortcuts</h2>
    <ul>
        <li><strong>F12:</strong> Toggle ImGui (if configured)</li>
        <li><strong>Mouse:</strong> Click and drag to move windows</li>
        <li><strong>Enter:</strong> Execute commands in console overlay</li>
        <li><strong>Escape:</strong> Close focused overlay window</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    
    <h3>Overlays Not Showing</h3>
    <ul>
        <li>Verify <code>cl_imgui</code> is enabled</li>
        <li>Check renderer supports ImGui (Vulkan recommended)</li>
        <li>Verify individual overlay CVars are set</li>
        <li>Check console for initialization errors</li>
    </ul>
    
    <h3>Performance Issues</h3>
    <ul>
        <li>Disable unused overlays</li>
        <li>Reduce frame history size if configurable</li>
        <li>Close overlay windows when not needed</li>
    </ul>
    
    <h3>Memory Overlay Empty</h3>
    <ul>
        <li>Build with <code>ENABLE_MEMORY_TRACKING=ON</code></li>
        <li>Enable <code>memtrack_enable</code> at runtime</li>
        <li>Verify memory tracking is initialized</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="imgui">ImGui Debug Overlays Documentation</a> - Complete reference</li>
        <li><a href="core/memory-safety">Memory Safety & Profiling</a> - Memory debugging</li>
        <li><a href="core/structured-logging">Structured Logging</a> - Logging system</li>
        <li><a href="development/debugging">Debugging Guide</a> - General debugging</li>
    </ul>
</div>

