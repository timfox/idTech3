// Example fragment shader demonstrating SDF grid ray tracing
// This shows how to use the sdf_grid_raytrace.glsl library

#version 450

// Include the SDF grid ray tracing library
// Note: In a real implementation, you would use proper include paths
// #include "sdf_grid_raytrace.glsl"

// For this example, we'll define a simple inline version
// In practice, you would include the full library

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

// Example: Simple SDF scene definition
// In a real implementation, this would come from a 3D texture or buffer
float map(vec3 p) {
    // Simple sphere SDF
    float sphere = length(p - vec3(0.0, 0.0, 0.0)) - 0.5;
    
    // Simple plane SDF
    float plane = p.y + 0.5;
    
    // Union
    return min(sphere, plane);
}

// Simple sphere tracing for comparison
float sphereTrace(vec3 ro, vec3 rd, float maxDist, int maxSteps) {
    float t = 0.0;
    for (int i = 0; i < maxSteps; i++) {
        vec3 p = ro + rd * t;
        float d = map(p);
        if (d < 0.001) return t;
        if (t > maxDist) break;
        t += d;
    }
    return -1.0; // No hit
}

// Simple normal computation using central differences
vec3 computeNormal(vec3 p) {
    vec2 e = vec2(0.001, 0.0);
    return normalize(vec3(
        map(p + e.xyy) - map(p - e.xyy),
        map(p + e.yxy) - map(p - e.yxy),
        map(p + e.yyx) - map(p - e.yyx)
    ));
}

void main() {
    // Screen space coordinates
    vec2 uv = v_texCoord * 2.0 - 1.0;
    uv.x *= 16.0 / 9.0; // Aspect ratio correction
    
    // Camera setup
    vec3 ro = vec3(0.0, 0.0, 3.0);  // Ray origin
    vec3 rd = normalize(vec3(uv, -1.0));  // Ray direction
    
    // Simple sphere tracing
    float t = sphereTrace(ro, rd, 10.0, 64);
    
    if (t > 0.0) {
        vec3 p = ro + rd * t;
        vec3 n = computeNormal(p);
        
        // Simple lighting
        vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
        float diff = max(dot(n, lightDir), 0.0);
        
        fragColor = vec4(vec3(0.5, 0.7, 1.0) * (0.3 + 0.7 * diff), 1.0);
    } else {
        // Background
        fragColor = vec4(0.1, 0.1, 0.15, 1.0);
    }
    
    // NOTE: This is a simplified example using sphere tracing.
    // To use the actual SDF grid ray tracing library, you would:
    // 1. Load SDF grid data from a 3D texture or buffer
    // 2. Traverse the grid using one of the methods (GST, SVS, SBS, SVO)
    // 3. When reaching a voxel, use intersectVoxelSDF() to find intersections
    // 4. Compute normals using computeAnalyticNormal() or computeContinuousNormal()
    // 5. Shade the surface
}
