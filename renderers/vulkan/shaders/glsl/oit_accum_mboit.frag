#version 450
/* Moment Transparency / MBOIT pass 2 (experimental): WBOIT-style accum weighted by moment T(z).
 * Forward+ lights on set 4 via shared Burley+GGX + compact cluster lists.
 */
#extension GL_GOOGLE_include_directive : require
#include "forward_plus_cluster.glsl"
layout (constant_id = 0) const int manual_depth_test = 0;
layout (constant_id = 1) const int forward_plus_lit = 0;

layout(set = 0, binding = 0) uniform sampler2D tex0;
layout(set = 1, binding = 0) uniform sampler2D opaqueDepthTex;
layout(set = 2, binding = 0) uniform sampler2D momentsTex;
layout(set = 3, binding = 0) uniform sampler2D b0Tex;

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
	vec4 base = textureLod(tex0, frag_tex_coord0, 0.0) * frag_color0;
	float alpha = clamp(base.a, 0.0, 0.999);
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

	vec3 litRgb = base.rgb;
	if ( forward_plus_lit != 0 ) {
		vec3 N = normalize( cross( dFdx( frag_world_pos ), dFdy( frag_world_pos ) ) );
		if ( dot( N, N ) < 1e-8 ) {
			N = vec3( 0.0, 0.0, 1.0 );
		}
		vec3 V = normalize( fp_params.fp_view_org.xyz - frag_world_pos );
		bool clusterOob = false;
		uint lightCount = 0u;
		vec3 addLit = FpEval_ForwardPlusAdd( base.rgb, N, V, frag_world_pos, 0.45, 0.0,
			pc.lightingDebug, clusterOob, lightCount );
		if ( clusterOob || any( isnan( addLit ) ) || any( isinf( addLit ) ) ) {
			out_color = vec4( 1.0, 0.0, 1.0, 1.0 );
			out_reveal = 0.0;
			return;
		}
		litRgb = ( pc.lightingDebug == 6 ) ? addLit : ( base.rgb + addLit );
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
