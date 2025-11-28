<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Deferred Rendering in id Tech 3 - Technical Documentation</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
            line-height: 1.6;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
            color: #333;
        }
        pre {
            background: #f5f5f5;
            padding: 15px;
            border-radius: 5px;
            overflow-x: auto;
        }
        code {
            font-family: 'Consolas', 'Monaco', monospace;
        }
        .note {
            background: #e7f3fe;
            border-left: 4px solid #2196F3;
            padding: 15px;
            margin: 15px 0;
        }
        table {
            border-collapse: collapse;
            width: 100%;
            margin: 15px 0;
        }
        th, td {
            border: 1px solid #ddd;
            padding: 8px;
            text-align: left;
        }
        th {
            background-color: #f5f5f5;
        }
    </style>
</head>
<body>
    <h1>Deferred Rendering in Quake3e with Vulkan HDR and PBR</h1>
    
    <div class="note">
        <strong>Note:</strong> This documentation assumes you have already implemented the basic Vulkan renderer and PBR material system as described in the project's README.
    </div>

    <h2>Table of Contents</h2>
    <ul>
        <li><a href="#overview">Overview</a></li>
        <li><a href="#architecture">Architecture</a></li>
        <li><a href="#implementation">Implementation Steps</a></li>
        <li><a href="#integration">Integration with Existing Systems</a></li>
        <li><a href="#configuration">Configuration</a></li>
        <li><a href="#debugging">Debugging and Tools</a></li>
    </ul>

    <h2 id="overview">Overview</h2>
    <p>This document outlines the implementation of deferred rendering in Quake3e using Vulkan, with support for HDR and PBR materials. The system leverages modern rendering techniques while maintaining compatibility with the existing engine architecture.</p>

    <h2 id="architecture">Architecture</h2>
    <h3>1. G-Buffer Layout</h3>
    <pre><code>// G-Buffer attachments
struct GBuffer {
    // Position (RGB32F)
    vec3 worldPos;
    
    // Normal (RGB16F)
    vec3 normal;
    
    // Albedo (RGBA8)
    vec4 albedo;
    
    // Material Properties (RGBA8)
    float metallic;
    float roughness;
    float ao;
    float emissive;
    
    // Additional Data (RGBA8)
    float depth;
    float stencil;
    float unused1;
    float unused2;
};</code></pre>

    <h3>2. Render Pass Setup</h3>
    <pre><code>// Vulkan render pass creation
VkRenderPassCreateInfo renderPassInfo = {};
renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

// G-Buffer attachments
VkAttachmentDescription attachments[] = {
    // Position
    {
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    },
    // Normal
    {
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        // ... similar configuration
    },
    // Albedo
    {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        // ... similar configuration
    },
    // Material Properties
    {
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        // ... similar configuration
    }
};</code></pre>

    <h2 id="implementation">Implementation Steps</h2>

    <h3>1. G-Buffer Generation</h3>
    <p>First pass renders scene geometry into G-Buffer:</p>
    <pre><code>// Vertex shader
layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vTexCoord;

void main() {
    vWorldPos = (model * vec4(position, 1.0)).xyz;
    vNormal = mat3(model) * normal;
    vTexCoord = texCoord;
    gl_Position = projection * view * vec4(vWorldPos, 1.0);
}</code></pre>

    <pre><code>// Fragment shader
layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gAlbedo;
layout(location = 3) out vec4 gMaterial;

void main() {
    // Sample PBR textures
    vec4 albedo = texture(albedoMap, vTexCoord);
    vec4 metallicRoughness = texture(metallicRoughnessMap, vTexCoord);
    
    gPosition = vec4(vWorldPos, 1.0);
    gNormal = vec4(normalize(vNormal), 1.0);
    gAlbedo = albedo;
    gMaterial = vec4(metallicRoughness.r, metallicRoughness.g, ao, emissive);
}</code></pre>

    <h3>2. Lighting Pass</h3>
    <p>Second pass computes lighting using G-Buffer data:</p>
    <pre><code>// Lighting shader
layout(binding = 0) uniform sampler2D gPosition;
layout(binding = 1) uniform sampler2D gNormal;
layout(binding = 2) uniform sampler2D gAlbedo;
layout(binding = 3) uniform sampler2D gMaterial;

layout(location = 0) out vec4 fragColor;

void main() {
    // Sample G-Buffer
    vec3 worldPos = texture(gPosition, texCoord).rgb;
    vec3 normal = texture(gNormal, texCoord).rgb;
    vec4 albedo = texture(gAlbedo, texCoord);
    vec4 material = texture(gMaterial, texCoord);
    
    // PBR lighting calculation
    vec3 F0 = mix(vec3(0.04), albedo.rgb, material.r);
    vec3 Lo = calculatePBR(normal, worldPos, albedo.rgb, F0, 
                          material.r, material.g, material.b);
    
    // HDR output
    fragColor = vec4(Lo, 1.0);
}</code></pre>

    <h3>3. Post-Processing</h3>
    <p>Final pass applies HDR tonemapping and bloom:</p>
    <pre><code>// Post-process shader
layout(binding = 0) uniform sampler2D hdrColor;
layout(binding = 1) uniform sampler2D bloomBlur;

layout(location = 0) out vec4 fragColor;

void main() {
    vec3 hdrColor = texture(hdrColor, texCoord).rgb;
    vec3 bloomColor = texture(bloomBlur, texCoord).rgb;
    
    // Combine HDR and bloom
    vec3 color = hdrColor + bloomColor;
    
    // Tonemapping
    color = tonemapReinhard(color);
    
    // Gamma correction
    color = pow(color, vec3(1.0/2.2));
    
    fragColor = vec4(color, 1.0);
}</code></pre>

    <h2 id="integration">Integration with Existing Systems</h2>
    <p>The deferred renderer integrates with:</p>
    <ul>
        <li>PBR material system</li>
        <li>HDR rendering pipeline</li>
        <li>Atmospheric scattering sky system</li>
        <li>Octree voxel global illumination</li>
    </ul>

    <h2 id="configuration">Configuration</h2>
    <pre><code>// Console variables
r_deferred "1"        // Enable/disable deferred rendering
r_hdr "1"            // Enable HDR
r_tonemap "1"        // Enable tonemapping
r_bloom "1"          // Enable bloom


