#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main()
{
	/* Shadow miss = unoccluded (lit). Hit path uses SkipClosestHit so payload stays 0. */
	hitValue = vec3( 1.0 );
}
