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

layout( set = 0, binding = 7, std430 ) readonly buffer WorldNormalSSBO {
	float nxyz[];
} worldNormal;

vec3 sampleWorldAlbedo( void )
{
	uint n = uint( worldAlbedo.rgb.length() ) / 3u;
	if ( gl_InstanceCustomIndexEXT != 0 || n == 0u ||
		gl_PrimitiveID < 0 || uint( gl_PrimitiveID ) >= n ) {
		return vec3( 0.72, 0.70, 0.66 );
	}
	uint i = uint( gl_PrimitiveID ) * 3u;
	return vec3( worldAlbedo.rgb[i], worldAlbedo.rgb[i + 1u], worldAlbedo.rgb[i + 2u] );
}

vec3 sampleWorldNormal( void )
{
	uint n = uint( worldNormal.nxyz.length() ) / 3u;
	vec3 towardRay = normalize( -gl_WorldRayDirectionEXT );
	if ( gl_InstanceCustomIndexEXT != 0 || n == 0u ||
		gl_PrimitiveID < 0 || uint( gl_PrimitiveID ) >= n ) {
		return towardRay;
	}
	uint i = uint( gl_PrimitiveID ) * 3u;
	vec3 N = vec3( worldNormal.nxyz[i], worldNormal.nxyz[i + 1u], worldNormal.nxyz[i + 2u] );
	float len2 = dot( N, N );
	if ( len2 < 1e-8 ) {
		return towardRay;
	}
	N *= inversesqrt( len2 );
	if ( dot( N, towardRay ) < 0.0 ) {
		N = -N;
	}
	return N;
}

void main()
{
	vec3 base = sampleWorldAlbedo();
	vec3 N = sampleWorldNormal();
	vec3 V = normalize( -gl_WorldRayDirectionEXT );
	float ndv = max( dot( N, V ), 0.0 );
	vec3 emissive = vec3( 0.08, 0.07, 0.05 ) * ( 0.35 + 0.65 * ndv );

	ptPayload = vec4( base * ( 0.55 + 0.45 * ndv ) + emissive, 1.0 );
}
