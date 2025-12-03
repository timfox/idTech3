<?php
/**
 * PBR Material Creation Guide
 */
$title = 'PBR Material Creation Guide - id Tech 3 Documentation';
$breadcrumbs = [
    '/rendering' => 'Rendering',
    '/rendering/pbr-materials' => 'PBR Material Creation'
];
?>

<div class="content-section">
    <h1>PBR Material Creation Guide</h1>
    
    <blockquote>
        <strong>Complete Guide:</strong> This guide explains how to create Physically Based Rendering (PBR) materials for use with the id Tech 3 engine's Vulkan renderer. Learn texture naming conventions, workflows, and best practices for creating realistic materials.
    </blockquote>

    <div class="section">
        <h2>Overview</h2>
        <p>PBR (Physically Based Rendering) provides realistic material rendering by simulating how light interacts with surfaces. The id Tech 3 engine supports two PBR workflows:</p>
        
        <div class="feature-list">
            <h3>Supported Workflows</h3>
            <ul>
                <li><strong>Metallic/Roughness Workflow</strong> (recommended) - Uses ORM textures</li>
                <li><strong>Specular/Gloss Workflow</strong> (alternative) - Uses separate specular maps</li>
            </ul>
        </div>
    </div>

    <div class="section">
        <h2>Prerequisites</h2>
        <p>Before creating PBR materials, ensure the following settings are configured:</p>
        
        <div class="code-block">
            <pre><code>// Enable PBR rendering
set r_pbr 1

// Framebuffer objects (REQUIRED)
set r_fbo 1

// Use Vulkan renderer
set cl_renderer vulkan

// Vertex buffer objects (recommended)
set r_vbo 1</code></pre>
        </div>

        <div class="note">
            <strong>Important:</strong> <code>r_fbo 1</code> is <strong>REQUIRED</strong> for PBR rendering. Without framebuffer objects enabled, PBR materials will not work correctly.
        </div>
    </div>

    <div class="section">
        <h2>Texture Naming Conventions</h2>
        <p>The engine automatically detects PBR textures based on naming conventions. Place textures in <code>textures/</code> or <code>textures/pbr/</code> directories.</p>
        
        <h3>Base Texture</h3>
        <ul>
            <li><strong>File:</strong> <code>texturename.tga</code> (or <code>.jpg</code>, <code>.png</code>)</li>
            <li><strong>Description:</strong> The main diffuse/albedo texture</li>
            <li><strong>Format:</strong> RGB color values (sRGB color space)</li>
            <li><strong>Example:</strong> <code>brick_wall.tga</code></li>
        </ul>

        <h3>Normal Map</h3>
        <ul>
            <li><strong>File:</strong> <code>texturename_normal.tga</code></li>
            <li><strong>Description:</strong> Surface detail and bump mapping</li>
            <li><strong>Format:</strong> RGB normal map (standard tangent-space normals)</li>
            <li><strong>Channels:</strong>
                <ul>
                    <li>R: X normal component</li>
                    <li>G: Y normal component</li>
                    <li>B: Z normal component (usually 1.0)</li>
                </ul>
            </li>
            <li><strong>Example:</strong> <code>brick_wall_normal.tga</code></li>
            <li><strong>CVARs:</strong> <code>r_baseNormalX</code> and <code>r_baseNormalY</code> control normal map intensity</li>
        </ul>

        <h3>Parallax Mapping</h3>
        <p>Normal maps support parallax occlusion mapping:</p>
        <ul>
            <li><strong>CVAR:</strong> <code>r_baseParallax</code> controls parallax depth (default: 0.05)</li>
            <li>Higher values create more pronounced depth effect</li>
        </ul>
    </div>

    <div class="section">
        <h2>Metallic/Roughness Workflow (Recommended)</h2>
        <p>This is the most common PBR workflow and uses an ORM (Occlusion/Roughness/Metallic) texture.</p>
        
        <h3>ORM Texture</h3>
        <ul>
            <li><strong>File:</strong> <code>texturename_orm.tga</code> or <code>texturename_rmo.tga</code></li>
            <li><strong>Description:</strong> Combined material properties</li>
            <li><strong>Format:</strong> RGB texture with specific channel meanings</li>
        </ul>

        <h3>ORM Format (Occlusion/Roughness/Metallic)</h3>
        <ul>
            <li><strong>R Channel:</strong> Ambient Occlusion (0 = fully occluded, 1 = no occlusion)</li>
            <li><strong>G Channel:</strong> Roughness (0 = smooth/mirror-like, 1 = rough/matte)</li>
            <li><strong>B Channel:</strong> Metallic (0 = dielectric/non-metal, 1 = metal)</li>
        </ul>

        <h3>RMO Format (Roughness/Metallic/Occlusion)</h3>
        <ul>
            <li><strong>R Channel:</strong> Roughness</li>
            <li><strong>G Channel:</strong> Metallic</li>
            <li><strong>B Channel:</strong> Ambient Occlusion</li>
        </ul>

        <p><strong>Example:</strong> <code>brick_wall_orm.tga</code></p>

        <h3>Material Properties</h3>
        
        <h4>Metallic Surfaces</h4>
        <ul>
            <li><strong>Metallic = 1.0:</strong> Pure metals (gold, silver, chrome)</li>
            <li><strong>Metallic = 0.0:</strong> Non-metals (wood, stone, fabric)</li>
            <li><strong>Metallic = 0.5:</strong> Mixed materials (rusted metal, painted metal)</li>
        </ul>

        <h4>Roughness Values</h4>
        <ul>
            <li><strong>Roughness = 0.0:</strong> Mirror-like surfaces (chrome, water)</li>
            <li><strong>Roughness = 0.1-0.3:</strong> Smooth surfaces (polished wood, ceramic)</li>
            <li><strong>Roughness = 0.4-0.7:</strong> Average surfaces (concrete, brick)</li>
            <li><strong>Roughness = 0.8-1.0:</strong> Rough surfaces (fabric, rough stone)</li>
        </ul>

        <h4>Ambient Occlusion</h4>
        <ul>
            <li>Darkens crevices and areas where light doesn't reach</li>
            <li>Adds depth and realism to materials</li>
            <li>Usually baked from 3D models or painted manually</li>
        </ul>
    </div>

    <div class="section">
        <h2>Specular/Gloss Workflow (Alternative)</h2>
        <p>This workflow uses separate specular and gloss maps.</p>
        
        <h3>Specular Map</h3>
        <ul>
            <li><strong>File:</strong> <code>texturename_spec.tga</code></li>
            <li><strong>Description:</strong> Specular color and intensity</li>
            <li><strong>Format:</strong> RGB specular color</li>
            <li><strong>Usage:</strong> Defines the color and intensity of reflections</li>
            <li><strong>CVAR:</strong> <code>r_baseSpecular</code> controls base specular value (default: 0.04)</li>
        </ul>

        <h3>Gloss Map</h3>
        <ul>
            <li>Usually stored in the alpha channel of the specular map</li>
            <li><strong>Gloss = 1.0:</strong> Smooth, reflective surface</li>
            <li><strong>Gloss = 0.0:</strong> Rough, matte surface</li>
        </ul>
    </div>

    <div class="section">
        <h2>Texture Formats</h2>
        
        <h3>Supported Formats</h3>
        <ul>
            <li><strong>TGA</strong> (recommended for best quality)</li>
            <li><strong>JPG</strong> (smaller file size, lossy compression)</li>
            <li><strong>PNG</strong> (lossless compression, supports alpha)</li>
        </ul>

        <h3>Color Space</h3>
        <ul>
            <li>Textures should be in <strong>sRGB color space</strong></li>
            <li>The engine handles sRGB to linear conversion automatically</li>
            <li>Normal maps should be in linear space (not sRGB)</li>
        </ul>

        <h3>Resolution Guidelines</h3>
        <ul>
            <li><strong>Base textures:</strong> Match your target resolution (512x512, 1024x1024, 2048x2048)</li>
            <li><strong>Normal maps:</strong> Same resolution as base texture</li>
            <li><strong>ORM maps:</strong> Same resolution as base texture (can be lower quality)</li>
            <li><strong>Maximum texture size:</strong> 2048x2048 (configurable via <code>r_maxTextureSize</code>)</li>
        </ul>
    </div>

    <div class="section">
        <h2>Creating PBR Textures</h2>
        
        <h3>Method 1: From 3D Software</h3>
        <ol>
            <li>Create materials in Blender, Substance Painter, or similar</li>
            <li>Export textures using the ORM workflow</li>
            <li>Ensure proper channel assignments:
                <ul>
                    <li>Ambient Occlusion → R channel</li>
                    <li>Roughness → G channel</li>
                    <li>Metallic → B channel</li>
                </ul>
            </li>
        </ol>

        <h3>Method 2: Manual Creation</h3>
        <ol>
            <li><strong>Base Texture:</strong> Create or source your main color texture</li>
            <li><strong>Normal Map:</strong> Generate from height map or paint manually</li>
            <li><strong>ORM Map:</strong>
                <ul>
                    <li>Paint or generate ambient occlusion</li>
                    <li>Create roughness map (white = rough, black = smooth)</li>
                    <li>Create metallic map (white = metal, black = non-metal)</li>
                    <li>Combine into RGB channels</li>
                </ul>
            </li>
        </ol>

        <h3>Method 3: Using Texture Tools</h3>
        <ul>
            <li><strong>Substance Designer/Painter:</strong> Industry standard for PBR materials</li>
            <li><strong>Materialize:</strong> Free tool for generating PBR textures</li>
            <li><strong>xNormal:</strong> Generate normal maps from height maps</li>
            <li><strong>GIMP/Photoshop:</strong> Manual texture creation and editing</li>
        </ul>
    </div>

    <div class="section">
        <h2>Shader Configuration</h2>
        <p>The engine automatically handles PBR shaders, but you can customize materials via CVARs:</p>
        
        <div class="code-block">
            <pre><code>// Normal map intensity
set r_baseNormalX "1.0"      // X-axis intensity
set r_baseNormalY "1.0"      // Y-axis intensity

// Parallax mapping depth
set r_baseParallax "0.05"    // Depth effect (0.0 = disabled)

// Base specular value (for non-metals)
set r_baseSpecular "0.04"    // Standard dielectric specular</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Environment Mapping</h2>
        <p>For realistic reflections, enable cube mapping:</p>
        
        <div class="code-block">
            <pre><code>set r_cubeMapping "1"</code></pre>
        </div>

        <p>This creates environment reflections based on the surrounding scene. The engine generates cubemaps automatically or you can provide custom ones.</p>

        <h3>Cubemap Settings</h3>
        <ul>
            <li><strong>Irradiance Size:</strong> 64x64 (for ambient lighting)</li>
            <li><strong>Reflection Size:</strong> 256x256 (for reflections)</li>
            <li>Configured in renderer code, not via CVARs</li>
        </ul>
    </div>

    <div class="section">
        <h2>HDR Rendering</h2>
        <p>Enable HDR for better color accuracy:</p>
        
        <div class="code-block">
            <pre><code>set r_hdr "1"</code></pre>
        </div>

        <p>HDR (High Dynamic Range) provides:</p>
        <ul>
            <li>Better color accuracy</li>
            <li>Reduced color banding</li>
            <li>Improved lighting quality</li>
            <li>Required for bloom effects</li>
        </ul>
    </div>

    <div class="section">
        <h2>Material Examples</h2>
        
        <h3>Example 1: Brick Wall</h3>
        <div class="code-block">
            <pre><code>brick_wall.tga          // Base color texture
brick_wall_normal.tga  // Normal map for surface detail
brick_wall_orm.tga     // ORM map:
                       //   R: Ambient occlusion in mortar lines
                       //   G: Roughness (brick is fairly rough)
                       //   B: Metallic (0.0 - brick is non-metallic)</code></pre>
        </div>

        <h3>Example 2: Metal Surface</h3>
        <div class="code-block">
            <pre><code>metal_plate.tga        // Base color (slightly tinted)
metal_plate_normal.tga // Normal map (scratches, dents)
metal_plate_orm.tga    // ORM map:
                       //   R: Ambient occlusion
                       //   G: Roughness (0.1-0.3 for polished metal)
                       //   B: Metallic (1.0 - pure metal)</code></pre>
        </div>

        <h3>Example 3: Wood Surface</h3>
        <div class="code-block">
            <pre><code>wood_plank.tga         // Base color (wood grain)
wood_plank_normal.tga  // Normal map (grain detail)
wood_plank_orm.tga     // ORM map:
                       //   R: Ambient occlusion in grain
                       //   G: Roughness (0.3-0.5 for wood)
                       //   B: Metallic (0.0 - wood is non-metallic)</code></pre>
        </div>
    </div>

    <div class="section">
        <h2>Testing Your Materials</h2>
        <ol>
            <li><strong>Load the mod:</strong> <code>./quake3e +set fs_game mymod</code></li>
            <li><strong>Enable PBR:</strong> Verify <code>r_pbr</code> shows <code>1</code></li>
            <li><strong>Load a test map:</strong> Create or use a map with your textures</li>
            <li><strong>Check console:</strong> Look for texture loading messages</li>
            <li><strong>Adjust settings:</strong> Use CVARs to fine-tune appearance</li>
        </ol>
    </div>

    <div class="section">
        <h2>Troubleshooting</h2>
        
        <h3>Textures Not Loading</h3>
        <ul>
            <li>Check file naming conventions match exactly</li>
            <li>Verify texture format is supported (TGA, JPG, PNG)</li>
            <li>Check console for error messages</li>
            <li>Ensure textures are in correct directory (<code>textures/</code> or <code>textures/pbr/</code>)</li>
        </ul>

        <h3>PBR Not Working</h3>
        <ul>
            <li>Verify <code>r_pbr 1</code> is set</li>
            <li>Ensure <code>r_fbo 1</code> is enabled (REQUIRED)</li>
            <li>Check that you're using Vulkan renderer: <code>cl_renderer vulkan</code></li>
            <li>Verify texture naming matches conventions</li>
        </ul>

        <h3>Materials Look Wrong</h3>
        <ul>
            <li>Check ORM channel assignments (R/G/B)</li>
            <li>Verify normal map is in correct format (not sRGB)</li>
            <li>Adjust <code>r_baseNormalX/Y</code> for normal map intensity</li>
            <li>Check <code>r_baseSpecular</code> for specular workflow</li>
        </ul>

        <h3>Performance Issues</h3>
        <ul>
            <li>Reduce texture resolution if needed</li>
            <li>Disable cube mapping: <code>set r_cubeMapping 0</code></li>
            <li>Disable HDR: <code>set r_hdr 0</code></li>
            <li>Lower parallax depth: <code>set r_baseParallax 0.02</code></li>
        </ul>
    </div>

    <div class="section">
        <h2>Advanced Features</h2>
        
        <h3>Custom Shaders</h3>
        <ul>
            <li>Place custom shader files in <code>shaders/</code> directory</li>
            <li>Use Quake III shader syntax</li>
            <li>PBR shaders are handled automatically by the renderer</li>
        </ul>

        <h3>Texture Swizzling</h3>
        <p>The engine supports texture channel swizzling for different ORM formats:</p>
        <ul>
            <li>ORM (Occlusion/Roughness/Metallic) - default</li>
            <li>RMO (Roughness/Metallic/Occlusion)</li>
            <li>Other combinations via renderer configuration</li>
        </ul>

        <h3>Material Variants</h3>
        <p>Create multiple material variants:</p>
        <ul>
            <li><code>texture_clean_orm.tga</code> - Clean version</li>
            <li><code>texture_dirty_orm.tga</code> - Worn version</li>
            <li><code>texture_wet_orm.tga</code> - Wet version (lower roughness)</li>
        </ul>
    </div>

    <div class="section">
        <h2>Resources</h2>
        <ul>
            <li><a href="https://learnopengl.com/PBR/Theory" target="_blank">PBR Theory Guide</a> - Learn OpenGL PBR tutorial</li>
            <li><a href="https://docs.substance3d.com/sddoc/pbr-workflows-172820612.html" target="_blank">Substance PBR Guide</a> - Substance 3D documentation</li>
            <li><a href="https://www.boundingboxsoftware.com/materialize/" target="_blank">Materialize Tool</a> - Free PBR texture generator</li>
            <li><a href="rendering/pbr">PBR Shader Documentation</a> - Engine PBR shader reference</li>
        </ul>
    </div>

    <div class="section">
        <h2>Summary</h2>
        <p>Creating PBR materials requires:</p>
        <ol>
            <li>Base color texture (albedo)</li>
            <li>Normal map for surface detail</li>
            <li>ORM map (Occlusion/Roughness/Metallic)</li>
            <li>Proper naming conventions</li>
            <li>Correct texture formats and color spaces</li>
        </ol>
        
        <p>Follow the naming conventions, enable PBR rendering, and your materials will automatically use the PBR pipeline for realistic lighting and reflections.</p>
    </div>

    <div class="section">
        <h2>Related Documentation</h2>
        <ul>
            <li><a href="rendering/pbr">PBR Shader Documentation</a> - Shader syntax and parameters</li>
            <li><a href="rendering/vulkan">Vulkan Renderer</a> - Renderer details</li>
            <li><a href="rendering/ray-tracing">Ray Tracing</a> - Hardware-accelerated ray tracing</li>
            <li><a href="renderer/pbr-pipeline">PBR Pipeline</a> - Technical implementation details</li>
        </ul>
    </div>
</div>

