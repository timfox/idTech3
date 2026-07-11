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

void main()
{
	vec3 base = hybrid1_sampleHitAlbedo( albedoTex );
	float roughness = clamp( specRadiance.w, 0.02, 1.0 );
	vec3 hit = base;

	float iblMode = h1.params3.y;
	if ( h1.params1.w > 0.5 && iblMode >= 0.5 ) {
		vec3 ibl = hybrid1_samplePrefilter( prefilterTex, gl_WorldRayDirectionEXT, roughness );
		if ( iblMode >= 1.5 ) {
			/* Split-sum style: weight by (1-rough) approx EnvBRDF without LUT bind */
			hit += ibl * mix( 0.15, 1.0, 1.0 - roughness );
		} else {
			hit += ibl * ( 1.0 - roughness );
		}
	} else {
		hit += vec3( 0.02 );
	}

	specRadiance.rgb = hit;
}
