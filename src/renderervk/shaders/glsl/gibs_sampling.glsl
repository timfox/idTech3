// GIBS Surfel Sampling Functions
// Include this in PBR shaders to sample surfel-based indirect lighting

#ifndef GIBS_SAMPLING_GLSL
#define GIBS_SAMPLING_GLSL

#include "gibs_surfel.glsl"

// Simple spatial hash function for grid-based acceleration structure
// This is a basic implementation - a full spatial hash would require
// a separate buffer storing grid cells and surfel indices
uint spatialHash(vec3 pos, float cellSize) {
    ivec3 cell = ivec3(floor(pos / cellSize));
    // Simple hash function combining cell coordinates
    return uint(cell.x * 73856093u) ^ uint(cell.y * 19349663u) ^ uint(cell.z * 83492791u);
}

// Sample nearby surfels for indirect lighting
vec3 sampleGIBSIrradiance(vec3 worldPos, vec3 normal) {
    vec3 indirectLight = vec3(0.0);
    float totalWeight = 0.0;
    
    // Search radius based on surfel radius
    float searchRadius = ubo.surfelRadius * 2.0;
    float cellSize = searchRadius; // Grid cell size matches search radius
    
    // Sample nearby surfels
    // TODO: Implement full spatial acceleration structure (grid hash or octree)
    // For now, use improved sampling with spatial hash-based selection
    uint sampleCount = min(ubo.surfelCount, 64u); // Limit samples for performance
    uint step = max(1u, ubo.surfelCount / sampleCount);
    
    // Use spatial hash to bias sampling toward nearby cells
    uint baseHash = spatialHash(worldPos, cellSize);
    
    for (uint i = 0; i < sampleCount; i++) {
        // Use hash-based index selection to improve spatial locality
        uint hashIndex = (baseHash + i * 12345u) % ubo.surfelCount;
        uint index = hashIndex;
        
        if (index >= ubo.surfelCount) break;
        
        Surfel surfel = surfels[index];
        
        // Skip inactive surfels
        if ((surfel.flags & GIBS_SURFEL_ACTIVE) == 0) continue;
        if ((surfel.flags & GIBS_SURFEL_VALID) == 0) continue;
        
        // Calculate distance to surfel
        vec3 toSurfel = surfel.position - worldPos;
        float distance = length(toSurfel);
        
        // Skip if too far
        if (distance > searchRadius) continue;
        
        // Check if surfel normal is compatible
        float normalDot = dot(normal, surfel.normal);
        if (normalDot < 0.0) continue; // Skip back-facing surfels
        
        // Weight based on distance and normal alignment
        float distanceWeight = 1.0 - smoothstep(0.0, searchRadius, distance);
        float normalWeight = max(0.0, normalDot);
        float confidenceWeight = surfel.confidence;
        
        float weight = distanceWeight * normalWeight * confidenceWeight;
        
        if (weight > 0.001) {
            indirectLight += surfel.irradiance * weight;
            totalWeight += weight;
        }
    }
    
    // Normalize and apply intensity
    if (totalWeight > 0.001) {
        indirectLight = indirectLight / totalWeight * ubo.intensity;
    }
    
    return indirectLight;
}

#endif // GIBS_SAMPLING_GLSL

