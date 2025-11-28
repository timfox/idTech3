<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PBR Shader Tutorial</title>
    <style>
        @font-face {
            font-family: 'Fusion';
            src: url('/fonts/fusion.ttf') format('truetype');
        }
        body {
            font-family: Arial, sans-serif;
            line-height: 1.6;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
            color: #333;
        }
        h1, h2, h3, h4 {
            font-family: 'Fusion', Arial, sans-serif;
            color: #2c3e50;
        }
        pre {
            background-color: #f8f9fa;
            padding: 15px;
            border-radius: 5px;
            overflow-x: auto;
        }
        code {
            font-family: 'Courier New', Courier, monospace;
            background-color: #f8f9fa;
            padding: 2px 4px;
            border-radius: 3px;
        }
        .note {
            background-color: #e7f3fe;
            border-left: 4px solid #2196F3;
            padding: 15px;
            margin: 15px 0;
        }
        .warning {
            background-color: #fff3cd;
            border-left: 4px solid #ffc107;
            padding: 15px;
            margin: 15px 0;
        }
        .material-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
            gap: 20px;
            margin: 20px 0;
        }
        .material-card {
            border: 1px solid #ddd;
            padding: 15px;
            border-radius: 5px;
        }
    </style>
</head>
<body>
    <h1>PBR Shader Tutorial</h1>
    
    <h2>Table of Contents</h2>
    <ul>
        <li><a href="#introduction">Introduction</a></li>
        <li><a href="#shader-location">Shader Location and Structure</a></li>
        <li><a href="#using-shaders">Using Shaders in Maps</a></li>
        <li><a href="#assigning-shaders">Assigning Shaders to Props</a></li>
        <li><a href="#material-properties">Understanding Material Properties</a></li>
        <li><a href="#best-practices">Best Practices</a></li>
        <li><a href="#troubleshooting">Troubleshooting</a></li>
        <li><a href="#pbr-implementation">PBR Implementation in id Tech 3 with Vulkan</a></li>
    </ul>

    <h2 id="introduction">Introduction</h2>
    <p>
        This tutorial will guide you through using the PBR (Physically Based Rendering) shaders in your game.
        These shaders provide realistic material rendering based on real-world physical properties.
    </p>

    <h2 id="shader-location">Shader Location and Structure</h2>
    <p>
        For production use, PBR shaders should be organized in a proper mod structure:
    </p>
    <pre>
idtech3/
└─ pbr_demo/            (+set fs_game pbr_demo)
    ├─ scripts/
    │   └─ pbr_materials.shader    // Shader definitions
    ├─ shaders/vulkan/             // SPIR-V shader blobs
    └─ textures/pbr/               // PBR texture maps
    </pre>

    <div class="note">
        <strong>Why this structure?</strong>
        <ul>
            <li>VFS override order - fs_game is searched before baseq3, preventing conflicts with vanilla assets</li>
            <li>Editor auto-scan - Radiant and NetRadiant automatically detect shaders in scripts/</li>
            <li>One-click packaging - Easy to create .pk3/.orb files for distribution</li>
        </ul>
    </div>

    <h2 id="installation">Installation</h2>
    <ol>
        <li>Download the PBR demo pack (pbr_demo.pk3)</li>
        <li>Place it in your id Tech 3 mods folder</li>
        <li>Launch the game with these parameters:
            <pre>+set fs_game pbr_demo +set r_renderer vulkan</pre>
        </li>
    </ol>

    <h2 id="texture-maps">Texture Maps and Naming</h2>
    <p>
        The Vulkan backend automatically detects PBR texture maps based on these exact suffixes:
    </p>
    <div class="material-card">
        <h4>Required Suffixes</h4>
        <ul>
            <li><code>_albedo</code> - Base color/diffuse texture</li>
            <li><code>_normal</code> - Normal map for surface detail</li>
            <li><code>_roughness</code> - Surface roughness (0-1)</li>
            <li><code>_metallic</code> - Metallic map (0-1)</li>
            <li><code>_ao</code> - Ambient occlusion</li>
            <li><code>_emissive</code> - Self-illumination</li>
        </ul>
    </div>

    <div class="warning">
        <strong>Important:</strong> Always use these exact suffixes to enable automatic map detection in the Vulkan backend.
    </div>

    <h2 id="shader-syntax">Shader Syntax</h2>
    <p>
        Here's the recommended minimal shader pattern for PBR materials:
    </p>
    <pre>
textures/pbr/metal_plate
{
    // Lightmap stage (unchanged)
    { map $lightmap }

    // PBR stage
    {
        map        textures/pbr/metal_plate_albedo
        normalMap  textures/pbr/metal_plate_normal
        rmoMap     textures/pbr/metal_plate_rmo   // R (Rough) G (Met) B (Occ)

        // Optional overrides
        specularScale 0.8 0.8 0.8   // if you need a tinted metal
        parallaxDepth 0.05          // only if you packed height in alpha
        blendfunc GL_DST_COLOR GL_ZERO
    }
}
    </pre>

    <div class="note">
        <strong>Performance Tip:</strong> Using packed maps (like rmoMap) instead of separate textures reduces GPU bind calls and improves performance.
    </div>

    <h2 id="packaging">Packaging for Distribution</h2>
    <ol>
        <li>Organize your files in the pbr_demo/ structure</li>
        <li>Compile GLSL shaders to SPIR-V and place in shaders/vulkan/</li>
        <li>Zip the entire pbr_demo/ folder to create pbr_demo.pk3</li>
        <li>Test the .pk3 on a clean id Tech 3 installation</li>
    </ol>

    <div class="material-card">
        <h4>Quick Reference: File Structure</h4>
        <pre>
pbr_demo/
├── scripts/
│   └── pbr_materials.shader
├── shaders/
│   └── vulkan/
│       ├── pbr.vert.spv
│       └── pbr.frag.spv
└── textures/
    └── pbr/
        ├── metal_plate_albedo.tga
        ├── metal_plate_normal.tga
        └── metal_plate_rmo.tga
        </pre>
    </div>

    <h2 id="using-shaders">Using Shaders in Maps</h2>
    <h3>Method 1: Direct Texture Assignment</h3>
    <pre>
// In your .map file
{
    "classname" "worldspawn"
    "textures/pbr/gold_polished" "textures/pbr/gold_polished"
    "textures/pbr/marble_polished" "textures/pbr/marble_polished"
}
    </pre>

    <h3>Method 2: Using Shader Keywords</h3>
    <pre>
// In your .shader file
textures/mymap/wall
{
    qer_editorimage textures/mymap/wall
    {
        map $lightmap
    }
    {
        map textures/mymap/wall
        blendfunc filter
    }
    {
        map textures/pbr/marble_polished
        blendfunc add
    }
}
    </pre>

    <div class="note">
        <strong>Note:</strong> Always ensure your map has proper lighting setup for PBR materials to look their best.
    </div>

    <h2 id="assigning-shaders">Assigning Shaders to Props</h2>
    <h3>Method 1: Using the Editor</h3>
    <ol>
        <li>Select the prop in the editor</li>
        <li>Open the properties panel</li>
        <li>In the "Shader" field, enter the path to the PBR shader (e.g., <code>textures/pbr/gold_polished</code>)</li>
    </ol>

    <h3>Method 2: Using Entity Properties</h3>
    <pre>
// In your .map file
{
    "classname" "prop_static"
    "model" "models/props/statue.md3"
    "shader" "textures/pbr/bronze"
}
    </pre>

    <h2 id="material-properties">Understanding Material Properties</h2>
    <div class="material-grid">
        <div class="material-card">
            <h3>Metals</h3>
            <p>Use <code>specularScale</code> for metallic materials:</p>
            <pre>
specularScale 1.0 0.86 0.57  // Gold color
roughness 0.1                // Very smooth
            </pre>
        </div>
        <div class="material-card">
            <h3>Non-Metals</h3>
            <p>Use <code>specularReflectance</code> for non-metallic materials:</p>
            <pre>
specularReflectance 0.05     // Non-metallic
roughness 0.4                // Matte finish
            </pre>
        </div>
        <div class="material-card">
            <h3>Fabrics</h3>
            <p>Use the <code>cloth</code> keyword for fabric materials:</p>
            <pre>
specularReflectance 0.02     // Very low specular
roughness 0.9                // Very rough
cloth                        // Use cloth BRDF
            </pre>
        </div>
    </div>

    <h2 id="best-practices">Best Practices</h2>
    <ul>
        <li>Always use appropriate roughness values for the material type</li>
        <li>Use <code>specularScale</code> for metals and <code>specularReflectance</code> for non-metals</li>
        <li>Include lightmap support in your shaders</li>
        <li>Use the cloth BRDF for fabric materials</li>
        <li>Test materials under different lighting conditions</li>
    </ul>

    <div class="warning">
        <strong>Warning:</strong> Avoid mixing metallic and non-metallic properties in the same shader.
    </div>

    <h2 id="troubleshooting">Troubleshooting</h2>
    <h3>Common Issues</h3>
    <ul>
        <li><strong>Material looks too shiny:</strong> Increase the roughness value</li>
        <li><strong>Material looks too dull:</strong> Decrease the roughness value</li>
        <li><strong>Metallic material looks wrong:</strong> Check if you're using <code>specularScale</code> instead of <code>specularReflectance</code></li>
        <li><strong>Fabric looks incorrect:</strong> Make sure to use the <code>cloth</code> keyword</li>
    </ul>

    <h3>Performance Considerations</h3>
    <ul>
        <li>Use appropriate texture sizes (1024x1024 or 2048x2048 for most materials)</li>
        <li>Consider using texture atlases for similar materials</li>
        <li>Use mipmaps for better performance at distance</li>
    </ul>

    <div class="note">
        <strong>Tip:</strong> You can use the in-game console command <code>r_showshader</code> to debug shader assignments.
    </div>

    <h2 id="pbr-implementation">PBR Implementation in id Tech 3 with Vulkan</h2>
    
    <h3>PBR Texture Maps</h3>
    <p>
        The Vulkan PBR implementation supports multiple texture maps for each material. These are automatically detected based on file suffixes:
    </p>
    <pre>
// Example texture naming convention
textures/pbr/metal/
    metal_albedo.tga    // Base color
    metal_normal.tga    // Normal map
    metal_roughness.tga // Roughness map
    metal_metallic.tga  // Metallic map
    metal_ao.tga        // Ambient occlusion
    metal_emissive.tga  // Emissive map
    </pre>

    <div class="material-card">
        <h4>Supported Texture Map Types</h4>
        <ul>
            <li><code>_albedo</code> - Base color/diffuse texture</li>
            <li><code>_normal</code> - Normal map for surface detail</li>
            <li><code>_roughness</code> - Surface roughness (0-1)</li>
            <li><code>_metallic</code> - Metallic map (0-1)</li>
            <li><code>_ao</code> - Ambient occlusion</li>
            <li><code>_emissive</code> - Self-illumination</li>
        </ul>
    </div>

    <h3>Shader Parameters</h3>
    <p>
        The following PBR-specific parameters can be used in your shaders:
    </p>
    <pre>
// Example PBR shader with all parameters
textures/pbr/example_pbr
{
    {
        map $lightmap
    }
    {
        map textures/pbr/example_albedo
        specMap textures/pbr/example_metallic
        normalMap textures/pbr/example_normal
        roughness 0.5
        metallic 1.0
        specularReflectance 0.5
        normalScale 1.0
        parallaxDepth 0.1
        parallaxBias -0.02
        blendfunc GL_DST_COLOR GL_ZERO
    }
}
    </pre>

    <div class="material-card">
        <h4>PBR Shader Parameters</h4>
        <ul>
            <li><code>roughness</code> - Surface roughness (0-1)</li>
            <li><code>metallic</code> - Metallic factor (0-1)</li>
            <li><code>specularReflectance</code> - Non-metallic specular intensity</li>
            <li><code>normalScale</code> - Normal map intensity</li>
            <li><code>parallaxDepth</code> - Parallax mapping depth</li>
            <li><code>parallaxBias</code> - Parallax mapping bias</li>
        </ul>
    </div>

    <h3>Vulkan-Specific Features</h3>
    <p>
        The Vulkan implementation includes several advanced features:
    </p>
    <ul>
        <li>Automatic texture map detection and loading</li>
        <li>Support for normal mapping with parallax occlusion</li>
        <li>Physically-based lighting calculations</li>
        <li>Optimized texture compression</li>
    </ul>

    <h3>Creating a Complete PBR Material</h3>
    <pre>
// Complete PBR material example
textures/pbr/metal_plate
{
    // Lightmap stage
    {
        map $lightmap
    }
    
    // Main PBR stage
    {
        // Base color and metallic properties
        map textures/pbr/metal_plate_albedo
        specMap textures/pbr/metal_plate_metallic
        rgbGen const ( 0.0 0.0 0.0 )
        specularScale 0.8 0.8 0.8  // Metallic color
        
        // Surface properties
        roughness 0.3              // Slightly rough
        metallic 1.0               // Fully metallic
        
        // Normal mapping
        normalMap textures/pbr/metal_plate_normal
        normalScale 1.0
        
        // Parallax mapping
        parallaxDepth 0.1
        parallaxBias -0.02
        
        // Blending
        blendfunc GL_DST_COLOR GL_ZERO
    }
}
    </pre>

    <h3>Performance Optimization</h3>
    <div class="material-card">
        <h4>Texture Flags</h4>
        <pre>
// Example of optimized texture loading
textures/pbr/optimized_material
{
    {
        map $lightmap
    }
    {
        map textures/pbr/material_albedo
        specMap textures/pbr/material_metallic
        // Enable mipmaps for better performance
        noMipMaps
        // Enable texture compression
        noPicMip
        blendfunc GL_DST_COLOR GL_ZERO
    }
}
        </pre>
    </div>

    <h3>Debugging PBR Materials</h3>
    <p>
        Use these console commands to debug PBR materials:
    </p>
    <ul>
        <li><code>r_showshader</code> - Shows current shader information</li>
        <li><code>r_showpbr</code> - Toggles PBR visualization modes</li>
        <li><code>r_showtextures</code> - Shows texture loading information</li>
    </ul>

    <div class="warning">
        <strong>Important:</strong> When using PBR materials, ensure that:
        <ul>
            <li>All texture maps are in the correct format (TGA recommended)</li>
            <li>Texture dimensions are powers of 2</li>
            <li>Normal maps are in tangent space</li>
            <li>Metallic maps are grayscale</li>
        </ul>
    </div>

    <h3>Common PBR Material Recipes</h3>
    <div class="material-grid">
        <div class="material-card">
            <h4>Polished Metal</h4>
            <pre>
roughness 0.1
metallic 1.0
specularScale 0.8 0.8 0.8
normalScale 0.5
            </pre>
        </div>
        <div class="material-card">
            <h4>Rough Plastic</h4>
            <pre>
roughness 0.8
metallic 0.0
specularReflectance 0.05
normalScale 1.0
            </pre>
        </div>
        <div class="material-card">
            <h4>Glossy Wood</h4>
            <pre>
roughness 0.3
metallic 0.0
specularReflectance 0.04
normalScale 1.0
            </pre>
        </div>
    </div>

    <div class="note">
        <strong>Tip:</strong> For best results, create a test map with different lighting conditions to verify your PBR materials look correct in all situations.
    </div>
</body>
</html> 