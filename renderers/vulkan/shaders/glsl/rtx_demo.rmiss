#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
	/* Sky tint */
	hitValue = vec3( 0.35, 0.52, 0.85 );
}
