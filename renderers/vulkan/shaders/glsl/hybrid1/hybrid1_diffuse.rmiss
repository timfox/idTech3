#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#include "hybrid1_ubo.glsl"
#include "hybrid1_ibl.glsl"

layout( location = 0 ) rayPayloadInEXT vec3 diffuseRadiance;

layout( set = 0, binding = 7 ) uniform samplerCube irradianceTex;

void main()
{
	if ( h1.params1.w > 0.5 ) {
		diffuseRadiance = hybrid1_sampleIrradiance( irradianceTex, gl_WorldRayDirectionEXT );
	} else {
		diffuseRadiance = vec3( 0.35, 0.42, 0.55 );
	}
}
