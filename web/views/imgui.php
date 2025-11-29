<?php
/**
 * ImGui Debug Overlays Documentation
 */
$title = 'ImGui Debug Overlays - id Tech 3 Documentation';
$breadcrumbs = [
    '/imgui' => 'ImGui Debug Overlays'
];
?>

<h1>ImGui Debug Overlays</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 engine now includes comprehensive ImGui-based debug overlays for real-time debugging and profiling. These overlays provide detailed information about performance, memory, networking, rendering, and more.</p>
    
    <div class="feature-list">
        <h3>Available Overlays</h3>
        <ul>
            <li><strong>Performance Overlay:</strong> Real-time FPS, frame time, and performance graphs</li>
            <li><strong>Memory Overlay:</strong> Memory usage statistics and leak detection</li>
            <li><strong>Network Overlay:</strong> Network statistics and connection information</li>
            <li><strong>Renderer Overlay:</strong> Renderer information and performance counters</li>
            <li><strong>CVar Browser:</strong> Interactive console variable browser and editor</li>
            <li><strong>Console Overlay:</strong> Console output viewer with filtering</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Enabling ImGui</h2>
    <p>First, enable ImGui support:</p>
    <div class="code-block">
        <pre><code>/set cl_imgui 1</code></pre>
    </div>
</div>

<div class="section">
    <h2>Performance Overlay</h2>
    <p>Displays real-time performance metrics:</p>
    <ul>
        <li><strong>FPS:</strong> Current frames per second</li>
        <li><strong>Frame Time:</strong> Time per frame in milliseconds</li>
        <li><strong>Frame Time History:</strong> Graph showing frame time over the last 120 frames</li>
        <li><strong>FPS History:</strong> Graph showing FPS over the last 120 frames</li>
        <li><strong>Client State:</strong> Current connection state</li>
        <li><strong>Server/Client Time:</strong> Time synchronization information</li>
    </ul>
    
    <h3>Usage</h3>
    <div class="code-block">
        <pre><code># Enable performance overlay
/set cl_imgui_debug_performance 1</code></pre>
    </div>
</div>

<div class="section">
    <h2>Memory Overlay</h2>
    <p>Shows memory usage statistics (requires <code>ENABLE_MEMORY_TRACKING</code>):</p>
    <ul>
        <li><strong>Total Memory:</strong> Allocated, freed, current, and peak usage</li>
        <li><strong>Memory by Type:</strong> Breakdown by memory type (HUNK, ZONE, TEMP, SOUND, RENDERER, etc.)</li>
        <li><strong>Leak Detection:</strong> Number of detected memory leaks</li>
        <li><strong>Leak Reporting:</strong> Button to generate detailed leak reports</li>
    </ul>
    
    <h3>Usage</h3>
    <div class="code-block">
        <pre><code># Enable memory overlay
/set cl_imgui_debug_memory 1</code></pre>
    </div>
    
    <p><strong>Note:</strong> Requires building with <code>ENABLE_MEMORY_TRACKING=ON</code> for full functionality.</p>
</div>

<div class="section">
    <h2>Network Overlay</h2>
    <p>Displays network statistics:</p>
    <ul>
        <li><strong>Bytes Sent/Received:</strong> Total network traffic</li>
        <li><strong>Request Statistics:</strong> Total, successful, and failed requests</li>
        <li><strong>Success Rate:</strong> Percentage of successful requests</li>
        <li><strong>Response Times:</strong> Average and last response time</li>
        <li><strong>Protocol Usage:</strong> HTTP/2 vs HTTP/1.1, IPv6 vs IPv4</li>
        <li><strong>Connection Info:</strong> Server address, ping, packet loss</li>
    </ul>
    
    <h3>Usage</h3>
    <div class="code-block">
        <pre><code># Enable network overlay
/set cl_imgui_debug_network 1</code></pre>
    </div>
</div>

<div class="section">
    <h2>Renderer Overlay</h2>
    <p>Shows renderer information:</p>
    <ul>
        <li><strong>Renderer Info:</strong> Renderer name, vendor, version, extensions</li>
        <li><strong>Display Settings:</strong> Resolution, color/depth/stencil bits</li>
        <li><strong>Performance Counters:</strong> Renderer performance metrics (when <code>r_speeds</code> is enabled)</li>
    </ul>
    
    <h3>Usage</h3>
    <div class="code-block">
        <pre><code># Enable renderer overlay
/set cl_imgui_debug_renderer 1</code></pre>
    </div>
</div>

<div class="section">
    <h2>CVar Browser</h2>
    <p>Interactive console variable browser:</p>
    <ul>
        <li><strong>Filter:</strong> Search/filter CVars by name</li>
        <li><strong>CVar List:</strong> Scrollable list of all CVars</li>
        <li><strong>CVar Details:</strong> View name, value, type, flags, description</li>
        <li><strong>Edit Values:</strong> Change CVar values directly</li>
        <li><strong>Reset:</strong> Reset CVars to default values</li>
        <li><strong>Flags Display:</strong> Shows CVar flags (Archive, UserInfo, ServerInfo, ROM, Init, Latch)</li>
    </ul>
    
    <h3>Usage</h3>
    <div class="code-block">
        <pre><code># Enable CVar browser
/set cl_imgui_debug_cvars 1</code></pre>
    </div>
</div>

<div class="section">
    <h2>Console Overlay</h2>
    <p>Console output viewer:</p>
    <ul>
        <li><strong>Filter:</strong> Filter console output</li>
        <li><strong>Auto-scroll:</strong> Automatically scroll to latest output</li>
        <li><strong>Command Input:</strong> Execute console commands</li>
        <li><strong>Scrollable History:</strong> View console history</li>
    </ul>
    
    <h3>Usage</h3>
    <div class="code-block">
        <pre><code># Enable console overlay
/set cl_imgui_debug_console 1</code></pre>
    </div>
</div>

<div class="section">
    <h2>Main Menu</h2>
    <p>Debug menu bar with quick access to all overlays:</p>
    <ul>
        <li><strong>Debug Menu:</strong> Toggle individual overlays</li>
        <li><strong>Help Menu:</strong> Access help and information</li>
    </ul>
    
    <h3>Usage</h3>
    <div class="code-block">
        <pre><code># Enable main menu (default: enabled)
/set cl_imgui_debug_mainmenu 1</code></pre>
    </div>
</div>

<div class="section">
    <h2>Opening Debug Overlays</h2>
    
    <h3>Via Main Menu</h3>
    <p>The debug menu bar appears at the top when <code>cl_imgui_debug_mainmenu</code> is enabled (default: 1). Click "Debug" → Select overlay to toggle.</p>
    
    <h3>Via CVars</h3>
    <div class="code-block">
        <pre><code>/set cl_imgui_debug_performance 1
/set cl_imgui_debug_memory 1
/set cl_imgui_debug_network 1
/set cl_imgui_debug_renderer 1
/set cl_imgui_debug_cvars 1
/set cl_imgui_debug_console 1</code></pre>
    </div>
</div>

<div class="section">
    <h2>Closing Overlays</h2>
    <ul>
        <li>Click the "X" button on each overlay window, or</li>
        <li>Toggle the CVar off: <code>/set cl_imgui_debug_performance 0</code></li>
    </ul>
</div>

<div class="section">
    <h2>Keyboard Shortcuts</h2>
    <ul>
        <li><strong>F12:</strong> Toggle ImGui (if configured)</li>
        <li><strong>Mouse:</strong> Click and drag to move windows</li>
        <li><strong>Enter:</strong> Execute commands in console overlay</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Impact</h2>
    <ul>
        <li><strong>Minimal:</strong> Overlays are lightweight and only render when visible</li>
        <li><strong>Frame Time:</strong> Typically adds &lt; 0.1ms per visible overlay</li>
        <li><strong>Memory:</strong> Negligible memory overhead</li>
    </ul>
</div>

<div class="section">
    <h2>Tips</h2>
    <ul>
        <li><strong>Performance Monitoring:</strong> Keep the performance overlay visible during development to catch frame time spikes</li>
        <li><strong>Memory Debugging:</strong> Enable memory tracking (<code>ENABLE_MEMORY_TRACKING=ON</code>) and use the memory overlay to find leaks</li>
        <li><strong>Network Debugging:</strong> Use the network overlay to monitor connection quality and protocol usage</li>
        <li><strong>CVar Tweaking:</strong> Use the CVar browser to quickly find and modify settings without typing commands</li>
        <li><strong>Console History:</strong> Use the console overlay to review past output and filter for specific messages</li>
    </ul>
</div>

<div class="section">
    <h2>Integration</h2>
    <p>The debug overlays integrate seamlessly with:</p>
    <ul>
        <li><strong>Memory Tracking System:</strong> Shows memory statistics when enabled</li>
        <li><strong>Enhanced Networking:</strong> Displays network statistics from enhanced networking features</li>
        <li><strong>Structured Logging:</strong> Can display log output in console overlay (future enhancement)</li>
        <li><strong>Renderer:</strong> Shows renderer statistics and performance counters</li>
    </ul>
</div>

<div class="section">
    <h2>Customization</h2>
    <p>All overlays can be customized via CVars:</p>
    <ul>
        <li>Window positions are remembered</li>
        <li>Window sizes can be adjusted</li>
        <li>Overlays can be enabled/disabled individually</li>
        <li>Main menu can be hidden if desired</li>
    </ul>
</div>

<div class="section">
    <h2>Future Enhancements</h2>
    <p>Planned improvements:</p>
    <ul>
        <li><strong>Console Integration:</strong> Full console output capture and display</li>
        <li><strong>Log Viewer:</strong> View structured logs with filtering</li>
        <li><strong>Profiler Integration:</strong> Display Tracy profiler data</li>
        <li><strong>Entity Browser:</strong> Browse and inspect game entities</li>
        <li><strong>Shader Debugger:</strong> Debug shader compilation and execution</li>
        <li><strong>Asset Browser:</strong> Browse loaded assets (models, textures, sounds)</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    
    <h3>Overlays not showing</h3>
    <ul>
        <li>Ensure <code>cl_imgui</code> is enabled: <code>/set cl_imgui 1</code></li>
        <li>Check that ImGui backend is initialized (renderer support required)</li>
        <li>Verify CVars are set correctly</li>
    </ul>
    
    <h3>Performance issues</h3>
    <ul>
        <li>Disable unused overlays</li>
        <li>Reduce frame history size if needed</li>
        <li>Check renderer performance counters</li>
    </ul>
    
    <h3>Memory overlay empty</h3>
    <ul>
        <li>Build with <code>ENABLE_MEMORY_TRACKING=ON</code></li>
        <li>Ensure memory tracking is initialized</li>
    </ul>
    
    <h3>Network overlay empty</h3>
    <ul>
        <li>Ensure enhanced networking is enabled</li>
        <li>Check that network statistics are being collected</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="external/imgui-integration">ImGui Integration (C++)</a></li>
        <li><a href="external/cimgui-quake3e">CimGui + Quake3e Walkthrough</a></li>
        <li><a href="core/memory-safety">Memory Safety & Profiling</a></li>
        <li><a href="core/structured-logging">Structured Logging</a></li>
        <li><a href="development/debugging">Debugging Tools</a></li>
        <li><a href="networking/networking">Networking</a></li>
    </ul>
</div>
