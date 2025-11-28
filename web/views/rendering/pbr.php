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