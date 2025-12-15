// Shared random-number utilities for ray tracing.
// Can optionally use a blue-noise texture array when USE_BLUE_NOISE is defined,
// otherwise falls back to hash-based RNG.

#ifndef RT_RANDOM_GLSL
#define RT_RANDOM_GLSL

// Constants for blue-noise-style indexing (kept in sync with any host-side code).
#define NUM_BLUE_NOISE_TEX 128u
#define BLUE_NOISE_RES     256u

#ifdef USE_BLUE_NOISE

// Blue-noise texture array. When enabled, this should be bound in set 0 at
// binding 3 in the ray tracing descriptor set layout.
layout(binding = 3, set = 0) uniform sampler2DArray u_blueNoise;

// Get a blue noise sample
float rt_getBlueNoise(uvec2 pixel, uint frameIndex, uint sampleIndex) {
    uint frame_offset = frameIndex / NUM_BLUE_NOISE_TEX;
    uvec3 p = uvec3(pixel.x + frame_offset, pixel.y + (frame_offset << 4), sampleIndex);
    p.x %= BLUE_NOISE_RES;
    p.y %= BLUE_NOISE_RES;
    p.z %= NUM_BLUE_NOISE_TEX;
    return texelFetch(u_blueNoise, ivec3(p), 0).r;
}

// Get a blue noise sample that repeats per frame (for temporal accumulation)
float rt_getBlueNoiseRepeated(uvec2 pixel, uint sampleIndex) {
    uvec3 p = uvec3(pixel.x, pixel.y, sampleIndex);
    p.x %= BLUE_NOISE_RES;
    p.y %= BLUE_NOISE_RES;
    p.z %= NUM_BLUE_NOISE_TEX;
    return texelFetch(u_blueNoise, ivec3(p), 0).r;
}

// Generic RNG function, chooses between repeated and non-repeated blue noise
// or falls back to hash-based RNG.
float rt_getRNG(bool repeatPerFrame, uint sampleIndex, uvec2 pixel, uint frameIndex) {
    if (repeatPerFrame) {
        return rt_getBlueNoiseRepeated(pixel, sampleIndex);
    } else {
        return rt_getBlueNoise(pixel, frameIndex, sampleIndex);
    }
}

#else // USE_BLUE_NOISE

// Fallback: use hash-based RNG from rt_helpers.glsl.
// These declarations must match the existing helpers.
uint hash(uint x);
float random(uint seed);
vec2 random2(uint seed);

float rt_getRNG(bool /*repeatPerFrame*/, uint sampleIndex, uvec2 pixel, uint frameIndex) {
    // Fallback to hash-based random if blue noise is not enabled
    uint seed = hash(pixel.x + pixel.y * 10000u + frameIndex * 1000000u + sampleIndex);
    return random(seed);
}

#endif // USE_BLUE_NOISE

#endif // RT_RANDOM_GLSL