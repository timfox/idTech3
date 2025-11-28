<?php $title = "PBR Shader Documentation"; ?>

<div class='content-section'>
    <h1>Physically Based Rendering (PBR) Shader Documentation</h1>
    
    <h2>Introduction</h2>
    <p>This documentation covers the PBR shader system implementation. PBR shaders provide more realistic material rendering by simulating physical properties of surfaces.</p>
    
    <h2>Shader Stage Keywords</h2>
    
    <h3>Basic Material Properties</h3>
    <ul>
        <li><span class='keyword'>specularReflectance &lt;value&gt;</span><br>
            Controls the specular level of the surface:
            <ul>
                <li>Non-metallic surfaces: 0.04 to 0.08 (4% to 8% reflectance)</li>
                <li>Metallic surfaces: Higher values</li>
                <li>Range: [0.0, 1.0] in linear space</li>
            </ul>
        </li>
        <li><span class='keyword'>gloss &lt;value&gt;</span><br>
            Controls surface smoothness:
            <ul>
                <li>1.0 = Perfect mirror</li>
                <li>0.0 = Very rough surface</li>
                <li>Range: [0.0, 1.0]</li>
            </ul>
        </li>
        <li><span class='keyword'>roughness &lt;value&gt;</span><br>
            Alternative to gloss (roughness = 1.0 - gloss)
            <ul>
                <li>Range: [0.0, 1.0]</li>
            </ul>
        </li>
    </ul>
    
    <h3>Normal Mapping and Parallax</h3>
    <ul>
        <li><span class='keyword'>parallaxDepth &lt;value&gt;</span><br>
            Controls parallax correction depth when using normalHeightMap
            <ul>
                <li>Range: [0.0, 1.0]</li>
            </ul>
        </li>
        <li><span class='keyword'>parallaxBias &lt;value&gt;</span><br>
            Adjusts parallax correction offset
            <ul>
                <li>Helps prevent gaps between surfaces</li>
                <li>Range: [0.0, 1.0]</li>
            </ul>
        </li>
        <li><span class='keyword'>normalScale &lt;x&gt; &lt;y&gt; [height]</span><br>
            Scales normal map values and can flip channels
        </li>
    </ul>
    
    <h3>Texture Maps</h3>
    <ul>
        <li><span class='keyword'>normalMap &lt;name&gt;</span><br>
            Standard normal map
        </li>
        <li><span class='keyword'>normalHeightMap &lt;name&gt;</span><br>
            Normal map with height information in alpha channel
        </li>
        <li><span class='keyword'>specMap/specularMap &lt;name&gt;</span><br>
            Specular map with gloss in alpha channel
        </li>
    </ul>
    
    <h3>Material Maps</h3>
    <ul>
        <li><span class='keyword'>rmoMap/rmosMap &lt;name&gt;</span><br>
            Packed material map:
            <ul>
                <li><strong>Red:</strong> Roughness</li>
                <li><strong>Green:</strong> Metalness</li>
                <li><strong>Blue:</strong> Occlusion</li>
                <li><strong>Alpha:</strong> Specular scale (rmosMap only)</li>
            </ul>
        </li>
        <li><span class='keyword'>ormMap/ormsMap &lt;name&gt;</span><br>
            Packed material map:
            <ul>
                <li><strong>Red:</strong> Occlusion</li>
                <li><strong>Green:</strong> Roughness</li>
                <li><strong>Blue:</strong> Metalness</li>
                <li><strong>Alpha:</strong> Specular scale (ormsMap only)</li>
            </ul>
        </li>
        <li><span class='keyword'>moxrMap/mosrMap &lt;name&gt;</span><br>
            Packed material map:
            <ul>
                <li><strong>Red:</strong> Metalness</li>
                <li><strong>Green:</strong> Occlusion</li>
                <li><strong>Blue:</strong> Specular scale (mosrMap only)</li>
                <li><strong>Alpha:</strong> Roughness</li>
            </ul>
        </li>
    </ul>
    
    <h3>Special Effects</h3>
    <ul>
        <li><span class='keyword'>cloth</span><br>
            Changes BRDF to simulate fabric materials
        </li>
    </ul>
    
    <h2>Example Shader</h2>
    <div class='example'>
        <pre>textures/rend2/Gold
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
}</pre>
    </div>
    
    <p>This example creates a gold material using PBR properties without textures. The <span class="keyword">specularScale</span> values are set to create a gold-like appearance, and the <span class="keyword">roughness</span> is set to 0.5 for a semi-polished look.</p>
    
    <h2>Advanced Features</h2>
    
    <h3>Environment Mapping</h3>
    <p>PBR shaders support environment mapping for realistic reflections:</p>
    
    <div class='example'>
        <pre>textures/rend2/MetalSurface
{
    {
        map textures/environments/sky.tga
        tcGen environment
        specularScale 1.0 1.0 1.0
        roughness 0.1
    }
}</pre>
    </div>
    
    <h3>Performance Considerations</h3>
    <ul>
        <li><strong>Texture Resolution:</strong> Use appropriate resolution for target hardware</li>
        <li><strong>Packed Maps:</strong> Combine multiple maps into single textures when possible</li>
        <li><strong>LOD Systems:</strong> Implement level-of-detail for distant objects</li>
        <li><strong>Shader Complexity:</strong> Balance visual quality with performance requirements</li>
    </ul>
    
    <blockquote>
        <strong>Performance Tip:</strong> Use packed material maps (like rmoMap) to reduce texture memory usage and improve cache efficiency.
    </blockquote>
</div>

<style>
.content-section {
    color: white;
}

.keyword {
    background-color: rgba(52, 152, 219, 0.3);
    padding: 2px 6px;
    border-radius: 4px;
    font-family: monospace;
    font-weight: bold;
}

.example {
    background-color: rgba(0, 0, 0, 0.3);
    padding: 15px;
    border-radius: 8px;
    margin: 15px 0;
    border-left: 4px solid #3498db;
}

.example pre {
    color: #e6e6e6;
    font-family: 'Courier New', monospace;
    margin: 0;
    white-space: pre-wrap;
}

h1, h2, h3 {
    color: #3498db;
    text-shadow: 0 1px 2px rgba(0, 0, 0, 0.3);
}

ul {
    padding-left: 20px;
}

li {
    margin-bottom: 8px;
}
</style>
