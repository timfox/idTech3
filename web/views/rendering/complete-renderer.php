<?php
/**
 * Complete Renderer Documentation
 */
$title = 'Complete Renderer Guide - id Tech 3 Documentation';
$breadcrumbs = [
    '/rendering' => 'Rendering',
    '/rendering/complete-renderer' => 'Complete Renderer Guide'
];
?>

<div class="content-section">
    <h1>Complete Renderer Guide</h1>
    
    <blockquote>
        <strong>Comprehensive Renderer Documentation:</strong> This guide covers all aspects of the id Tech 3 rendering system, including all three renderer backends, modern features, and optimization techniques.
    </blockquote>

    <div class="section">
        <h2>Renderer Overview</h2>
        <p>The id Tech 3 engine supports three renderer backends, each optimized for different platforms and use cases:</p>
        
        <div class="feature-list">
            <h3>Renderer Backends</h3>
            <ul>
                <li><strong>Vulkan:</strong> Modern, cross-platform, recommended for all platforms</li>
                <li><strong>DirectX 12:</strong> Windows-only, with DXR ray tracing support</li>
                <li><strong>OpenGL:</strong> Legacy, maximum compatibility, fallback option</li>
            </ul>
        </div>
    </div>

    <div class="section">
        <h2>Vulkan Renderer</h2>
        
        <h3>Features</h3>
        <ul>
            <li>Modern graphics API with low CPU overhead</li>
            <li>Ray tracing support (Vulkan RTX)</li>
            <li>High-quality per-pixel dynamic lighting</li>
            <li>Very fast flares</li>
            <li>Anisotropic filtering</li>
            <li>Greatly reduced API overhead</li>
            <li>Flexible vertex buffer memory management</li>
            <li>Multiple command buffers</li>
            <li>Reversed depth buffer (eliminates z-fighting)</li>
            <li>Merged lightmaps (atlases)</li>
            <li>Multitexturing optimizations</li>
            <li>VBO caching for static world surfaces</li>
            <li>Debug markers for RenderDoc</li>
            <li>Offscreen rendering (FBO)</li>
            <li>ScreenMap texture rendering</li>
            <li>MSAA and SSAA support</li>
            <li>Per-window gamma correction</li>
            <li>HDR render targets</li>
            <li>Bloom post-processing</li>
            <li>Arbitrary resolution rendering</li>
            <li>Greyscale mode</li>
        </ul>

        <h3>Configuration</h3>
        <div class="code-block">
            <pre><code>// Enable Vulkan renderer
set cl_renderer "vulkan"

// Performance settings
set r_vbo "1"                      // Enable VBO caching
set r_flares "1"                  // Enable flares
set r_ext_texture_filter_anisotropic "16"  // Anisotropic filtering
set r_fbo "1"                     // Enable offscreen rendering
set r_hdr "1"                     // Enable HDR
set r_raytracing "1"              // Enable ray tracing (if supported)</code></pre>
        </div>

        <p>See <a href="rendering/vulkan">Vulkan Renderer</a> for complete documentation.</p>
    </div>

    <div class="section">
        <h2>DirectX 12 Renderer</h2>
        
        <h3>Features</h3>
        <ul>
            <li>Modern DirectX 12 API (feature level 12.0+)</li>
            <li>Triple buffering</li>
            <li>Command lists and descriptor heaps</li>
            <li>Root signatures and PSO caching</li>
            <li>Resource barriers and fence synchronization</li>
            <li>Multiple render targets (MRT)</li>
            <li>32-bit depth buffer</li>
            <li>DXGI swap chain (flip discard)</li>
            <li>D3D12 debug layer (debug builds)</li>
            <li>Automatic feature level detection</li>
            <li>Resource Binding Tiers 1-3 support</li>
            <li><strong>DirectX Raytracing (DXR):</strong> Hardware-accelerated ray tracing</li>
        </ul>

        <h3>Configuration</h3>
        <div class="code-block">
            <pre><code>// Enable DirectX 12 renderer
set cl_renderer "d3d12"

// DXR settings
set r_raytracing "1"              // Enable DXR
set r_dxr_quality "1"             // Quality level (0-2)
set r_dxr_reflections "1"         // Enable reflections
set r_dxr_shadows "1"            // Enable shadow rays</code></pre>
        </div>

        <p>See <a href="rendering/directx12">DirectX 12 Renderer</a> for complete documentation.</p>
    </div>

    <div class="section">
        <h2>OpenGL Renderer</h2>
        
        <h3>Features</h3>
        <ul>
            <li>OpenGL 1.1 compatible</li>
            <li>Uses newer features when available</li>
            <li>High-quality per-pixel dynamic lighting</li>
            <li>Merged lightmaps (atlases)</li>
            <li>VBO caching for static surfaces</li>
            <li>All offscreen rendering features</li>
            <li>Bloom reflection post-processing</li>
        </ul>

        <h3>Configuration</h3>
        <div class="code-block">
            <pre><code>// Enable OpenGL renderer
set cl_renderer "opengl"

// OpenGL-specific settings
set r_vbo "1"                     // Enable VBO caching
set r_dlightMode "1"              // Per-pixel lighting
set r_fbo "1"                     // Enable offscreen rendering</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Physically-Based Rendering (PBR)</h2>
        
        <h3>PBR Pipeline</h3>
        <p>Complete PBR implementation across all renderers:</p>
        <ul>
            <li>Metallic/Roughness workflow</li>
            <li>ORM texture support (Occlusion/Roughness/Metallic)</li>
            <li>Normal mapping with parallax</li>
            <li>Environment mapping and IBL</li>
            <li>HDR rendering</li>
            <li>ACES tonemapping</li>
        </ul>

        <h3>Texture Naming</h3>
        <div class="code-block">
            <pre><code>textures/metal/base.tga          // Base color/albedo
textures/metal/base_normal.tga   // Normal map
textures/metal/base_orm.tga      // ORM (Occlusion/Roughness/Metallic)
textures/metal/base_rmo.tga      // Alternative RMO format
textures/metal/base_spec.tga     // Specular map (legacy)</code></pre>
        </div>

        <p>See <a href="rendering/pbr">PBR Pipeline</a> for complete documentation.</p>
    </div>

    <div class="section">
        <h2>Ray Tracing</h2>
        
        <h3>Vulkan RTX</h3>
        <ul>
            <li>VK_KHR_ray_tracing_pipeline extension</li>
            <li>Hardware-accelerated ray tracing</li>
            <li>Realistic reflections</li>
            <li>Global illumination</li>
            <li>Accurate shadows</li>
        </ul>

        <h3>DirectX DXR</h3>
        <ul>
            <li>DirectX Raytracing API</li>
            <li>DXR Tier 1.1+ support</li>
            <li>GPU-accelerated acceleration structures</li>
            <li>Realistic lighting effects</li>
        </ul>

        <p>See <a href="rendering/ray-tracing">Ray Tracing</a> for complete documentation.</p>
    </div>

    <div class="section">
        <h2>HDR and Tone Mapping</h2>
        
        <h3>HDR Pipeline</h3>
        <ul>
            <li>High dynamic range render targets</li>
            <li>ACES tone mapping</li>
            <li>Bloom post-processing</li>
            <li>Color grading support</li>
            <li>Exposure control</li>
        </ul>

        <h3>Configuration</h3>
        <div class="code-block">
            <pre><code>set r_hdr "1"                     // Enable HDR
set r_tonemap "1"                // Enable tone mapping
set r_bloom "1"                  // Enable bloom
set r_exposure "1.0"             // Exposure value</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Animated Skyboxes</h2>
        
        <h3>Features</h3>
        <ul>
            <li>Frame-based flipbook animation</li>
            <li>Per-side independent animation</li>
            <li>Configurable animation speed</li>
            <li>Up to 24 frames per side</li>
            <li>Backward compatible</li>
        </ul>

        <h3>Shader Syntax</h3>
        <div class="code-block">
            <pre><code>skyParms env/sky 6 1
// 6 = number of frames
// 1 = frames per second</code></pre>
        </div>

        <p>See <a href="rendering/animated-skybox">Animated Skybox</a> for complete documentation.</p>
    </div>

    <div class="section">
        <h2>Performance Optimization</h2>
        
        <h3>Culling Techniques</h3>
        <ul>
            <li><strong>Frustum Culling:</strong> Remove objects outside view</li>
            <li><strong>Occlusion Culling:</strong> Skip hidden surfaces</li>
            <li><strong>LOD System:</strong> Level of detail for models</li>
            <li><strong>Light Culling:</strong> Optimize dynamic lights</li>
        </ul>

        <h3>Optimization CVARs</h3>
        <div class="code-block">
            <pre><code>set r_fastsky "1"                // Fast sky rendering
set r_dynamiclight "1"           // Dynamic lighting
set r_lodbias "0"                // LOD bias
set r_vertexlight "0"            // Vertex lighting (faster)
set r_smp "0"                    // Single-threaded rendering</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Shader System</h2>
        
        <h3>Shader Features</h3>
        <ul>
            <li>Multi-pass rendering</li>
            <li>Blend modes</li>
            <li>Texture stages</li>
            <li>Environment mapping</li>
            <li>Deform vertexes</li>
            <li>Surface sprites</li>
            <li>Custom shader programs</li>
        </ul>

        <p>See <a href="rendering/shaders">Shaders</a> for complete documentation.</p>
    </div>

    <div class="section">
        <h2>Related Documentation</h2>
        <ul>
            <li><a href="rendering/vulkan">Vulkan Renderer</a> - Vulkan details</li>
            <li><a href="rendering/directx12">DirectX 12 Renderer</a> - D3D12 details</li>
            <li><a href="rendering/pbr">PBR Pipeline</a> - PBR documentation</li>
            <li><a href="rendering/ray-tracing">Ray Tracing</a> - Ray tracing guide</li>
            <li><a href="rendering/animated-skybox">Animated Skybox</a> - Skybox animation</li>
            <li><a href="rendering/shaders">Shaders</a> - Shader system</li>
        </ul>
    </div>
</div>

