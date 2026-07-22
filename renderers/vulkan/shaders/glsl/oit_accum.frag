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
	int pad0;
	int pad1;
} pc;

void main() {
	vec4 base = textureLod(tex0, frag_tex_coord0, 0.0) * frag_color0;
	float alpha = clamp( base.a, 0.0, 0.999 );
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

	vec3 litRgb = base.rgb;
	if ( forward_plus_lit != 0 ) {
		vec3 N = normalize( cross( dFdx( frag_world_pos ), dFdy( frag_world_pos ) ) );
		if ( dot( N, N ) < 1e-8 ) {
			N = vec3( 0.0, 0.0, 1.0 );
		}
		vec3 V = normalize( fp_params.fp_view_org.xyz - frag_world_pos );
		bool clusterOob = false;
		uint lightCount = 0u;
		/* Default dielectric translucent: low metal, mid roughness (parity scene overrides via materials later). */
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
		} else if ( pc.parityCompare != 0 && alpha > 0.9 ) {
			/* Near-opaque: show lit term alone for split compare (host may crop). */
			litRgb = base.rgb + addLit;
		} else {
			litRgb = base.rgb + addLit;
		}
	}
	{
		float lum = dot( litRgb, vec3( 0.2126, 0.7152, 0.0722 ) );
		if ( lum > 4.0 ) {
			litRgb *= 4.0 / lum;
		}
		litRgb = max( litRgb, vec3( 0.0 ) );
	}

	float zTrad = clamp( 1.0 - DEPTH_TO_WEIGHT( gl_FragCoord.z ), 0.0, 1.0 );
	float aFactor = pow( min( 1.0, alpha * 10.0 ) + 0.01, 3.0 );
	float zFactor = pow( 1.0 - zTrad * 0.9, 3.0 );
	float w = clamp( aFactor * 1e8 * zFactor, 5e-2, 3e3 );
	if ( isnan( w ) || isinf( w ) ) {
		out_color = vec4( 1.0, 0.0, 1.0, 1.0 );
		out_reveal = 0.0;
		return;
	}
	out_color = vec4( litRgb * alpha, alpha ) * w;
	out_reveal = alpha;
}
