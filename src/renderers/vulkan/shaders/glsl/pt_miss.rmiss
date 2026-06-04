#version 460
#extension GL_EXT_ray_tracing : require

layout( location = 0 ) rayPayloadInEXT vec4 ptPayload; /* rgb + hitFlag in w (0=miss) */

void main()
{
	ptPayload = vec4( 0.35, 0.52, 0.85, 0.0 );
}
