<?php
/**
 * Vulkan Shaders Documentation
 */
$title = 'Vulkan Shaders - id Tech 3 Documentation';
$breadcrumbs = [
    '/rendering' => 'Rendering',
    '/rendering/shaders' => 'Vulkan Shaders'
];
?>

<h1>Vulkan Shaders</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Modern id Tech 3 implementations like Quake3e support Vulkan shaders, providing advanced graphics capabilities through SPIR-V compiled shaders. This system replaces the legacy OpenGL shader pipeline with more efficient and flexible Vulkan compute and graphics shaders.</p>
    
    <div class="feature-list">
        <h3>Vulkan Shader Features</h3>
        <ul>
            <li><strong>SPIR-V Format:</strong> Cross-platform binary shader format</li>
            <li><strong>Compute Shaders:</strong> General-purpose GPU computing</li>
            <li><strong>Ray Tracing:</strong> Hardware-accelerated ray tracing (RTX)</li>
            <li><strong>Descriptor Sets:</strong> Efficient resource binding</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Shader Pipeline</h2>
    
    <h3>Graphics Pipeline Stages</h3>
    <div class="code-block">
        <pre><code>// Vulkan graphics pipeline stages
VK_SHADER_STAGE_VERTEX_BIT                  // Vertex processing
VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT    // Tessellation control
VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT // Tessellation evaluation  
VK_SHADER_STAGE_GEOMETRY_BIT                // Geometry processing
VK_SHADER_STAGE_FRAGMENT_BIT                // Fragment/pixel processing
VK_SHADER_STAGE_COMPUTE_BIT                 // Compute operations

// Ray tracing stages (RTX)
VK_SHADER_STAGE_RAYGEN_BIT_KHR              // Ray generation
VK_SHADER_STAGE_ANY_HIT_BIT_KHR             // Any hit testing
VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR         // Closest hit shading
VK_SHADER_STAGE_MISS_BIT_KHR                // Ray miss handling</code></pre>
    </div>
    
    <h3>Shader Compilation</h3>
    <p>Vulkan shaders are compiled from GLSL to SPIR-V binary format:</p>
    <div class="code-block">
        <pre><code># Compile GLSL to SPIR-V
glslangValidator -V shader.vert -o shader.vert.spv
glslangValidator -V shader.frag -o shader.frag.spv

# With optimization
glslangValidator -V -O shader.vert -o shader.vert.spv

# Include directories and defines
glslangValidator -V -I./includes -DUSE_LIGHTING shader.vert</code></pre>
    </div>
</div>

<div class="section">
    <h2>Basic Vertex Shader</h2>
    
    <h3>Modern Vertex Processing</h3>
    <div class="code-block">
        <pre><code>#version 450

// Input attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inColor;

// Output to fragment shader
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec4 fragColor;

// Uniform buffer object
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 normalMatrix;
    vec4 lightPos;
    vec4 viewPos;
} ubo;

void main() {
    // Transform vertex to world space
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // Transform normal to world space
    fragNormal = mat3(ubo.normalMatrix) * inNormal;
    
    // Pass through texture coordinates and color
    fragTexCoord = inTexCoord;
    fragColor = inColor;
    
    // Final vertex position in clip space
    gl_Position = ubo.proj * ubo.view * worldPos;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Advanced Fragment Shader</h2>
    
    <h3>PBR Material Shading</h3>
    <div class="code-block">
        <pre><code>#version 450

// Input from vertex shader
layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragColor;

// Output color
layout(location = 0) out vec4 outColor;

// Textures
layout(binding = 1) uniform sampler2D texAlbedo;
layout(binding = 2) uniform sampler2D texNormal;
layout(binding = 3) uniform sampler2D texRoughness;
layout(binding = 4) uniform sampler2D texMetallic;
layout(binding = 5) uniform sampler2D texAO;

// Uniform buffer
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    mat4 normalMatrix;
    vec4 lightPos;
    vec4 viewPos;
} ubo;

// PBR functions
vec3 getNormalFromMap() {
    vec3 tangentNormal = texture(texNormal, fragTexCoord).xyz * 2.0 - 1.0;
    
    vec3 Q1 = dFdx(fragWorldPos);
    vec3 Q2 = dFdy(fragWorldPos);
    vec2 st1 = dFdx(fragTexCoord);
    vec2 st2 = dFdy(fragTexCoord);
    
    vec3 N = normalize(fragNormal);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    
    return normalize(TBN * tangentNormal);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159265359 * denom * denom;
    
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // Sample material properties
    vec3 albedo = pow(texture(texAlbedo, fragTexCoord).rgb * fragColor.rgb, vec3(2.2));
    float metallic = texture(texMetallic, fragTexCoord).r;
    float roughness = texture(texRoughness, fragTexCoord).r;
    float ao = texture(texAO, fragTexCoord).r;
    
    // Calculate vectors
    vec3 N = getNormalFromMap();
    vec3 V = normalize(ubo.viewPos.xyz - fragWorldPos);
    vec3 L = normalize(ubo.lightPos.xyz - fragWorldPos);
    vec3 H = normalize(V + L);
    
    // Calculate radiance
    float distance = length(ubo.lightPos.xyz - fragWorldPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = vec3(23.47, 21.31, 20.79) * attenuation; // Light color
    
    // Cook-Torrance BRDF
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;
    
    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo / 3.14159265359 + specular) * radiance * NdotL;
    
    // Ambient lighting
    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;
    
    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0/2.2));
    
    outColor = vec4(color, fragColor.a);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Compute Shaders</h2>
    
    <h3>Post-Processing Effects</h3>
    <div class="code-block">
        <pre><code>#version 450

// Local workgroup size
layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// Input and output images
layout(binding = 0, rgba8) uniform readonly image2D inputImage;
layout(binding = 1, rgba8) uniform writeonly image2D outputImage;

// Uniform buffer for parameters
layout(binding = 2) uniform ComputeUBO {
    float time;
    float intensity;
    vec2 resolution;
} ubo;

void main() {
    ivec2 texelCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 imageSize = imageSize(inputImage);
    
    // Bounds check
    if (texelCoord.x >= imageSize.x || texelCoord.y >= imageSize.y) {
        return;
    }
    
    // Sample input color
    vec4 color = imageLoad(inputImage, texelCoord);
    
    // Apply bloom effect
    vec2 uv = vec2(texelCoord) / ubo.resolution;
    
    // Gaussian blur for bloom
    vec4 bloom = vec4(0.0);
    float totalWeight = 0.0;
    
    const int kernelSize = 5;
    const float sigma = 2.0;
    
    for (int x = -kernelSize; x <= kernelSize; x++) {
        for (int y = -kernelSize; y <= kernelSize; y++) {
            ivec2 sampleCoord = texelCoord + ivec2(x, y);
            
            // Clamp to image bounds
            sampleCoord = clamp(sampleCoord, ivec2(0), imageSize - 1);
            
            float weight = exp(-(x*x + y*y) / (2.0 * sigma * sigma));
            bloom += imageLoad(inputImage, sampleCoord) * weight;
            totalWeight += weight;
        }
    }
    
    bloom /= totalWeight;
    
    // Combine original with bloom
    vec4 finalColor = color + bloom * ubo.intensity;
    
    // Store result
    imageStore(outputImage, texelCoord, finalColor);
}</code></pre>
    </div>
    
    <h3>Particle System Compute</h3>
    <div class="code-block">
        <pre><code>#version 450

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct Particle {
    vec3 position;
    float life;
    vec3 velocity;
    float size;
    vec4 color;
};

layout(std430, binding = 0) restrict buffer ParticleBuffer {
    Particle particles[];
};

layout(binding = 1) uniform ParticleUBO {
    float deltaTime;
    float time;
    vec3 gravity;
    uint particleCount;
    vec3 emitterPos;
    float emitterRate;
} ubo;

uint rng_state = uint(gl_GlobalInvocationID.x + time * 1000.0);

uint rand() {
    rng_state = rng_state * 747796405u + 2891336453u;
    uint result = ((rng_state >> ((rng_state >> 28) + 4)) ^ rng_state) * 277803737u;
    result = (result >> 22) ^ result;
    return result;
}

float randf() {
    return float(rand()) / 4294967295.0;
}

void main() {
    uint index = gl_GlobalInvocationID.x;
    if (index >= ubo.particleCount) return;
    
    Particle p = particles[index];
    
    if (p.life <= 0.0) {
        // Respawn particle
        p.position = ubo.emitterPos + vec3(
            (randf() - 0.5) * 2.0,
            (randf() - 0.5) * 2.0,
            (randf() - 0.5) * 2.0
        );
        
        p.velocity = vec3(
            (randf() - 0.5) * 10.0,
            randf() * 20.0 + 5.0,
            (randf() - 0.5) * 10.0
        );
        
        p.life = randf() * 5.0 + 1.0;
        p.size = randf() * 2.0 + 0.5;
        p.color = vec4(randf(), randf(), randf(), 1.0);
    } else {
        // Update existing particle
        p.velocity += ubo.gravity * ubo.deltaTime;
        p.position += p.velocity * ubo.deltaTime;
        p.life -= ubo.deltaTime;
        
        // Fade out over time
        float lifeFactor = p.life / 5.0;
        p.color.a = lifeFactor;
        p.size *= 0.999;
    }
    
    particles[index] = p;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Ray Tracing Shaders</h2>
    
    <h3>Ray Generation Shader</h3>
    <div class="code-block">
        <pre><code>#version 460
#extension GL_EXT_ray_tracing : require

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0, rgba8) uniform image2D image;

layout(binding = 2, set = 0) uniform CameraUBO {
    mat4 viewInverse;
    mat4 projInverse;
} cam;

layout(location = 0) rayPayloadEXT vec3 hitValue;

void main() {
    const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 inUV = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
    vec2 d = inUV * 2.0 - 1.0;

    vec4 origin = cam.viewInverse * vec4(0, 0, 0, 1);
    vec4 target = cam.projInverse * vec4(d.x, d.y, 1, 1);
    vec4 direction = cam.viewInverse * vec4(normalize(target.xyz), 0);

    float tmin = 0.001;
    float tmax = 10000.0;

    traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, 
                origin.xyz, tmin, direction.xyz, tmax, 0);

    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(hitValue, 1.0));
}</code></pre>
    </div>
    
    <h3>Closest Hit Shader</h3>
    <div class="code-block">
        <pre><code>#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) rayPayloadInEXT vec3 hitValue;
layout(location = 1) rayPayloadEXT bool shadowed;

hitAttributeEXT vec3 attribs;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 3, set = 0) buffer readonly Vertices { vec4 vertices[]; };
layout(binding = 4, set = 0) buffer readonly Indices { uint indices[]; };

struct Material {
    vec3 albedo;
    float metallic;
    vec3 emission;
    float roughness;
};

layout(binding = 5, set = 0) buffer readonly Materials { Material materials[]; };

void main() {
    // Get triangle indices
    uint index = indices[3 * gl_PrimitiveID];
    uint index1 = indices[3 * gl_PrimitiveID + 1];
    uint index2 = indices[3 * gl_PrimitiveID + 2];
    
    // Get vertex positions
    vec3 v0 = vertices[index].xyz;
    vec3 v1 = vertices[index1].xyz;
    vec3 v2 = vertices[index2].xyz;
    
    // Calculate barycentric coordinates
    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    
    // Interpolate position and normal
    vec3 worldPos = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
    vec3 normal = normalize(cross(v1 - v0, v2 - v0));
    
    // Get material
    Material mat = materials[gl_InstanceCustomIndexEXT];
    
    // Simple lighting calculation
    vec3 lightDir = normalize(vec3(1, 1, 1));
    float NdotL = max(dot(normal, lightDir), 0.0);
    
    // Cast shadow ray
    float tmin = 0.001;
    float tmax = 10000.0;
    vec3 shadowRayDir = lightDir;
    
    shadowed = true;
    traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT, 
                0xff, 1, 0, 1, worldPos, tmin, shadowRayDir, tmax, 1);
    
    float shadow = shadowed ? 0.3 : 1.0;
    
    hitValue = mat.albedo * NdotL * shadow + mat.emission;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Shader Resources and Binding</h2>
    
    <h3>Descriptor Set Layout</h3>
    <div class="code-block">
        <pre><code>// C++ descriptor set setup
VkDescriptorSetLayoutBinding bindings[] = {
    // UBO binding
    {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    },
    // Texture bindings
    {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    },
    // Storage buffer for compute
    {
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .pImmutableSamplers = nullptr
    }
};</code></pre>
    </div>
    
    <h3>Push Constants</h3>
    <div class="code-block">
        <pre><code>// GLSL push constants
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    float time;
    int flags;
} push;

// C++ push constant range
VkPushConstantRange pushConstantRange = {
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    .offset = 0,
    .size = sizeof(PushConstants)
};</code></pre>
    </div>
</div>

<div class="section">
    <h2>Shader Debugging</h2>
    
    <h3>Debug Output</h3>
    <div class="code-block">
        <pre><code># Enable Vulkan validation layers for shader debugging
export VK_LAYER_PATH=/path/to/vulkan/explicit_layer.d
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation

# Enable shader debug info compilation
glslangValidator -V -g shader.vert -o shader.vert.spv

# Use debugging tools
renderdoc-cmd capture --vulkan myapp
gfxreconstruct-replay capture.gfxr</code></pre>
    </div>
    
    <h3>Common Shader Issues</h3>
    <div class="troubleshooting">
        <h4>Binding mismatches</h4>
        <ul>
            <li>Verify descriptor set layout matches shader bindings</li>
            <li>Check binding numbers and descriptor types</li>
            <li>Ensure stage flags are correct</li>
        </ul>
        
        <h4>SPIR-V compilation errors</h4>
        <ul>
            <li>Use latest glslangValidator version</li>
            <li>Check GLSL version compatibility</li>
            <li>Verify extension requirements</li>
        </ul>
        
        <h4>Performance issues</h4>
        <ul>
            <li>Minimize dynamic branching in shaders</li>
            <li>Use appropriate precision qualifiers</li>
            <li>Optimize texture sampling patterns</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Skybox Shaders</h2>
    <p>The engine supports both static and animated skyboxes:</p>
    <ul>
        <li><strong>Static Skyboxes:</strong> Use <code>skyParms</code> command with standard texture naming</li>
        <li><strong>Animated Skyboxes:</strong> Use <code>skyParmsFlipbook</code> command for frame-based animation</li>
    </ul>
    <p>See <a href="rendering/animated-skybox">Animated Skybox</a> for detailed documentation on creating animated sky effects.</p>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="rendering/vulkan">Vulkan Renderer</a></li>
        <li><a href="rendering/pbr">PBR Materials</a></li>
        <li><a href="rendering/global-illumination">Global Illumination</a></li>
        <li><a href="rendering/animated-skybox">Animated Skybox</a></li>
        <li><a href="tools/asset-tools">Asset Pipeline</a></li>
    </ul>
</div> 