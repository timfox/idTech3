#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#include "hybrid1_ubo.glsl"
#include "hybrid1_ibl.glsl"
#include "hybrid1_hit.glsl"

layout( location = 0 ) rayPayloadInEXT vec3 diffuseRadiance;

layout( set = 0, binding = 7 ) uniform samplerCube irradianceTex;
layout( set = 0, binding = 8 ) uniform sampler2D albedoTex;

void main()
{
	vec3 base = hybrid1_sampleHitAlbedo( albedoTex );
	vec3 N = hybrid1_sampleHitNormal();
	vec3 hit = base * 0.15;

	if ( h1.params3.z > 0.5 ) {
		vec3 L = normalize( h1.sunDirection.xyz );
		float sunScale = max( h1.outputSize.z, 0.0 );
		float ndl = max( dot( N, L ), 0.0 );
		hit += base * sunScale * ndl * 0.35;
		if ( h1.params1.w > 0.5 ) {
			hit += base * hybrid1_sampleIrradiance( irradianceTex, N );
		}
	} else {
		hit += vec3( 0.02 );
	}

	diffuseRadiance = hit;
}
