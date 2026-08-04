#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : require

#include "surface_material_decode.glsl"

layout( location = 0 ) rayPayloadInEXT vec4 ptPayload;

layout( set = 0, binding = 2, std140 ) uniform PtFrame {
	mat4 invViewProj;
	vec4 viewOrigin;
	vec4 outputSize;
	vec4 traceParams;
	vec4 bindlessMeta; /* x=texCount y=worldPrimCount */
} pt;

layout( set = 0, binding = 6, std430 ) readonly buffer WorldAlbedoSSBO {
	float rgb[];
} worldAlbedo;

layout( set = 0, binding = 7, std430 ) readonly buffer WorldNormalSSBO {
	float nxyz[];
} worldNormal;

layout( set = 0, binding = 8 ) uniform sampler2D bindlessDiffuse[];

struct PtPrimMaterial {
	uint textureIndex;
	uint uvSetFlags;
};

layout( set = 0, binding = 9, std430 ) readonly buffer PrimMaterialSSBO {
	PtPrimMaterial mats[];
} primMat;

layout( set = 0, binding = 10, std430 ) readonly buffer PrimUvSSBO {
	float uv6[];
} primUv;

hitAttributeEXT vec2 baryCoord;

vec3 sampleBindlessAlbedo( void )
{
	uint texCount = uint( max( pt.bindlessMeta.x, 0.0 ) );
	if ( texCount == 0u || gl_PrimitiveID < 0 ) {
		return vec3( -1.0 );
	}
	uint primIdx = uint( gl_PrimitiveID );
	if ( gl_InstanceCustomIndexEXT == 1 ) {
		primIdx += uint( max( pt.bindlessMeta.y, 0.0 ) );
	}
	uint nMat = uint( primMat.mats.length() );
	if ( nMat == 0u || primIdx >= nMat ) {
		return vec3( -1.0 );
	}
	uint texIdx = primMat.mats[primIdx].textureIndex;
	if ( texIdx == 0xFFFFFFFFu || texIdx >= texCount ) {
		return vec3( -1.0 );
	}
	vec2 uv = vec2( 0.5 );
	uint nuv = uint( primUv.uv6.length() );
	if ( nuv >= ( primIdx + 1u ) * 6u ) {
		uint base = primIdx * 6u;
		vec2 uv0 = vec2( primUv.uv6[base + 0u], primUv.uv6[base + 1u] );
		vec2 uv1 = vec2( primUv.uv6[base + 2u], primUv.uv6[base + 3u] );
		vec2 uv2 = vec2( primUv.uv6[base + 4u], primUv.uv6[base + 5u] );
		float w = max( 1.0 - baryCoord.x - baryCoord.y, 0.0 );
		uv = uv0 * w + uv1 * baryCoord.x + uv2 * baryCoord.y;
	}
	vec3 c = texture( nonuniformEXT( bindlessDiffuse[texIdx] ), uv ).rgb;
	if ( dot( c, c ) <= 1e-8 ) {
		return vec3( -1.0 );
	}
	return c;
}

vec3 sampleWorldAlbedo( void )
{
	vec3 b = sampleBindlessAlbedo();
	if ( b.x >= 0.0 ) {
		return b;
	}
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
	SurfaceMaterial material = SurfaceMaterialDecodeLegacy( base, 1.0, N,
		SURFACE_LEGACY_EMISSIVE, 0u, 0u, OPAQUE_OWNER_SPECIALIZED, 0u );
	base = material.baseColor;
	N = material.normalWS;
	vec3 V = normalize( -gl_WorldRayDirectionEXT );
	float ndv = max( dot( N, V ), 0.0 );
	vec3 emissive = material.emissive;

	ptPayload = vec4( base * ( 0.55 + 0.45 * ndv ) + emissive, 1.0 );
}
