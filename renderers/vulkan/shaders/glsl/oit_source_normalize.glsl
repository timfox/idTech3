/* Color Pipeline Phase 2.2 — shared WBOIT source normalization.
 * Internal representation:
 *   unassociatedRadiance = scene-linear lit radiance (not yet × opacity)
 *   opacity              = alpha in [0,1]
 *   associatedRadiance   = unassociatedRadiance * opacity
 * Accumulation (frozen contract) consumes unassociatedRadiance + opacity once:
 *   accum.rgb += unassociatedRadiance * opacity * weight
 *   accum.a   += opacity * weight
 */
#ifndef OIT_SOURCE_NORMALIZE_GLSL
#define OIT_SOURCE_NORMALIZE_GLSL

const uint OIT_SOURCE_ALPHA_STRAIGHT = 0u;
const uint OIT_SOURCE_ALPHA_PREMULTIPLIED = 1u;
const uint OIT_SOURCE_ALPHA_OPAQUE = 2u;
const uint OIT_SOURCE_ALPHA_ADDITIVE = 3u;
const uint OIT_SOURCE_ALPHA_MASKED = 4u;
const uint OIT_SOURCE_ALPHA_MULTIPLICATIVE = 5u;
const uint OIT_SOURCE_ALPHA_UNKNOWN = 6u;

const uint OIT_SAMPLE_FLAG_CLAMPED_DIV = 1u;
const uint OIT_SAMPLE_FLAG_ZERO_ALPHA_RGB = 2u;
const uint OIT_SAMPLE_FLAG_NEAR_ZERO_BRIGHT = 4u;
const uint OIT_SAMPLE_FLAG_NON_FINITE = 8u;
const uint OIT_SAMPLE_FLAG_REJECTED = 16u;

const uint OIT_INPUT_UNASSOCIATED = 1u;
const uint OIT_INPUT_ASSOCIATED = 2u;
const uint OIT_INPUT_INVALID = 3u;

struct OitSurfaceSample {
	vec3 unassociatedRadiance;
	vec3 associatedRadiance;
	float opacity;
	uint sourceEncoding;
	uint flags;
};

struct OitSourcePolicy {
	float epsilon;
	int edgePolicy;
	bool allowEmissiveAtZeroAlpha;
};

OitSurfaceSample NormalizeOitSource( vec4 decodedSource, uint encoding, OitSourcePolicy policy )
{
	OitSurfaceSample s;
	s.unassociatedRadiance = vec3( 0.0 );
	s.associatedRadiance = vec3( 0.0 );
	s.opacity = 0.0;
	s.sourceEncoding = encoding;
	s.flags = 0u;

	float eps = max( policy.epsilon, 1e-5 );
	float a = clamp( decodedSource.a, 0.0, 1.0 );

	if ( any( isnan( decodedSource ) ) || any( isinf( decodedSource ) ) ) {
		s.flags = OIT_SAMPLE_FLAG_NON_FINITE | OIT_SAMPLE_FLAG_REJECTED;
		return s;
	}

	if ( encoding == OIT_SOURCE_ALPHA_ADDITIVE ||
		encoding == OIT_SOURCE_ALPHA_MASKED ||
		encoding == OIT_SOURCE_ALPHA_MULTIPLICATIVE ) {
		s.flags = OIT_SAMPLE_FLAG_REJECTED;
		s.opacity = a;
		s.unassociatedRadiance = decodedSource.rgb;
		return s;
	}

	if ( encoding == OIT_SOURCE_ALPHA_OPAQUE ) {
		s.opacity = 1.0;
		s.unassociatedRadiance = decodedSource.rgb;
		s.associatedRadiance = decodedSource.rgb;
		return s;
	}

	if ( encoding == OIT_SOURCE_ALPHA_PREMULTIPLIED ) {
		s.opacity = a;
		s.associatedRadiance = decodedSource.rgb;
		if ( a > eps ) {
			s.unassociatedRadiance = decodedSource.rgb / a;
		} else {
			s.unassociatedRadiance = vec3( 0.0 );
			s.flags |= OIT_SAMPLE_FLAG_CLAMPED_DIV;
			if ( policy.allowEmissiveAtZeroAlpha ) {
				/* associated kept for emissive diagnostics; accum uses unassociated=0 */
			}
		}
	} else {
		/* STRAIGHT / UNKNOWN → classic compatibility: treat as unassociated */
		s.opacity = a;
		s.unassociatedRadiance = decodedSource.rgb;
		s.associatedRadiance = decodedSource.rgb * a;
	}

	float lum = dot( s.unassociatedRadiance, vec3( 0.2126, 0.7152, 0.0722 ) );
	if ( a <= 0.0 && lum > 1e-4 ) {
		s.flags |= OIT_SAMPLE_FLAG_ZERO_ALPHA_RGB;
		if ( policy.edgePolicy == 1 ) {
			s.unassociatedRadiance = vec3( 0.0 );
			s.associatedRadiance = vec3( 0.0 );
		}
	}
	if ( a > 0.0 && a < 0.02 && lum > 0.25 ) {
		s.flags |= OIT_SAMPLE_FLAG_NEAR_ZERO_BRIGHT;
	}

	/* associatedRadiance = unassociatedRadiance * opacity (identity).
	 * Premul near-zero: keep authored associated only when emissive-at-zero allowed. */
	if ( encoding == OIT_SOURCE_ALPHA_PREMULTIPLIED && a <= eps ) {
		if ( !policy.allowEmissiveAtZeroAlpha ) {
			s.associatedRadiance = vec3( 0.0 );
		}
	} else {
		s.associatedRadiance = s.unassociatedRadiance * s.opacity;
	}
	return s;
}

#endif
