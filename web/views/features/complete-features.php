<?php
/**
 * Complete Features List
 */
$title = 'Complete Features - id Tech 3 Documentation';
$breadcrumbs = [
    '/features' => 'Features',
    '/features/complete-features' => 'Complete Features'
];
?>

<div class="content-section">
    <h1>Complete Feature List</h1>
    
    <blockquote>
        <strong>Comprehensive Feature Documentation:</strong> This page provides a complete list of all features, enhancements, and capabilities of the modern id Tech 3 engine.
    </blockquote>

    <div class="section">
        <h2>Renderer Features</h2>
        
        <h3>Multiple Renderer Backends</h3>
        <ul>
            <li><strong>Vulkan Renderer:</strong> Modern, cross-platform, recommended</li>
            <li><strong>DirectX 12 Renderer:</strong> Windows with DXR ray tracing</li>
            <li><strong>OpenGL Renderer:</strong> Legacy, maximum compatibility</li>
        </ul>

        <h3>Advanced Rendering Features</h3>
        <ul>
            <li><strong>Physically-Based Rendering (PBR):</strong> Complete PBR pipeline</li>
            <li><strong>Ray Tracing:</strong> Vulkan RTX and DirectX DXR support</li>
            <li><strong>HDR Rendering:</strong> High dynamic range with ACES tonemapping</li>
            <li><strong>Animated Skyboxes:</strong> Frame-based flipbook animation</li>
            <li><strong>Deferred Rendering:</strong> G-buffer based lighting</li>
            <li><strong>Global Illumination:</strong> Spherical Harmonics GI</li>
            <li><strong>Bloom:</strong> Post-processing bloom effects</li>
            <li><strong>Environment Mapping:</strong> Cube map reflections</li>
            <li><strong>Normal Mapping:</strong> With parallax support</li>
            <li><strong>Anisotropic Filtering:</strong> Improved texture quality</li>
        </ul>
        <p>See <a href="rendering/vulkan">Vulkan Renderer</a>, <a href="rendering/directx12">DirectX 12</a>, and <a href="rendering/pbr">PBR Pipeline</a> for details.</p>
    </div>

    <div class="section">
        <h2>Core Engine Features</h2>
        
        <h3>Memory System</h3>
        <ul>
            <li>Reworked Zone allocator (no OOM errors)</li>
            <li>Memory tracking with per-type statistics</li>
            <li>AddressSanitizer (ASan) support</li>
            <li>UndefinedBehaviorSanitizer (UBSan) support</li>
            <li>Memory leak detection</li>
            <li>Reduced server memory usage</li>
        </ul>

        <h3>Filesystem</h3>
        <ul>
            <li>Raised limits (20,000 maps per directory)</li>
            <li>Recursive directory scanning</li>
            <li>PK3 and PK3DIR support</li>
            <li>Improved mod detection</li>
            <li>Library loading improvements</li>
            <li>Directory traversal protection</li>
        </ul>

        <h3>Virtual Machine (QVM)</h3>
        <ul>
            <li>Significantly reworked interpreter</li>
            <li>IEEE 754 floating-point compliance</li>
            <li>Better error handling</li>
            <li>Improved performance</li>
            <li>Enhanced stability</li>
        </ul>

        <h3>Structured Logging</h3>
        <ul>
            <li>Log levels (DEBUG, INFO, WARN, ERROR, FATAL)</li>
            <li>Category-based filtering</li>
            <li>JSON output format</li>
            <li>Log rotation (size and time-based)</li>
            <li>Syslog integration</li>
        </ul>
    </div>

    <div class="section">
        <h2>Input System Features</h2>
        
        <ul>
            <li><strong>Raw Mouse Input:</strong> Automatic detection and use</li>
            <li><strong>Unlagged Processing:</strong> Reduced input latency</li>
            <li><strong>Minimize Hotkey:</strong> Window minimize/restore (Windows)</li>
            <li><strong>Video Pipe:</strong> FFmpeg integration for recording</li>
            <li><strong>Mouse Smoothing:</strong> Configurable smoothing options</li>
            <li><strong>Gamepad Support:</strong> SDL2 controller support</li>
        </ul>
        <p>See <a href="core/input-system">Input System</a> for complete documentation.</p>
    </div>

    <div class="section">
        <h2>Networking Features</h2>
        
        <h3>Enhanced Networking</h3>
        <ul>
            <li><strong>HTTP/2:</strong> Faster downloads with multiplexing</li>
            <li><strong>Connection Pooling:</strong> 20-50% faster downloads</li>
            <li><strong>Rate Limiting:</strong> Server DoS protection</li>
            <li><strong>IPv6 Support:</strong> Improved IPv6 handling</li>
            <li><strong>WebSocket:</strong> Real-time bidirectional communication</li>
            <li><strong>Lag Compensation:</strong> Server-side hit detection</li>
            <li><strong>Client Prediction:</strong> Responsive gameplay</li>
        </ul>
        <p>See <a href="networking/networking">Networking</a> and <a href="networking/websocket">WebSocket</a> for details.</p>
    </div>

    <div class="section">
        <h2>Developer Tools</h2>
        
        <h3>ImGui Integration</h3>
        <ul>
            <li>Real-time performance overlays</li>
            <li>Memory statistics</li>
            <li>Network debugging</li>
            <li>Renderer statistics</li>
            <li>Custom debug windows</li>
        </ul>
        <p>See <a href="imgui">ImGui Integration</a> for details.</p>

        <h3>Debugging Tools</h3>
        <ul>
            <li>RenderDoc integration</li>
            <li>Structured logging</li>
            <li>Memory profiling</li>
            <li>Performance monitoring</li>
            <li>Network debugging</li>
        </ul>
    </div>

    <div class="section">
        <h2>Mod System Features</h2>
        
        <h3>QVM Support</h3>
        <ul>
            <li>Traditional bytecode compilation</li>
            <li>Sandboxed execution</li>
            <li>Reworked for better performance</li>
        </ul>

        <h3>Native Library Support</h3>
        <ul>
            <li>Modern C11/C17/C23 features</li>
            <li>Full standard library access</li>
            <li>Better performance</li>
            <li>Easier debugging</li>
            <li>Cross-platform</li>
        </ul>
        <p>See <a href="development/modding">Modding Guide</a> for details.</p>
    </div>

    <div class="section">
        <h2>Platform Support</h2>
        
        <ul>
            <li><strong>Windows:</strong> 32/64-bit, DirectX 12 support</li>
            <li><strong>Linux:</strong> Full Vulkan and OpenGL support</li>
            <li><strong>macOS:</strong> OpenGL and Vulkan (via MoltenVK)</li>
            <li><strong>Android:</strong> Mobile platform support</li>
        </ul>
    </div>

    <div class="section">
        <h2>Performance Features</h2>
        
        <ul>
            <li>Level of Detail (LOD) system</li>
            <li>Occlusion culling</li>
            <li>Frustum culling</li>
            <li>Merged lightmaps</li>
            <li>VBO caching</li>
            <li>Multiple command buffers</li>
            <li>Reversed depth buffer</li>
            <li>Dynamic light optimization</li>
        </ul>
    </div>

    <div class="section">
        <h2>Related Documentation</h2>
        <ul>
            <li><a href="idtech3">Engine Features</a> - Overview</li>
            <li><a href="whats-new">What's New</a> - Recent enhancements</li>
            <li><a href="engine/architecture">Engine Architecture</a> - System architecture</li>
            <li><a href="rendering/vulkan">Vulkan Renderer</a> - Renderer details</li>
            <li><a href="core/memory-safety">Memory Safety</a> - Safety tools</li>
        </ul>
    </div>
</div>

