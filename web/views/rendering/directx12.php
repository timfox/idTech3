<?php
/**
 * DirectX 12 Renderer Documentation
 */
$title = 'DirectX 12 Renderer - id Tech 3 Documentation';
$breadcrumbs = [
    '/rendering' => 'Rendering',
    '/rendering/directx12' => 'DirectX 12 Renderer'
];
?>

<h1>DirectX 12 Renderer</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The id Tech 3 engine now includes DirectX 12 renderer support for Windows platforms, providing modern graphics API access with improved performance and features. The D3D12 renderer offers lower CPU overhead, better multi-threading support, and access to the latest GPU features including hardware-accelerated ray tracing.</p>
    
    <div class="feature-list">
        <h3>Key Features</h3>
        <ul>
            <li><strong>Modern Graphics API:</strong> Full DirectX 12 support with feature level 12.0+</li>
            <li><strong>Triple Buffering:</strong> Efficient frame presentation with triple buffering</li>
            <li><strong>Command Lists:</strong> Efficient command recording and execution</li>
            <li><strong>Descriptor Heaps:</strong> Optimized resource management</li>
            <li><strong>Root Signatures:</strong> Flexible shader resource binding</li>
            <li><strong>Pipeline State Objects:</strong> Pre-compiled rendering pipelines</li>
            <li><strong>Resource Barriers:</strong> Efficient resource state transitions</li>
            <li><strong>GPU Synchronization:</strong> Proper fence-based synchronization</li>
            <li><strong>Ray Tracing (DXR):</strong> DirectX Raytracing with GPU-accelerated acceleration structures</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Supported Features</h2>
    <ul>
        <li><strong>Feature Levels:</strong> 12.2, 12.1, 12.0, 11.1, 11.0 (with fallback)</li>
        <li><strong>Resource Binding Tiers:</strong> Tier 1, 2, and 3 support detection</li>
        <li><strong>Debug Layer:</strong> Optional D3D12 debug layer in debug builds</li>
        <li><strong>Swap Chain:</strong> DXGI swap chain with flip discard model</li>
        <li><strong>Render Targets:</strong> Multiple render target support</li>
        <li><strong>Depth Stencil:</strong> 32-bit depth buffer support</li>
        <li><strong>Ray Tracing (DXR):</strong> DirectX Raytracing support with acceleration structures</li>
    </ul>
</div>

<div class="section">
    <h2>Requirements</h2>
    
    <h3>Windows SDK</h3>
    <ul>
        <li><strong>Minimum:</strong> Windows 10 SDK (10.0.17763.0 or later)</li>
        <li><strong>Recommended:</strong> Latest Windows SDK</li>
        <li><strong>Visual Studio:</strong> 2019 or later (for D3D12 headers)</li>
    </ul>
    
    <h3>Hardware</h3>
    <ul>
        <li><strong>GPU:</strong> DirectX 12 compatible graphics card</li>
        <li><strong>Windows:</strong> Windows 10 or later</li>
        <li><strong>Driver:</strong> Latest graphics drivers recommended</li>
    </ul>
</div>

<div class="section">
    <h2>Building</h2>
    
    <h3>CMake Configuration</h3>
    <p>Enable D3D12 support:</p>
    <div class="code-block">
        <pre><code>cmake .. -DUSE_D3D12=ON</code></pre>
    </div>
    
    <h3>Visual Studio</h3>
    <p>D3D12 is automatically enabled on Windows builds when <code>USE_D3D12=ON</code>:</p>
    <div class="code-block">
        <pre><code>cmake .. -G "Visual Studio 17 2022" -A x64 -DUSE_D3D12=ON</code></pre>
    </div>
    
    <h3>Build Options</h3>
    <ul>
        <li><code>USE_D3D12</code>: Enable/disable DirectX 12 renderer (default: ON on Windows)</li>
        <li><code>USE_RENDERER_DLOPEN</code>: Build renderer as dynamic library (supports multiple renderers)</li>
    </ul>
</div>

<div class="section">
    <h2>Usage</h2>
    
    <h3>Selecting D3D12 Renderer</h3>
    <p>Set renderer at runtime:</p>
    <div class="code-block">
        <pre><code>/r_renderer d3d12</code></pre>
    </div>
    
    <p>Or set default in config:</p>
    <div class="code-block">
        <pre><code>set r_renderer d3d12</code></pre>
    </div>
    
    <h3>CVars</h3>
    <table class="settings-table">
        <tr>
            <th>CVar</th>
            <th>Description</th>
        </tr>
        <tr>
            <td><code>r_renderer</code></td>
            <td>Select renderer backend (opengl, vulkan, d3d12)</td>
        </tr>
        <tr>
            <td><code>r_d3d12_debug</code></td>
            <td>Enable D3D12 debug layer (debug builds only)</td>
        </tr>
        <tr>
            <td><code>r_d3d12_raytracing</code></td>
            <td>Enable ray tracing (if supported)</td>
        </tr>
    </table>
</div>

<div class="section">
    <h2>Architecture</h2>
    
    <h3>Directory Structure</h3>
    <div class="code-block">
        <pre><code>src/rendererd3d12/
├── d3d12.h          # D3D12 context and API
├── d3d12.c          # D3D12 initialization and management
├── tr_local.h       # Renderer local definitions
├── tr_common.h      # Common renderer definitions
├── tr_init.c        # Renderer initialization
└── tr_main.c        # Main renderer interface</code></pre>
    </div>
    
    <h3>Key Components</h3>
    <ul>
        <li><strong>D3D12 Context</strong> (<code>d3d12.h/.c</code>):
            <ul>
                <li>Device creation and management</li>
                <li>Swap chain management</li>
                <li>Command queue and lists</li>
                <li>Synchronization objects</li>
                <li>Descriptor heaps</li>
            </ul>
        </li>
        <li><strong>Renderer Interface</strong> (<code>tr_main.c</code>):
            <ul>
                <li>Public renderer API implementation</li>
                <li>Model/shader/texture registration</li>
                <li>Frame rendering</li>
            </ul>
        </li>
        <li><strong>Initialization</strong> (<code>tr_init.c</code>):
            <ul>
                <li>Window setup</li>
                <li>Resource creation</li>
                <li>State management</li>
            </ul>
        </li>
    </ul>
</div>

<div class="section">
    <h2>Ray Tracing (DXR)</h2>
    
    <h3>Overview</h3>
    <p>The D3D12 renderer includes DirectX Raytracing (DXR) support for hardware-accelerated ray tracing. DXR provides real-time ray tracing capabilities for advanced lighting, reflections, and shadows.</p>
    
    <h3>Requirements</h3>
    <ul>
        <li><strong>GPU:</strong> DirectX 12 compatible GPU with DXR support (NVIDIA RTX series, AMD RX 6000+)</li>
        <li><strong>Windows:</strong> Windows 10 version 1809 (October 2018 Update) or later</li>
        <li><strong>Driver:</strong> Latest graphics drivers with DXR support</li>
    </ul>
    
    <h3>Features</h3>
    <ul>
        <li><strong>Hardware Acceleration:</strong> Uses GPU ray tracing cores when available</li>
        <li><strong>Acceleration Structures:</strong> Bottom-level (BLAS) and top-level (TLAS) acceleration structures</li>
        <li><strong>Shader Binding Table:</strong> Efficient shader dispatch for ray tracing</li>
        <li><strong>HDR Output:</strong> Ray tracing writes to HDR buffer for proper tone mapping</li>
        <li><strong>Tier Detection:</strong> Automatically detects DXR tier (1.0, 1.1)</li>
    </ul>
    
    <h3>Usage</h3>
    <p>Ray tracing is automatically enabled if:</p>
    <ul>
        <li>GPU supports DXR</li>
        <li>Windows version is compatible</li>
        <li>Driver supports DXR</li>
    </ul>
    
    <p>To check ray tracing status:</p>
    <div class="code-block">
        <pre><code>/r_d3d12_raytracing 1  // Enable ray tracing (if supported)</code></pre>
    </div>
</div>

<div class="section">
    <h2>Implementation Status</h2>
    
    <h3>Completed</h3>
    <ul>
        <li>✅ D3D12 device creation</li>
        <li>✅ Swap chain creation</li>
        <li>✅ Render target setup</li>
        <li>✅ Command list recording</li>
        <li>✅ Basic synchronization</li>
        <li>✅ Root signature creation</li>
        <li>✅ Build system integration</li>
        <li>✅ DXR capability detection</li>
        <li>✅ Ray tracing device interfaces</li>
        <li>✅ Acceleration structure framework</li>
        <li>✅ Shader binding table creation</li>
        <li>✅ Ray tracing output buffer</li>
    </ul>
    
    <h3>In Progress</h3>
    <ul>
        <li>🔄 Shader compilation (HLSL)</li>
        <li>🔄 Pipeline state objects</li>
        <li>🔄 Texture loading and management</li>
        <li>🔄 Vertex/index buffer management</li>
        <li>🔄 Full rendering pipeline</li>
    </ul>
    
    <h3>Planned</h3>
    <ul>
        <li>⏳ Mesh rendering</li>
        <li>⏳ Shader system</li>
        <li>⏳ Texture system</li>
        <li>⏳ Lighting system</li>
        <li>⏳ Post-processing effects</li>
        <li>⏳ ImGui integration</li>
        <li>⏳ HLSL ray tracing shaders (ray generation, miss, closest hit)</li>
        <li>⏳ Acceleration structure building with geometry</li>
        <li>⏳ Ray tracing pipeline state with compiled shaders</li>
        <li>⏳ Ray tracing integration with rendering pipeline</li>
    </ul>
</div>

<div class="section">
    <h2>Performance Considerations</h2>
    
    <h3>Advantages</h3>
    <ul>
        <li><strong>Lower CPU Overhead:</strong> Command lists reduce CPU overhead</li>
        <li><strong>Better Multi-threading:</strong> Parallel command list recording</li>
        <li><strong>Efficient Resource Management:</strong> Descriptor heaps and resource barriers</li>
        <li><strong>Modern Features:</strong> Access to latest GPU features</li>
    </ul>
    
    <h3>Optimization Tips</h3>
    <ol>
        <li><strong>Use Command Lists Efficiently:</strong> Record commands once, execute multiple times</li>
        <li><strong>Batch Draw Calls:</strong> Group similar draws together</li>
        <li><strong>Minimize State Changes:</strong> Cache pipeline states</li>
        <li><strong>Use Resource Barriers Wisely:</strong> Batch barrier transitions</li>
        <li><strong>Optimize Descriptor Heaps:</strong> Reuse descriptors where possible</li>
    </ol>
</div>

<div class="section">
    <h2>Comparison with Other Renderers</h2>
    
    <h3>vs OpenGL</h3>
    <ul>
        <li><strong>Lower Overhead:</strong> D3D12 has less driver overhead</li>
        <li><strong>Better Control:</strong> More explicit resource management</li>
        <li><strong>Modern Features:</strong> Access to latest GPU features</li>
        <li><strong>Windows Only:</strong> D3D12 is Windows-specific</li>
    </ul>
    
    <h3>vs Vulkan</h3>
    <ul>
        <li><strong>Similar Architecture:</strong> Both are low-level APIs</li>
        <li><strong>Platform Specific:</strong> D3D12 is Windows-only, Vulkan is cross-platform</li>
        <li><strong>API Style:</strong> D3D12 uses COM, Vulkan uses C API</li>
        <li><strong>Feature Parity:</strong> Similar feature sets</li>
    </ul>
</div>

<div class="section">
    <h2>Debugging</h2>
    
    <h3>Debug Layer</h3>
    <p>Enable D3D12 debug layer in debug builds:</p>
    <div class="code-block">
        <pre><code>// Automatically enabled in _DEBUG builds</code></pre>
    </div>
    
    <p>The debug layer provides:</p>
    <ul>
        <li>Parameter validation</li>
        <li>Resource state tracking</li>
        <li>Memory leak detection</li>
        <li>Performance warnings</li>
    </ul>
    
    <h3>Common Issues</h3>
    
    <h4>Device Removed</h4>
    <ul>
        <li>Update graphics drivers</li>
        <li>Check for TDR (Timeout Detection and Recovery)</li>
        <li>Verify GPU compatibility</li>
    </ul>
    
    <h4>Swap Chain Creation Failed</h4>
    <ul>
        <li>Check window handle validity</li>
        <li>Verify format support</li>
        <li>Check feature level compatibility</li>
    </ul>
    
    <h4>Command List Errors</h4>
    <ul>
        <li>Ensure proper resource state transitions</li>
        <li>Verify resource lifetimes</li>
        <li>Check descriptor heap sizes</li>
    </ul>
</div>

<div class="section">
    <h2>Future Enhancements</h2>
    <p>Planned improvements:</p>
    <ul>
        <li><strong>Mesh Shaders:</strong> Support for mesh shader pipeline</li>
        <li><strong>Variable Rate Shading:</strong> VRS for performance optimization</li>
        <li><strong>Sampler Feedback:</strong> Advanced texture sampling</li>
        <li><strong>Meshlet Rendering:</strong> Efficient geometry processing</li>
        <li><strong>DXR Tier 1.1:</strong> Support for inline ray tracing</li>
    </ul>
</div>

<div class="section">
    <h2>Troubleshooting</h2>
    
    <h3>Build Issues</h3>
    
    <h4>Missing D3D12 Headers</h4>
    <ul>
        <li>Install Windows SDK</li>
        <li>Update Visual Studio</li>
        <li>Verify SDK version</li>
    </ul>
    
    <h4>Link Errors</h4>
    <ul>
        <li>Ensure d3d12.lib is linked</li>
        <li>Check dxgi.lib linkage</li>
        <li>Verify d3dcompiler.lib</li>
    </ul>
    
    <h3>Runtime Issues</h3>
    
    <h4>Renderer Not Found</h4>
    <ul>
        <li>Verify USE_D3D12=ON in CMake</li>
        <li>Check renderer DLL exists</li>
        <li>Verify Windows version</li>
    </ul>
    
    <h4>Initialization Failed</h4>
    <ul>
        <li>Check GPU compatibility</li>
        <li>Update graphics drivers</li>
        <li>Verify feature level support</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="rendering/vulkan">Vulkan Renderer</a></li>
        <li><a href="rendering/pbr">PBR Pipeline</a></li>
        <li><a href="renderer/vulkan-implementation">Vulkan Implementation</a></li>
        <li><a href="development/debugging">Debugging Tools</a></li>
    </ul>
</div>

