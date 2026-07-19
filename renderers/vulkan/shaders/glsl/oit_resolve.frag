#version 450
/* OIT resolve: composite opaque background + weighted-blended transparent layers.
 * RT0 stores weighted color / alpha (McGuire WBOIT), RT1 stores revealage = product(1 - alpha).
 * Correct composite (McGuire/Bavoil):
 *   C_avg = accum.rgb / max(accum.a, eps)
 *   C_out = C_avg * (1 - revealage) + C_bg * revealage
 * Debug modes via push constant (r_oitDebug). Invalid values → bright magenta.
 */
layout(set = 0, binding = 0) uniform sampler2D opaqueTex;
layout(set = 1, binding = 0) uniform sampler2D oitAccumTex;
layout(set = 2, binding = 0) uniform sampler2D oitRevealTex;
/* Optional MBOIT targets (bound when r_oit 2; unused samplers still valid). */
layout(set = 3, binding = 0) uniform sampler2D oitMomentsTex;
layout(set = 4, binding = 0) uniform sampler2D oitB0Tex;

layout(push_constant) uniform OitResolvePush {
	int debugMode; /* 0=composite, see r_oitDebug */
	int oitMode;   /* 1=WBOIT, 2=MBOIT */
	int pad0;
	int pad1;
} pc;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

bool oit_invalid( float v )
{
	return isnan( v ) || isinf( v );
}

bool oit_invalid3( vec3 v )
{
	return oit_invalid( v.x ) || oit_invalid( v.y ) || oit_invalid( v.z );
}

bool oit_invalid4( vec4 v )
{
	return oit_invalid3( v.rgb ) || oit_invalid( v.a );
}

vec3 oit_magenta( void )
{
	return vec3( 1.0, 0.0, 1.0 );
}

void main() {
	ivec2 px = ivec2( gl_FragCoord.xy );
	ivec2 opaqueSize = textureSize( opaqueTex, 0 );
	ivec2 accumSize = textureSize( oitAccumTex, 0 );
	ivec2 revealSize = textureSize( oitRevealTex, 0 );

	/* Guard against extent mismatch / OOB (half-res or stale descriptor). */
	if ( any( notEqual( opaqueSize, accumSize ) ) || any( notEqual( accumSize, revealSize ) ) ||
		any( greaterThanEqual( px, opaqueSize ) ) || any( lessThan( px, ivec2( 0 ) ) ) ) {
		out_color = vec4( oit_magenta(), 1.0 );
		return;
	}

	/* texelFetch: no LINEAR bleed across rows (avoids scanline-like banding). */
	vec3 opaque = texelFetch( opaqueTex, px, 0 ).rgb;
	vec4 accum = texelFetch( oitAccumTex, px, 0 );
	float revealage = texelFetch( oitRevealTex, px, 0 ).r;

	if ( oit_invalid3( opaque ) || oit_invalid4( accum ) || oit_invalid( revealage ) ) {
		out_color = vec4( oit_magenta(), 1.0 );
		return;
	}

	revealage = clamp( revealage, 0.0, 1.0 );
	/* Raise floor slightly so sparse/noisy weight sums do not amplify into solid slabs. */
	float wsum = max( accum.a, 1e-3 );
	vec3 c_avg = accum.rgb / wsum;
	if ( oit_invalid3( c_avg ) ) {
		out_color = vec4( oit_magenta(), 1.0 );
		return;
	}
	/* Cap average layer luminance so resolve cannot paint near-opaque HDR sheets. */
	{
		float lum = dot( max( c_avg, vec3( 0.0 ) ), vec3( 0.2126, 0.7152, 0.0722 ) );
		if ( lum > 8.0 ) {
			c_avg *= 8.0 / lum;
		}
	}

	float coverage = 1.0 - revealage;
	vec3 resolved = c_avg * coverage + opaque * revealage;
	if ( oit_invalid3( resolved ) ) {
		out_color = vec4( oit_magenta(), 1.0 );
		return;
	}

	int mode = pc.debugMode;
	if ( mode <= 0 ) {
		out_color = vec4( resolved, 1.0 );
		return;
	}

	/* Debug views (linear HDR; presented via normal post stack). */
	if ( mode == 1 ) {
		out_color = vec4( abs( accum.rgb ), 1.0 );
	} else if ( mode == 2 ) {
		out_color = vec4( vec3( clamp( accum.a, 0.0, 8.0 ) * 0.125 ), 1.0 );
	} else if ( mode == 3 ) {
		out_color = vec4( vec3( revealage ), 1.0 );
	} else if ( mode == 4 ) {
		/* Transmittance ≈ revealage for WBOIT product(1-α). */
		out_color = vec4( vec3( revealage ), 1.0 );
	} else if ( mode == 5 ) {
		out_color = vec4( c_avg * coverage, 1.0 );
	} else if ( mode == 6 ) {
		out_color = vec4( opaque, 1.0 );
	} else if ( mode == 7 ) {
		out_color = vec4( vec3( coverage ), 1.0 );
	} else if ( mode == 8 ) {
		/* Pass ownership: green=has OIT coverage, blue=opaque only. */
		out_color = vec4( coverage > 1e-4 ? vec3( 0.1, 0.9, 0.2 ) : vec3( 0.1, 0.2, 0.9 ), 1.0 );
	} else if ( mode == 9 ) {
		vec4 moments = texelFetch( oitMomentsTex, px, 0 );
		if ( oit_invalid4( moments ) ) {
			out_color = vec4( oit_magenta(), 1.0 );
		} else {
			out_color = vec4( abs( moments.rgb ), 1.0 );
		}
	} else if ( mode == 10 ) {
		float b0 = texelFetch( oitB0Tex, px, 0 ).r;
		if ( oit_invalid( b0 ) ) {
			out_color = vec4( oit_magenta(), 1.0 );
		} else {
			out_color = vec4( vec3( clamp( b0 * 0.25, 0.0, 1.0 ) ), 1.0 );
		}
	} else if ( mode == 11 ) {
		/* Cluster / light heuristic proxy: coverage × accum weight magnitude. */
		float heat = clamp( coverage * min( accum.a, 16.0 ) * 0.1, 0.0, 1.0 );
		out_color = vec4( heat, heat * 0.4, 1.0 - heat, 1.0 );
	} else if ( mode == 12 ) {
		/* Estimated layer/fragment count from product revealage (α≈const). */
		float est = -log( max( revealage, 1e-4 ) );
		out_color = vec4( vec3( clamp( est * 0.2, 0.0, 1.0 ) ), 1.0 );
	} else if ( mode == 13 ) {
		/* Transparent-pixel opaque depth (WBOIT binds depth on set 3; MBOIT shows moment.r). */
		float d = texelFetch( oitMomentsTex, px, 0 ).r;
		if ( oit_invalid( d ) ) {
			out_color = vec4( oit_magenta(), 1.0 );
		} else {
			out_color = vec4( vec3( clamp( d, 0.0, 1.0 ) ), 1.0 );
		}
	} else {
		out_color = vec4( resolved, 1.0 );
	}
}
