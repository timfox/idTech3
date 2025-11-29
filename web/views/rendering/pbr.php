<?php
/**
 * PBR Documentation view
 */
$title = 'PBR Shader Documentation - id Tech 3';
$breadcrumbs = [
    '/rendering' => 'Rendering',
    '/rendering/pbr' => 'PBR'
];
?>

<h1>Physically Based Rendering (PBR) Shader Documentation</h1>

<div class="section">
    <h2>Introduction</h2>
    <p>This documentation covers the PBR shader system implementation. PBR shaders provide more realistic material rendering by simulating physical properties of surfaces.</p>
    
    <div class="feature-list">
        <h3>Renderer Compatibility</h3>
        <p>PBR materials are supported across all renderer backends:</p>
        <ul>
            <li><strong><a href="rendering/vulkan">Vulkan Renderer</a>:</strong> Full PBR support with HDR and advanced post-processing</li>
            <li><strong><a href="rendering/directx12">DirectX 12 Renderer</a>:</strong> Full PBR support with DXR ray tracing capabilities</li>
            <li><strong>OpenGL Renderer:</strong> PBR support with compatibility mode</li>
        </ul>
        <p>All renderers use the same shader syntax and material properties, ensuring consistency across platforms.</p>
    </div>
</div>

<div class="section">
    <h2>Shader Stage Keywords</h2>
    
    <h3>Basic Material Properties</h3>
    <ul class="property-list">
        <li>
            <code class="keyword">specularReflectance &lt;value&gt;</code>
            <div class="description">
                Controls the specular level of the surface:
                <ul>
                    <li>Non-metallic surfaces: 0.04 to 0.08 (4% to 8% reflectance)</li>
                    <li>Metallic surfaces: Higher values</li>
                    <li>Range: [0.0, 1.0] in linear space</li>
                </ul>
            </div>
        </li>
        
        <li>
            <code class="keyword">gloss &lt;value&gt;</code>
            <div class="description">
                Controls surface smoothness:
                <ul>
                    <li>1.0 = Perfect mirror</li>
                    <li>0.0 = Very rough surface</li>
                    <li>Range: [0.0, 1.0]</li>
                </ul>
            </div>
        </li>
        
        <li>
            <code class="keyword">roughness &lt;value&gt;</code>
            <div class="description">
                Alternative to gloss (roughness = 1.0 - gloss)
                <ul>
                    <li>Range: [0.0, 1.0]</li>
                </ul>
            </div>
        </li>
    </ul>
</div>

<div class="section">
    <h2>Normal Mapping and Parallax</h2>
    <ul class="property-list">
        <li>
            <code class="keyword">parallaxDepth &lt;value&gt;</code>
            <div class="description">
                Controls parallax correction depth when using normalHeightMap
                <ul>
                    <li>Range: [0.0, 1.0]</li>
                </ul>
            </div>
        </li>
        
        <li>
            <code class="keyword">parallaxBias &lt;value&gt;</code>
            <div class="description">
                Adjusts parallax correction offset
                <ul>
                    <li>Helps prevent gaps between surfaces</li>
                    <li>Range: [0.0, 1.0]</li>
                </ul>
            </div>
        </li>
        
        <li>
            <code class="keyword">normalScale &lt;x&gt; &lt;y&gt; [height]</code>
            <div class="description">
                Scales normal map values and can flip channels
            </div>
        </li>
    </ul>
</div>

<div class="section">
    <h2>Texture Maps</h2>
    <ul class="property-list">
        <li>
            <code class="keyword">normalMap &lt;name&gt;</code>
            <div class="description">Standard normal map</div>
        </li>
            
        <li>
            <code class="keyword">normalHeightMap &lt;name&gt;</code>
            <div class="description">Normal map with height information in alpha channel</div>
        </li>
            
        <li>
            <code class="keyword">specMap/specularMap &lt;name&gt;</code>
            <div class="description">Specular map with gloss in alpha channel</div>
        </li>
    </ul>
</div>

<div class="section">
    <h2>Material Maps</h2>
    <?php include __DIR__ . '/../partials/material-maps.php'; ?>
</div>

<div class="section">
    <h2>Example Shader</h2>
    <div class="code-example">
        <pre><code>textures/rend2/Gold
{
    {
        map $lightmap
    }
    {
        map $whiteimage
        specMap $whiteimage
        rgbGen const ( 0.0 0.0 0.0 )
        specularScale 0.8 0.466807 0.254887
        roughness 0.5
        blendfunc GL_DST_COLOR GL_ZERO
    }
}</code></pre>
        <div class="example-description">
            <p>This example creates a gold material using PBR properties without textures. The specularScale values are set to create a gold-like appearance, and the roughness is set to 0.5 for a semi-polished look.</p>
        </div>
    </div>
</div>

<div class="section">
    <h2>Configuration</h2>
    <p>Enable PBR rendering in your configuration:</p>
    <div class="code-block">
        <pre><code># Enable PBR materials
seta r_pbr "1"

# PBR quality settings (if available)
seta r_pbr_quality "2"  # 0=Low, 1=Medium, 2=High</code></pre>
    </div>
</div>

<div class="section">
    <h2>Best Practices</h2>
    <ul>
        <li>Use proper material maps (albedo, normal, roughness/metallic) for best results</li>
        <li>Keep specularReflectance values realistic (0.04-0.08 for non-metals)</li>
        <li>Combine with HDR rendering and tonemapping for realistic lighting</li>
        <li>Test materials in different lighting conditions</li>
        <li>Use the <a href="imgui">ImGui debug overlays</a> to inspect material properties in real-time</li>
    </ul>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="rendering/vulkan">Vulkan Renderer</a> - Recommended renderer for PBR</li>
        <li><a href="rendering/directx12">DirectX 12 Renderer</a> - Windows renderer with DXR</li>
        <li><a href="tonemapping">HDR and ACES Tonemapping</a> - Essential for realistic PBR lighting</li>
        <li><a href="luts">3D LUT Color Grading</a> - Post-processing for PBR materials</li>
        <li><a href="renderer/pbr-pipeline">PBR Pipeline</a> - Technical implementation details</li>
        <li><a href="development/map-making">Map Making</a> - Creating maps with PBR materials</li>
        <li><a href="imgui">ImGui Debug Overlays</a> - Real-time material inspection</li>
    </ul>
</div> 