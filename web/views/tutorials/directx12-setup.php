<?php
/**
 * DirectX 12 Setup Tutorial
 */
$title = 'DirectX 12 Setup Tutorial - id Tech 3 Documentation';
$breadcrumbs = [
    '/tutorials' => 'Tutorials',
    '/tutorials/directx12-setup' => 'DirectX 12 Setup Tutorial'
];
?>

<h1>DirectX 12 Setup Tutorial</h1>

<div class="section">
    <h2>Introduction</h2>
    <p>This tutorial will guide you through setting up and using the DirectX 12 renderer in id Tech 3. The DirectX 12 renderer provides modern graphics capabilities including DirectX Raytracing (DXR) support on Windows.</p>
    
    <div class="feature-list">
        <h3>What You'll Learn</h3>
        <ul>
            <li>System requirements and compatibility</li>
            <li>Building with DirectX 12 support</li>
            <li>Enabling the DirectX 12 renderer</li>
            <li>Configuring DXR (ray tracing)</li>
            <li>Optimizing performance</li>
            <li>Troubleshooting common issues</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Prerequisites</h2>
    <ul>
        <li>Windows 10 version 1809 or later (or Windows 11)</li>
        <li>DirectX 12 compatible graphics card</li>
        <li>Visual Studio 2019 or later (for building)</li>
        <li>Windows SDK 10.0.17763.0 or later</li>
        <li>Basic understanding of graphics APIs</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: System Requirements</h2>
    
    <h3>Step 1: Check Windows Version</h3>
    <p>DirectX 12 requires Windows 10 version 1809 or later:</p>
    <div class="code-block">
        <pre><code># Check Windows version
winver

# Or in PowerShell
Get-ComputerInfo | Select WindowsVersion</code></pre>
    </div>
    <p><strong>Minimum:</strong> Windows 10 version 1809 (October 2018 Update)</p>
    <p><strong>Recommended:</strong> Windows 10 version 2004 or later, or Windows 11</p>
    
    <h3>Step 2: Check Graphics Card Compatibility</h3>
    <p>Verify your GPU supports DirectX 12:</p>
    <div class="code-block">
        <pre><code># Run DirectX Diagnostic Tool
dxdiag

# Check "Display" tab for:
# - DirectX Version: DirectX 12
# - Feature Levels: 12_0, 12_1</code></pre>
    </div>
    
    <h3>Step 3: Check DXR Support (Optional)</h3>
    <p>For ray tracing, verify DXR support:</p>
    <ul>
        <li><strong>NVIDIA:</strong> RTX series (RTX 2060 and above)</li>
        <li><strong>AMD:</strong> RX 6000 series and above</li>
        <li><strong>Intel:</strong> Arc series</li>
    </ul>
    <p>Check in dxdiag or GPU-Z for DXR/ray tracing support</p>
</div>

<div class="section">
    <h2>Tutorial: Building with DirectX 12</h2>
    
    <h3>Step 1: Install Prerequisites</h3>
    <p>Install required development tools:</p>
    <ul>
        <li><strong>Visual Studio 2019/2022:</strong> With C++ desktop development workload</li>
        <li><strong>Windows SDK:</strong> Version 10.0.17763.0 or later</li>
        <li><strong>CMake:</strong> Version 3.15 or later</li>
    </ul>
    
    <h3>Step 2: Configure CMake</h3>
    <p>Configure the build with DirectX 12 support:</p>
    <div class="code-block">
        <pre><code>cd build
cmake .. -DENABLE_D3D12=ON -G "Visual Studio 16 2019" -A x64

# Or for Visual Studio 2022
cmake .. -DENABLE_D3D12=ON -G "Visual Studio 17 2022" -A x64</code></pre>
    </div>
    
    <h3>Step 3: Build</h3>
    <div class="code-block">
        <pre><code># Build using CMake
cmake --build . --config Release

# Or open in Visual Studio and build
# Open idtech3.sln and build</code></pre>
    </div>
    
    <h3>Step 4: Verify Build</h3>
    <p>Check that DirectX 12 renderer was built:</p>
    <div class="code-block">
        <pre><code># Check for renderer DLL
dir idtech3_d3d12_x86_64.dll

# Should exist if build succeeded</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Enabling DirectX 12 Renderer</h2>
    
    <h3>Step 1: Set Renderer</h3>
    <p>Enable DirectX 12 renderer:</p>
    <div class="code-block">
        <pre><code># Set renderer to DirectX 12
/set r_renderer d3d12

# Or via command line
./idtech3.x86_64 +set r_renderer d3d12</code></pre>
    </div>
    
    <h3>Step 2: Verify Renderer</h3>
    <p>Check that DirectX 12 is active:</p>
    <div class="code-block">
        <pre><code># Check renderer info
/r_info

# Should show:
# Renderer: DirectX 12
# Vendor: NVIDIA/AMD/Intel
# Version: DirectX 12.x</code></pre>
    </div>
    
    <h3>Step 3: Check Feature Level</h3>
    <p>Verify supported feature levels:</p>
    <div class="code-block">
        <pre><code># Feature levels supported:
# - 12_0: Basic DirectX 12
# - 12_1: Enhanced features
# - 12_2: Latest features (if available)</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Configuring DXR (Ray Tracing)</h2>
    
    <h3>Step 1: Check DXR Availability</h3>
    <p>Verify DXR is available:</p>
    <div class="code-block">
        <pre><code># Check DXR support
/r_dxr_enable

# Should show: 1 if supported, 0 if not</code></pre>
    </div>
    
    <h3>Step 2: Enable Ray Tracing</h3>
    <p>Enable DXR if supported:</p>
    <div class="code-block">
        <pre><code># Enable DXR
/set r_dxr_enable 1

# Set ray tracing quality
/set r_dxr_quality 1  # 0=off, 1=low, 2=medium, 3=high</code></pre>
    </div>
    
    <h3>Step 3: Configure Ray Tracing Settings</h3>
    <div class="code-block">
        <pre><code># Ray tracing quality levels
/set r_dxr_quality 1   # Low - better performance
/set r_dxr_quality 2   # Medium - balanced
/set r_dxr_quality 3   # High - best quality, lower performance

# Ray tracing resolution (if supported)
/set r_dxr_resolution 0.5  # 0.5 = half resolution (faster)
/set r_dxr_resolution 1.0  # 1.0 = full resolution</code></pre>
    </div>
    
    <h3>Step 4: Performance Considerations</h3>
    <p>Ray tracing has significant performance impact:</p>
    <ul>
        <li>Enable only on RTX/RX 6000+ GPUs</li>
        <li>Start with low quality</li>
        <li>Monitor FPS impact</li>
        <li>Disable if performance is unacceptable</li>
    </ul>
</div>

<div class="section">
    <h2>Tutorial: Performance Optimization</h2>
    
    <h3>Step 1: Enable Triple Buffering</h3>
    <p>Triple buffering reduces input latency:</p>
    <div class="code-block">
        <pre><code># Enable triple buffering
/set r_d3d12_triple_buffering 1</code></pre>
    </div>
    
    <h3>Step 2: Configure Command Lists</h3>
    <p>Optimize command list usage:</p>
    <div class="code-block">
        <pre><code># Number of command lists (default: 2)
/set r_d3d12_command_lists 2

# Increase for better parallelization (if CPU allows)
/set r_d3d12_command_lists 4</code></pre>
    </div>
    
    <h3>Step 3: Tune Resource Barriers</h3>
    <p>Optimize resource barrier batching:</p>
    <div class="code-block">
        <pre><code># Enable barrier batching (default: on)
/set r_d3d12_batch_barriers 1</code></pre>
    </div>
    
    <h3>Step 4: Monitor Performance</h3>
    <p>Use ImGui overlay to monitor performance:</p>
    <div class="code-block">
        <pre><code># Enable performance overlay
/set cl_imgui 1
/set cl_imgui_debug_performance 1
/set cl_imgui_debug_renderer 1</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Complete Configuration</h2>
    
    <h3>Recommended Settings</h3>
    <p>Add to your <code>autoexec.cfg</code>:</p>
    <div class="code-block">
        <pre><code>// DirectX 12 Renderer Configuration
set r_renderer d3d12

// Triple Buffering
set r_d3d12_triple_buffering 1

// Command Lists
set r_d3d12_command_lists 2

// DXR (Ray Tracing) - if supported
set r_dxr_enable 1
set r_dxr_quality 2  // Medium quality

// Performance Monitoring
set cl_imgui 1
set cl_imgui_debug_performance 1</code></pre>
    </div>
    
    <h3>High Performance Configuration</h3>
    <div class="code-block">
        <pre><code>// Maximum Performance
set r_renderer d3d12
set r_d3d12_triple_buffering 1
set r_d3d12_command_lists 4
set r_dxr_enable 0  // Disable ray tracing for performance</code></pre>
    </div>
    
    <h3>High Quality Configuration</h3>
    <div class="code-block">
        <pre><code>// Maximum Quality (requires RTX/RX 6000+)
set r_renderer d3d12
set r_d3d12_triple_buffering 1
set r_dxr_enable 1
set r_dxr_quality 3  // High quality
set r_dxr_resolution 1.0  // Full resolution</code></pre>
    </div>
</div>

<div class="section">
    <h2>Tutorial: Troubleshooting</h2>
    
    <h3>Renderer Not Available</h3>
    <p><strong>Problem:</strong> DirectX 12 renderer not found.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Verify build included D3D12: <code>-DENABLE_D3D12=ON</code></li>
        <li>Check <code>idtech3_d3d12_x86_64.dll</code> exists</li>
        <li>Verify Windows version is 1809 or later</li>
        <li>Check graphics drivers are up to date</li>
        <li>Verify DirectX 12 is available: <code>dxdiag</code></li>
    </ul>
    
    <h3>Low Performance</h3>
    <p><strong>Problem:</strong> Poor FPS with DirectX 12.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Disable ray tracing if enabled: <code>/set r_dxr_enable 0</code></li>
        <li>Reduce ray tracing quality: <code>/set r_dxr_quality 1</code></li>
        <li>Update graphics drivers</li>
        <li>Check CPU usage (D3D12 is more CPU-intensive)</li>
        <li>Try Vulkan renderer for comparison</li>
    </ul>
    
    <h3>DXR Not Available</h3>
    <p><strong>Problem:</strong> Ray tracing not working.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Verify GPU supports DXR (RTX/RX 6000+)</li>
        <li>Check Windows version (1809+ required)</li>
        <li>Update graphics drivers</li>
        <li>Verify DXR is enabled in driver settings</li>
        <li>Check <code>/r_dxr_enable</code> shows 1</li>
    </ul>
    
    <h3>Visual Artifacts</h3>
    <p><strong>Problem:</strong> Rendering glitches or artifacts.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Update graphics drivers</li>
        <li>Disable ray tracing temporarily</li>
        <li>Check for overheating</li>
        <li>Verify GPU is not overclocked too much</li>
        <li>Try different quality settings</li>
    </ul>
    
    <h3>Crash on Startup</h3>
    <p><strong>Problem:</strong> Game crashes when using D3D12.</p>
    <p><strong>Solutions:</strong></p>
    <ul>
        <li>Check Windows Event Viewer for error details</li>
        <li>Verify Windows SDK version is correct</li>
        <li>Update graphics drivers</li>
        <li>Try running as administrator</li>
        <li>Check for conflicting software</li>
        <li>Use Vulkan or OpenGL as fallback</li>
    </ul>
</div>

<div class="section">
    <h2>Best Practices</h2>
    <ul>
        <li><strong>Keep Drivers Updated:</strong> Latest drivers often improve D3D12 performance</li>
        <li><strong>Test Feature Levels:</strong> Verify your GPU supports required feature level</li>
        <li><strong>Monitor Performance:</strong> Use ImGui overlay to track FPS</li>
        <li><strong>Start Conservative:</strong> Begin with default settings, then optimize</li>
        <li><strong>Use Triple Buffering:</strong> Improves frame pacing</li>
        <li><strong>DXR Selectively:</strong> Only enable ray tracing on capable GPUs</li>
        <li><strong>Fallback Option:</strong> Keep Vulkan/OpenGL as alternatives</li>
    </ul>
</div>

<div class="section">
    <h2>Feature Comparison</h2>
    <table>
        <tr>
            <th>Feature</th>
            <th>DirectX 12</th>
            <th>Vulkan</th>
            <th>OpenGL</th>
        </tr>
        <tr>
            <td>Platform</td>
            <td>Windows 10/11</td>
            <td>Windows/Linux/macOS</td>
            <td>All platforms</td>
        </tr>
        <tr>
            <td>Ray Tracing</td>
            <td>Yes (DXR)</td>
            <td>Yes (VK_KHR_ray_tracing)</td>
            <td>No</td>
        </tr>
        <tr>
            <td>Performance</td>
            <td>High</td>
            <td>High</td>
            <td>Medium</td>
        </tr>
        <tr>
            <td>CPU Overhead</td>
            <td>Low</td>
            <td>Low</td>
            <td>High</td>
        </tr>
        <tr>
            <td>Triple Buffering</td>
            <td>Yes</td>
            <td>Yes</td>
            <td>Limited</td>
        </tr>
    </table>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="rendering/directx12">DirectX 12 Renderer Documentation</a> - Complete reference</li>
        <li><a href="rendering/vulkan">Vulkan Renderer</a> - Cross-platform alternative</li>
        <li><a href="getting-started/configuration">Configuration Guide</a> - General configuration</li>
        <li><a href="imgui">ImGui Debug Overlays</a> - Performance monitoring</li>
    </ul>
</div>

