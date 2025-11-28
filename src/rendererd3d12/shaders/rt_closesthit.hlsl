/*
===========================================================================
Closest Hit Shader (HLSL)
===========================================================================
*/

#include "rt_common.hlsl"

[shader("closesthit")]
void ClosestHitShader(inout RayPayload payload : SV_RayPayload, in Attributes attr : SV_IntersectionAttributes)
{
    // Simple shading based on hit distance and normal
    // This is a placeholder - will be expanded with proper material system
    float3 hitColor = float3(0.8, 0.8, 0.8);
    
    // Apply simple distance-based attenuation
    float attenuation = 1.0 / (1.0 + payload.hitDistance * 0.01);
    payload.color = hitColor * attenuation;
}

