#version 460
#extension GL_EXT_ray_tracing : require

layout( location = 0 ) rayPayloadInEXT vec4 ptPayload;

layout( set = 0, binding = 2, std140 ) uniform PtFrame {
	mat4 invViewProj;
	vec4 viewOrigin;
	vec4 outputSize;
	vec4 traceParams;
} pt;

void main()
{
	vec3 base = vec3( 0.72, 0.70, 0.66 );
	vec3 emissive = vec3( 0.08, 0.07, 0.05 );
	ptPayload = vec4( base + emissive, 1.0 );
}
