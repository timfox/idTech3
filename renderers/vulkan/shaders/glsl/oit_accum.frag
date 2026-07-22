#version 450
/* Weighted blended OIT (production):
 *  RT0 accumulates (color * alpha * weight, alpha * weight)
 *  RT1 tracks revealage = product(1 - alpha)
 * Optional Forward+ dynamic lights (set 2) when r_oitForwardPlus is on.
 * Lighting uses shared Burley+GGX (forward_plus_light_eval.glsl).
 */
#extension GL_GOOGLE_include_directive : require
#include "forward_plus_cluster.glsl"
#define DEPTH_TO_WEIGHT(z) (z)

layout (constant_id = 0) const int manual_depth_test = 0;
layout (constant_id = 1) const int forward_plus_lit = 0;

layout(set = 0, binding = 0) uniform sampler2D tex0;
layout(set = 1, binding = 0) uniform sampler2D opaqueDepthTex;

layout(set = 2, binding = 0) readonly buffer FpLightSSBO {
	vec4 fp_light_data[];
} fp_lights;
layout(set = 2, binding = 1) readonly buffer FpTileSSBO {
	uint fp_tile_cells[];
} fp_tiles;
layout(std430, set = 2, binding = 2) readonly buffer FpParamSSBO {
	mat4 fp_clip_from_world;
	uvec4 fp_tiles_xy_viewport;
	vec4 fp_view_org;
	uvec4 fp_cluster_meta;
	vec4 fp_cluster_z_range;
} fp_params;

#include "shadow_contract.glsl"
layout(std430, set = 3, binding = 0) readonly buffer ShadowContractSSBO {
	GpuShadowGpuRecord records[];
} shadows;
layout(set = 3, binding = 1) uniform sampler2D sunShadowMap;

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
	float coverageScale; /* <1 softens near-opaque tex alpha (Q3 glass) */
	/* Pack as floats — vec3 would 16-align and desync from C float[3]. */
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

void main() {
	vec4 base = textureLod(tex0, frag_tex_coord0, 0.0) * frag_color0;
	float alpha = clamp( base.a, 0.0, 0.999 );
	/* OpenArena / Q3 glass often stores opacity in blend mode with texture alpha ≈ 1.
	 * Soften coverage so WBOIT revealage still shows the opaque background. */
	if ( pc.coverageScale > 0.0 && pc.coverageScale < 0.999 && alpha > 0.85 ) {
		float vertA = clamp( frag_color0.a, 0.05, 1.0 );
		alpha = clamp( pc.coverageScale * vertA, 0.04, 0.82 );
	}
	if ( alpha < 1e-3 ) discard;
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

	/*
	 * Lighting: vertex color often carries Q3 light; add ambient stand-in when near-white,
	 * directional sun (unshadowed), then optional Forward+ tile lights.
	 */
	vec3 N = normalize( cross( dFdx( frag_world_pos ), dFdy( frag_world_pos ) ) );
	if ( dot( N, N ) < 1e-8 ) {
		N = vec3( 0.0, 0.0, 1.0 );
	}
	/* Face toward camera so backfaces still get some sun on two-sided glass. */
	vec3 V = normalize( fp_params.fp_view_org.xyz - frag_world_pos );
	if ( dot( N, V ) < 0.0 ) {
		N = -N;
	}

	float vertLum = dot( frag_color0.rgb, vec3( 0.2126, 0.7152, 0.0722 ) );
	float amb = clamp( pc.sunAmbient, 0.0, 1.0 );
	/* If vertex is already lit (dark or colored), keep base; if white, add ambient fill. */
	float ambMix = smoothstep( 0.85, 0.98, vertLum );
	vec3 litRgb = base.rgb * mix( 1.0, max( amb, 0.15 ), ambMix );

	if ( pc.sunStrength > 1e-4 ) {
		vec3 L = normalize( vec3( pc.sunDirX, pc.sunDirY, pc.sunDirZ ) );
		float NL = max( dot( N, L ), 0.0 );
		/* Soft wrap so thin glass picks up some sun even at grazing angles. */
		float wrap = clamp( NL * 0.85 + 0.15, 0.0, 1.0 );
		uint cascades = uint( clamp( pc._pad0, 1.0, 4.0 ) );
		float sunVis = 1.0;
		if ( cascades > 0u && ( shadows.records[0].flags & 1u ) != 0u ) {
			sunVis = ShadowContract_SampleCSM_BestFit(
				shadows.records[0], shadows.records[1], shadows.records[2], shadows.records[3],
				sunShadowMap, frag_world_pos, 1.0, cascades );
		}
		litRgb += base.rgb * vec3( pc.sunColorR, pc.sunColorG, pc.sunColorB ) *
			wrap * pc.sunStrength * sunVis;
	}

	if ( forward_plus_lit != 0 ) {
		bool clusterOob = false;
		uint lightCount = 0u;
		float roughness = 0.45;
		float metalness = 0.0;
		vec3 addLit = FpEval_ForwardPlusAdd( base.rgb, N, V, frag_world_pos, roughness, metalness,
			pc.lightingDebug, clusterOob, lightCount );
		if ( clusterOob || any( isnan( addLit ) ) || any( isinf( addLit ) ) ) {
			out_color = vec4( 1.0, 0.0, 1.0, 1.0 );
			out_reveal = 0.0;
			return;
		}
		if ( pc.lightingDebug == 6 ) {
			litRgb = addLit;
		} else {
			litRgb = litRgb + addLit;
		}
	}

	/* Stage B fog: fog lit surface radiance once by camera→fragment transmittance.
	 * Opaque background is assumed already fogged; resolve must not fog transparent again.
	 * Mode 1 = production per-fragment T. Mode 2/3 = same T path today (moments optional later). */
	{
		float viewDepth = length( frag_world_pos - fp_params.fp_view_org.xyz );
		float Tfog = 1.0;
		float dens = max( pc.fogDensity, 0.0 );
		if ( pc.fogMode >= 1 && dens > 1e-6 ) {
			Tfog = clamp( exp( -pc.fogDensity * max( viewDepth, 0.0 ) ), 0.0, 1.0 );
			/* Surface radiance attenuated; no in-scatter into accum (keeps WBOIT weights stable). */
			litRgb *= Tfog;
		}
		/* Fog debug views always available when cheat debug > 0 (even density 0). */
		if ( pc.fogDebug == 1 ) {
			litRgb = vec3( clamp( viewDepth * 0.002, 0.0, 1.0 ) );
		} else if ( pc.fogDebug == 2 ) {
			litRgb = vec3( ( dens > 1e-6 ) ? Tfog : 1.0 );
		} else if ( pc.fogDebug == 3 ) {
			/* In-scatter placeholder (mode 1 does not add fog color into accum). */
			litRgb = vec3( 0.0 );
		} else if ( pc.fogDebug == 4 ) {
			/* Weighted depth proxy: alpha * normalized depth. */
			litRgb = vec3( clamp( viewDepth * 0.002, 0.0, 1.0 ) * alpha );
		} else if ( pc.fogDebug == 5 ) {
			litRgb = vec3( Tfog * alpha );
		} else if ( pc.fogDebug == 6 ) {
			/* Magenta = density set but fogMode legacy/off (double-fog risk with post stack). */
			litRgb = ( pc.fogMode < 1 && dens > 1e-6 ) ? vec3( 1.0, 0.0, 1.0 ) : vec3( Tfog );
		} else if ( pc.fogDebug == 7 ) {
			/* Difference cue: show (1-T) — larger = more attenuated vs unfogged lit. */
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

	/* McGuire weight with moderated scale so depth still discriminates (avoid always-hit 3e3). */
	float zTrad = clamp( 1.0 - DEPTH_TO_WEIGHT( gl_FragCoord.z ), 0.0, 1.0 );
	float aFactor = pow( min( 1.0, alpha * 10.0 ) + 0.01, 3.0 );
	float zFactor = pow( 1.0 - zTrad * 0.9, 3.0 );
	float w = clamp( aFactor * 1e3 * zFactor, 1e-2, 3e3 );
	if ( isnan( w ) || isinf( w ) ) {
		out_color = vec4( 1.0, 0.0, 1.0, 1.0 );
		out_reveal = 0.0;
		return;
	}
	out_color = vec4( litRgb * alpha, alpha ) * w;
	out_reveal = alpha;
}
