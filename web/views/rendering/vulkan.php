<?php
/**
 * Vulkan Renderer Documentation
 */
$title = 'Vulkan Renderer - id Tech 3 Documentation';
$breadcrumbs = [
    '/rendering' => 'Rendering',
    '/rendering/vulkan' => 'Vulkan Renderer'
];
?>

<h1>Vulkan Renderer</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 Vulkan renderer provides modern graphics capabilities while maintaining compatibility with classic Quake III Arena content. It offers significant performance improvements and advanced rendering features.</p>
    
    <div class="feature-list">
        <h3>Key Features</h3>
        <ul>
            <li>High-performance Vulkan API implementation</li>
            <li>HDR (High Dynamic Range) rendering support</li>
            <li>ACES tonemapping integration</li>
            <li>Physically-Based Rendering (PBR) materials</li>
            <li>Advanced post-processing effects</li>
            <li>3D LUT (Look-Up Table) color grading</li>
            <li>FFXV-style raymarching sky</li>
            <li>Deferred rendering pipeline</li>
            <li>Spherical Harmonics Global Illumination</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>System Requirements</h2>
    <ul>
        <li><strong>GPU:</strong> Vulkan 1.1 capable graphics card</li>
        <li><strong>Drivers:</strong> Latest GPU drivers with Vulkan support</li>
        <li><strong>OS:</strong> Windows 7+, Linux, macOS 10.15+</li>
        <li><strong>Development:</strong> Vulkan SDK for building from source</li>
    </ul>
</div>

<div class="section">
    <h2>Installation</h2>
    
    <h3>Pre-built Binaries</h3>
    <p>The Vulkan renderer is included by default in releases. Enable it with:</p>
    <div class="code-block">
        <pre><code>quake3e +set r_renderer vulkan</code></pre>
    </div>
    
    <h3>Building from Source</h3>
    <p>To build with Vulkan support:</p>
    <?php include __DIR__ . '/../partials/dependencies-list.php'; ?>
    
    <div class="code-block">
        <pre><code># Linux/macOS
make USE_VULKAN=1 RENDERER_DEFAULT=vulkan

# Windows (Visual Studio)
# Set renderervk as project dependency instead of renderer</code></pre>
    </div>
</div>

<div class="section">
    <h2>Configuration</h2>
    
    <h3>Basic Settings</h3>
    <?php include __DIR__ . '/../partials/graphics-settings-table.php'; ?>
    
    <h3>Advanced Features</h3>
    <div class="code-block">
        <pre><code># Enable HDR rendering
seta r_hdr "1"

# Set tonemapping mode (0=disabled, 1=ACES)
seta r_tonemapping "1"

# Enable PBR materials
seta r_pbr "1"

# Enable 3D LUT color grading
seta r_lut "1"

# Enable raymarching sky
seta r_raymarchSky "1"</code></pre>
    </div>
</div>

<div class="section">
    <h2>Renderer Comparison</h2>
    <p>The engine supports multiple renderer backends. Choose the one that best fits your needs:</p>
    <ul>
        <li><strong><a href="rendering/vulkan">Vulkan:</a></strong> Cross-platform, high performance, modern features (recommended)</li>
        <li><strong><a href="rendering/directx12">DirectX 12:</a></strong> Windows-only, excellent performance, DXR ray tracing support</li>
        <li><strong>OpenGL:</strong> Legacy support, maximum compatibility</li>
    </ul>
    <p>All renderers support PBR materials, HDR rendering, and modern post-processing effects. See <a href="rendering/directx12">DirectX 12 Renderer</a> for Windows-specific features.</p>
</div>

<div class="section">
    <h2>Advanced Topics</h2>
    <ul>
        <li><a href="rendering/pbr">Physically-Based Rendering (PBR)</a></li>
        <li><a href="tonemapping">HDR and ACES Tonemapping</a></li>
        <li><a href="luts">3D LUT Color Grading</a></li>
        <li><a href="raymarching_sky">FFXV-Style Raymarching Sky</a></li>
        <li><a href="rendering/animated-skybox">Animated Skybox (Flipbook)</a></li>
        <li><a href="deferred">Deferred Rendering Pipeline</a></li>
        <li><a href="SHGI">Spherical Harmonics Global Illumination</a></li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    
    <h3>Common Issues</h3>
    <div class="troubleshooting">
        <h4>Vulkan validation layer errors</h4>
        <p>Enable validation layers during development:</p>
        <div class="code-block">
            <pre><code>export VK_LAYER_PATH=/path/to/vulkan/layers
quake3e +set r_renderer vulkan +set r_vkValidation 1</code></pre>
        </div>
        
        <h4>Performance issues</h4>
        <ul>
            <li>Update GPU drivers to the latest version</li>
            <li>Ensure GPU supports Vulkan 1.1+</li>
            <li>Adjust quality settings in the graphics menu</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="rendering/directx12">DirectX 12 Renderer</a> - Windows alternative with DXR support</li>
        <li><a href="rendering/pbr">PBR Pipeline</a> - Physically-based rendering</li>
        <li><a href="rendering/animated-skybox">Animated Skybox</a> - Dynamic sky effects</li>
        <li><a href="renderer/renderdoc-debugging">RenderDoc Debugging</a> - Graphics debugging tools</li>
        <li><a href="development/debugging">Debugging Tools</a> - General debugging</li>
    </ul>
</div>

<div class="section">
    <h2>Resources</h2>
    <ul>
        <li><a href="https://vulkan.lunarg.com">Vulkan SDK</a></li>
        <li><a href="https://www.khronos.org/vulkan/">Vulkan Documentation</a></li>
        <li><a href="https://www.khronos.org/blog/vulkan-memory-management">Vulkan Memory Management</a></li>
    </ul>
</div> 