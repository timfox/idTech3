/* Shared GpuShadowGpuRecord layout (std430) — matches vk_shadow_contract.c */
#ifndef SHADOW_CONTRACT_GLSL
#define SHADOW_CONTRACT_GLSL

struct GpuShadowGpuRecord {
	uint type;
	uint textureIndex;
	uint layerOrPage;
	uint flags;
	mat4 worldToShadow;
	vec4 atlasScaleBias;
	vec4 depthBiasParams;
	vec4 filterParams;
	uint slot;
	uint cascade;
	uint generation;
	uint extentW;
	uint extentH;
	uint _pad0;
	uint _pad1;
	uint _pad2;
};

/*
 * Match Forward+ / gen_frag.tmpl sun-shadow convention:
 *   depth = ndc.z * 0.5 + 0.5
 *   lit when (depth - bias) <= mapSample
 * (CSM atlas is standard [0,1] depth, not scene reversed-Z.)
 */
float ShadowContract_SampleMap( sampler2D shadowMap, vec2 uv, float compareDepth )
{
	if ( uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 ) {
		return 1.0;
	}
	float sampleDepth = texture( shadowMap, uv ).r;
	return ( compareDepth <= sampleDepth ) ? 1.0 : 0.0;
}

/*
 * Sample cascade shadow using contract record + atlas depth.
 * worldPos: world-space receiver position.
 * Returns 1 = lit, 0 = shadowed.
 */
float ShadowContract_SampleCascade(
	GpuShadowGpuRecord rec,
	sampler2D shadowMap,
	vec3 worldPos,
	float strength )
{
	if ( ( rec.flags & 1u ) == 0u || strength <= 0.0 ) {
		return 1.0;
	}
	vec4 clip = rec.worldToShadow * vec4( worldPos, 1.0 );
	if ( abs( clip.w ) <= 1e-6 ) {
		return 1.0;
	}
	vec3 ndc = clip.xyz / clip.w;
	vec2 uvLocal = ndc.xy * 0.5 + 0.5;
	float depth = ndc.z * 0.5 + 0.5;
	if ( any( lessThan( uvLocal, vec2( 0.0 ) ) ) || any( greaterThan( uvLocal, vec2( 1.0 ) ) ) ||
		depth <= 0.0 || depth >= 1.0 ) {
		return 1.0; /* out of cascade → treat as lit (same soft policy as missing cascade) */
	}

	/* atlasScaleBias: xy = tile scale, zw = tile offset (cascade 0 often 1,0 / 0,0) */
	float tileScaleX = ( rec.atlasScaleBias.x > 0.0 ) ? rec.atlasScaleBias.x : 1.0;
	float tileScaleY = ( rec.atlasScaleBias.y > 0.0 ) ? rec.atlasScaleBias.y : tileScaleX;
	vec2 uv = uvLocal * vec2( tileScaleX, tileScaleY ) + rec.atlasScaleBias.zw;

	float bias = max( rec.depthBiasParams.x, 0.0 );
	float compareDepth = depth - bias;
	float radius = max( rec.filterParams.x, 0.0 );
	ivec2 sz = textureSize( shadowMap, 0 );
	vec2 texel = vec2( radius ) / max( vec2( sz ), vec2( 1.0 ) );

	float vis;
	if ( radius <= 0.0 || texel.x <= 0.0 || texel.y <= 0.0 ) {
		vis = ShadowContract_SampleMap( shadowMap, uv, compareDepth );
	} else {
		vis = 0.0;
		vis += ShadowContract_SampleMap( shadowMap, uv, compareDepth );
		vis += ShadowContract_SampleMap( shadowMap, uv + vec2( texel.x, 0.0 ), compareDepth );
		vis += ShadowContract_SampleMap( shadowMap, uv - vec2( texel.x, 0.0 ), compareDepth );
		vis += ShadowContract_SampleMap( shadowMap, uv + vec2( 0.0, texel.y ), compareDepth );
		vis += ShadowContract_SampleMap( shadowMap, uv - vec2( 0.0, texel.y ), compareDepth );
		vis *= 0.2;
	}
	return mix( 1.0, vis, clamp( strength, 0.0, 1.0 ) );
}

#endif
