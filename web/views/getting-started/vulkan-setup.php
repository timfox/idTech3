<?php
/**
 * Vulkan Setup Documentation
 */
$title = 'Vulkan Setup - Getting Started';
$breadcrumbs = [
    '/getting-started' => 'Getting Started',
    '/getting-started/vulkan-setup' => 'Vulkan Setup'
];
?>

<h1>Vulkan Setup Guide</h1>

<div class="section">
    <h2>Overview</h2>
    <p>This guide covers setting up Vulkan support for modern id Tech 3 engines like Quake3e. Vulkan provides significant performance improvements and modern graphics features compared to legacy OpenGL rendering.</p>
    
    <div class="feature-list">
        <h3>Vulkan Benefits</h3>
        <ul>
            <li><strong>Performance:</strong> Lower CPU overhead and better multi-threading</li>
            <li><strong>Modern Features:</strong> Compute shaders, ray tracing, advanced lighting</li>
            <li><strong>Efficiency:</strong> Reduced driver overhead and better resource management</li>
            <li><strong>Cross-Platform:</strong> Consistent behavior across Windows, Linux, and macOS</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>System Requirements</h2>
    
    <h3>Hardware Requirements</h3>
    <ul>
        <li><strong>GPU:</strong> Vulkan 1.1+ compatible graphics card</li>
        <li><strong>NVIDIA:</strong> GTX 600 series or newer (Kepler+)</li>
        <li><strong>AMD:</strong> GCN 1.0 or newer (HD 7000 series+)</li>
        <li><strong>Intel:</strong> Gen 7.5 or newer (Haswell+)</li>
        <li><strong>Apple:</strong> Metal-compatible hardware with MoltenVK</li>
    </ul>
    
    <h3>Software Requirements</h3>
    <ul>
        <li><strong>Operating System:</strong> Windows 10+, Linux 4.4+, macOS 10.15+</li>
        <li><strong>Graphics Drivers:</strong> Latest drivers with Vulkan support</li>
        <li><strong>Vulkan Runtime:</strong> Vulkan SDK or redistributable</li>
        <li><strong>Engine:</strong> Vulkan-compatible id Tech 3 variant (Quake3e, etc.)</li>
    </ul>
</div>

<div class="section">
    <h2>Driver Installation</h2>
    
    <h3>NVIDIA Drivers</h3>
    <div class="code-block">
        <pre><code># Windows - Download from NVIDIA website
# Latest Game Ready or Studio drivers

# Linux - Ubuntu/Debian
sudo apt update
sudo apt install nvidia-driver-470 nvidia-utils-470
sudo apt install vulkan-utils vulkan-tools

# Arch Linux
sudo pacman -S nvidia nvidia-utils vulkan-icd-loader vulkan-tools

# Verify installation
vulkaninfo
nvidia-smi</code></pre>
    </div>
    
    <h3>AMD Drivers</h3>
    <div class="code-block">
        <pre><code># Windows - AMD Adrenalin Software
# Download from AMD website

# Linux - Mesa drivers (open source)
sudo apt install mesa-vulkan-drivers vulkan-utils

# AMD Pro drivers (proprietary)
# Download from AMD website for professional use

# Verify installation
vulkaninfo | grep "GPU"</code></pre>
    </div>
    
    <h3>Intel Drivers</h3>
    <div class="code-block">
        <pre><code># Windows - Intel Graphics Drivers
# Download from Intel website

# Linux - Mesa Intel drivers
sudo apt install intel-media-va-driver vulkan-intel

# Verify Intel GPU Vulkan support
vulkaninfo | grep -i intel</code></pre>
    </div>
</div>

<div class="section">
    <h2>Vulkan SDK Installation</h2>
    
    <h3>Windows Installation</h3>
    <div class="code-block">
        <pre><code># Download Vulkan SDK from LunarG
# https://vulkan.lunarg.com/sdk/home

# Install Vulkan SDK with development tools
VulkanSDK-1.3.x.x-Installer.exe

# Verify installation
echo %VULKAN_SDK%
vkcube.exe</code></pre>
    </div>
    
    <h3>Linux Installation</h3>
    <div class="code-block">
        <pre><code># Ubuntu/Debian - Install Vulkan packages
sudo apt update
sudo apt install vulkan-sdk vulkan-tools vulkan-validationlayers-dev

# Add Vulkan repository for latest version
wget -qO - http://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-focal.list \
    http://packages.lunarg.com/vulkan/lunarg-vulkan-focal.list

# Install latest SDK
sudo apt update
sudo apt install vulkan-sdk

# Verify installation
vulkaninfo
vkcube</code></pre>
    </div>
    
    <h3>macOS Installation (MoltenVK)</h3>
    <div class="code-block">
        <pre><code># Install via Homebrew
brew install vulkan-loader vulkan-tools

# Or download Vulkan SDK from LunarG
# Includes MoltenVK for Metal translation

# Verify installation
vulkaninfo
vkcube</code></pre>
    </div>
</div>

<div class="section">
    <h2>Engine Configuration</h2>
    
    <h3>Quake3e Vulkan Setup</h3>
    <div class="code-block">
        <pre><code># Download Quake3e with Vulkan support
# From https://github.com/ec-/Quake3e

# Extract to game directory
# Copy Quake 3 Arena assets (pak0.pk3, pak1.pk3, pak2.pk3)

# Launch with Vulkan renderer
quake3e.exe +set r_backend vk

# Or set in configuration
seta r_backend "vk"              // Use Vulkan renderer
seta r_device "0"                // GPU device index
seta r_validation "0"            // Disable validation (release)
seta r_renderScale "1.0"         // Render scale multiplier</code></pre>
    </div>
    
    <h3>Vulkan Renderer Settings</h3>
    <div class="code-block">
        <pre><code># Essential Vulkan configuration
seta r_backend "vk"              // Enable Vulkan renderer
seta r_device "0"                // Primary GPU (0 = first device)
seta r_vsync "1"                 // Enable V-Sync
seta r_swapChainImages "3"       // Triple buffering

# Performance settings
seta r_hdr "1"                   // HDR rendering
seta r_bloom "1"                 // Bloom effects
seta r_postProcess "1"           // Post-processing pipeline
seta r_ext_multisample "4"       // MSAA samples

# Advanced Vulkan features
seta r_compute "1"               // Enable compute shaders
seta r_rayTracing "0"            // RTX ray tracing (if supported)
seta r_validation "0"            // Validation layers (debug only)</code></pre>
    </div>
</div>

<div class="section">
    <h2>Performance Optimization</h2>
    
    <h3>GPU Selection</h3>
    <div class="code-block">
        <pre><code># List available GPUs
vulkaninfo | grep "deviceName"

# Select specific GPU in engine
seta r_device "0"                // Primary GPU
seta r_device "1"                // Secondary GPU (if available)

# Check GPU memory
vulkaninfo | grep -A 5 "memoryHeaps"</code></pre>
    </div>
    
    <h3>Memory Management</h3>
    <div class="code-block">
        <pre><code># Vulkan memory settings
seta r_vkDeviceLocalMemory "1"   // Use device local memory
seta r_vkStagingBuffer "64"      // Staging buffer size (MB)
seta r_vkDescriptorPoolSize "1024" // Descriptor pool size

# Texture streaming
seta r_textureStreaming "1"      // Enable texture streaming
seta r_textureStreamingMemory "512" // Streaming memory (MB)</code></pre>
    </div>
    
    <h3>Rendering Pipeline</h3>
    <div class="code-block">
        <pre><code># Pipeline optimization
seta r_pipelineCache "1"         // Enable pipeline caching
seta r_shaderCache "1"           // Enable shader caching
seta r_asyncShaders "1"          // Asynchronous shader compilation

# Multi-threading
seta r_multithreading "1"        // Enable multi-threading
seta r_workerThreads "4"         // Number of worker threads</code></pre>
    </div>
</div>

<div class="section">
    <h2>Debugging and Validation</h2>
    
    <h3>Enable Validation Layers</h3>
    <div class="code-block">
        <pre><code># Enable validation for debugging
seta r_validation "1"            // Enable validation layers
seta r_debugMarkers "1"          // GPU debugging markers
seta r_verbose "1"               // Verbose logging

# Environment variables for validation
export VK_LAYER_KHRONOS_validation=1
export VK_LOADER_DEBUG=all

# Launch with validation
quake3e.exe +set r_validation 1 +set developer 1</code></pre>
    </div>
    
    <h3>Debugging Tools</h3>
    <div class="code-block">
        <pre><code># RenderDoc for frame capture
renderdoc-cmd capture quake3e.exe

# Vulkan configurator
vkconfig.exe

# NVIDIA Nsight Graphics (for NVIDIA GPUs)
nsight-gfx.exe

# Check validation layer output
tail -f vulkan_validation.log</code></pre>
    </div>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    
    <h3>Common Issues</h3>
    <div class="troubleshooting">
        <h4>Vulkan not detected</h4>
        <ul>
            <li>Install latest graphics drivers</li>
            <li>Verify GPU supports Vulkan 1.1+</li>
            <li>Check vulkaninfo output</li>
            <li>Install Vulkan runtime/SDK</li>
        </ul>
        
        <h4>Poor performance</h4>
        <ul>
            <li>Disable validation layers in release</li>
            <li>Reduce MSAA and post-processing</li>
            <li>Check GPU memory usage</li>
            <li>Update to latest drivers</li>
        </ul>
        
        <h4>Crashes or artifacts</h4>
        <ul>
            <li>Enable validation layers for debugging</li>
            <li>Check shader compilation errors</li>
            <li>Verify GPU driver stability</li>
            <li>Test with different GPU devices</li>
        </ul>
        
        <h4>macOS MoltenVK issues</h4>
        <ul>
            <li>Update to latest MoltenVK version</li>
            <li>Check Metal compatibility</li>
            <li>Verify macOS version requirements</li>
            <li>Test with validation disabled</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Advanced Configuration</h2>
    
    <h3>Extension Management</h3>
    <div class="code-block">
        <pre><code># Check available extensions
vulkaninfo | grep -A 100 "Device Extensions"

# Enable specific extensions in engine
seta r_vkDebugUtils "1"          // Debug utilities
seta r_vkValidation "1"          // Validation features
seta r_vkRayTracing "1"          // Ray tracing (if supported)

# Platform-specific extensions
# Windows: VK_KHR_win32_surface
# Linux: VK_KHR_xcb_surface, VK_KHR_xlib_surface  
# macOS: VK_MVK_macos_surface</code></pre>
    </div>
    
    <h3>Profile-Based Settings</h3>
    <div class="code-block">
        <pre><code># Performance profile
exec "vulkan_performance.cfg"
seta r_hdr "0"
seta r_bloom "0"
seta r_ext_multisample "0"
seta r_renderScale "0.8"

# Quality profile  
exec "vulkan_quality.cfg"
seta r_hdr "1"
seta r_bloom "1"
seta r_ext_multisample "8"
seta r_renderScale "1.2"

# RTX profile (NVIDIA RTX cards)
exec "vulkan_rtx.cfg"
seta r_rayTracing "1"
seta r_dlss "1"</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="rendering/vulkan">Vulkan Renderer</a></li>
        <li><a href="rendering/shaders">Vulkan Shaders</a></li>
        <li><a href="getting-started/installation">Engine Installation</a></li>
        <li><a href="development/debugging">Debugging Guide</a></li>
    </ul>
</div> 