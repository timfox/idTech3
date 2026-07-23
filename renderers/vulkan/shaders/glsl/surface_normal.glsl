/*
 * Shared surface normal evaluation contract — Deferred Honesty M2 / Forward+ parity.
 *
 * PreparedNormal EvaluateSurfaceNormal( material, geometry ) is implemented by
 * CalcNormal() in gen_frag.tmpl for both G-buffer export and Forward+ shade.
 *
 * Contract:
 *   - geometric normal from interpolated vertex normal
 *   - MikkTSpace-compatible tangent frame when normal maps present
 *   - normal-map decode AG (engine convention) with normalScale
 *   - two-sided: rely on cull + abs(N·V) elsewhere; do not flip silently here
 *   - output: world-space unit normal for deferred direct export
 *   - invalid/zero tangent → geometric normal fallback (no NaN)
 *
 * Debug: r_gbufferDebug 2 = decoded normal (existing deferred debug).
 */
#ifndef SURFACE_NORMAL_GLSL
#define SURFACE_NORMAL_GLSL

struct PreparedNormal {
	vec3 geometric;
	vec3 shading; /* after normal map */
	float confidence; /* 1 = mapped or clean geo; <1 = fallback */
};

PreparedNormal EvaluateSurfaceNormalGeo( vec3 geoN ) {
	PreparedNormal p;
	float lenSq = dot( geoN, geoN );
	if ( lenSq <= 1e-8 ) {
		p.geometric = vec3( 0.0, 0.0, 1.0 );
		p.shading = p.geometric;
		p.confidence = 0.25;
		return p;
	}
	p.geometric = geoN * inversesqrt( lenSq );
	p.shading = p.geometric;
	p.confidence = 1.0;
	return p;
}

#endif
