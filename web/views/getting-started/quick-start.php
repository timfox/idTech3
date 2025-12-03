<?php
/**
 * Quick Start Guide view
 */
$title = 'Quick Start Guide - id Tech 3 Documentation';
$breadcrumbs = [
    '/getting-started' => 'Getting Started',
    '/getting-started/quick-start' => 'Quick Start Guide'
];
?>

<h1>Quick Start Guide</h1>

<div class="section">
    <h2>Getting Up and Running</h2>
    <p>This guide will help you get id Tech 3 built and running quickly.</p>
    
    <div class="step">
        <h3>Step 1: Clone the Repository</h3>
        <div class="code-block">
            <pre><code>git clone https://github.com/timfox/idtech3
cd idtech3</code></pre>
        </div>
    </div>

    <div class="step">
        <h3>Step 2: Build the Engine</h3>
        <?php include __DIR__ . '/../partials/build-instructions.php'; ?>
    </div>

    <div class="step">
        <h3>Step 3: Configure Settings</h3>
        <p>Copy the example configuration:</p>
        <div class="code-block">
            <pre><code>cp config/example.cfg config/quake3.cfg</code></pre>
        </div>
        
        <p>Edit <code>config/quake3.cfg</code> to set your preferred renderer:</p>
        <?php include __DIR__ . '/../partials/config-example.php'; ?>
    </div>

    <div class="step">
        <h3>Step 4: Run the Engine</h3>
        <p>Run with your preferred renderer:</p>
        <div class="code-block">
            <pre><code># Vulkan renderer (recommended, cross-platform)
./quake3e +set fs_basepath . +set r_renderer vulkan

# DirectX 12 renderer (Windows only, with DXR support)
./quake3e +set fs_basepath . +set r_renderer d3d12

# OpenGL renderer (legacy, maximum compatibility)
./quake3e +set fs_basepath . +set r_renderer opengl</code></pre>
        </div>
    </div>
</div>

<div class="section">
    <h2>New Features to Try</h2>
    <p>Once running, explore these modern features:</p>
    <ul>
        <li><strong>ImGui Debug Overlays:</strong> Enable with <code>/set cl_imgui 1</code> - See <a href="imgui">ImGui Debug Overlays</a></li>
        <li><strong>Structured Logging:</strong> Configure with <code>/set log_format 1</code> for JSON logs - See <a href="core/structured-logging">Structured Logging</a></li>
        <li><strong>Memory Tracking:</strong> Monitor memory usage - See <a href="core/memory-safety">Memory Safety & Profiling</a></li>
        <li><strong>Enhanced Networking:</strong> HTTP/2 and WebSocket support - See <a href="networking/networking">Networking</a></li>
        <li><strong>Animated Skyboxes:</strong> Create dynamic sky effects - See <a href="rendering/animated-skybox">Animated Skybox</a></li>
    </ul>
</div>

<div class="section">
    <h2>Next Steps</h2>
    <ul>
        <li><a href="getting-started/installation">Detailed Installation Guide</a></li>
        <li><a href="getting-started/configuration">Configuration Options</a></li>
        <li><a href="getting-started/vulkan-setup">Vulkan Setup Guide</a></li>
        <li><a href="rendering/vulkan">Vulkan Renderer Features</a></li>
        <li><a href="rendering/directx12">DirectX 12 Renderer</a> (Windows)</li>
        <li><a href="development/map-making">Creating Your First Map</a></li>
    </ul>
</div>

<div class="section">
    <h2>Tutorials</h2>
    <p>Learn how to use the new features with step-by-step tutorials:</p>
    <ul>
        <li><a href="tutorials/structured-logging">Structured Logging Tutorial</a> - Set up modern logging</li>
        <li><a href="tutorials/memory-profiling">Memory Profiling Tutorial</a> - Find and fix memory issues</li>
        <li><a href="tutorials/imgui-overlays">ImGui Debug Overlays Tutorial</a> - Use in-game debugging tools</li>
        <li><a href="tutorials/websocket">WebSocket Integration Tutorial</a> - Real-time communication</li>
        <li><a href="tutorials/animated-skybox">Creating Animated Skyboxes</a> - Dynamic sky effects</li>
        <li><a href="tutorials/enhanced-networking">Enhanced Networking Setup</a> - HTTP/2, pooling, IPv6</li>
        <li><a href="tutorials/directx12-setup">DirectX 12 Setup Tutorial</a> - Windows renderer with DXR</li>
        <li><a href="pbr_tutorial">PBR Shader Tutorial</a> - Physically-based rendering</li>
    </ul>
</div> 