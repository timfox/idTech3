#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#include "raygun_common.glsl"

layout( location = 0 ) rayPayloadInEXT vec3 primaryColor;

void main()
{
	primaryColor = rgSkyColor( normalize( gl_WorldRayDirectionEXT ) );
}
