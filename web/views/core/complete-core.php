<?php
/**
 * Complete Core Systems Documentation
 */
$title = 'Complete Core Systems - id Tech 3 Documentation';
$breadcrumbs = [
    '/core' => 'Core Engine',
    '/core/complete-core' => 'Complete Core Systems'
];
?>

<div class="content-section">
    <h1>Complete Core Systems Guide</h1>
    
    <blockquote>
        <strong>Core Engine Systems:</strong> This guide covers all core engine systems including memory management, filesystem, input, console, virtual machine, and modern enhancements.
    </blockquote>

    <div class="section">
        <h2>Memory Management</h2>
        
        <h3>Memory Zones</h3>
        <p>The engine uses multiple memory allocators:</p>
        <ul>
            <li><strong>Zone Allocator:</strong> General-purpose, reworked for reliability</li>
            <li><strong>Hunk Allocator:</strong> Large, persistent allocations</li>
            <li><strong>Temp Allocator:</strong> Short-lived, frame-based allocations</li>
            <li><strong>Tagged Allocations:</strong> Memory tracking by subsystem</li>
        </ul>

        <h3>Memory Safety</h3>
        <ul>
            <li>AddressSanitizer (ASan) support</li>
            <li>UndefinedBehaviorSanitizer (UBSan) support</li>
            <li>Memory leak detection</li>
            <li>Bounds checking</li>
            <li>Comprehensive memory tracking</li>
        </ul>

        <h3>Improvements</h3>
        <ul>
            <li>No more out-of-memory errors</li>
            <li>Reduced server memory usage</li>
            <li>Better DoS protection</li>
            <li>Per-type memory statistics</li>
        </ul>

        <p>See <a href="core/memory-management">Memory Management</a> and <a href="core/memory-safety">Memory Safety</a> for details.</p>
    </div>

    <div class="section">
        <h2>Filesystem</h2>
        
        <h3>Virtual File System</h3>
        <ul>
            <li>PK3 archive support</li>
            <li>PK3DIR directory support</li>
            <li>Recursive directory scanning</li>
            <li>File search paths</li>
            <li>Mod detection and loading</li>
        </ul>

        <h3>Improvements</h3>
        <ul>
            <li>Raised limits (20,000 maps per directory)</li>
            <li>Recursive scanning with FS_MATCH_SUBDIRS</li>
            <li>Improved mod support</li>
            <li>Reworked library loading</li>
            <li>Directory traversal protection</li>
        </ul>

        <p>See <a href="core/filesystem">Filesystem</a> and <a href="core/filesystem-improvements">Filesystem Improvements</a> for details.</p>
    </div>

    <div class="section">
        <h2>Input System</h2>
        
        <h3>Input Features</h3>
        <ul>
            <li><strong>Raw Mouse Input:</strong> Automatic detection and use</li>
            <li><strong>Unlagged Processing:</strong> Reduced input latency</li>
            <li><strong>Keyboard:</strong> Key mapping and bindings</li>
            <li><strong>Mouse:</strong> Smoothing and acceleration</li>
            <li><strong>Gamepad:</strong> SDL2 controller support</li>
            <li><strong>Minimize Hotkey:</strong> Window minimize/restore (Windows)</li>
            <li><strong>Video Pipe:</strong> FFmpeg integration</li>
        </ul>

        <h3>Configuration</h3>
        <div class="code-block">
            <pre><code>set in_mouse "1"                  // Enable mouse
set in_rawmouse "1"               // Raw mouse input
set in_lagged "0"                 // Unlagged processing
set in_minimize "F11"             // Minimize hotkey (Windows)
set in_sensitivity "5.0"          // Mouse sensitivity</code></pre>
        </div>

        <p>See <a href="core/input-system">Input System</a> and <a href="core/input-improvements">Input Improvements</a> for details.</p>
    </div>

    <div class="section">
        <h2>Console System</h2>
        
        <h3>Features</h3>
        <ul>
            <li>CVAR management</li>
            <li>Command execution</li>
            <li>Command history</li>
            <li>Auto-completion</li>
            <li>Script execution</li>
            <li>Console variables with descriptions</li>
        </ul>

        <h3>CVAR Types</h3>
        <ul>
            <li><strong>CVAR_ARCHIVE:</strong> Saved to config</li>
            <li><strong>CVAR_LATCH:</strong> Requires restart</li>
            <li><strong>CVAR_CHEAT:</strong> Cheat protection</li>
            <li><strong>CVAR_USERINFO:</strong> Sent to server</li>
            <li><strong>CVAR_SERVERINFO:</strong> Server info</li>
        </ul>

        <p>See <a href="core/console-system">Console System</a> for details.</p>
    </div>

    <div class="section">
        <h2>Virtual Machine (QVM)</h2>
        
        <h3>QVM System</h3>
        <ul>
            <li>Bytecode interpreter</li>
            <li>Sandboxed execution</li>
            <li>Game logic execution</li>
            <li>Mod support</li>
        </ul>

        <h3>Improvements</h3>
        <ul>
            <li>Significantly reworked interpreter</li>
            <li>IEEE 754 floating-point compliance</li>
            <li>Better error handling</li>
            <li>Improved performance</li>
            <li>Enhanced stability</li>
            <li>Better memory management</li>
        </ul>

        <p>See <a href="core/virtual-machine">Virtual Machine</a> and <a href="core/qvm-improvements">QVM Improvements</a> for details.</p>
    </div>

    <div class="section">
        <h2>Structured Logging</h2>
        
        <h3>Log Levels</h3>
        <ul>
            <li><strong>DEBUG:</strong> Debug information</li>
            <li><strong>INFO:</strong> Informational messages</li>
            <li><strong>WARN:</strong> Warnings</li>
            <li><strong>ERROR:</strong> Errors</li>
            <li><strong>FATAL:</strong> Fatal errors</li>
        </ul>

        <h3>Features</h3>
        <ul>
            <li>Category-based filtering</li>
            <li>JSON output format</li>
            <li>Log rotation (size and time-based)</li>
            <li>Syslog integration</li>
            <li>Backward compatible</li>
        </ul>

        <h3>Configuration</h3>
        <div class="code-block">
            <pre><code>set log_level "INFO"              // Log level
set log_category "renderer,network"  // Categories
set log_json "1"                  // JSON output
set log_file "q3.log"             // Log file</code></pre>
        </div>

        <p>See <a href="core/structured-logging">Structured Logging</a> for complete documentation.</p>
    </div>

    <div class="section">
        <h2>Entity Component System (ECS)</h2>
        
        <h3>ECS Features</h3>
        <ul>
            <li>Component-based architecture</li>
            <li>System-based processing</li>
            <li>EnTT library integration</li>
            <li>Physics integration (Bullet optional)</li>
            <li>Lua scripting support</li>
            <li>Network synchronization</li>
        </ul>

        <h3>Components</h3>
        <ul>
            <li><strong>TransformComponent:</strong> Position, rotation, scale</li>
            <li><strong>PhysicsComponent:</strong> Velocity, acceleration, mass</li>
            <li><strong>HealthComponent:</strong> Health, armor</li>
            <li><strong>NetworkComponent:</strong> Network synchronization</li>
        </ul>

        <p>See <a href="core/entity-system">Entity System</a> for details.</p>
    </div>

    <div class="section">
        <h2>Main Loop</h2>
        
        <h3>Execution Flow</h3>
        <div class="code-block">
            <pre><code>while (running) {
    // 1. Process input
    IN_Frame()
    
    // 2. Network communication
    CL_Frame()  // Client
    SV_Frame()  // Server
    
    // 3. Game logic update
    VM_Call()   // QVM or native
    
    // 4. Rendering
    CL_RenderFrame()
    
    // 5. Sound mixing
    S_Update()
}</code></pre>
        </div>

        <h3>Frame Timing</h3>
        <ul>
            <li><strong>Server Tick:</strong> Fixed 20ms intervals</li>
            <li><strong>Client Frame:</strong> Variable, matches display refresh</li>
            <li><strong>Interpolation:</strong> Smooth rendering between ticks</li>
            <li><strong>Prediction:</strong> Client-side prediction</li>
        </ul>

        <p>See <a href="core/main-loop">Main Loop</a> for details.</p>
    </div>

    <div class="section">
        <h2>Related Documentation</h2>
        <ul>
            <li><a href="core/memory-management">Memory Management</a> - Memory system</li>
            <li><a href="core/memory-safety">Memory Safety</a> - Safety tools</li>
            <li><a href="core/filesystem">Filesystem</a> - File system</li>
            <li><a href="core/input-system">Input System</a> - Input handling</li>
            <li><a href="core/console-system">Console System</a> - Console and CVARs</li>
            <li><a href="core/virtual-machine">Virtual Machine</a> - QVM system</li>
            <li><a href="core/structured-logging">Structured Logging</a> - Logging system</li>
            <li><a href="core/entity-system">Entity System</a> - ECS architecture</li>
        </ul>
    </div>
</div>

