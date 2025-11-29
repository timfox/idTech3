<?php
/**
 * id Tech 3 Engine Features Overview
 */
$title = 'id Tech 3 Engine Features - Documentation';
$breadcrumbs = [
    '/idtech3' => 'id Tech 3 Features'
];
?>

<h1>id Tech 3 Engine Features</h1>

<div class="section">
    <h2>Core Features</h2>
    <ul>
        <li>Advanced 3D rendering engine with support for:
            <ul>
                <li>Dynamic lighting and shadows</li>
                <li>Curved surfaces and bezier patches</li>
                <li>Particle systems and effects</li>
                <li>Dynamic skyboxes and environment mapping</li>
                <li><strong>Animated skyboxes</strong> with frame-based flipbook animation</li>
                <li>Physically-Based Rendering (PBR) materials</li>
                <li>High Dynamic Range (HDR) rendering</li>
                <li>Ray tracing support (Vulkan RTX, DirectX DXR)</li>
            </ul>
        </li>
        <li>Cross-platform support:
            <ul>
                <li>Windows (32/64-bit)</li>
                <li>Linux/Unix</li>
                <li>macOS</li>
                <li>Android</li>
            </ul>
        </li>
        <li>Multiple renderer backends:
            <ul>
                <li><strong>Vulkan</strong> (default, cross-platform, recommended)</li>
                <li><strong>DirectX 12</strong> (Windows, with DXR ray tracing)</li>
                <li><strong>OpenGL</strong> (legacy, maximum compatibility)</li>
            </ul>
        </li>
    </ul>
</div>

<div class="section">
    <h2>Recent Enhancements</h2>
    <div class="feature-list">
        <h3>Modern Developer Tools</h3>
        <ul>
            <li><strong>ImGui Debug Overlays:</strong> Real-time performance, memory, network, and renderer statistics</li>
            <li><strong>Structured Logging:</strong> Modern logging system with levels, categories, and JSON output</li>
            <li><strong>Memory Safety Tools:</strong> AddressSanitizer, UndefinedBehaviorSanitizer, and comprehensive memory tracking</li>
            <li><strong>Enhanced Networking:</strong> HTTP/2, connection pooling, rate limiting, IPv6 improvements, and WebSocket support</li>
        </ul>
        
        <h3>Renderer Improvements</h3>
        <ul>
            <li><strong>DirectX 12 Renderer:</strong> Full D3D12 support with DXR ray tracing</li>
            <li><strong>Animated Skyboxes:</strong> Frame-based skybox animation system</li>
            <li><strong>PBR Pipeline:</strong> Physically-based rendering across all renderers</li>
            <li><strong>HDR & Tonemapping:</strong> ACES tonemapping and HDR rendering</li>
        </ul>
        
        <h3>Code Quality</h3>
        <ul>
            <li>Fixed unsafe string operations</li>
            <li>Added header guards and error handling</li>
            <li>Unit test framework</li>
            <li>Static analysis support</li>
        </ul>
        
        <h3>Core System Improvements</h3>
        <ul>
            <li><strong>Input System:</strong> Raw mouse input, unlagged processing, minimize hotkey, FFmpeg video pipe</li>
            <li><strong>Filesystem:</strong> Raised limits (20,000 maps), recursive scanning, improved mod support</li>
            <li><strong>Memory System:</strong> Reworked Zone allocator, no OOM errors, reduced server memory usage</li>
            <li><strong>QVM:</strong> Significantly reworked virtual machine, better performance and reliability</li>
        </ul>
    </div>
    <p>See <a href="whats-new">What's New</a> for complete details on recent enhancements.</p>
</div>

<div class="section">
    <h2>Technical Capabilities</h2>
    <div class="note">
        <strong>Note:</strong> The engine supports both client and dedicated server modes, with optional unified client/server builds.
    </div>
    <ul>
        <li>Network Architecture:
            <ul>
                <li>Client-server architecture</li>
                <li>UDP-based networking</li>
                <li>Lag compensation</li>
                <li>Server-side hit detection</li>
                <li><strong>HTTP/2 support</strong> for faster downloads</li>
                <li><strong>WebSocket support</strong> for real-time bidirectional communication</li>
                <li><strong>Connection pooling</strong> and rate limiting</li>
            </ul>
        </li>
        <li>Physics System:
            <ul>
                <li>BSP-based collision detection</li>
                <li>Dynamic object physics</li>
                <li>Vehicle physics support</li>
                <li>Water and liquid simulation</li>
            </ul>
        </li>
        <li>Audio System:
            <ul>
                <li>3D positional audio</li>
                <li>Environmental audio effects</li>
                <li>Multiple audio backends (OpenAL, SDL)</li>
            </ul>
        </li>
    </ul>
</div>

<div class="section">
    <h2>Development Features</h2>
    <ul>
        <li>Mod Support:
            <ul>
                <li>QVM-based mod system</li>
                <li>Dynamic library loading</li>
                <li>Custom shader support</li>
                <li>Map and asset loading</li>
            </ul>
        </li>
        <li>Development Tools:
            <ul>
                <li>Integrated console system</li>
                <li><strong>ImGui debug overlays</strong> for real-time debugging</li>
                <li><strong>Structured logging</strong> with filtering and JSON output</li>
                <li><strong>Memory tracking</strong> and leak detection</li>
                <li>Performance monitoring</li>
                <li>Network debugging tools</li>
                <li>RenderDoc integration for graphics debugging</li>
            </ul>
        </li>
    </ul>
</div>

<div class="section">
    <h2>Performance Optimizations</h2>
    <ul>
        <li>Rendering:
            <ul>
                <li>Level of detail (LOD) system</li>
                <li>Occlusion culling</li>
                <li>Frustum culling</li>
                <li>Dynamic light optimization</li>
                <li>Deferred rendering pipeline</li>
                <li>Spherical Harmonics Global Illumination</li>
            </ul>
        </li>
        <li>Memory Management:
            <ul>
                <li>Efficient memory allocation</li>
                <li><strong>Memory tracking</strong> with per-type statistics</li>
                <li>Resource streaming</li>
                <li>Texture compression</li>
                <li>Asset caching</li>
                <li>Reworked Zone memory allocator</li>
            </ul>
        </li>
        <li>Networking:
            <ul>
                <li><strong>Connection pooling</strong> for 20-50% faster downloads</li>
                <li><strong>HTTP/2</strong> for 10-30% faster on high-latency connections</li>
                <li>Rate limiting to prevent server overload</li>
            </ul>
        </li>
    </ul>
</div>

<div class="section">
    <h2>History</h2>
    <p>id Tech 3 has a rich history spanning over two decades:</p>
    <ul>
        <li>Originally developed by id Software (1996-1999)</li>
        <li>Released with Quake III Arena (December 1999)</li>
        <li>Used in numerous commercial games (2000-2005)</li>
        <li>Open sourced under GPL v2.0 (August 2005)</li>
        <li>Evolved through community development (2005-present)</li>
        <li>Modern enhancements with Vulkan, DirectX 12, PBR, and more</li>
    </ul>
    <p>See <a href="history">History of id Tech 3</a> for a comprehensive overview of the engine's development, impact, and evolution.</p>
</div>

<div class="section">
    <h2>Documentation</h2>
    <p>Comprehensive documentation is available for all features:</p>
    <ul>
        <li><a href="getting-started/quick-start">Quick Start Guide</a></li>
        <li><a href="history">History of id Tech 3</a></li>
        <li><a href="whats-new">What's New</a> - Recent enhancements</li>
        <li><a href="rendering/vulkan">Vulkan Renderer</a></li>
        <li><a href="rendering/directx12">DirectX 12 Renderer</a></li>
        <li><a href="core/structured-logging">Structured Logging</a></li>
        <li><a href="core/memory-safety">Memory Safety & Profiling</a></li>
        <li><a href="networking/networking">Enhanced Networking</a></li>
        <li><a href="imgui">ImGui Debug Overlays</a></li>
        <li><a href="rendering/animated-skybox">Animated Skybox</a></li>
    </ul>
</div>

<div class="note">
    <strong>Note:</strong> The engine is designed to be highly configurable through console variables and configuration files, allowing for extensive customization of both visual and gameplay features. All new features are opt-in and backward compatible.
</div>
