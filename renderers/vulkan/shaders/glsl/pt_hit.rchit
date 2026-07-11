#version 460
#extension GL_EXT_ray_tracing : require

layout( location = 0 ) rayPayloadInEXT vec4 ptPayload;

layout( set = 0, binding = 2, std140 ) uniform PtFrame {
	mat4 invViewProj;
	vec4 viewOrigin;
	vec4 outputSize;
	vec4 traceParams;
} pt;

layout( set = 0, binding = 6, std430 ) readonly buffer WorldAlbedoSSBO {
	float rgb[];
} worldAlbedo;

void main()
{
	vec3 base = vec3( 0.72, 0.70, 0.66 );
	vec3 emissive = vec3( 0.08, 0.07, 0.05 );
	uint n = uint( worldAlbedo.rgb.length() ) / 3u;

	if ( gl_InstanceCustomIndexEXT == 0 && n > 0u &&
		gl_PrimitiveID >= 0 && uint( gl_PrimitiveID ) < n ) {
		uint i = uint( gl_PrimitiveID ) * 3u;
		base = vec3( worldAlbedo.rgb[i], worldAlbedo.rgb[i + 1u], worldAlbedo.rgb[i + 2u] );
	}

	ptPayload = vec4( base + emissive, 1.0 );
}
