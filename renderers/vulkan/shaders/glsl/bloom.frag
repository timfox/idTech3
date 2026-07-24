#version 450
#extension GL_GOOGLE_include_directive : require

#include "depth_view.glsl"

layout(set = 0, binding = 0) uniform sampler2D texture0;
layout(set = 1, binding = 0) uniform sampler2D depthTex;
layout(set = 2, binding = 0) uniform PostFXParams {
	mat4 invViewProj;
	mat4 prevViewProj;
	mat4 viewMatrix;
	vec4 motionBlur;
	vec4 depthOfField;
	vec4 frameInfo;
	vec4 depthParams;
	vec4 toneMapParams0;
	vec4 toneMapParams1;
	vec4 colorBalance;
	vec4 colorGrade;
	vec4 colorGrade2;
	vec4 shadowsLift;
	vec4 midsGamma;
	vec4 highlightsGain;
	vec4 splitShadow;
	vec4 splitHighlight;
	vec4 lensEffects0;
	vec4 lensEffects1;
	vec4 runtimeFlags;
	vec4 lutParams;
	vec4 autoExposureParams;
	vec4 localExposureParams;
	vec4 taaParams;
	vec4 temporalValidity;
	vec4 weaponTemporalParams;
	vec4 temporalDebugParams;
} postfx;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 3) const float threshold = 0.6;
layout(constant_id = 5) const int extract_mode = 0;
layout(constant_id = 6) const int base_modulate = 0;
layout(constant_id = 12) const float knee = 0.5;
/* IQ P1-C firefly suppression (bloom extract only — SceneHDR untouched). */
layout(constant_id = 29) const int firefly_clamp = 1;
layout(constant_id = 30) const float firefly_ratio = 4.0;
layout(constant_id = 31) const float firefly_absolute = 0.25;
layout(constant_id = 32) const int firefly_neighborhood = 1;
layout(constant_id = 33) const int firefly_debug = 0;

const vec3 sRGB = vec3( 0.2126, 0.7152, 0.0722 );
const float BLOOM_EDGE_DEPTH_SHARP = 64.0;
const float BLOOM_EDGE_SUPPRESS_REL = 0.08;

float luma( vec3 c ) {
	return max( dot( sRGB, c ), 0.0 );
}

float softWeight( float v ) {
	float k = max( knee, 0.001 );
	return smoothstep( threshold, threshold + k, v );
}

/*
 * Exposure-relative bloom: threshold against exposed luminance so the knee stays
 * perceptually stable as eye adaptation moves. autoExposureParams.z packs
 * adaptedExposure (see vk_postfx_params.c).
 */
float bloomMeterLuma( float sceneLuma ) {
	float adapted = max( postfx.autoExposureParams.z, 1e-4 );
	/* z <= 0 means EV-relative disabled by host packing. */
	if ( postfx.autoExposureParams.z <= 0.0 ) {
		return sceneLuma;
	}
	return sceneLuma * adapted;
}

float viewDepthAt( vec2 uv ) {
	float d = textureLod( depthTex, uv, 0.0 ).r;
	return Depth_LinearizeReversedZ( d, postfx.depthParams.x, postfx.depthParams.y );
}

/* Depth-aware firefly reference: ignore unrelated surfaces at silhouettes so
 * bright foreground does not raise the local reference on background pixels
 * (and vice versa), which seeds bloom energy outside the true mesh contour. */
float robustNeighborhoodLuma( vec2 uv ) {
	vec2 texel = 1.0 / vec2( textureSize( texture0, 0 ) );
	float samples[9];
	int n = 0;
	float centerView = viewDepthAt( uv );
	float centerL = luma( textureLod( texture0, uv, 0.0 ).rgb );

	samples[n++] = centerL;

	int yMin = -1;
	int yMax = 1;
	int xMin = -1;
	int xMax = 1;

	for ( int y = yMin; y <= yMax; y++ ) {
		for ( int x = xMin; x <= xMax; x++ ) {
			if ( x == 0 && y == 0 ) {
				continue;
			}
			/* Cross-only mode: center was added above; retain all four axial
			 * neighbors and skip only diagonals. */
			if ( firefly_neighborhood <= 0 && x != 0 && y != 0 ) {
				continue;
			}
			vec2 suv = uv + vec2( float( x ), float( y ) ) * texel;
			float sv = viewDepthAt( suv );
			float dw = Depth_BilateralWeight( centerView, sv, BLOOM_EDGE_DEPTH_SHARP );
			if ( dw < 0.05 ) {
				continue;
			}
			samples[n++] = luma( textureLod( texture0, suv, 0.0 ).rgb );
		}
	}

	for ( int i = 1; i < n; i++ ) {
		float key = samples[i];
		int j = i - 1;
		while ( j >= 0 && samples[j] > key ) {
			samples[j + 1] = samples[j];
			j--;
		}
		samples[j + 1] = key;
	}

	if ( firefly_neighborhood >= 2 && n >= 5 ) {
		float sum = 0.0;
		for ( int i = 1; i < n - 1; i++ ) {
			sum += samples[i];
		}
		return sum / float( n - 2 );
	}
	return samples[n / 2];
}

/* Suppress extract on the far/dark side of a strong depth edge when a near
 * bright neighbor would otherwise leak into BloomSourceHDR before the pyramid. */
float silhouetteExtractGate( vec2 uv, float centerLuma ) {
	vec2 texel = 1.0 / vec2( textureSize( depthTex, 0 ) );
	float centerView = viewDepthAt( uv );
	float maxNearLuma = centerLuma;
	float maxRel = 0.0;
	vec2 offs[4] = vec2[]( vec2( texel.x, 0.0 ), vec2( -texel.x, 0.0 ),
		vec2( 0.0, texel.y ), vec2( 0.0, -texel.y ) );
	for ( int i = 0; i < 4; i++ ) {
		vec2 suv = uv + offs[i];
		float sv = viewDepthAt( suv );
		float rel = abs( sv - centerView ) / max( centerView, 1e-3 );
		maxRel = max( maxRel, rel );
		/* Neighbor is in front of center (smaller positive view-depth). */
		if ( sv + 1e-3 < centerView ) {
			maxNearLuma = max( maxNearLuma, luma( textureLod( texture0, suv, 0.0 ).rgb ) );
		}
	}
	if ( maxRel < BLOOM_EDGE_SUPPRESS_REL ) {
		return 1.0;
	}
	/* Far-side pixel next to much brighter foreground: do not seed bloom. */
	if ( maxNearLuma > centerLuma * 1.35 + 0.05 ) {
		return 0.0;
	}
	return 1.0;
}

vec3 applyFireflyClamp( vec3 source, out float localRef, out float clampedLuma, out float removed ) {
	float centerLuma = luma( source );
	localRef = robustNeighborhoodLuma( frag_tex_coord );
	float allowed = localRef * max( firefly_ratio, 1.0 ) + max( firefly_absolute, 0.0 );
	clampedLuma = min( centerLuma, allowed );
	removed = max( centerLuma - clampedLuma, 0.0 );
	if ( centerLuma > 1e-6 ) {
		return source * ( clampedLuma / centerLuma );
	}
	return vec3( 0.0 );
}

void main() {
	vec3 base = textureLod( texture0, frag_tex_coord, 0.0 ).rgb;
	vec3 original = base;
	float localRef = 0.0;
	float clampedLuma = 0.0;
	float removed = 0.0;

	if ( firefly_clamp != 0 ) {
		base = applyFireflyClamp( base, localRef, clampedLuma, removed );
	} else {
		localRef = luma( base );
		clampedLuma = localRef;
	}

	if ( firefly_debug == 1 ) {
		out_color = vec4( original, 1.0 );
		return;
	}
	if ( firefly_debug == 2 ) {
		out_color = vec4( vec3( localRef ), 1.0 );
		return;
	}
	if ( firefly_debug == 3 ) {
		out_color = vec4( removed > 1e-4 ? vec3( 1.0, 0.2, 0.0 ) : vec3( 0.0 ), 1.0 );
		return;
	}
	if ( firefly_debug == 4 ) {
		out_color = vec4( base, 1.0 );
		return;
	}
	if ( firefly_debug == 5 ) {
		out_color = vec4( vec3( removed ), 1.0 );
		return;
	}

	float edgeGate = silhouetteExtractGate( frag_tex_coord, luma( base ) );
	base *= edgeGate;

	float weight;
	if ( extract_mode == 1 ) {
		weight = softWeight( bloomMeterLuma( ( base.r + base.g + base.b ) * 0.33333333 ) );
	} else if ( extract_mode == 2 ) {
		weight = softWeight( bloomMeterLuma( dot( sRGB, base ) ) );
	} else {
		float brightest = max( max( base.r, base.g ), base.b );
		weight = softWeight( bloomMeterLuma( brightest ) );
	}

	if ( firefly_debug == 6 ) {
		out_color = vec4( base * weight, 1.0 );
		return;
	}
	if ( firefly_debug == 7 ) {
		out_color = vec4( vec3( edgeGate ), 1.0 );
		return;
	}

	if ( weight > 0.0 ) {
		if ( base_modulate != 0 ) {
			if ( base_modulate == 1 )
				base *= base;
			else
				base *= dot( sRGB, base );
		}
		out_color = vec4( base * weight, 1.0 );
	} else {
		out_color = vec4( 0.0 );
	}
}
