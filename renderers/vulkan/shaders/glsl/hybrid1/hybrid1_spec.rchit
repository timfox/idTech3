#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#include "hybrid1_ubo.glsl"
#include "hybrid1_ibl.glsl"
#include "hybrid1_hit.glsl"
#include "hybrid1_brdf.glsl"

layout( location = 0 ) rayPayloadInEXT vec4 specRadiance;

layout( set = 0, binding = 6 ) uniform samplerCube prefilterTex;
layout( set = 0, binding = 8 ) uniform sampler2D albedoTex;
layout( set = 0, binding = 10 ) uniform sampler2D brdfLut;

void main()
{
	vec3 base = hybrid1_sampleHitAlbedo( albedoTex );
	float roughness = clamp( specRadiance.w, 0.02, 1.0 );
	vec3 hit = base;

	float iblMode = h1.params3.y;
	if ( h1.params1.w > 0.5 && iblMode >= 0.5 ) {
		vec3 ibl = hybrid1_samplePrefilter( prefilterTex, gl_WorldRayDirectionEXT, roughness );
		if ( iblMode >= 1.5 ) {
			float NdotV = clamp( abs( dot( normalize( -gl_WorldRayDirectionEXT ), vec3( 0.0, 0.0, 1.0 ) ) ), 0.0, 1.0 );
			/* Prefer payload.w-stored roughness with approximate NdotV from ray; primary uses G-buffer */
			vec2 envBrdf = hybrid1_sampleEnvBRDF( brdfLut, NdotV, roughness );
			vec3 F0 = mix( vec3( 0.04 ), base, clamp( 1.0 - roughness, 0.0, 1.0 ) );
			hit += ibl * ( F0 * envBrdf.x + envBrdf.y );
		} else {
			hit += ibl * ( 1.0 - roughness );
		}
	} else {
		hit += vec3( 0.02 );
	}

	specRadiance.rgb = hit;
}
