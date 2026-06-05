#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#include "hybrid1_ubo.glsl"
#include "hybrid1_ibl.glsl"
#include "hybrid1_hit.glsl"

layout( location = 0 ) rayPayloadInEXT vec4 specRadiance;

layout( set = 0, binding = 6 ) uniform samplerCube prefilterTex;
layout( set = 0, binding = 8 ) uniform sampler2D albedoTex;

void main()
{
	vec3 base = hybrid1_sampleHitAlbedo( albedoTex );
	vec3 emissive = vec3( 0.06, 0.05, 0.04 );
	vec3 hit = base + emissive;

	if ( h1.params1.w > 0.5 ) {
		vec3 ibl = hybrid1_samplePrefilter( prefilterTex, gl_WorldRayDirectionEXT, specRadiance.w );
		hit += ibl * ( 1.0 - specRadiance.w );
	}

	specRadiance.rgb = hit;
}
