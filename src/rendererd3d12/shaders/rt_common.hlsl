/*
===========================================================================
Ray Tracing Common Definitions (HLSL)
===========================================================================
*/

#ifndef RT_COMMON_HLSL
#define RT_COMMON_HLSL

// Ray payload structure
struct RayPayload
{
    float3 color;
    float hitDistance;
};

// Ray attributes
struct Attributes
{
    float2 barycentrics;
};

// Camera constants
struct CameraConstants
{
    float4x4 viewInverse;
    float4x4 projInverse;
    float3 position;
    float nearPlane;
    float3 direction;
    float farPlane;
    float2 resolution;
    float exposure;
    int frameIndex;
    int samplesPerPixel;
    int debugMagenta;
};

// Acceleration structure
RaytracingAccelerationStructure g_AccelerationStructure : register(t0);

// Output image
RWTexture2D<float4> g_Output : register(u0);

// Camera constants buffer
ConstantBuffer<CameraConstants> g_CameraConstants : register(b0);

// Helper function to get world ray origin
float3 GetWorldRayOrigin(float4x4 viewInverse)
{
    return viewInverse[3].xyz;
}

// Helper function to get world ray direction
float3 GetWorldRayDirection(float2 uv, float4x4 viewInverse, float4x4 projInverse)
{
    float4 clipSpace = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    clipSpace.y = -clipSpace.y; // Flip Y
    
    float4 viewSpace = mul(projInverse, clipSpace);
    viewSpace /= viewSpace.w;
    
    float4 worldSpace = mul(viewInverse, viewSpace);
    return normalize(worldSpace.xyz);
}

// Simple hash function for RNG
uint hash(uint x)
{
    x ^= x >> 16;
    x *= 0x85ebca6b;
    x ^= x >> 13;
    x *= 0xc2b2ae35;
    x ^= x >> 16;
    return x;
}

// Generate random number
float GetRandom(uint2 pixel, uint frameIndex, uint offset)
{
    uint seed = hash(pixel.x + hash(pixel.y + hash(frameIndex + offset)));
    return float(seed) / float(0xFFFFFFFF);
}

#endif // RT_COMMON_HLSL

