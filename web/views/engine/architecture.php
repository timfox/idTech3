<?php
/**
 * Engine Architecture - Complete Overview
 */
$title = 'Engine Architecture - id Tech 3 Documentation';
$breadcrumbs = [
    '/engine' => 'Engine',
    '/engine/architecture' => 'Architecture'
];
?>

<div class="content-section">
    <h1>id Tech 3 Engine Architecture</h1>
    
    <blockquote>
        <strong>Complete Overview:</strong> This document provides a comprehensive overview of the id Tech 3 engine architecture, including all subsystems, their interactions, and modern enhancements.
    </blockquote>

    <div class="section">
        <h2>Architecture Overview</h2>
        <p>The id Tech 3 engine follows a modular, client-server architecture designed for multiplayer gaming. The engine is divided into several major subsystems that communicate through well-defined interfaces.</p>
        
        <div class="feature-list">
            <h3>Core Design Principles</h3>
            <ul>
                <li><strong>Modularity:</strong> Subsystems operate independently with clear interfaces</li>
                <li><strong>Client-Server:</strong> Authoritative server with client-side prediction</li>
                <li><strong>Performance:</strong> Optimized for 60+ FPS gameplay</li>
                <li><strong>Extensibility:</strong> QVM-based mod system and native library support</li>
                <li><strong>Cross-Platform:</strong> Windows, Linux, macOS, Android support</li>
            </ul>
        </div>
    </div>

    <div class="section">
        <h2>Major Subsystems</h2>
        
        <h3>1. Common Subsystem (qcommon)</h3>
        <p>The foundation of the engine, providing shared services used by all subsystems:</p>
        <ul>
            <li><strong>Memory Management:</strong> Zone allocator, Hunk allocator, Temp allocator</li>
            <li><strong>Filesystem:</strong> Virtual file system with PK3 support, recursive scanning</li>
            <li><strong>Console System:</strong> CVAR management, command execution</li>
            <li><strong>Math Library:</strong> Vector, matrix, and quaternion operations</li>
            <li><strong>Networking:</strong> Low-level UDP networking, packet handling</li>
            <li><strong>Virtual Machine:</strong> QVM interpreter for game logic</li>
            <li><strong>Structured Logging:</strong> Modern logging with levels and categories</li>
        </ul>
        <p>See <a href="core/engine-subsystems">Engine Subsystems</a> for detailed information.</p>

        <h3>2. Renderer Subsystem</h3>
        <p>Handles all graphics rendering with support for multiple backends:</p>
        <ul>
            <li><strong>Vulkan Renderer:</strong> Modern, cross-platform, recommended</li>
            <li><strong>DirectX 12 Renderer:</strong> Windows-only with DXR ray tracing</li>
            <li><strong>OpenGL Renderer:</strong> Legacy, maximum compatibility</li>
            <li><strong>Features:</strong> PBR, HDR, ray tracing, animated skyboxes, deferred rendering</li>
        </ul>
        <p>See <a href="rendering/vulkan">Vulkan Renderer</a>, <a href="rendering/directx12">DirectX 12 Renderer</a>, and <a href="rendering/pbr">PBR Pipeline</a> for details.</p>

        <h3>3. Client Subsystem (client)</h3>
        <p>Client-side game logic and rendering coordination:</p>
        <ul>
            <li><strong>Prediction:</strong> Client-side prediction for responsive gameplay</li>
            <li><strong>Interpolation:</strong> Smooth entity movement between snapshots</li>
            <li><strong>Effects:</strong> Particle systems, sound effects, visual feedback</li>
            <li><strong>UI:</strong> HUD rendering, menu system, console</li>
            <li><strong>ImGui Integration:</strong> Debug overlays and developer tools</li>
        </ul>

        <h3>4. Server Subsystem (server)</h3>
        <p>Authoritative game server logic:</p>
        <ul>
            <li><strong>Entity Management:</strong> Game entities, spawn, update, removal</li>
            <li><strong>Physics:</strong> BSP collision detection, movement, hit detection</li>
            <li><strong>Networking:</strong> Snapshot generation, client communication</li>
            <li><strong>Game Rules:</strong> Match logic, scoring, timers</li>
            <li><strong>DoS Protection:</strong> Rate limiting, resource limits</li>
        </ul>

        <h3>5. Input Subsystem</h3>
        <p>Handles all user input:</p>
        <ul>
            <li><strong>Keyboard:</strong> Key mapping, bindings, console input</li>
            <li><strong>Mouse:</strong> Raw input support, unlagged processing</li>
            <li><strong>Gamepad:</strong> Controller support (SDL2)</li>
            <li><strong>Features:</strong> Minimize hotkey, video pipe, input smoothing</li>
        </ul>
        <p>See <a href="core/input-system">Input System</a> for complete documentation.</p>

        <h3>6. Sound Subsystem</h3>
        <p>Audio playback and 3D positioning:</p>
        <ul>
            <li><strong>3D Audio:</strong> Positional sound with distance attenuation</li>
            <li><strong>Codecs:</strong> WAV, OGG Vorbis support</li>
            <li><strong>Mixing:</strong> Multiple channels, volume control</li>
            <li><strong>Backends:</strong> OpenAL, SDL2 audio</li>
        </ul>
        <p>See <a href="sound/sound">Sound System</a> for details.</p>

        <h3>7. Network Subsystem</h3>
        <p>Client-server communication:</p>
        <ul>
            <li><strong>Protocol:</strong> UDP-based with reliable message channels</li>
            <li><strong>Snapshots:</strong> Delta-compressed game state updates</li>
            <li><strong>Lag Compensation:</strong> Server-side hit detection with history</li>
            <li><strong>HTTP/2:</strong> Faster downloads with connection pooling</li>
            <li><strong>WebSocket:</strong> Real-time bidirectional communication</li>
        </ul>
        <p>See <a href="networking/networking">Networking</a> and <a href="networking/websocket">WebSocket Support</a> for details.</p>
    </div>

    <div class="section">
        <h2>Modern Enhancements</h2>
        
        <h3>Entity Component System (ECS)</h3>
        <p><strong>New:</strong> Optional ECS architecture using EnTT library:</p>
        <ul>
            <li>Component-based entity management</li>
            <li>System-based processing</li>
            <li>Integration with existing engine entities</li>
            <li>Physics integration (Bullet Physics optional)</li>
            <li>Lua scripting support for entities</li>
        </ul>
        <p>See <a href="core/entity-system">Entity System</a> for ECS documentation.</p>

        <h3>Lua Scripting</h3>
        <p><strong>New:</strong> Lua scripting support for game logic:</p>
        <ul>
            <li>Entity scripts with OnUpdate hooks</li>
            <li>Event system integration</li>
            <li>Coroutine support for async operations</li>
            <li>Encounter and sequence systems</li>
            <li>Full engine API bindings</li>
        </ul>

        <h3>Physically-Based Rendering (PBR)</h3>
        <p><strong>Enhanced:</strong> Complete PBR pipeline across all renderers:</p>
        <ul>
            <li>Metallic/Roughness workflow</li>
            <li>ORM texture support (Occlusion/Roughness/Metallic)</li>
            <li>Normal mapping with parallax support</li>
            <li>Environment mapping and reflections</li>
            <li>HDR rendering with ACES tonemapping</li>
        </ul>
        <p>See <a href="rendering/pbr">PBR Pipeline</a> for complete documentation.</p>

        <h3>Ray Tracing</h3>
        <p><strong>New:</strong> Hardware-accelerated ray tracing:</p>
        <ul>
            <li>Vulkan RTX support</li>
            <li>DirectX DXR support</li>
            <li>Realistic lighting and reflections</li>
            <li>Performance optimizations</li>
        </ul>
        <p>See <a href="rendering/ray-tracing">Ray Tracing</a> for details.</p>
    </div>

    <div class="section">
        <h2>Execution Flow</h2>
        
        <h3>Engine Startup Sequence</h3>
        <div class="code-block">
            <pre><code>Com_Init()
  ├── Memory system initialization
  ├── Filesystem initialization
  ├── Console system initialization
  ├── CVAR registration
  ├── Network initialization
  ├── Input system initialization
  ├── Sound system initialization
  ├── Renderer initialization
  └── Game module loading (QVM or native)</code></pre>
        </div>

        <h3>Main Loop</h3>
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
        <p>The engine uses a fixed timestep for game logic (typically 20ms = 50 FPS server) while rendering runs at display refresh rate:</p>
        <ul>
            <li><strong>Server Tick:</strong> Fixed 20ms intervals (configurable)</li>
            <li><strong>Client Frame:</strong> Variable, matches display refresh rate</li>
            <li><strong>Interpolation:</strong> Smooth rendering between server ticks</li>
            <li><strong>Prediction:</strong> Client-side prediction for responsiveness</li>
        </ul>
    </div>

    <div class="section">
        <h2>Memory Architecture</h2>
        
        <h3>Memory Zones</h3>
        <p>The engine uses multiple memory allocators for different purposes:</p>
        <ul>
            <li><strong>Zone Allocator:</strong> General-purpose, reworked for reliability</li>
            <li><strong>Hunk Allocator:</strong> Large, persistent allocations</li>
            <li><strong>Temp Allocator:</strong> Short-lived, frame-based allocations</li>
            <li><strong>Tagged Allocations:</strong> Memory tracking by subsystem</li>
        </ul>
        <p>See <a href="core/memory-management">Memory Management</a> for details.</p>

        <h3>Memory Safety</h3>
        <p>Modern memory safety tools integrated:</p>
        <ul>
            <li>AddressSanitizer (ASan) support</li>
            <li>UndefinedBehaviorSanitizer (UBSan) support</li>
            <li>Memory leak detection</li>
            <li>Bounds checking</li>
            <li>Comprehensive memory tracking</li>
        </ul>
        <p>See <a href="core/memory-safety">Memory Safety</a> for configuration.</p>
    </div>

    <div class="section">
        <h2>Mod System Architecture</h2>
        
        <h3>QVM (Quake Virtual Machine)</h3>
        <p>Traditional bytecode-based mod system:</p>
        <ul>
            <li>Compiled C code to bytecode</li>
            <li>Sandboxed execution environment</li>
            <li>Limited standard library access</li>
            <li>Significantly reworked for better performance</li>
        </ul>
        <p>See <a href="core/virtual-machine">Virtual Machine</a> for QVM details.</p>

        <h3>Native Library Loading</h3>
        <p><strong>New:</strong> Support for native shared libraries:</p>
        <ul>
            <li>Modern C11/C17/C23 features</li>
            <li>Full standard library access</li>
            <li>Better performance (no VM overhead)</li>
            <li>Easier debugging</li>
            <li>Cross-platform support</li>
        </ul>
        <p>See <a href="development/modding">Modding Guide</a> for native compilation.</p>
    </div>

    <div class="section">
        <h2>Renderer Architecture</h2>
        
        <h3>Renderer Backends</h3>
        <p>Three renderer implementations share common rendering logic:</p>
        <ul>
            <li><strong>RendererCommon:</strong> Shared rendering code</li>
            <li><strong>RendererVK:</strong> Vulkan-specific implementation</li>
            <li><strong>RendererD3D12:</strong> DirectX 12-specific implementation</li>
            <li><strong>RendererOpenGL:</strong> OpenGL-specific implementation</li>
        </ul>

        <h3>Rendering Pipeline</h3>
        <div class="code-block">
            <pre><code>1. Culling (Frustum, Occlusion)
2. Batching (Material, Shader)
3. Command Generation
4. GPU Submission
5. Present</code></pre>
        </div>

        <h3>Modern Features</h3>
        <ul>
            <li><strong>Deferred Rendering:</strong> G-buffer generation, lighting pass</li>
            <li><strong>PBR Pipeline:</strong> Material evaluation, IBL</li>
            <li><strong>Ray Tracing:</strong> Hardware-accelerated reflections</li>
            <li><strong>HDR Pipeline:</strong> Tone mapping, bloom</li>
        </ul>
        <p>See <a href="rendering/vulkan">Vulkan Renderer</a> and <a href="renderer/pbr-pipeline">PBR Pipeline</a> for details.</p>
    </div>

    <div class="section">
        <h2>Network Architecture</h2>
        
        <h3>Client-Server Model</h3>
        <p>The engine uses an authoritative server model:</p>
        <ul>
            <li><strong>Server Authority:</strong> All game state decisions made server-side</li>
            <li><strong>Client Prediction:</strong> Client predicts movement for responsiveness</li>
            <li><strong>Lag Compensation:</strong> Server rewinds time for hit detection</li>
            <li><strong>Snapshot System:</strong> Delta-compressed state updates</li>
        </ul>

        <h3>Protocol</h3>
        <ul>
            <li><strong>Transport:</strong> UDP with reliable message channels</li>
            <li><strong>Compression:</strong> Delta compression for snapshots</li>
            <li><strong>Rate Limiting:</strong> Configurable client rate limits</li>
            <li><strong>HTTP/2:</strong> Faster downloads with multiplexing</li>
            <li><strong>WebSocket:</strong> Real-time bidirectional communication</li>
        </ul>
        <p>See <a href="networking/networking">Networking</a> for complete documentation.</p>
    </div>

    <div class="section">
        <h2>Build System</h2>
        
        <h3>CMake Build System</h3>
        <p>Modern CMake-based build system:</p>
        <ul>
            <li>Cross-platform support</li>
            <li>Multiple renderer selection</li>
            <li>Optional feature flags</li>
            <li>Dependency management</li>
            <li>Install targets</li>
        </ul>
        <p>See <a href="modernization/build-systems">Build Systems</a> for details.</p>

        <h3>Compiler Support</h3>
        <ul>
            <li>GCC 9+ (C17/C23 support)</li>
            <li>Clang 10+ (C17/C23 support)</li>
            <li>MSVC 2019+ (Windows)</li>
            <li>Static analysis tools (clang-tidy, cppcheck)</li>
        </ul>
    </div>

    <div class="section">
        <h2>Related Documentation</h2>
        <ul>
            <li><a href="core/engine-subsystems">Engine Subsystems</a> - Detailed subsystem documentation</li>
            <li><a href="rendering/vulkan">Vulkan Renderer</a> - Vulkan renderer details</li>
            <li><a href="rendering/directx12">DirectX 12 Renderer</a> - D3D12 renderer details</li>
            <li><a href="core/memory-management">Memory Management</a> - Memory system architecture</li>
            <li><a href="networking/networking">Networking</a> - Network architecture</li>
            <li><a href="core/filesystem">Filesystem</a> - Virtual file system</li>
            <li><a href="development/modding">Modding</a> - Mod system architecture</li>
        </ul>
    </div>
</div>

