#version 450
/* Moment Transparency / MBOIT pass 2: WBOIT-style accum weighted by moment T(z).
 * Samples pass-1 moments + b0, reconstructs transmittance (Cantelli/MSM-style),
 * then accumulates (color * alpha * T, alpha * T) and revealage.
 * Optional Forward+ dynamic lights on set 4 when r_oitForwardPlus is on.
 */
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

layout(location = 0) in vec2 frag_tex_coord0;
layout(location = 1) in vec4 frag_color0;
layout(location = 2) in vec3 frag_world_pos;

layout(location = 0) out vec4 out_color;
layout(location = 1) out float out_reveal;

/* Fraction of optical depth closer than z (biased). β=0.25 overestimation. */
float AbsorbanceCloser( float b0, vec4 b, float z )
{
	float inv = 1.0 / max(b0, 1e-5);
	float mean = b.x * inv;
	float mean2 = b.y * inv;
	float var = max(mean2 - mean * mean, 1e-6);
	float t = z - mean;
	float pGe;
	if ( t <= 0.0 ) {
		/* Cantelli: most mass is at/behind mean when querying in front */
		pGe = var / (var + t * t + 1e-6);
		pGe = 1.0 - clamp(pGe, 0.0, 1.0);
	} else {
		pGe = var / (var + t * t);
		pGe = 1.0 - clamp(pGe, 0.0, 1.0);
	}
	/* Overestimation weight β = 0.25 (Münstermann / Moment Transparency) */
	pGe = mix(pGe, 1.0, 0.25);
	return clamp(pGe, 0.0, 1.0) * b0;
}

vec3 oit_forward_plus_add( vec3 baseRgb, vec3 N, vec3 worldPos )
{
	vec3 fpAdd = vec3( 0.0 );
	uint tilesX = uint( fp_lights.fp_light_data[1].x + 0.5 );
	uint tilesY = uint( fp_lights.fp_light_data[1].y + 0.5 );
	float vw = fp_lights.fp_light_data[1].z;
	float vh = fp_lights.fp_light_data[1].w;
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
	float viewDepth = abs( wc.w );
	uint slice = 0u;
	if ( zSlices > 1u ) {
		float z = clamp( viewDepth, zNear, zFar );
		float t = ( zMode == 1u )
			? ( log( z / zNear ) / max( log( zFar / zNear ), 1e-5 ) )
			: ( ( z - zNear ) / ( zFar - zNear ) );
		t = clamp( t, 0.0, 0.9999 );
		slice = min( uint( t * float( zSlices ) ), zSlices - 1u );
	}
	uint flatTiles = max( tilesX * tilesY, 1u );
	uint tileId = ( zSlices > 1u ) ? ( ty * tilesX + tx + slice * flatTiles ) : ( ty * tilesX + tx );
	uint tbase = tileId * 8u;
	float nLights = fp_lights.fp_light_data[0].x;
	uint maxPerTile = uint( max( fp_lights.fp_light_data[0].z + 0.5, 1.0 ) );
	maxPerTile = min( maxPerTile, 8u );
	for ( uint k = 0u; k < maxPerTile; k++ ) {
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
	float alpha = clamp(base.a, 0.0, 0.999);
	if (alpha < 0.01) discard;

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
		litRgb += oit_forward_plus_add( base.rgb, N, frag_world_pos );
	}

	ivec2 px = ivec2(gl_FragCoord.xy);
	vec4 b = texelFetch(momentsTex, px, 0);
	float b0 = texelFetch(b0Tex, px, 0).r;
	float z = clamp(gl_FragCoord.z, 0.0, 1.0);
	float absCloser = AbsorbanceCloser(b0, b, z);
	float T = exp(-absCloser);
	T = clamp(T, 0.0, 1.0);

	float w = alpha * T;
	out_color = vec4(litRgb * w, w);
	out_reveal = alpha;
}
