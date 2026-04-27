#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
	hitValue = vec3( 0.95, 0.15, 0.15 );
}
