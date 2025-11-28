<?php
/**
 * Spherical Harmonics Global Illumination - id Tech 3 Engine Documentation
 */
$title = 'Spherical Harmonics Global Illumination - id Tech 3 Documentation';
$breadcrumbs = [
    '/rendering' => 'Rendering',
    '/rendering/SHGI' => 'Spherical Harmonics GI'
];
?>

<h1>Spherical Harmonics Global Illumination</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Spherical Harmonics Global Illumination (SHGI) provides efficient indirect lighting for dynamic objects in JKSunny's PBR port. This implementation uses low-order spherical harmonics to approximate incoming radiance at any point in 3D space, enabling real-time global illumination with minimal performance overhead.</p>
    
    <div class="feature-list">
        <h3>Key Features</h3>
        <ul>
            <li><strong>Light Probe Network:</strong> Strategic placement of SH probes throughout the scene</li>
            <li><strong>Real-time Updates:</strong> Dynamic probe updates using Vulkan compute shaders</li>
            <li><strong>PBR Integration:</strong> Seamless integration with the existing PBR pipeline</li>
            <li><strong>Temporal Stability:</strong> Advanced filtering to prevent flickering</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Spherical Harmonics Theory</h2>
    
    <h3>Mathematical Foundation</h3>
    <div class="code-block">
        <pre><code>// Spherical Harmonics basis functions (up to 3rd order)
// L = 0 (DC component)
float SH_0_0(vec3 dir) {
    return 0.282095; // sqrt(1/(4*pi))
}

// L = 1 (linear components)
float SH_1_n1(vec3 dir) { return -0.488603 * dir.y; }    // -sqrt(3/(4*pi)) * y
float SH_1_0(vec3 dir)  { return  0.488603 * dir.z; }    //  sqrt(3/(4*pi)) * z  
float SH_1_p1(vec3 dir) { return -0.488603 * dir.x; }    // -sqrt(3/(4*pi)) * x

// L = 2 (quadratic components)
float SH_2_n2(vec3 dir) { return 1.092548 * dir.x * dir.y; }
float SH_2_n1(vec3 dir) { return -1.092548 * dir.y * dir.z; }
float SH_2_0(vec3 dir)  { return 0.315392 * (3.0*dir.z*dir.z - 1.0); }
float SH_2_p1(vec3 dir) { return -1.092548 * dir.x * dir.z; }
float SH_2_p2(vec3 dir) { return 0.546274 * (dir.x*dir.x - dir.y*dir.y); }

// Project incoming radiance onto SH basis
vec3 ProjectToSH(vec3 radiance, vec3 direction) {
    vec3 sh[9];
    sh[0] = radiance * SH_0_0(direction);
    sh[1] = radiance * SH_1_n1(direction);
    sh[2] = radiance * SH_1_0(direction);
    sh[3] = radiance * SH_1_p1(direction);
    sh[4] = radiance * SH_2_n2(direction);
    sh[5] = radiance * SH_2_n1(direction);
    sh[6] = radiance * SH_2_0(direction);
    sh[7] = radiance * SH_2_p1(direction);
    sh[8] = radiance * SH_2_p2(direction);
    return sh;
}</code></pre>
    </div>
    
    <h3>SH Convolution for Diffuse Lighting</h3>
    <div class="code-block">
        <pre><code>// Convolution factors for diffuse BRDF (cosine-weighted hemisphere)
// These factors are precomputed: integral of SH basis * cos(theta) over hemisphere
const float SH_DIFFUSE_CONV[9] = {
    3.141593,    // L=0: pi
    2.094395,    // L=1: 2*pi/3
    2.094395,    // L=1: 2*pi/3
    2.094395,    // L=1: 2*pi/3
    0.785398,    // L=2: pi/4
    0.785398,    // L=2: pi/4
    0.785398,    // L=2: pi/4
    0.785398,    // L=2: pi/4
    0.785398     // L=2: pi/4
};

// Apply diffuse convolution to SH coefficients
void ConvolveSHDiffuse(vec3 sh_in[9], vec3 sh_out[9]) {
    for (int i = 0; i < 9; i++) {
        sh_out[i] = sh_in[i] * SH_DIFFUSE_CONV[i];
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Engine Integration</h2>
    
    <h3>Light Probe Data Structure</h3>
    <div class="code-block">
        <pre><code>// tr_shgi.h - Spherical Harmonics Global Illumination system
typedef struct shProbe_s {
    vec3_t          position;           // World space position
    float           radius;             // Influence radius
    vec3_t          sh_coeffs[9];       // RGB SH coefficients (L0-L2)
    
    // Update tracking
    int             lastUpdateFrame;    // Last frame this probe was updated
    float           updatePriority;     // Priority for next update (0-1)
    qboolean        isDynamic;          // Can be updated in real-time
    
    // Neighboring probes for interpolation
    int             neighbors[8];       // Indices of neighboring probes
    float           neighborWeights[8]; // Interpolation weights
    int             numNeighbors;       // Number of valid neighbors
    
    // Debug information
    vec3_t          debugColor;         // Visualization color
    int             debugId;            // Unique identifier
    
} shProbe_t;

typedef struct shgiSystem_s {
    // Probe storage
    shProbe_t*      probes;             // Array of light probes
    int             numProbes;          // Current number of probes
    int             maxProbes;          // Maximum allocated probes
    
    // Vulkan resources
    VkBuffer        probeBuffer;        // GPU buffer for SH coefficients
    VmaAllocation   probeAllocation;    // VMA allocation for probe buffer
    VkDescriptorSet descriptorSet;      // Descriptor set for probe data
    
    // Compute pipeline for updates
    VkPipeline      updatePipeline;     // Compute pipeline for probe updates
    VkPipelineLayout updateLayout;      // Pipeline layout for updates
    
    // Configuration
    cvar_t*         sh_enable;          // Enable/disable SHGI
    cvar_t*         sh_probeSpacing;    // Automatic probe spacing
    cvar_t*         sh_updateRate;      // Updates per second
    cvar_t*         sh_debugVisualize;  // Debug visualization
    
} shgiSystem_t;

extern shgiSystem_t shgi;</code></pre>
    </div>
    
    <h3>Initialization and Setup</h3>
    <div class="code-block">
        <pre><code>// Initialize the SHGI system
qboolean SHGI_Init(void) {
    Com_Printf("Initializing Spherical Harmonics Global Illumination\n");
    
    // Register CVars
    shgi.sh_enable = Cvar_Get("r_shgi", "1", CVAR_ARCHIVE);
    shgi.sh_probeSpacing = Cvar_Get("r_shgi_probeSpacing", "256", CVAR_ARCHIVE);
    shgi.sh_updateRate = Cvar_Get("r_shgi_updateRate", "30", CVAR_ARCHIVE);
    shgi.sh_debugVisualize = Cvar_Get("r_shgi_debug", "0", 0);
    
    if (!shgi.sh_enable->integer) {
        Com_Printf("SHGI disabled by cvar\n");
        return qfalse;
    }
    
    // Allocate probe storage
    shgi.maxProbes = MAX_PROBES;
    shgi.probes = Z_Malloc(sizeof(shProbe_t) * shgi.maxProbes);
    shgi.numProbes = 0;
    
    // Create Vulkan resources
    if (!SHGI_InitVulkanResources()) {
        Com_Printf("^1Failed to initialize SHGI Vulkan resources\n");
        return qfalse;
    }
    
    // Register console commands
    Cmd_AddCommand("shgi_place", SHGI_PlaceProbe_f);
    Cmd_AddCommand("shgi_clear", SHGI_ClearProbes_f);
    Cmd_AddCommand("shgi_update", SHGI_ForceUpdate_f);
    
    Com_Printf("SHGI initialized with %d probes\n", shgi.maxProbes);
    return qtrue;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Probe Placement and Management</h2>
    
    <h3>Automatic Probe Placement</h3>
    <div class="code-block">
        <pre><code>// Automatic probe placement based on scene geometry
void SHGI_GenerateProbeNetwork(void) {
    float spacing = shgi.sh_probeSpacing->value;
    vec3_t minBounds, maxBounds;
    
    // Calculate world bounds from all loaded BSP data
    SHGI_CalculateWorldBounds(minBounds, maxBounds);
    
    Com_Printf("Generating probe network with spacing %.1f\n", spacing);
    
    // Clear existing probes
    shgi.numProbes = 0;
    
    // Generate grid of potential probe positions
    for (float x = minBounds[0]; x <= maxBounds[0]; x += spacing) {
        for (float y = minBounds[1]; y <= maxBounds[1]; y += spacing) {
            for (float z = minBounds[2]; z <= maxBounds[2]; z += spacing) {
                vec3_t position = {x, y, z};
                
                // Check if position is valid for probe placement
                if (SHGI_IsValidProbePosition(position)) {
                    SHGI_CreateProbe(position, spacing * 0.5f);
                }
                
                if (shgi.numProbes >= shgi.maxProbes) {
                    Com_Printf("^3Reached maximum probe limit (%d)\n", shgi.maxProbes);
                    goto done;
                }
            }
        }
    }
    
done:
    Com_Printf("Generated %d probes\n", shgi.numProbes);
}

qboolean SHGI_IsValidProbePosition(vec3_t position) {
    trace_t trace;
    vec3_t start, end;
    
    // Check if position is inside solid geometry
    VectorCopy(position, start);
    VectorCopy(position, end);
    end[2] += 16.0f; // Small offset upward
    
    CM_BoxTrace(&trace, start, end, vec3_origin, vec3_origin, 0, CONTENTS_SOLID);
    
    if (trace.allsolid) {
        return qfalse; // Inside solid geometry
    }
    
    // Check minimum distance to existing probes
    float minDistance = shgi.sh_probeSpacing->value * 0.7f;
    
    for (int i = 0; i < shgi.numProbes; i++) {
        float distance = Distance(position, shgi.probes[i].position);
        if (distance < minDistance) {
            return qfalse; // Too close to existing probe
        }
    }
    
    return qtrue;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Real-time SH Updates</h2>
    
    <h3>Compute Shader for Probe Updates</h3>
    <div class="code-block">
        <pre><code>// shgi_update.comp - Compute shader for updating SH probes
#version 450

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) buffer ProbeBuffer {
    vec3 sh_coeffs[9 * MAX_PROBES]; // Interleaved SH coefficients
};

layout(set = 0, binding = 1) uniform ProbeUniforms {
    vec3 probe_positions[MAX_PROBES];
    float probe_radii[MAX_PROBES];
    int num_probes;
    int update_offset;              // Which probes to update this dispatch
    int samples_per_probe;          // Number of hemisphere samples
    float time;                     // Current time for temporal filtering
};

layout(set = 0, binding = 2) uniform sampler2D environment_map;

// Spherical Harmonics basis functions
float SH_basis(int l, int m, vec3 dir) {
    if (l == 0) {
        return 0.282095; // Y_0^0
    } else if (l == 1) {
        if (m == -1) return -0.488603 * dir.y;     // Y_1^-1
        if (m ==  0) return  0.488603 * dir.z;     // Y_1^0
        if (m ==  1) return -0.488603 * dir.x;     // Y_1^1
    } else if (l == 2) {
        if (m == -2) return  1.092548 * dir.x * dir.y;                    // Y_2^-2
        if (m == -1) return -1.092548 * dir.y * dir.z;                    // Y_2^-1
        if (m ==  0) return  0.315392 * (3.0*dir.z*dir.z - 1.0);         // Y_2^0
        if (m ==  1) return -1.092548 * dir.x * dir.z;                    // Y_2^1
        if (m ==  2) return  0.546274 * (dir.x*dir.x - dir.y*dir.y);     // Y_2^2
    }
    return 0.0;
}

// Generate hemisphere sampling direction
vec3 get_hemisphere_direction(int sample_index, int total_samples) {
    float phi = (2.0 * 3.14159265359 * sample_index) / total_samples;
    float cos_theta = sqrt(float(sample_index) / float(total_samples));
    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
    
    return vec3(
        sin_theta * cos(phi),
        sin_theta * sin(phi),
        cos_theta
    );
}

void main() {
    uint probe_index = gl_GlobalInvocationID.x + update_offset;
    
    if (probe_index >= num_probes) {
        return;
    }
    
    vec3 probe_pos = probe_positions[probe_index];
    
    // Accumulate SH coefficients for this probe
    vec3 sh_accum[9];
    for (int i = 0; i < 9; i++) {
        sh_accum[i] = vec3(0.0);
    }
    
    // Sample hemisphere around probe position
    for (int sample = 0; sample < samples_per_probe; sample++) {
        vec3 direction = get_hemisphere_direction(sample, samples_per_probe);
        
        // Sample incoming radiance from environment
        float theta = acos(direction.z);
        float phi = atan(direction.y, direction.x);
        vec2 uv = vec2(phi / (2.0 * 3.14159265359), theta / 3.14159265359);
        vec3 radiance = texture(environment_map, uv).rgb;
        
        // Weight by cosine for diffuse lighting
        float cos_theta = max(0.0, direction.z);
        radiance *= cos_theta;
        
        // Project onto SH basis
        int sh_index = 0;
        for (int l = 0; l <= 2; l++) {
            for (int m = -l; m <= l; m++) {
                float basis_value = SH_basis(l, m, direction);
                sh_accum[sh_index] += radiance * basis_value;
                sh_index++;
            }
        }
    }
    
    // Normalize by number of samples
    float norm_factor = (4.0 * 3.14159265359) / float(samples_per_probe);
    for (int i = 0; i < 9; i++) {
        sh_accum[i] *= norm_factor;
    }
    
    // Temporal filtering to reduce flickering
    float temporal_blend = 0.95; // Keep 95% of previous frame
    
    for (int i = 0; i < 9; i++) {
        int buffer_index = int(probe_index) * 9 + i;
        vec3 prev_coeffs = sh_coeffs[buffer_index];
        sh_coeffs[buffer_index] = mix(sh_accum[i], prev_coeffs, temporal_blend);
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Shader Integration</h2>
    
    <h3>PBR Fragment Shader Integration</h3>
    <div class="code-block">
        <pre><code>// Fragment shader modifications for SHGI integration
layout(set = 2, binding = 0) buffer readonly SHProbeBuffer {
    vec3 sh_coefficients[9 * MAX_PROBES];
};

layout(set = 2, binding = 1) uniform SHUniforms {
    vec3 probe_positions[MAX_PROBES];
    float probe_radii[MAX_PROBES];
    int num_probes;
    float sh_intensity;
};

// Sample spherical harmonics at a given position
vec3 sample_sh_lighting(vec3 world_pos, vec3 normal) {
    if (num_probes == 0) {
        return vec3(0.0);
    }
    
    // Find nearest probes for interpolation
    const int MAX_INFLUENCES = 4;
    int probe_indices[MAX_INFLUENCES];
    float probe_weights[MAX_INFLUENCES];
    int num_influences = 0;
    
    // Find influencing probes
    for (int i = 0; i < num_probes && num_influences < MAX_INFLUENCES; i++) {
        vec3 probe_pos = probe_positions[i];
        float distance = length(world_pos - probe_pos);
        float radius = probe_radii[i];
        
        if (distance < radius) {
            float weight = 1.0 - smoothstep(0.0, radius, distance);
            probe_indices[num_influences] = i;
            probe_weights[num_influences] = weight;
            num_influences++;
        }
    }
    
    if (num_influences == 0) {
        return vec3(0.0);
    }
    
    // Normalize weights
    float total_weight = 0.0;
    for (int i = 0; i < num_influences; i++) {
        total_weight += probe_weights[i];
    }
    
    for (int i = 0; i < num_influences; i++) {
        probe_weights[i] /= total_weight;
    }
    
    // Interpolate SH coefficients
    vec3 interpolated_sh[9];
    for (int sh_index = 0; sh_index < 9; sh_index++) {
        interpolated_sh[sh_index] = vec3(0.0);
        
        for (int i = 0; i < num_influences; i++) {
            int probe_index = probe_indices[i];
            int buffer_index = probe_index * 9 + sh_index;
            interpolated_sh[sh_index] += sh_coefficients[buffer_index] * probe_weights[i];
        }
    }
    
    // Evaluate SH for the given normal direction
    vec3 sh_lighting = vec3(0.0);
    
    // L=0 (DC component)
    sh_lighting += interpolated_sh[0] * 0.282095;
    
    // L=1 (linear components)
    sh_lighting += interpolated_sh[1] * (-0.488603 * normal.y);
    sh_lighting += interpolated_sh[2] * ( 0.488603 * normal.z);
    sh_lighting += interpolated_sh[3] * (-0.488603 * normal.x);
    
    // L=2 (quadratic components) 
    sh_lighting += interpolated_sh[4] * ( 1.092548 * normal.x * normal.y);
    sh_lighting += interpolated_sh[5] * (-1.092548 * normal.y * normal.z);
    sh_lighting += interpolated_sh[6] * ( 0.315392 * (3.0*normal.z*normal.z - 1.0));
    sh_lighting += interpolated_sh[7] * (-1.092548 * normal.x * normal.z);
    sh_lighting += interpolated_sh[8] * ( 0.546274 * (normal.x*normal.x - normal.y*normal.y));
    
    return max(vec3(0.0), sh_lighting * sh_intensity);
}

// Modified main PBR lighting function
vec3 calculate_pbr_lighting(PBRMaterial material, vec3 world_pos, vec3 view_dir, vec3 normal) {
    vec3 albedo = material.base_color.rgb;
    float metallic = material.metallic_factor;
    float roughness = material.roughness_factor;
    
    // Calculate F0 (surface reflection at normal incidence)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    vec3 total_lighting = vec3(0.0);
    
    // Direct lighting (unchanged)
    for (int i = 0; i < num_lights; i++) {
        total_lighting += calculate_direct_light(lights[i], world_pos, view_dir, normal, 
                                               albedo, metallic, roughness, F0);
    }
    
    // Image-based lighting (IBL) - unchanged
    vec3 ibl_lighting = calculate_ibl(view_dir, normal, albedo, metallic, roughness, F0);
    total_lighting += ibl_lighting;
    
    // Spherical Harmonics Global Illumination - NEW
    vec3 shgi_lighting = sample_sh_lighting(world_pos, normal);
    
    // Blend SHGI with albedo for diffuse contribution
    vec3 kS = fresnel_schlick_roughness(max(dot(normal, view_dir), 0.0), F0, roughness);
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic; // Metallic surfaces don't have diffuse
    
    total_lighting += kD * albedo * shgi_lighting;
    
    return total_lighting;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Debug Visualization</h2>
    
    <h3>Console Commands</h3>
    <div class="code-block">
        <pre><code>// Console commands for SHGI debugging
void SHGI_PlaceProbe_f(void) {
    if (Cmd_Argc() != 4) {
        Com_Printf("Usage: shgi_place <x> <y> <z>\n");
        return;
    }
    
    vec3_t position;
    position[0] = atof(Cmd_Argv(1));
    position[1] = atof(Cmd_Argv(2));
    position[2] = atof(Cmd_Argv(3));
    
    if (SHGI_IsValidProbePosition(position)) {
        SHGI_CreateProbe(position, shgi.sh_probeSpacing->value * 0.5f);
        Com_Printf("Placed probe %d at (%.1f, %.1f, %.1f)\n", 
                  shgi.numProbes - 1, position[0], position[1], position[2]);
    } else {
        Com_Printf("Invalid probe position\n");
    }
}

void SHGI_ClearProbes_f(void) {
    shgi.numProbes = 0;
    Com_Printf("Cleared all SHGI probes\n");
}

void SHGI_ForceUpdate_f(void) {
    if (Cmd_Argc() == 1) {
        // Update all probes
        for (int i = 0; i < shgi.numProbes; i++) {
            shgi.probes[i].updatePriority = 1.0f;
            shgi.probes[i].lastUpdateFrame = -1;
        }
        Com_Printf("Marked all %d probes for update\n", shgi.numProbes);
    } else {
        // Update specific probe
        int probeId = atoi(Cmd_Argv(1));
        if (probeId >= 0 && probeId < shgi.numProbes) {
            shgi.probes[probeId].updatePriority = 1.0f;
            shgi.probes[probeId].lastUpdateFrame = -1;
            Com_Printf("Marked probe %d for update\n", probeId);
        } else {
            Com_Printf("Invalid probe ID: %d\n", probeId);
        }
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Performance Considerations</h2>
    
    <div class="feature-list">
        <h3>Optimization Strategies</h3>
        <ul>
            <li><strong>Adaptive Quality:</strong> Dynamically adjust update rates based on performance</li>
            <li><strong>Spatial Culling:</strong> Only update probes near the player or dynamic objects</li>
            <li><strong>Temporal Coherence:</strong> Use filtering to maintain stability across frames</li>
            <li><strong>LOD System:</strong> Different update frequencies for near/far probes</li>
        </ul>
    </div>
    
    <h3>Performance Metrics</h3>
    <div class="code-block">
        <pre><code>// Performance monitoring for SHGI
typedef struct shgiPerformance_s {
    float updateTime;           // Time spent updating probes (ms)
    float renderTime;           // Time spent rendering with SHGI (ms)
    int probesUpdated;          // Number of probes updated this frame
    int totalProbes;            // Total number of active probes
    float memoryUsage;          // GPU memory usage (MB)
} shgiPerformance_t;

void SHGI_PrintPerformance(void) {
    shgiPerformance_t perf;
    SHGI_GatherPerformanceStats(&perf);
    
    Com_Printf("=== SHGI Performance ===\n");
    Com_Printf("Update time: %.2f ms\n", perf.updateTime);
    Com_Printf("Render overhead: %.2f ms\n", perf.renderTime);
    Com_Printf("Probes updated: %d / %d\n", perf.probesUpdated, perf.totalProbes);
    Com_Printf("GPU memory: %.2f MB\n", perf.memoryUsage);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/renderer/vulkan-implementation">Vulkan Renderer</a></li>
        <li><a href="/renderer/pbr-pipeline">PBR Pipeline</a></li>
        <li><a href="/rendering/global-illumination">Global Illumination</a></li>
        <li><a href="/performance/optimization">Performance Optimization</a></li>
        <li><a href="/development/debugging">Debugging Tools</a></li>
    </ul>
</div>