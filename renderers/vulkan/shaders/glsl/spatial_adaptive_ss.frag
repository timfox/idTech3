#version 450

/*
 * Raster Ultra 2.1 — current-frame adaptive spatial supersample (history-free).
 * Classifies high-risk pixels from luma + depth gradients; only those receive
 * extra neighborhood samples. Low-risk pixels pass through unchanged.
 * Does not read previous-frame color.
 */

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 1, binding = 0) uniform sampler2D depthTexture;

layout(push_constant) uniform SpatialAaParams {
	float invResolutionX;
	float invResolutionY;
	float riskThreshold;   /* 0..1; higher = fewer pixels supersampled */
	float sampleBudget;    /* 0..1 soft gate for multi-tap strength */
	float sharpen;         /* mild unsharp on resolved high-risk only */
	float debugMode;       /* 0 off, 1 risk heat, 2 force all, 3 force none */
} pc;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

const vec3 LUMA = vec3( 0.2126, 0.7152, 0.0722 );

float luma( vec3 c ) {
	return dot( c, LUMA );
}

vec3 sampleColor( vec2 uv ) {
	return textureLod( colorTexture, uv, 0.0 ).rgb;
}

float sampleDepth( vec2 uv ) {
	return textureLod( depthTexture, uv, 0.0 ).r;
}

/*
 * Edge-aware current-frame neighborhood (same family as taa.frag spatialCurrentFallback).
 */
vec3 spatialResolve( vec2 uv, vec2 texel ) {
	vec3 c0 = sampleColor( uv );
	vec3 cL = sampleColor( uv + vec2( -texel.x, 0.0 ) );
	vec3 cR = sampleColor( uv + vec2(  texel.x, 0.0 ) );
	vec3 cU = sampleColor( uv + vec2( 0.0, -texel.y ) );
	vec3 cD = sampleColor( uv + vec2( 0.0,  texel.y ) );
	/* Optional diagonal taps when budget allows. */
	vec3 cNW = sampleColor( uv + vec2( -texel.x, -texel.y ) );
	vec3 cNE = sampleColor( uv + vec2(  texel.x, -texel.y ) );
	vec3 cSW = sampleColor( uv + vec2( -texel.x,  texel.y ) );
	vec3 cSE = sampleColor( uv + vec2(  texel.x,  texel.y ) );

	float l0 = max( luma( c0 ), 1e-4 );
	float wL = 1.0 / ( 1.0 + abs( max( luma( cL ), 0.0 ) - l0 ) * 8.0 );
	float wR = 1.0 / ( 1.0 + abs( max( luma( cR ), 0.0 ) - l0 ) * 8.0 );
	float wU = 1.0 / ( 1.0 + abs( max( luma( cU ), 0.0 ) - l0 ) * 8.0 );
	float wD = 1.0 / ( 1.0 + abs( max( luma( cD ), 0.0 ) - l0 ) * 8.0 );
	float diagScale = clamp( pc.sampleBudget, 0.0, 1.0 );
	float wNW = diagScale / ( 1.0 + abs( max( luma( cNW ), 0.0 ) - l0 ) * 8.0 );
	float wNE = diagScale / ( 1.0 + abs( max( luma( cNE ), 0.0 ) - l0 ) * 8.0 );
	float wSW = diagScale / ( 1.0 + abs( max( luma( cSW ), 0.0 ) - l0 ) * 8.0 );
	float wSE = diagScale / ( 1.0 + abs( max( luma( cSE ), 0.0 ) - l0 ) * 8.0 );
	float wSum = 1.0 + wL + wR + wU + wD + wNW + wNE + wSW + wSE;
	vec3 resolved = ( c0 + cL * wL + cR * wR + cU * wU + cD * wD +
		cNW * wNW + cNE * wNE + cSW * wSW + cSE * wSE ) / wSum;
	if ( pc.sharpen > 0.001 ) {
		vec3 blur = ( cL + cR + cU + cD ) * 0.25;
		resolved = max( resolved + ( resolved - blur ) * pc.sharpen * 0.35, vec3( 0.0 ) );
	}
	return resolved;
}

void main() {
	vec2 uv = frag_tex_coord;
	vec2 texel = vec2( pc.invResolutionX, pc.invResolutionY );
	vec3 c0 = sampleColor( uv );

	if ( pc.debugMode > 2.5 ) {
		out_color = vec4( c0, 1.0 );
		return;
	}

	float d0 = sampleDepth( uv );
	float dL = sampleDepth( uv + vec2( -texel.x, 0.0 ) );
	float dR = sampleDepth( uv + vec2(  texel.x, 0.0 ) );
	float dU = sampleDepth( uv + vec2( 0.0, -texel.y ) );
	float dD = sampleDepth( uv + vec2( 0.0,  texel.y ) );

	float depthGrad = max( max( abs( d0 - dL ), abs( d0 - dR ) ),
		max( abs( d0 - dU ), abs( d0 - dD ) ) );

	float l0 = luma( c0 );
	float lL = luma( sampleColor( uv + vec2( -texel.x, 0.0 ) ) );
	float lR = luma( sampleColor( uv + vec2(  texel.x, 0.0 ) ) );
	float lU = luma( sampleColor( uv + vec2( 0.0, -texel.y ) ) );
	float lD = luma( sampleColor( uv + vec2( 0.0,  texel.y ) ) );
	float lumaGrad = max( max( abs( l0 - lL ), abs( l0 - lR ) ),
		max( abs( l0 - lU ), abs( l0 - lD ) ) );

	/* Sky / invalid depth: skip (SMAA handles silhouettes vs sky separately). */
	bool invalidDepth = ( d0 <= 0.0 || d0 >= 1.0 );
	float risk = clamp( lumaGrad * 4.0 + depthGrad * 40.0, 0.0, 1.0 );
	if ( invalidDepth ) {
		risk *= 0.25;
	}

	float thr = clamp( pc.riskThreshold, 0.02, 0.95 );
	bool highRisk = ( risk >= thr ) || ( pc.debugMode > 1.5 );

	if ( pc.debugMode > 0.5 && pc.debugMode < 1.5 ) {
		out_color = vec4( risk, risk * 0.4, 1.0 - risk, 1.0 );
		return;
	}

	if ( !highRisk ) {
		out_color = vec4( c0, 1.0 );
		return;
	}

	/* Soft budget: below full budget, lerp resolve strength. */
	float budget = clamp( pc.sampleBudget, 0.0, 1.0 );
	vec3 resolved = spatialResolve( uv, texel );
	vec3 outRgb = mix( c0, resolved, budget );
	out_color = vec4( outRgb, 1.0 );
}
