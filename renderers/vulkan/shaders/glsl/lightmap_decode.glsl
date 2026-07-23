/*
 * Shared lightmap decode — Deferred Honesty M2.
 * Used conceptually by Forward+ gen_frag and deferred static term.
 *
 * Decoded value semantics (id Tech 3 / OA BSP lightmaps):
 *   - scene-linear irradiance / baked diffuse radiance proxy after overbright scale
 *   - NOT exposure-scaled, NOT fogged, NOT tone-mapped
 *   - when lightmap_srgb_decode=1, input is gamma-encoded (q3map2 -gamma style)
 *
 * Overbright: lightmap_scale specialization (typically identityLight / overbrightBits).
 */
#ifndef LIGHTMAP_DECODE_GLSL
#define LIGHTMAP_DECODE_GLSL

vec3 LightmapDecodeSRGB( vec3 c ) {
	return c * c; /* approx sRGB→linear (matches gen_frag lightmapDecode) */
}

vec3 LightmapDecodeIrradiance( vec3 raw, float scale, int srgbDecode ) {
	vec3 c = raw;
	if ( srgbDecode > 0 ) {
		c = LightmapDecodeSRGB( c );
	}
	return c * scale;
}

/* Deferred static diffuse (mixed material): baseColor × (1-metal) × irradiance. */
vec3 DeferredStaticDiffuseFromLightmap( vec3 baseColor, float metalness, vec3 lmIrradiance, float ao ) {
	vec3 kD = vec3( 1.0 - clamp( metalness, 0.0, 1.0 ) );
	return baseColor * kD * max( lmIrradiance, vec3( 0.0 ) ) * clamp( ao, 0.0, 1.0 );
}

#endif
