#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#include "hybrid1_ubo.glsl"
#include "hybrid1_ibl.glsl"

layout( location = 0 ) rayPayloadInEXT vec4 specRadiance;

layout( set = 0, binding = 6 ) uniform samplerCube prefilterTex;

void main()
{
	if ( h1.params1.w > 0.5 ) {
		specRadiance.rgb = hybrid1_samplePrefilter( prefilterTex, gl_WorldRayDirectionEXT, specRadiance.w );
	} else {
		specRadiance.rgb = vec3( 0.35, 0.42, 0.55 );
	}
}
