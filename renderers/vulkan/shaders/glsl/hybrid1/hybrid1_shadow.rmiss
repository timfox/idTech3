#version 460
#extension GL_EXT_ray_tracing : require

layout( location = 0 ) rayPayloadInEXT vec2 shadowPayload;

void main()
{
	shadowPayload = vec2( 1.0, gl_RayTmaxEXT );
}
