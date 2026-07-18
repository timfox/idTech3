/* Shared RTX bindless albedo sample (Phase A.1b centroid / A.1c bary UV).
 * Requires: PrimMaterialSSBO + bindlessDiffuse[] bound; optional PrimUvSSBO.
 * Caller provides texCount / worldPrimCount / instanceCustom / primId / bary.
 */
#ifndef RTX_BINDLESS_SAMPLE_GLSL
#define RTX_BINDLESS_SAMPLE_GLSL

struct RtxPrimMaterialRec {
	uint textureIndex;
	uint uvSetFlags;
};

vec3 rtx_bindless_sample_albedo(
	uint texCount,
	uint worldPrimCount,
	uint instanceCustom,
	int primId,
	vec2 bary,
	sampler2D bindlessDiffuse[],
	RtxPrimMaterialRec mats[],
	float primUv[] /* 6 floats per prim: u0 v0 u1 v1 u2 v2; may be empty */
) {
	if ( texCount == 0u || primId < 0 ) {
		return vec3( -1.0 );
	}
	uint primIdx = uint( primId );
	if ( instanceCustom == 1u ) {
		primIdx += worldPrimCount;
	}
	uint nMat = uint( mats.length() );
	if ( nMat == 0u || primIdx >= nMat ) {
		return vec3( -1.0 );
	}
	uint texIdx = mats[primIdx].textureIndex;
	if ( texIdx == 0xFFFFFFFFu || texIdx >= texCount ) {
		return vec3( -1.0 );
	}

	vec2 uv = vec2( 0.5 );
	uint nuv = uint( primUv.length() );
	if ( nuv >= ( primIdx + 1u ) * 6u ) {
		uint base = primIdx * 6u;
		vec2 uv0 = vec2( primUv[base + 0u], primUv[base + 1u] );
		vec2 uv1 = vec2( primUv[base + 2u], primUv[base + 3u] );
		vec2 uv2 = vec2( primUv[base + 4u], primUv[base + 5u] );
		float w = max( 1.0 - bary.x - bary.y, 0.0 );
		uv = uv0 * w + uv1 * bary.x + uv2 * bary.y;
	}

	vec3 c = texture( nonuniformEXT( bindlessDiffuse[texIdx] ), uv ).rgb;
	if ( dot( c, c ) <= 1e-8 ) {
		return vec3( -1.0 );
	}
	return c;
}

#endif
