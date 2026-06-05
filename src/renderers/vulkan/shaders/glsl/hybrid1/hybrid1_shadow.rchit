#version 460
#extension GL_EXT_ray_tracing : require

layout( location = 0 ) rayPayloadInEXT float shadowVis;

void main()
{
	shadowVis = 0.0;
}
