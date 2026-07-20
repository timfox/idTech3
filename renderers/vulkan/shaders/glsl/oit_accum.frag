#version 450
/* Weighted blended OIT:
 *  RT0 accumulates (color * alpha * weight, alpha * weight)
 *  RT1 tracks revealage = product(1 - alpha)
 * Optional Forward+ dynamic lights (set 2) when r_oitForwardPlus is on.
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

layout(location = 0) in vec2 frag_tex_coord0;
layout(location = 1) in vec4 frag_color0;
layout(location = 2) in vec3 frag_world_pos;

layout(location = 0) out vec4 out_color;
layout(location = 1) out float out_reveal;

vec3 oit_forward_plus_add( vec3 baseRgb, vec3 N, vec3 worldPos, out bool clusterOob )
{
	vec3 fpAdd = vec3( 0.0 );
	clusterOob = false;
	uint tilesX = fp_params.fp_tiles_xy_viewport.x;
	uint tilesY = fp_params.fp_tiles_xy_viewport.y;
	float vw = float( fp_params.fp_tiles_xy_viewport.z );
	float vh = float( fp_params.fp_tiles_xy_viewport.w );
	if ( tilesX == 0u || tilesY == 0u || vw <= 1.0 || vh <= 1.0 ) {
		tilesX = uint( fp_lights.fp_light_data[1].x + 0.5 );
		tilesY = uint( fp_lights.fp_light_data[1].y + 0.5 );
		vw = fp_lights.fp_light_data[1].z;
		vh = fp_lights.fp_light_data[1].w;
	}
	if ( tilesX == 0u || tilesY == 0u || vw <= 1.0 || vh <= 1.0 ) {
		return fpAdd;
	}
	float tilePxX = vw / max( float( tilesX ), 1.0 );
	float tilePxY = vh / max( float( tilesY ), 1.0 );
	vec4 wc = fp_params.fp_clip_from_world * vec4( worldPos, 1.0 );
	if ( abs( wc.w ) <= 1e-5 ) {
		return fpAdd;
	}
	vec3 ndc = wc.xyz / wc.w;
	if ( ndc.z < -1.0 || ndc.z > 1.0 || ndc.x < -1.05 || ndc.x > 1.05 || ndc.y < -1.05 || ndc.y > 1.05 ) {
		return fpAdd;
	}
	vec2 px;
	px.x = 0.5 * ( 1.0 + ndc.x ) * vw;
	px.y = 0.5 * ( 1.0 + ndc.y ) * vh;
	uint tx = min( uint( px.x / tilePxX ), tilesX - 1u );
	uint ty = min( uint( px.y / tilePxY ), tilesY - 1u );
	uint zSlices = max( fp_params.fp_cluster_meta.x, 1u );
	uint zMode = fp_params.fp_cluster_meta.y;
	float zNear = max( fp_params.fp_cluster_z_range.x, 1e-3 );
	float zFar = max( fp_params.fp_cluster_z_range.y, zNear + 1e-3 );
	uint slice = fp_view_depth_to_slice( abs( wc.w ), zSlices, zMode, zNear, zFar );
	uint tileId = fp_cluster_index( tx, ty, tilesX, tilesY, slice, zSlices );
	uint clusterCount = tilesX * tilesY * zSlices;
	uint tileLen = clusterCount * 8u;
	if ( tileId >= clusterCount ) {
		clusterOob = true;
		return fpAdd;
	}
	uint tbase = tileId * 8u;
	float nLights = fp_lights.fp_light_data[0].x;
	uint maxPerTile = uint( max( fp_lights.fp_light_data[0].z + 0.5, 1.0 ) );
	maxPerTile = min( maxPerTile, 8u );
	for ( uint k = 0u; k < maxPerTile; k++ ) {
		if ( tbase + k >= tileLen ) {
			clusterOob = true;
			break;
		}
		uint li = fp_tiles.fp_tile_cells[ tbase + k ];
		if ( li == 0xFFFFFFFFu ) {
			continue;
		}
		if ( float( li ) + 0.5 >= nLights ) {
			continue;
		}
		uint b0 = 2u + li * 4u;
		vec3 lpos = fp_lights.fp_light_data[ b0 ].xyz;
		float rad = max( fp_lights.fp_light_data[ b0 ].w, 1e-3 );
		vec4 lc = fp_lights.fp_light_data[ b0 + 1u ];
		vec4 lpack = fp_lights.fp_light_data[ b0 + 2u ];
		vec3 Ldir;
		float att = 0.0;
		float NLfp = 0.0;
		if ( lc.w < 0.5 ) {
			vec3 Lw = lpos - worldPos;
			float dist = length( Lw );
			if ( dist > rad ) {
				continue;
			}
			float dr = dist / max( rad, 1e-4 );
			att = clamp( 1.0 - dr * dr, 0.0, 1.0 );
			Ldir = Lw / max( dist, 1e-4 );
			NLfp = max( dot( N, Ldir ), 0.0 );
		} else {
			vec3 axis = normalize( vec3( lpack.x, lpack.y, lpack.z ) );
			Ldir = -axis;
			att = 1.0;
			NLfp = max( dot( N, Ldir ), 0.0 );
		}
		if ( att <= 0.0 || NLfp <= 0.0 ) {
			continue;
		}
		vec3 lightRgb = lc.rgb * att * NLfp;
		float fpAdditive = fp_lights.fp_light_data[ b0 + 3u ].z;
		float addBoost = mix( 1.0, 1.25, step( 0.5, fpAdditive ) );
		fpAdd += baseRgb * lightRgb * addBoost;
	}
	return fpAdd;
}

void main() {
	vec4 base = textureLod(tex0, frag_tex_coord0, 0.0) * frag_color0;
	/* Match MBOIT: alpha→1 collapses revealage to 0 (near-opaque glass). */
	float alpha = clamp( base.a, 0.0, 0.999 );
	/* Soft glows / rings: hard 0.01 discard dithered the falloff into checkerboard. */
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
		/* Reversed-Z: discard fragments farther than opaque (lower depth). */
		if ( gl_FragCoord.z + 1e-5 < opaqueDepth ) discard;
	}

	vec3 litRgb = base.rgb;
	if ( forward_plus_lit != 0 ) {
		vec3 N = normalize( cross( dFdx( frag_world_pos ), dFdy( frag_world_pos ) ) );
		if ( dot( N, N ) < 1e-8 ) {
			N = vec3( 0.0, 0.0, 1.0 );
		}
		bool clusterOob = false;
		litRgb += oit_forward_plus_add( base.rgb, N, frag_world_pos, clusterOob );
		if ( clusterOob || any( isnan( litRgb ) ) || any( isinf( litRgb ) ) ) {
			out_color = vec4( 1.0, 0.0, 1.0, 1.0 );
			out_reveal = 0.0;
			return;
		}
	}
	/* Soft-cap HDR before WBOIT weighting — unbounded Forward+ blew resolve into near-opaque. */
	{
		float lum = dot( litRgb, vec3( 0.2126, 0.7152, 0.0722 ) );
		if ( lum > 4.0 ) {
			litRgb *= 4.0 / lum;
		}
	}

	/*
	 * McGuire/Bavoil WBOIT weight (JCGT 2013), adapted for reversed-Z.
	 * Paper assumes gl_FragCoord.z with 0=near, 1=far. We map rz→zTrad first.
	 * Clamping to [5e-2, 3e3] prevents fp16 underflow that resolves as stipple bands.
	 */
	float zTrad = clamp( 1.0 - DEPTH_TO_WEIGHT( gl_FragCoord.z ), 0.0, 1.0 );
	float aFactor = pow( min( 1.0, alpha * 10.0 ) + 0.01, 3.0 );
	float zFactor = pow( 1.0 - zTrad * 0.9, 3.0 );
	float w = clamp( aFactor * 1e8 * zFactor, 5e-2, 3e3 );
	/* Premultiplied accumulate: (Ci*ai*w, ai*w); revealage via blend product(1-ai).
	 * Additive bucket uses a pipeline with reveal write-mask off (host). */
	out_color = vec4( litRgb * alpha, alpha ) * w;
	out_reveal = alpha;
}
