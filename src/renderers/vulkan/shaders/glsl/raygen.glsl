#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "rt_defines.glsl"
#include "rt_helpers.glsl"
#include "rt_random.glsl"

// Ray tracing structures
struct RayPayload {
    vec3 color;
    uint recursion_depth;
    vec3 normal;
    vec2 uv;
    uint material_id;
};

struct ShadowPayload {
    float shadowFactor;
};

// Acceleration structure
layout(binding = 0, set = 0) uniform accelerationStructureEXT tlas;

// Output image
layout(binding = 1, set = 0, rgba8) uniform image2D output_image;

// Camera uniform buffer
layout(binding = 2, set = 0) uniform CameraUBO {
    mat4 view_inverse;
    mat4 proj_inverse;
    vec3 camera_position;
    float near_plane;
    float far_plane;
    uint frame_index;
} camera;

// Use existing material system - material_params_t structure
// This matches the Vulkan renderer's material system
struct MaterialData {
    // Dynamic state
    float wetness;
    float damage;
    float corruption;
    float magicGlow;
    float temperature;
    float time;
    vec3 magicColor;
    float _pad0;
    vec3 damageColor;
    float _pad1;

    // Layered/PBR baseline
    vec3 baseColor;
    float roughness;
    vec3 emissive;
    float metallic;
    float normalScale;
    float clearcoat;
    float clearcoatRoughness;
    float anisotropy;
    vec3 anisotropyDirection;
    float _pad2;
    vec3 subsurfaceColor;
    float subsurfaceScale;
    vec3 sheenColor;
    float sheenRoughness;

    // Procedural parameters
    vec4 customParams[4]; // User-defined parameters

    // System flags
    uint flags;
};

layout(binding = 3, set = 0) buffer Materials { MaterialData materials[]; };

// Light data
struct LightData {
    vec4 position; // w = type
    vec4 color;    // w = intensity
    vec4 direction;// w = cone angle (for spot lights)
    vec4 params;   // x = radius, y = falloff
};

layout(binding = 4, set = 0) buffer Lights { LightData lights[]; };
layout(binding = 5, set = 0) uniform LightCount { uint light_count; };

// Texture samplers
layout(binding = 6, set = 0) uniform sampler2D textures[];

// Per-surface material indices
layout(binding = 7, set = 0) buffer SurfaceMaterialIndices { uint surfaceMaterialIndices[]; };

layout(push_constant) uniform PushConstants {
    uint max_recursion_depth;
    uint samples_per_pixel;
    uint enable_shadows;
    uint enable_reflections;
    uint enable_gi;
    uint gi_samples;
} push_constants;

layout(location = 0) rayPayloadEXT RayPayload payload;
layout(location = 1) rayPayloadEXT ShadowPayload shadow_payload;

vec3 linear_to_srgb(vec3 color) {
    return mix(
        pow(color, vec3(1.0 / 2.4)) * 1.055 - 0.055,
        color * 12.92,
        step(color, vec3(0.0031308))
    );
}

vec3 sample_environment(vec3 dir) {
    // Simple sky gradient
    float t = 0.5 * (dir.y + 1.0);
    return mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), t) * 0.5;
}

vec3 compute_lighting(vec3 position, vec3 normal, vec3 view_dir, MaterialData material, uint recursion_depth) {
    vec3 color = material.emissive;

    // Ambient
    color += material.baseColor.rgb * 0.1;

    // Direct lighting (simplified)
    for (uint i = 0; i < min(light_count, 8u); ++i) {
        LightData light = lights[i];
        vec3 light_dir;
        float attenuation = 1.0;

        if (light.position.w == LIGHT_TYPE_DIRECTIONAL) {
            light_dir = normalize(-light.direction.xyz);
            attenuation = 1.0;
        } else {
            // Point light
            vec3 light_vec = light.position.xyz - position;
            float distance = length(light_vec);
            light_dir = light_vec / distance;
            attenuation = 1.0 / (1.0 + distance * distance * 0.1);
        }

        float n_dot_l = max(dot(normal, light_dir), 0.0);

        if (n_dot_l > 0.0) {
            // Shadow test
            if (push_constants.enable_shadows != 0) {
                shadow_payload.shadowFactor = 1.0; // Default to fully lit
                vec3 shadow_origin = position + normal * 0.001; // Small offset to avoid self-shadowing
                traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT,
                           0xFF, RT_SHADOW_RAY_INDEX, 0, RT_SHADOW_RAY_INDEX,
                           shadow_origin, 0.001, light_dir, 1000.0, 1);

                if (shadow_payload.shadowFactor < 0.5) {
                    continue; // Skip this light if in shadow
                }
            }

            // PBR lighting using material system parameters
        vec3 albedo = material.baseColor.rgb * (1.0 - material.metallic);

        // Apply material damage/corruption effects
        if ((material.flags & 2) != 0) { // MATERIAL_DAMAGED
            albedo = mix(albedo, material.damageColor, material.damage);
        }
        if ((material.flags & 4) != 0) { // MATERIAL_MAGICAL
            albedo += material.magicColor * material.magicGlow;
        }

        // Diffuse (Lambert)
        vec3 diffuse = albedo * n_dot_l;

        // Specular (GGX-based)
        vec3 halfway = normalize(light_dir + view_dir);
        float n_dot_h = max(dot(normal, halfway), 0.0);
        float roughness_sq = material.roughness * material.roughness;
        float denom = n_dot_h * n_dot_h * (roughness_sq - 1.0) + 1.0;
        float D = roughness_sq / (PI * denom * denom);

        float k = (material.roughness + 1.0) * (material.roughness + 1.0) / 8.0;
        float G = n_dot_l / (n_dot_l * (1.0 - k) + k) *
                  n_dot_l / (n_dot_l * (1.0 - k) + k); // Simplified for single-sided

        vec3 F0 = mix(vec3(0.04), albedo, material.metallic);
        vec3 F = F0 + (1.0 - F0) * pow(1.0 - max(dot(view_dir, halfway), 0.0), 5.0);

        vec3 specular = (D * G * F) / max(4.0 * n_dot_l * max(dot(normal, view_dir), 0.0), 0.001);

        color += (diffuse * (1.0 - F) + specular) * light.color.rgb * light.color.w * attenuation;
        }
    }

    // Reflections
    if (push_constants.enable_reflections != 0 && recursion_depth < push_constants.max_recursion_depth) {
        float reflectivity = material.metallic + material.clearcoat * 0.1;
        if (reflectivity > 0.01) {
            vec3 reflect_dir = reflect(-view_dir, normal);
            RayPayload reflect_payload;
            reflect_payload.recursion_depth = recursion_depth + 1;

            traceRayEXT(tlas, gl_RayFlagsOpaqueEXT, 0xFF, RT_PRIMARY_RAY_INDEX, 0, RT_PRIMARY_RAY_INDEX,
                       position + normal * 0.001, 0.001, reflect_dir, 1000.0, 0);

            vec3 reflect_color = reflect_payload.color;
            if (reflect_color == vec3(0.0)) {
                reflect_color = sample_environment(reflect_dir);
            }
            color += reflect_color * reflectivity * 0.5; // Blend with reflection
        }
    }

    // Global Illumination
    if (push_constants.enable_gi != 0 && recursion_depth == 0) {
        vec3 gi_color = sample_global_illumination(position, normal, recursion_depth);
        color += gi_color * material.baseColor.rgb * 0.3; // Subtle GI contribution
    }

    return color;
}

vec3 sample_global_illumination(vec3 position, vec3 normal, uint recursion_depth) {
    if (recursion_depth >= push_constants.max_recursion_depth) {
        return sample_environment(normal);
    }

    vec3 gi_color = vec3(0.0);
    uint samples = min(push_constants.gi_samples, 4u);

    for (uint i = 0; i < samples; ++i) {
        // Generate random direction in hemisphere
        vec2 rand_uv = vec2(random_float(), random_float());
        vec3 sample_dir = random_hemisphere_direction(normal, rand_uv);

        RayPayload gi_payload;
        gi_payload.recursion_depth = recursion_depth + 1;

        traceRayEXT(tlas, gl_RayFlagsOpaqueEXT, 0xFF, RT_PRIMARY_RAY_INDEX, 0, RT_PRIMARY_RAY_INDEX,
                   position + normal * 0.001, 0.001, sample_dir, 50.0, 0);

        vec3 sample_color = gi_payload.color;
        if (sample_color == vec3(0.0)) {
            sample_color = sample_environment(sample_dir);
        }

        gi_color += sample_color;
    }

    return gi_color / float(samples);
}

void main() {
    // Calculate ray direction
    vec2 pixel_center = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    vec2 uv = pixel_center / vec2(gl_LaunchSizeEXT.xy);
    uv = uv * 2.0 - 1.0; // Convert to NDC

    vec4 origin = camera.view_inverse * vec4(0.0, 0.0, 0.0, 1.0);
    vec4 target = camera.proj_inverse * vec4(uv.x, uv.y, 1.0, 1.0);
    target = camera.view_inverse * vec4(target.xyz / target.w, 1.0);

    vec3 ray_origin = origin.xyz / origin.w;
    vec3 ray_direction = normalize(target.xyz / target.w - ray_origin);

    // Initialize payload
    payload.color = vec3(0.0);
    payload.recursion_depth = 0;
    payload.material_id = 0;

    // Trace primary ray
    traceRayEXT(tlas, gl_RayFlagsOpaqueEXT, 0xFF, RT_PRIMARY_RAY_INDEX, 0, RT_PRIMARY_RAY_INDEX,
               ray_origin, camera.near_plane, ray_direction, camera.far_plane, 0);

    // If we hit nothing, sample environment
    if (payload.color == vec3(0.0)) {
        payload.color = sample_environment(ray_direction);
    } else {
        // Compute lighting for the hit point
        MaterialData material = materials[payload.material_id];
        vec3 hit_position = ray_origin + ray_direction * (camera.near_plane + length(payload.normal) * 0.1);
        vec3 view_dir = normalize(camera.camera_position - hit_position);
        payload.color = compute_lighting(hit_position, payload.normal, view_dir, material, payload.recursion_depth);
    }

    // Tone mapping and output
    vec3 final_color = linear_to_srgb(payload.color);
    imageStore(output_image, ivec2(gl_LaunchIDEXT.xy), vec4(final_color, 1.0));
}