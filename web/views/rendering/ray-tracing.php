<?php
/**
 * Ray Tracing Documentation
 */
$title = 'Ray Tracing - id Tech 3 Documentation';
$breadcrumbs = [
    '/rendering' => 'Rendering',
    '/rendering/ray-tracing' => 'Ray Tracing'
];
?>

<div class="content-section">
    <h1>Ray Tracing in id Tech 3</h1>
    
    <blockquote>
        <strong>Hardware-Accelerated Ray Tracing:</strong> The id Tech 3 engine supports hardware-accelerated ray tracing through Vulkan RTX and DirectX DXR, enabling realistic lighting, reflections, and global illumination effects.
    </blockquote>

    <div class="section">
        <h2>Overview</h2>
        <p>Ray tracing provides physically accurate lighting and reflections by simulating the path of light rays through a scene. The engine supports two ray tracing implementations:</p>
        
        <div class="feature-list">
            <h3>Ray Tracing Implementations</h3>
            <ul>
                <li><strong>Vulkan RTX:</strong> Cross-platform ray tracing using VK_KHR_ray_tracing_pipeline</li>
                <li><strong>DirectX DXR:</strong> Windows-only ray tracing using DirectX Raytracing API</li>
            </ul>
        </div>
    </div>

    <div class="section">
        <h2>Requirements</h2>
        
        <h3>Hardware Requirements</h3>
        <ul>
            <li><strong>GPU:</strong> NVIDIA RTX series (20xx, 30xx, 40xx) or AMD RX 6000+ series</li>
            <li><strong>Vulkan:</strong> VK_KHR_ray_tracing_pipeline extension support</li>
            <li><strong>DirectX:</strong> D3D12 ray tracing tier 1.1+ support</li>
            <li><strong>Drivers:</strong> Latest GPU drivers with ray tracing support</li>
        </ul>

        <h3>Software Requirements</h3>
        <ul>
            <li><strong>Vulkan:</strong> Vulkan 1.2+ with ray tracing extensions</li>
            <li><strong>DirectX:</strong> Windows 10 version 1809+ or Windows 11</li>
            <li><strong>Renderer:</strong> Vulkan or DirectX 12 renderer enabled</li>
        </ul>
    </div>

    <div class="section">
        <h2>Enabling Ray Tracing</h2>
        
        <h3>Vulkan Renderer</h3>
        <div class="code-block">
            <pre><code>// Enable Vulkan renderer
set cl_renderer "vulkan"

// Enable ray tracing
set r_raytracing "1"

// Configure ray tracing quality
set r_raytracing_samples "1"      // Samples per pixel (1-4)
set r_raytracing_maxDepth "2"     // Maximum ray bounces
set r_raytracing_denoise "1"      // Enable denoising</code></pre>
        </div>

        <h3>DirectX 12 Renderer</h3>
        <div class="code-block">
            <pre><code>// Enable DirectX 12 renderer
set cl_renderer "d3d12"

// Enable ray tracing (DXR)
set r_raytracing "1"

// DXR-specific settings
set r_dxr_quality "1"              // Quality level (0-2)
set r_dxr_reflections "1"          // Enable reflections
set r_dxr_shadows "1"              // Enable shadow rays</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Ray Tracing Features</h2>
        
        <h3>Reflections</h3>
        <p>Realistic surface reflections using ray-traced reflections:</p>
        <ul>
            <li>Accurate reflection of environment and objects</li>
            <li>Configurable reflection quality</li>
            <li>Fallback to environment mapping when ray tracing unavailable</li>
            <li>Performance scaling with sample count</li>
        </ul>

        <h3>Global Illumination</h3>
        <p>Indirect lighting simulation:</p>
        <ul>
            <li>Bounced light from surfaces</li>
            <li>Realistic ambient occlusion</li>
            <li>Color bleeding between surfaces</li>
            <li>Configurable bounce depth</li>
        </ul>

        <h3>Shadows</h3>
        <p>Hardware-accelerated shadow rays:</p>
        <ul>
            <li>Accurate soft shadows</li>
            <li>Contact shadows</li>
            <li>Transparent shadow support</li>
            <li>Performance-optimized shadow culling</li>
        </ul>
    </div>

    <div class="section">
        <h2>Performance Considerations</h2>
        
        <h3>Optimization Tips</h3>
        <ul>
            <li><strong>Sample Count:</strong> Lower samples (1-2) for better performance</li>
            <li><strong>Ray Depth:</strong> Limit bounces to 1-2 for most scenes</li>
            <li><strong>Denoising:</strong> Enable denoising to reduce samples needed</li>
            <li><strong>Selective Ray Tracing:</strong> Use ray tracing only for key surfaces</li>
            <li><strong>Hybrid Rendering:</strong> Combine ray tracing with rasterization</li>
        </ul>

        <h3>Performance CVARs</h3>
        <div class="code-block">
            <pre><code>// Performance tuning
set r_raytracing_samples "1"       // Lower = faster
set r_raytracing_maxDepth "1"      // Lower = faster
set r_raytracing_denoise "1"       // Enable for better quality at lower samples
set r_raytracing_resolution "0.5"  // Half resolution (0.5) for performance</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Shader Integration</h2>
        
        <h3>Ray Tracing Shaders</h3>
        <p>Ray tracing shaders are automatically generated by the renderer. Custom shaders can request ray-traced reflections:</p>
        <div class="code-block">
            <pre><code>textures/metal/reflective
{
    {
        map textures/metal/base.tga
    }
    {
        // Ray-traced reflections (if enabled)
        // Falls back to environment mapping if ray tracing unavailable
        map $envmap
        blendFunc GL_ONE GL_ONE
        tcGen environment
    }
}</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Acceleration Structures</h2>
        
        <h3>Bottom-Level Acceleration Structures (BLAS)</h3>
        <p>Per-model acceleration structures:</p>
        <ul>
            <li>Built from model geometry</li>
            <li>Cached for static geometry</li>
            <li>Rebuilt for animated models</li>
            <li>Optimized BVH construction</li>
        </ul>

        <h3>Top-Level Acceleration Structure (TLAS)</h3>
        <p>Scene-level acceleration structure:</p>
        <ul>
            <li>Contains all BLAS instances</li>
            <li>Rebuilt each frame for dynamic objects</li>
            <li>Instance transforms updated</li>
            <li>Efficient update for static scenes</li>
        </ul>
    </div>

    <div class="section">
        <h2>Troubleshooting</h2>
        
        <h3>Ray Tracing Not Working</h3>
        <ul>
            <li>Verify GPU supports ray tracing (check console output)</li>
            <li>Ensure latest drivers are installed</li>
            <li>Check renderer is Vulkan or DirectX 12</li>
            <li>Verify <code>r_raytracing</code> is set to 1</li>
            <li>Check console for ray tracing initialization messages</li>
        </ul>

        <h3>Performance Issues</h3>
        <ul>
            <li>Reduce sample count</li>
            <li>Lower ray depth</li>
            <li>Enable denoising</li>
            <li>Use half-resolution ray tracing</li>
            <li>Disable ray tracing for less important effects</li>
        </ul>

        <h3>Visual Artifacts</h3>
        <ul>
            <li>Increase sample count for cleaner results</li>
            <li>Enable denoising to reduce noise</li>
            <li>Check acceleration structure updates</li>
            <li>Verify geometry is properly included in BLAS</li>
        </ul>
    </div>

    <div class="section">
        <h2>Related Documentation</h2>
        <ul>
            <li><a href="rendering/vulkan">Vulkan Renderer</a> - Vulkan RTX implementation</li>
            <li><a href="rendering/directx12">DirectX 12 Renderer</a> - DXR implementation</li>
            <li><a href="rendering/pbr">PBR Pipeline</a> - Physically-based rendering</li>
            <li><a href="rendering/global-illumination">Global Illumination</a> - GI techniques</li>
        </ul>
    </div>
</div>

