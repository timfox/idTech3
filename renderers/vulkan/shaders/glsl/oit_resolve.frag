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
	int directTest; /* 0=off, 1=clear+resolve, 2=synthetic UV gradient composite */
	int bucket; /* 0=alpha/moments, 1=additive color-only */
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

float oit_mboit_coverage_from_b0( float b0 )
{
	float opticalDepth = clamp( b0, 0.0, 32.0 );
	float coverage = 1.0 - exp( -opticalDepth );
	if ( oit_invalid( coverage ) ) {
		return 0.0;
	}
	return clamp( coverage, 0.0, 1.0 );
}

float oit_mboit_mean_depth( float b0, vec4 moments )
{
	if ( b0 <= 1e-5 || oit_invalid4( moments ) ) {
		return 0.0;
	}
	return clamp( moments.x / max( b0, 1e-5 ), 0.0, 1.0 );
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
	vec4 moments = texelFetch( oitMomentsTex, px, 0 );
	float b0 = texelFetch( oitB0Tex, px, 0 ).r;

	if ( oit_invalid3( opaque ) || oit_invalid4( accum ) || oit_invalid( revealage ) ) {
		out_color = vec4( oit_magenta(), 1.0 );
		return;
	}
	if ( pc.oitMode == 2 && ( oit_invalid4( moments ) || oit_invalid( b0 ) ) ) {
		out_color = vec4( oit_magenta(), 1.0 );
		return;
	}

	/* r_oitDirectTest 2: ignore attachment contents; synthetic UV gradient composite.
	 * Clean half-blend of opaque + (u,v,0.25) proves resolve addressing/lifecycle
	 * independent of transparent geometry (Phase B6). */
	if ( pc.directTest >= 2 ) {
		float u = float( px.x ) / float( max( opaqueSize.x - 1, 1 ) );
		float v = float( px.y ) / float( max( opaqueSize.y - 1, 1 ) );
		vec3 synth = vec3( u, v, 0.25 );
		out_color = vec4( mix( opaque, synth, 0.5 ), 1.0 );
		return;
	}

	/* Additive particles/coronas deliberately do not write revealage. Their
	 * color must therefore bypass coverage reconstruction and layer directly
	 * over the already-resolved bucket-0/opaque color. */
	if ( pc.bucket == 1 ) {
		vec3 additive = max( accum.rgb, vec3( 0.0 ) );
		vec3 resolvedAdditive = opaque + additive;
		out_color = vec4( oit_invalid3( resolvedAdditive ) ? opaque : resolvedAdditive, 1.0 );
		return;
	}

	revealage = clamp( revealage, 0.0, 1.0 );
	/* Empty accumulation: preserve opaque (never black). */
	if ( accum.a < 1e-5 && length( accum.rgb ) < 1e-5 ) {
		out_color = vec4( opaque, 1.0 );
		return;
	}
	float wsum = max( accum.a, 1e-4 );
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

	float coverage = ( pc.oitMode == 2 ) ? oit_mboit_coverage_from_b0( b0 ) : ( 1.0 - revealage );
	if ( pc.oitMode == 2 ) {
		revealage = 1.0 - coverage;
	}
	if ( coverage < 1e-5 ) {
		out_color = vec4( opaque, 1.0 );
		return;
	}
	/* Soften only ultra-thin coverage (was 0.04 — crushed soft glass). */
	{
		float soft = smoothstep( 0.0, 0.008, coverage );
		coverage *= soft;
		revealage = 1.0 - coverage;
		if ( coverage < 1e-5 ) {
			out_color = vec4( opaque, 1.0 );
			return;
		}
	}
	/* Near-black average: gently lift toward opaque instead of hard silhouette kill. */
	{
		float avgLum = dot( max( c_avg, vec3( 0.0 ) ), vec3( 0.2126, 0.7152, 0.0722 ) );
		if ( coverage > 0.25 && avgLum < 0.008 ) {
			float keep = clamp( avgLum / 0.008, 0.15, 1.0 );
			c_avg = mix( opaque * 0.35, c_avg, keep );
		}
	}
	vec3 resolved = c_avg * coverage + opaque * revealage;
	if ( oit_invalid3( resolved ) ) {
		out_color = vec4( opaque, 1.0 ); /* preserve opaque on NaN/Inf — never full black */
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
		out_color = oit_invalid4( moments ) ? vec4( oit_magenta(), 1.0 ) :
			vec4( abs( moments.rgb ), 1.0 );
	} else if ( mode == 10 ) {
		out_color = oit_invalid( b0 ) ? vec4( oit_magenta(), 1.0 ) :
			vec4( vec3( clamp( b0 * 0.25, 0.0, 1.0 ) ), 1.0 );
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
	} else if ( mode == 14 ) {
		/* Constant-color diagnostic (Phase B5): ignore accum RGB; coverage from reveal. */
		out_color = vec4( mix( opaque, vec3( 1.0, 0.0, 1.0 ), coverage ), 1.0 );
	} else if ( mode == 15 ) {
		/* Resolve addressing diagnostic: FragCoord UV (bands here ⇒ wrong extent/fetch). */
		float u = float( px.x ) / float( max( opaqueSize.x - 1, 1 ) );
		float v = float( px.y ) / float( max( opaqueSize.y - 1, 1 ) );
		out_color = vec4( u, v, 0.0, 1.0 );
	} else if ( mode == 16 ) {
		/* Invalid-value mask: magenta if any NaN/Inf in accum/reveal/resolved. */
		bool bad = oit_invalid4( accum ) || oit_invalid( revealage ) || oit_invalid3( resolved );
		out_color = bad ? vec4( oit_magenta(), 1.0 ) : vec4( 0.0, 0.2, 0.0, 1.0 );
	} else if ( mode == 17 ) {
		/* Empty-pixel preservation: green = opaque passthrough, yellow = blended. */
		out_color = ( coverage < 1e-4 || ( accum.a < 1e-4 && length( accum.rgb ) < 1e-4 ) )
			? vec4( 0.1, 0.9, 0.2, 1.0 ) : vec4( 0.9, 0.85, 0.1, 1.0 );
	} else if ( mode == 18 ) {
		/* Opaque input to resolve (fog_scene / pre-OIT HDR). */
		out_color = vec4( opaque, 1.0 );
	} else if ( mode == 19 ) {
		/* Final resolved coverage. */
		out_color = vec4( vec3( coverage ), 1.0 );
	} else if ( mode == 20 ) {
		/* MBOIT optical-depth coverage from b0 (WBOIT shows revealage-derived coverage). */
		out_color = vec4( vec3( ( pc.oitMode == 2 ) ? oit_mboit_coverage_from_b0( b0 ) : coverage ), 1.0 );
	} else if ( mode == 21 ) {
		/* MBOIT first moment mean depth. */
		out_color = vec4( vec3( oit_mboit_mean_depth( b0, moments ) ), 1.0 );
	} else {
		out_color = vec4( resolved, 1.0 );
	}
}
