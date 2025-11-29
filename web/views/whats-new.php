<?php
/**
 * What's New - Recent Enhancements
 */
$title = "What's New - id Tech 3 Documentation";
$breadcrumbs = [
    '/whats-new' => "What's New"
];
?>

<h1>What's New</h1>

<div class="section">
    <h2>Recent Enhancements</h2>
    <p>This page highlights the major enhancements and new features added to the id Tech 3 engine. These improvements modernize the codebase while maintaining backward compatibility.</p>
</div>

<div class="section">
    <h2>Renderer Enhancements</h2>
    
    <h3>DirectX 12 Renderer</h3>
    <p><strong>New:</strong> Full DirectX 12 renderer support for Windows platforms.</p>
    <ul>
        <li>Modern graphics API with lower CPU overhead</li>
        <li>DirectX Raytracing (DXR) support for hardware-accelerated ray tracing</li>
        <li>Triple buffering and efficient command list recording</li>
        <li>Feature level 12.0+ support with automatic fallback</li>
    </ul>
    <p>See <a href="rendering/directx12">DirectX 12 Renderer</a> for complete documentation.</p>
    
    <h3>Animated Skybox (Flipbook)</h3>
    <p><strong>New:</strong> Support for animated skyboxes with frame-based animation.</p>
    <ul>
        <li>Per-side independent animation</li>
        <li>Configurable animation speed (frames per second)</li>
        <li>Up to 24 frames per side</li>
        <li>Backward compatible with existing skybox shaders</li>
    </ul>
    <p>See <a href="rendering/animated-skybox">Animated Skybox</a> for shader syntax and examples.</p>
</div>

<div class="section">
    <h2>Core Engine Improvements</h2>
    
    <h3>Input System Enhancements</h3>
    <p><strong>Enhanced:</strong> Significant improvements to input handling and usability.</p>
    <ul>
        <li>Raw mouse input support - automatic detection and use when available</li>
        <li>Unlagged mouse event processing - reduced input latency</li>
        <li>Window minimize hotkey (<code>in_minimize</code>) - Windows-only, replaces Q3Minimizer</li>
        <li>Video pipe support - FFmpeg integration for better quality recording</li>
        <li>Improved mouse smoothing and acceleration options</li>
    </ul>
    <p>See <a href="core/input-improvements">Input System Improvements</a> for complete details.</p>
    
    <h3>Filesystem Enhancements</h3>
    <p><strong>Enhanced:</strong> Major filesystem improvements for better scalability.</p>
    <ul>
        <li>Raised filesystem limits - up to 20,000 maps per directory</li>
        <li>Recursive directory scanning - <code>FS_MATCH_SUBDIRS</code> flag support</li>
        <li>Improved mod support - sorted lists, cleaned descriptions</li>
        <li>Reworked library loading - predefined search paths, more secure</li>
        <li>Fixed directory traversal crashes</li>
    </ul>
    <p>See <a href="core/filesystem-improvements">Filesystem Improvements</a> for details.</p>
    
    <h3>Memory System Improvements</h3>
    <p><strong>Enhanced:</strong> Reworked memory allocator and management.</p>
    <ul>
        <li>Reworked Zone memory allocator - no more out-of-memory errors</li>
        <li>Improved server-side memory usage - significantly reduced</li>
        <li>Better DoS protection - memory limits and resource protection</li>
        <li>Enhanced memory tracking - per-type statistics</li>
    </ul>
    <p>See <a href="core/memory-improvements">Memory System Improvements</a> for complete information.</p>
    
    <h3>QVM (Virtual Machine) Improvements</h3>
    <p><strong>Enhanced:</strong> Significantly reworked QVM for better performance and reliability.</p>
    <ul>
        <li>Optimized interpreter - faster execution</li>
        <li>Improved memory management - data segment reuse, better allocation</li>
        <li>IEEE 754 compliance - proper NaN handling, predictable floating-point</li>
        <li>Better error handling - more descriptive messages, improved debugging</li>
        <li>Enhanced stability - better bounds checking, memory protection</li>
    </ul>
    <p>See <a href="core/qvm-improvements">QVM Improvements</a> for technical details.</p>
    
    <h3>Structured Logging System</h3>
    <p><strong>New:</strong> Modern logging system replacing legacy <code>Com_Printf</code>.</p>
    <ul>
        <li>Log levels: DEBUG, INFO, WARN, ERROR, FATAL</li>
        <li>Category-based filtering (renderer, network, filesystem, etc.)</li>
        <li>JSON output format for log aggregation systems</li>
        <li>Automatic log rotation (size-based and time-based)</li>
        <li>Syslog integration for Linux/Unix</li>
        <li>Backward compatible with existing code</li>
    </ul>
    <p>See <a href="core/structured-logging">Structured Logging</a> for configuration and usage.</p>
    
    <h3>Memory Safety & Profiling</h3>
    <p><strong>New:</strong> Comprehensive memory safety tools and profiling capabilities.</p>
    <ul>
        <li>AddressSanitizer (ASan) support for detecting memory errors</li>
        <li>UndefinedBehaviorSanitizer (UBSan) for undefined behavior detection</li>
        <li>Memory tracking system with per-type statistics</li>
        <li>Automatic leak detection and reporting</li>
        <li>Valgrind integration for Linux</li>
        <li>Dr. Memory integration for Windows</li>
    </ul>
    <p>See <a href="core/memory-safety">Memory Safety & Profiling</a> for usage and best practices.</p>
    
    <h3>Code Quality Improvements</h3>
    <p><strong>Enhanced:</strong> Significant refactoring for safety and maintainability.</p>
    <ul>
        <li>Fixed unsafe <code>strcpy()</code> usage - replaced with <code>Q_strncpyz()</code></li>
        <li>Modernized <code>Com_sprintf()</code> with bounds checking</li>
        <li>Added header guards to prevent multiple inclusion</li>
        <li>Standardized error handling with helper macros</li>
        <li>Added unit test framework</li>
        <li>Static analysis support (clang-tidy, cppcheck)</li>
    </ul>
</div>

<div class="section">
    <h2>Networking Enhancements</h2>
    
    <h3>HTTP/2 Support</h3>
    <p><strong>New:</strong> HTTP/2 protocol support for faster downloads.</p>
    <ul>
        <li>Multiplexing: Multiple requests over a single connection</li>
        <li>Header compression for reduced overhead</li>
        <li>10-30% faster on high-latency connections</li>
        <li>Automatic fallback to HTTP/1.1 if not supported</li>
    </ul>
    
    <h3>Connection Pooling</h3>
    <p><strong>New:</strong> HTTP connection pooling for improved performance.</p>
    <ul>
        <li>Reuse connections to reduce overhead</li>
        <li>20-50% faster for repeated downloads from same server</li>
        <li>Configurable pool size and timeout</li>
    </ul>
    
    <h3>Rate Limiting</h3>
    <p><strong>New:</strong> Intelligent rate limiting to prevent server overload.</p>
    <ul>
        <li>Configurable requests per second limit</li>
        <li>Concurrent request limiting</li>
        <li>Prevents being blocked by servers</li>
    </ul>
    
    <h3>IPv6 Improvements</h3>
    <p><strong>Enhanced:</strong> Better IPv6 support and dual-stack handling.</p>
    <ul>
        <li>Configurable IPv6 preference</li>
        <li>Automatic detection and support</li>
        <li>Better connectivity in IPv6-only environments</li>
    </ul>
    
    <h3>WebSocket Support</h3>
    <p><strong>New:</strong> Real-time bidirectional communication support.</p>
    <ul>
        <li>Client connections (ws:// and wss://)</li>
        <li>Automatic reconnection with exponential backoff</li>
        <li>Event callbacks for connect, disconnect, and errors</li>
        <li>Up to 16 concurrent connections</li>
        <li>Integrated with engine event loop</li>
    </ul>
    <p>See <a href="networking/websocket">WebSocket Support</a> for API reference and examples.</p>
    
    <p>See <a href="networking/networking">Networking</a> for complete documentation on all networking features.</p>
</div>

<div class="section">
    <h2>Developer Tools</h2>
    
    <h3>ImGui Debug Overlays</h3>
    <p><strong>New:</strong> Comprehensive in-engine debugging and profiling overlays.</p>
    <ul>
        <li><strong>Performance Overlay:</strong> Real-time FPS, frame time, and performance graphs</li>
        <li><strong>Memory Overlay:</strong> Memory usage statistics and leak detection</li>
        <li><strong>Network Overlay:</strong> Network statistics and connection information</li>
        <li><strong>Renderer Overlay:</strong> Renderer information and performance counters</li>
        <li><strong>CVar Browser:</strong> Interactive console variable browser and editor</li>
        <li><strong>Console Overlay:</strong> Console output viewer with filtering</li>
    </ul>
    <p>See <a href="imgui">ImGui Debug Overlays</a> for usage and configuration.</p>
</div>

<div class="section">
    <h2>Build System Improvements</h2>
    
    <h3>Modern CMake Configuration</h3>
    <ul>
        <li>Cross-platform build support</li>
        <li>Dependency management</li>
        <li>IDE integration (Visual Studio, CLion, VS Code)</li>
        <li>Parallel builds for faster compilation</li>
    </ul>
    
    <h3>Static Analysis Support</h3>
    <ul>
        <li>clang-tidy integration</li>
        <li>cppcheck support</li>
        <li>Automated code quality checks</li>
    </ul>
    
    <h3>Unit Test Framework</h3>
    <ul>
        <li>Simple assertion macros</li>
        <li>Test statistics tracking</li>
        <li>Easy-to-use test runner</li>
    </ul>
    
    <p>See <a href="modernization/build-systems">Modern Build Systems</a> for details.</p>
</div>

<div class="section">
    <h2>Migration Guide</h2>
    <p>Most enhancements are backward compatible. Here's what you need to know:</p>
    
    <h3>For Users</h3>
    <ul>
        <li>All existing configurations continue to work</li>
        <li>New features are opt-in via CVars</li>
        <li>No changes required to existing maps or mods</li>
    </ul>
    
    <h3>For Developers</h3>
    <ul>
        <li>Existing <code>Com_Printf</code> calls automatically use structured logging</li>
        <li>Memory tracking is optional and can be enabled at build time</li>
        <li>New APIs are additive - old code continues to work</li>
        <li>See individual feature documentation for migration details</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Improvements</h2>
    <ul>
        <li><strong>Connection Pooling:</strong> 20-50% faster downloads from same server</li>
        <li><strong>HTTP/2:</strong> 10-30% faster on high-latency connections</li>
        <li><strong>DirectX 12:</strong> Lower CPU overhead, better multi-threading</li>
        <li><strong>Memory Tracking:</strong> Minimal overhead (~5-10%) when enabled</li>
        <li><strong>Structured Logging:</strong> Efficient filtering and formatting</li>
    </ul>
</div>

<div class="section">
    <h2>Documentation Updates</h2>
    <p>All new features are fully documented:</p>
    <ul>
        <li><a href="rendering/directx12">DirectX 12 Renderer</a></li>
        <li><a href="rendering/animated-skybox">Animated Skybox</a></li>
        <li><a href="core/structured-logging">Structured Logging</a></li>
        <li><a href="core/memory-safety">Memory Safety & Profiling</a></li>
        <li><a href="networking/networking">Enhanced Networking</a></li>
        <li><a href="networking/websocket">WebSocket Support</a></li>
        <li><a href="imgui">ImGui Debug Overlays</a></li>
    </ul>
</div>

<div class="section">
    <h2>Historical Context</h2>
    <p>These modern enhancements build upon id Tech 3's rich history, which spans over two decades from its original commercial release through open source development to current community-driven improvements.</p>
    <p>See <a href="history">History of id Tech 3</a> to learn about the engine's origins, development, commercial success, open source release, and evolution.</p>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="history">History of id Tech 3</a> - Engine development and evolution</li>
        <li><a href="getting-started/quick-start">Quick Start Guide</a></li>
        <li><a href="idtech3">id Tech 3 Engine Features</a></li>
        <li><a href="modernization/modern-cpp">Modern C++ Features</a></li>
        <li><a href="core/engine-subsystems">Engine Subsystems</a></li>
        <li><a href="development/debugging">Debugging Tools</a></li>
    </ul>
</div>

