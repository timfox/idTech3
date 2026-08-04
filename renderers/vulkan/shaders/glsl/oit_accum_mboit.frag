#version 450
/* Moment Transparency / MBOIT pass 2 (experimental): WBOIT-style accum weighted by moment T(z).
 * Forward+ lights on set 4 via shared Burley+GGX + compact cluster lists.
 */
#extension GL_GOOGLE_include_directive : require
#include "forward_plus_cluster.glsl"
#include "depth_view.glsl"
#include "lightmap_decode.glsl"
layout (constant_id = 0) const int manual_depth_test = 0;
layout (constant_id = 1) const int forward_plus_lit = 0;

layout(set = 0, binding = 0) uniform sampler2D tex0;
layout(set = 1, binding = 0) uniform sampler2D opaqueDepthTex;
layout(set = 2, binding = 0) uniform sampler2D momentsTex;
layout(set = 3, binding = 0) uniform sampler2D b0Tex;
layout(set = 6, binding = 0) uniform sampler2D normalMap;
layout(set = 7, binding = 0) uniform sampler2D physicalMap;
layout(set = 8, binding = 0) uniform sampler2D emissiveMap;
layout(set = 9, binding = 0) uniform sampler2D lightmap;

layout(set = 4, binding = 0) readonly buffer FpLightSSBO {
	vec4 fp_light_data[];
} fp_lights;
layout(set = 4, binding = 1) readonly buffer FpTileSSBO {
	uint fp_tile_cells[];
} fp_tiles;
layout(std430, set = 4, binding = 2) readonly buffer FpParamSSBO {
	mat4 fp_clip_from_world;
	uvec4 fp_tiles_xy_viewport;
	vec4 fp_view_org;
	uvec4 fp_cluster_meta;
	vec4 fp_cluster_z_range;
	vec4 fp_view_forward;
} fp_params;

#include "shadow_contract.glsl"
layout(std430, set = 5, binding = 0) readonly buffer ShadowContractSSBO {
	GpuShadowGpuRecord records[];
} shadows;
layout(set = 5, binding = 1) uniform sampler2D sunShadowMap;

#define CLUSTER_LIST_CELLS fp_tiles.fp_tile_cells
#include "cluster_light_list.glsl"
#include "forward_plus_light_eval.glsl"

layout(location = 0) in vec2 frag_tex_coord0;
layout(location = 1) in vec4 frag_color0;
layout(location = 2) in vec3 frag_world_pos;

layout(location = 0) out vec4 out_color;
layout(location = 1) out float out_reveal;

layout(push_constant) uniform Transform {
	mat4 mvp;
	mat4 prevMvp;
	mat4 model;
	int lightingDebug;
	int parityCompare;
	int fogMode;
	int fogDebug;
	float fogDensity;
	float coverageScale;
	float sunDirX;
	float sunDirY;
	float sunDirZ;
	float sunStrength;
	float sunColorR;
	float sunColorG;
	float sunColorB;
	float sunAmbient;
	float _pad0;
	float _pad1;
} pc;

float AbsorbanceCloser( float b0, vec4 b, float z )
{
	float inv = 1.0 / max(b0, 1e-5);
	float mean = b.x * inv;
	float mean2 = b.y * inv;
	float var = max(mean2 - mean * mean, 1e-6);
	float t = z - mean;
	float pGe;
	if ( t <= 0.0 ) {
		pGe = var / (var + t * t + 1e-6);
		pGe = 1.0 - clamp(pGe, 0.0, 1.0);
	} else {
		pGe = var / (var + t * t);
		pGe = 1.0 - clamp(pGe, 0.0, 1.0);
	}
	pGe = mix(pGe, 1.0, 0.25);
	return clamp(pGe, 0.0, 1.0) * b0;
}

void main() {
	vec4 base = texture( tex0, frag_tex_coord0 ) * frag_color0;
	float alpha = clamp(base.a, 0.0, 0.999);
	if ( pc.coverageScale > 0.0 && pc.coverageScale < 0.999 && alpha > 0.85 ) {
		float vertA = clamp( frag_color0.a, 0.05, 1.0 );
		alpha = clamp( pc.coverageScale * vertA, 0.04, 0.82 );
	}
	if (alpha < 1e-3) discard;
	if ( isnan( alpha ) || isinf( alpha ) || any( isnan( base.rgb ) ) || any( isinf( base.rgb ) ) ) {
		out_color = vec4( 1.0, 0.0, 1.0, 1.0 );
		out_reveal = 0.0;
		return;
	}

	if ( manual_depth_test != 0 ) {
		ivec2 depthSize = textureSize( opaqueDepthTex, 0 );
		vec2 depthUv = gl_FragCoord.xy / vec2( depthSize );
		float opaqueDepth = textureLod( opaqueDepthTex, depthUv, 0.0 ).r;
		if ( gl_FragCoord.z + 1e-5 < opaqueDepth ) discard;
	}

	vec3 N = normalize( cross( dFdx( frag_world_pos ), dFdy( frag_world_pos ) ) );
	if ( dot( N, N ) < 1e-8 ) {
		N = vec3( 0.0, 0.0, 1.0 );
	}
	SurfaceMaterial surfaceMaterial = SurfaceMaterialDecodeLegacy(
		base.rgb, alpha, N, vec3( 0.0 ), 0u, 0u,
		OPAQUE_OWNER_FORWARD_PLUS, 0u );
	uint materialFlags = uint( max( pc.parityCompare, 0 ) ) >> 8;
	if ( ( materialFlags & 1u ) != 0u ) {
		vec3 dpdx = dFdx( frag_world_pos );
		vec3 dpdy = dFdy( frag_world_pos );
		vec2 duvDx = dFdx( frag_tex_coord0 );
		vec2 duvDy = dFdy( frag_tex_coord0 );
		vec3 T = normalize( dpdx * duvDy.y - dpdy * duvDx.y );
		vec3 B = normalize( -dpdx * duvDy.x + dpdy * duvDx.x );
		vec3 nTS = texture( normalMap, frag_tex_coord0 ).xyz * 2.0 - 1.0;
		N = normalize( mat3( T, B, N ) * nTS );
	}
	if ( ( materialFlags & 2u ) != 0u ) {
		vec4 orms = texture( physicalMap, frag_tex_coord0 );
		surfaceMaterial = SurfaceMaterialDecodeCanonical(
			base.rgb, alpha, N, mix( 0.01, 1.0, orms.g ), orms.b, orms.r,
			vec3( 0.0 ), 0.0, mix( 0.01, 1.0, orms.g ), 0.0,
			0u, 0u, OPAQUE_OWNER_FORWARD_PLUS, 0u );
	}
	if ( ( materialFlags & 4u ) != 0u ) {
		surfaceMaterial.emissive = max( texture( emissiveMap, frag_tex_coord0 ).rgb, vec3( 0.0 ) );
	}
	if ( ( materialFlags & 8u ) != 0u ) {
		surfaceMaterial.baseColor *= LightmapDecodeIrradiance(
			texture( lightmap, frag_tex_coord0 ).rgb, 1.0, 0 );
	}
	base.rgb = surfaceMaterial.baseColor;
	N = surfaceMaterial.normalWS;
	vec3 V = normalize( fp_params.fp_view_org.xyz - frag_world_pos );
	if ( dot( N, V ) < 0.0 ) {
		N = -N;
	}
	float vertLum = dot( frag_color0.rgb, vec3( 0.2126, 0.7152, 0.0722 ) );
	float ambMix = smoothstep( 0.85, 0.98, vertLum );
	vec3 litRgb = base.rgb * mix( 1.0, max( clamp( pc.sunAmbient, 0.0, 1.0 ), 0.15 ), ambMix );
	if ( pc.sunStrength > 1e-4 ) {
		vec3 L = normalize( vec3( pc.sunDirX, pc.sunDirY, pc.sunDirZ ) );
		float wrap = clamp( max( dot( N, L ), 0.0 ) * 0.85 + 0.15, 0.0, 1.0 );
		uint cascades = uint( clamp( pc._pad0, 1.0, 4.0 ) );
		float sunVis = 1.0;
		if ( cascades > 0u && ( shadows.records[0].flags & 1u ) != 0u ) {
			float znShadow = max( fp_params.fp_cluster_z_range.x, 1e-3 );
			float zfShadow = max( fp_params.fp_cluster_z_range.y, znShadow + 1e-3 );
			float viewDist = Depth_ReconstructPositiveViewDepth( gl_FragCoord.z, znShadow, zfShadow );
			if ( fp_params.fp_view_forward.w > 0.5 ) {
				viewDist = Depth_PositiveViewFromWorld( frag_world_pos, fp_params.fp_view_org.xyz,
					fp_params.fp_view_forward.xyz );
			}
			sunVis = ShadowContract_SampleCSM_FromRecords(
				shadows.records[0], shadows.records[1], shadows.records[2], shadows.records[3],
				sunShadowMap, frag_world_pos, viewDist, 1.0, cascades );
		}
		litRgb += base.rgb * vec3( pc.sunColorR, pc.sunColorG, pc.sunColorB ) *
			wrap * pc.sunStrength * sunVis;
	}
	if ( forward_plus_lit != 0 ) {
		bool clusterOob = false;
		uint lightCount = 0u;
		vec3 addLit = FpEval_ForwardPlusAdd( surfaceMaterial.baseColor, surfaceMaterial.normalWS,
			V, frag_world_pos, surfaceMaterial.perceptualRoughness, surfaceMaterial.metallic,
			pc.lightingDebug, clusterOob, lightCount );
		addLit += surfaceMaterial.emissive;
		if ( clusterOob || any( isnan( addLit ) ) || any( isinf( addLit ) ) ) {
			out_color = vec4( 1.0, 0.0, 1.0, 1.0 );
			out_reveal = 0.0;
			return;
		}
		litRgb = ( pc.lightingDebug == 6 ) ? addLit : ( litRgb + addLit );
	}
	{
		float zn = max( fp_params.fp_cluster_z_range.x, 1e-3 );
		float zf = max( fp_params.fp_cluster_z_range.y, zn + 1e-3 );
		if ( !( zf > zn ) ) {
			zn = 8.0;
			zf = 8192.0;
		}
		float viewDepth;
		if ( fp_params.fp_view_forward.w > 0.5 ) {
			viewDepth = Depth_PositiveViewFromWorld( frag_world_pos, fp_params.fp_view_org.xyz,
				fp_params.fp_view_forward.xyz );
		} else {
			viewDepth = Depth_ReconstructPositiveViewDepth( gl_FragCoord.z, zn, zf );
		}
		float Tfog = 1.0;
		float dens = max( pc.fogDensity, 0.0 );
		if ( pc.fogMode >= 1 && dens > 1e-6 ) {
			Tfog = clamp( exp( -pc.fogDensity * max( viewDepth, 0.0 ) ), 0.0, 1.0 );
			litRgb *= Tfog;
		}
		if ( pc.fogDebug == 1 ) {
			litRgb = vec3( clamp( viewDepth * 0.002, 0.0, 1.0 ) );
		} else if ( pc.fogDebug == 2 ) {
			litRgb = vec3( ( dens > 1e-6 ) ? Tfog : 1.0 );
		} else if ( pc.fogDebug == 6 ) {
			litRgb = ( pc.fogMode < 1 && dens > 1e-6 ) ? vec3( 1.0, 0.0, 1.0 ) : vec3( Tfog );
		} else if ( pc.fogDebug == 7 ) {
			litRgb = vec3( 1.0 - Tfog );
		}
	}
	{
		float lum = dot( litRgb, vec3( 0.2126, 0.7152, 0.0722 ) );
		if ( lum > 4.0 ) {
			litRgb *= 4.0 / lum;
		}
		litRgb = max( litRgb, vec3( 0.0 ) );
	}

	ivec2 px = ivec2(gl_FragCoord.xy);
	ivec2 momentsSize = textureSize( momentsTex, 0 );
	if ( px.x < 0 || px.y < 0 || px.x >= momentsSize.x || px.y >= momentsSize.y ) {
		discard;
	}
	vec4 b = texelFetch(momentsTex, px, 0);
	float b0 = texelFetch(b0Tex, px, 0).r;
	if ( any( isnan( b ) ) || any( isinf( b ) ) || isnan( b0 ) || isinf( b0 ) ) {
		out_color = vec4( 1.0, 0.0, 1.0, 1.0 );
		out_reveal = 0.0;
		return;
	}
	float z = clamp(gl_FragCoord.z, 0.0, 1.0);
	float absCloser = AbsorbanceCloser(b0, b, z);
	float T = exp(-absCloser);
	T = clamp(T, 0.0, 1.0);
	if ( isnan( T ) || isinf( T ) ) {
		out_color = vec4( 1.0, 0.0, 1.0, 1.0 );
		out_reveal = 0.0;
		return;
	}

	out_color = vec4( litRgb * alpha, alpha ) * T;
	out_color = max( out_color, vec4( 0.0 ) );
	out_color.a = max( out_color.a, 1e-4 );
	out_reveal = alpha;
}
