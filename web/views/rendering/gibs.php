<?php
$title = "Global Illumination Based on Surfels (GIBS)";
?>

<h1>Global Illumination Based on Surfels (GIBS)</h1>

<p>GIBS provides efficient real-time global illumination by caching indirect lighting in surfels (surface elements) distributed throughout the scene. This approach is more efficient than pure path tracing while providing high-quality indirect lighting.</p>

<h2>Overview</h2>

<p>GIBS (Global Illumination Based on Surfels) is a real-time global illumination system that uses surfels (surface elements) to cache indirect lighting information. Based on the SIGGRAPH 2021 paper and the SurfelGI reference implementation, GIBS provides:</p>

<ul>
    <li>Real-time indirect diffuse illumination calculation</li>
    <li>Surface element (surfel) based lighting system</li>
    <li>Hardware ray tracing integration</li>
    <li>Dynamic lighting without pre-baking</li>
    <li>No special mesh or UV requirements</li>
</ul>

<h2>How It Works</h2>

<h3>Surfel System</h3>
<p>Surfels are small surface elements that store cached indirect lighting information:</p>

<pre><code>struct Surfel {
    vec3 position;      // World space position
    vec3 normal;       // Surface normal
    float radius;      // Surfel radius
    vec3 irradiance;   // Cached indirect irradiance (RGB)
    float confidence;  // Confidence value (0-1)
    uint age;          // Age in frames
    uint flags;        // Surfel flags
};</code></pre>

<h3>Workflow</h3>
<ol>
    <li><strong>Surfel Spawning</strong>: Surfels are spawned on geometric surfaces based on G-buffer data</li>
    <li><strong>Irradiance Update</strong>: Each frame, a subset of surfels are updated using ray tracing to sample indirect lighting</li>
    <li><strong>Surfel Sampling</strong>: During rendering, nearby surfels are sampled to add indirect lighting to the scene</li>
    <li><strong>Lifecycle Management</strong>: Stale surfels are removed, and new ones are spawned as needed</li>
</ol>

<h2>Files</h2>

<h3>Core Implementation</h3>
<ul>
    <li><code>src/renderervk/vk_gibs.h</code> - Header file with data structures and API declarations</li>
    <li><code>src/renderervk/vk_gibs.c</code> - Main implementation file with initialization, update, and shutdown logic</li>
</ul>

<h3>Shaders</h3>
<ul>
    <li><code>src/renderervk/shaders/glsl/gibs_surfel.glsl</code> - Surfel data structure and helper functions</li>
    <li><code>src/renderervk/shaders/glsl/gibs_spawn.comp</code> - Compute shader for spawning surfels on surfaces</li>
    <li><code>src/renderervk/shaders/glsl/gibs_update.comp</code> - Compute shader for updating surfel irradiance via ray tracing</li>
    <li><code>src/renderervk/shaders/glsl/gibs_sampling.glsl</code> - Helper functions for sampling surfels in PBR shaders</li>
</ul>

<h2>Configuration</h2>

<h3>CVars</h3>
<ul>
    <li><code>r_gibs</code> - Enable/disable GIBS (0/1, default: 0)</li>
    <li><code>r_gibs_surfelRadius</code> - Surfel radius in world units (default: 0.1)</li>
    <li><code>r_gibs_maxSurfels</code> - Maximum number of surfels (default: 1048576 = 1M)</li>
    <li><code>r_gibs_updateRate</code> - Update frequency in frames (default: 4, lower = more accurate but slower)</li>
    <li><code>r_gibs_intensity</code> - Intensity multiplier (default: 1.0, range: 0.0-2.0)</li>
    <li><code>r_gibs_samples</code> - Samples per surfel update (default: 16, range: 1-64, higher = smoother but slower)</li>
</ul>

<h3>Enabling GIBS</h3>
<ol>
    <li>Enable ray tracing first:
        <pre><code>\r_raytracing 1
\r_fbo 1</code></pre>
    </li>
    <li>Enable GIBS:
        <pre><code>\r_gibs 1</code></pre>
    </li>
    <li>Adjust settings as needed:
        <pre><code>\r_gibs_surfelRadius 0.1
\r_gibs_maxSurfels 1048576
\r_gibs_updateRate 4
\r_gibs_intensity 1.0
\r_gibs_samples 16</code></pre>
    </li>
</ol>

<h2>Integration</h2>

<h3>Initialization</h3>
<p>GIBS is initialized after the ray tracing system:</p>
<pre><code>#ifdef USE_VULKAN_RAY_TRACING
    vk_rt_init();
    vk_gibs_init(); // Initialize GIBS after ray tracing
#endif</code></pre>

<h3>Per-Frame Update</h3>
<p>GIBS is updated each frame in <code>vk_begin_frame()</code>:</p>
<pre><code>#ifdef USE_VULKAN_RAY_TRACING
    if (vk.gibs.enabled && vk.gibs.initialized) {
        vk_gibs_update();
    }
#endif</code></pre>

<h3>Shutdown</h3>
<p>GIBS is shut down before ray tracing cleanup:</p>
<pre><code>#ifdef USE_VULKAN_RAY_TRACING
    vk_gibs_shutdown(); // Shutdown GIBS before ray tracing
    vk_rt_shutdown();
#endif</code></pre>

<h2>Shader Integration</h2>

<h3>PBR Fragment Shader</h3>
<p>To use GIBS in your PBR shader, include the sampling helper:</p>
<pre><code>#include "gibs_sampling.glsl"

// In your lighting calculation
vec3 indirectLighting = sampleGIBSIrradiance(worldPos, normal);
finalColor += indirectLighting * albedo;
</code></pre>

<h3>Surfel Sampling</h3>
<p>The <code>sampleGIBSIrradiance()</code> function:</p>
<ul>
    <li>Finds nearby surfels based on world position</li>
    <li>Weights contributions by distance and normal alignment</li>
    <li>Applies confidence weighting</li>
    <li>Returns blended indirect irradiance</li>
</ul>

<h2>Performance Considerations</h2>

<h3>Surfel Count</h3>
<p>More surfels provide better quality but increase memory and computation cost. The default maximum is 1 million surfels, which provides good quality for most scenes.</p>

<h3>Update Rate</h3>
<p>The update rate controls how frequently surfels are updated. Lower values (e.g., 2-4 frames) provide faster adaptation to lighting changes but higher computational cost. Higher values (e.g., 8-16 frames) improve performance but may show delayed lighting updates.</p>

<h3>Samples Per Surfel</h3>
<p>More samples per surfel provide smoother indirect lighting but slower updates. The default of 16 samples provides a good balance between quality and performance.</p>

<h3>Spatial Lookup</h3>
<p>The current implementation uses linear search for surfel lookup (O(n)). For better performance with many surfels, consider implementing a spatial acceleration structure such as:</p>
<ul>
    <li>Grid-based spatial hash</li>
    <li>Octree</li>
    <li>BVH (Bounding Volume Hierarchy)</li>
</ul>

<h2>Implementation Status</h2>

<h3>✅ Completed</h3>
<ul>
    <li>Data structures and storage system</li>
    <li>Buffer allocation and management</li>
    <li>CVar system</li>
    <li>Basic initialization and shutdown</li>
    <li>Compute shader code (spawn and update)</li>
    <li>Surfel sampling helper functions</li>
    <li>Frame update logic</li>
</ul>

<h3>⚠️ Partially Implemented</h3>
<ul>
    <li>Pipeline creation (shader modules need to be compiled first)</li>
    <li>Descriptor set creation and binding</li>
    <li>Uniform buffer updates (needs proper matrix inversion)</li>
    <li>Surfel spawning integration (needs G-buffer access)</li>
</ul>

<h3>❌ Not Yet Implemented</h3>
<ul>
    <li>Shader compilation (shaders need to be compiled to SPIR-V)</li>
    <li>Pipeline creation functions (<code>vk_gibs_create_pipelines()</code>)</li>
    <li>Descriptor set layout creation</li>
    <li>Integration with PBR fragment shader (add surfel sampling)</li>
    <li>Spatial acceleration structure for efficient surfel lookup</li>
    <li>Surfel culling and removal of stale surfels</li>
    <li>Proper matrix inversion for view/projection matrices</li>
</ul>

<h2>Next Steps</h2>

<h3>1. Shader Compilation</h3>
<p>The compute shaders need to be compiled to SPIR-V:</p>
<pre><code>cd src/renderervk/shaders
glslc gibs_spawn.comp -o gibs_spawn.spv
glslc gibs_update.comp -o gibs_update.spv
</code></pre>

<h3>2. Pipeline Creation</h3>
<p>Implement <code>vk_gibs_create_pipelines()</code> function to:</p>
<ul>
    <li>Load compiled shader modules</li>
    <li>Create descriptor set layouts</li>
    <li>Create compute pipelines</li>
    <li>Create and bind descriptor sets</li>
</ul>

<h3>3. PBR Integration</h3>
<p>Add surfel sampling to the PBR fragment shader:</p>
<ul>
    <li>Include <code>gibs_sampling.glsl</code></li>
    <li>Call <code>sampleGIBSIrradiance()</code> in lighting calculation</li>
    <li>Blend with existing indirect lighting</li>
</ul>

<h3>4. Spatial Acceleration</h3>
<p>Implement a spatial data structure (e.g., grid or BVH) for efficient surfel lookup instead of linear search.</p>

<h3>5. Matrix Utilities</h3>
<p>Add proper matrix inversion functions for view/projection matrices.</p>

<h2>Requirements</h2>

<ul>
    <li>Hardware ray tracing support (Vulkan ray tracing extensions)</li>
    <li>Vulkan renderer enabled</li>
    <li>FBO (Frame Buffer Object) support enabled</li>
</ul>

<h2>Best Practices</h2>

<ul>
    <li>Works best with static or slowly moving geometry</li>
    <li>Indirect lighting updates are distributed across multiple frames for performance</li>
    <li>Surfel confidence decays over time to handle dynamic lighting changes</li>
    <li>Adjust surfel radius based on scene scale</li>
    <li>Use lower update rates for static scenes, higher rates for dynamic scenes</li>
</ul>

<h2>References</h2>

<ul>
    <li><a href="https://github.com/W298/SurfelGI">SurfelGI GitHub Repository</a></li>
    <li>SIGGRAPH 2021: "Global Illumination Based on Surfels"</li>
    <li>Falcor Framework (reference implementation)</li>
</ul>

