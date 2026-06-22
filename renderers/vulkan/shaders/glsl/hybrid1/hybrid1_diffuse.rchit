#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#include "hybrid1_ubo.glsl"
#include "hybrid1_hit.glsl"

layout( location = 0 ) rayPayloadInEXT vec3 diffuseRadiance;

layout( set = 0, binding = 8 ) uniform sampler2D albedoTex;

void main()
{
	vec3 base = hybrid1_sampleHitAlbedo( albedoTex );
	vec3 emissive = vec3( 0.04, 0.03, 0.02 );
	diffuseRadiance = base + emissive;
}
