/*
===========================================================================
Miss Shader (HLSL)
===========================================================================
*/

#include "rt_common.hlsl"

[shader("miss")]
void MissShader(inout RayPayload payload : SV_RayPayload)
{
    // Sky color (simple gradient)
    float3 skyColor = float3(0.5, 0.7, 1.0);
    payload.color = skyColor;
    payload.hitDistance = 1000.0;
}

