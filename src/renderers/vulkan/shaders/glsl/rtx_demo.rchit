#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
	/* Simple diffuse-style albedo for BSP hits (no texture fetch yet). */
	hitValue = vec3( 0.72, 0.70, 0.66 );
}
