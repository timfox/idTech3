/*
===========================================================================
Ray Generation Shader (HLSL)
===========================================================================
*/

#include "rt_common.hlsl"

[shader("raygeneration")]
void RayGenShader()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 dimensions = DispatchRaysDimensions().xy;
    
    // Bounds check
    if (pixel.x >= dimensions.x || pixel.y >= dimensions.y)
    {
        return;
    }
    
    float2 pixelCenter = float2(pixel) + float2(0.5, 0.5);
    float2 uv = pixelCenter / g_CameraConstants.resolution;
    
    // Debug mode: gradient test
    if (g_CameraConstants.debugMagenta != 0)
    {
        g_Output[pixel] = float4(uv, 0.0, 1.0);
        return;
    }
    
    // Get camera position and ray direction
    float3 origin = GetWorldRayOrigin(g_CameraConstants.viewInverse);
    float3 direction = GetWorldRayDirection(uv, g_CameraConstants.viewInverse, g_CameraConstants.projInverse);
    
    // Add jitter for anti-aliasing
    if (g_CameraConstants.samplesPerPixel > 1)
    {
        float jx = GetRandom(pixel, g_CameraConstants.frameIndex, 0);
        float jy = GetRandom(pixel, g_CameraConstants.frameIndex, 1);
        float2 jitter = (float2(jx, jy) - 0.5) / g_CameraConstants.resolution;
        direction = GetWorldRayDirection(uv + jitter, g_CameraConstants.viewInverse, g_CameraConstants.projInverse);
    }
    
    // Initialize payload
    RayPayload payload;
    payload.color = float3(0.0, 0.0, 0.0);
    payload.hitDistance = 0.0;
    
    // Trace primary ray
    uint rayFlags = RAY_FLAG_NONE;
    float tMin = g_CameraConstants.nearPlane;
    float tMax = g_CameraConstants.farPlane;
    
    TraceRay(
        g_AccelerationStructure,    // AccelerationStructure
        rayFlags,                   // RayFlags
        0xFF,                       // InstanceInclusionMask
        0,                          // RayContributionToHitGroupIndex
        0,                          // MultiplierForGeometryContributionToHitGroupIndex
        0,                          // MissShaderIndex
        origin,                     // Origin
        tMin,                       // TMin
        direction,                  // Direction
        tMax,                       // TMax
        0,                          // Payload
        payload                     // Payload
    );
    
    // Apply exposure and write output
    float3 finalColor = payload.color * g_CameraConstants.exposure;
    g_Output[pixel] = float4(finalColor, 1.0);
}

