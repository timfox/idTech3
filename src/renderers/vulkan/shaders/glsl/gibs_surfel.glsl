// GIBS Surfel Data Structure
// Matches CPU-side SurfelGPU structure (std430 layout)

struct Surfel {
    vec3 position;      // World space position
    vec3 normal;        // Surface normal
    float radius;       // Surfel radius
    vec3 irradiance;    // Cached indirect irradiance (RGB)
    float confidence;   // Confidence value (0-1)
    uint age;           // Age in frames
    uint flags;         // Surfel flags
};

#define GIBS_SURFEL_ACTIVE 0x01
#define GIBS_SURFEL_VALID  0x02
#define GIBS_SURFEL_STALE  0x04

// Uniform buffer for GIBS compute shaders
layout(std140, binding = 0) uniform GIBSUniforms {
    mat4 viewInverse;
    mat4 projInverse;
    vec3 cameraPos;
    float time;
    uint surfelCount;
    uint frameIndex;
    float surfelRadius;
    float maxRayDistance;
    uint samplesPerSurfel;
    float intensity;
    uint updateRate;
    uint64_t tlasAddress;
} ubo;

// Surfel storage buffer
layout(std430, binding = 1) restrict buffer SurfelBuffer {
    Surfel surfels[];
};

// Helper functions
vec3 sampleHemisphere(vec2 u, vec3 normal) {
    // Cosine-weighted hemisphere sampling
    float z = u.x;
    float r = sqrt(max(0.0, 1.0 - z * z));
    float phi = 2.0 * PI * u.y;
    
    vec3 localDir = vec3(r * cos(phi), r * sin(phi), z);
    
    // Build orthonormal basis from normal
    vec3 tangent = normalize(cross(normal, vec3(1.0, 0.0, 0.0)));
    if (length(tangent) < 0.001) {
        tangent = normalize(cross(normal, vec3(0.0, 1.0, 0.0)));
    }
    vec3 bitangent = cross(normal, tangent);
    
    return normalize(tangent * localDir.x + bitangent * localDir.y + normal * localDir.z);
}

float computeSurfelConfidence(float distance, float maxDistance) {
    // Confidence decreases with distance
    return 1.0 - smoothstep(0.0, maxDistance, distance);
}

