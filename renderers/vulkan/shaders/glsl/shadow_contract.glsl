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

float ShadowContract_CascadeFar( vec4 splits, int cascade )
{
	if ( cascade <= 0 ) {
		return splits.x;
	}
	if ( cascade == 1 ) {
		return splits.y;
	}
	if ( cascade == 2 ) {
		return splits.z;
	}
	return splits.w;
}

/*
 * Raw cascade sample. Returns -1 when outside cascade frustum (Forward+ parity),
 * else visibility in [0,1] before strength mix.
 */
float ShadowContract_SampleCascadeRaw(
	GpuShadowGpuRecord rec,
	sampler2D shadowMap,
	vec3 worldPos )
{
	if ( ( rec.flags & 1u ) == 0u ) {
		return -1.0;
	}
	vec4 clip = rec.worldToShadow * vec4( worldPos, 1.0 );
	if ( abs( clip.w ) <= 1e-6 ) {
		return -1.0;
	}
	vec3 ndc = clip.xyz / clip.w;
	vec2 uvLocal = ndc.xy * 0.5 + 0.5;
	float depth = ndc.z * 0.5 + 0.5;
	if ( any( lessThan( uvLocal, vec2( 0.0 ) ) ) || any( greaterThan( uvLocal, vec2( 1.0 ) ) ) ||
		depth <= 0.0 || depth >= 1.0 ) {
		return -1.0;
	}

	/* atlasScaleBias: xy = tile scale, zw = tile offset */
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
	return vis;
}

/*
 * Sample one cascade record + apply strength.
 * worldPos: world-space receiver. Out-of-cascade → lit (1).
 */
float ShadowContract_SampleCascade(
	GpuShadowGpuRecord rec,
	sampler2D shadowMap,
	vec3 worldPos,
	float strength )
{
	if ( strength <= 0.0 ) {
		return 1.0;
	}
	float vis = ShadowContract_SampleCascadeRaw( rec, shadowMap, worldPos );
	if ( vis < 0.0 ) {
		return 1.0;
	}
	return mix( 1.0, vis, clamp( strength, 0.0, 1.0 ) );
}

/*
 * Multi-cascade CSM select + optional edge blend (matches gen_frag.tmpl pbrSunShadowVisibility).
 * records: SSBO array; cascadeCount 1..4; splits = cascade far view depths;
 * viewDist = distance from camera (same metric as Forward+ length(world-eye)).
 */
float ShadowContract_SampleCSM(
	GpuShadowGpuRecord rec0,
	GpuShadowGpuRecord rec1,
	GpuShadowGpuRecord rec2,
	GpuShadowGpuRecord rec3,
	sampler2D shadowMap,
	vec3 worldPos,
	float viewDist,
	float strength,
	uint cascadeCount,
	vec4 splits,
	float nearZ,
	float blend )
{
	if ( strength <= 0.0 || cascadeCount == 0u ) {
		return 1.0;
	}
	int count = int( clamp( float( cascadeCount ), 1.0, 4.0 ) );
	float z = max( viewDist, max( nearZ, 0.1 ) );
	float blendFrac = clamp( blend, 0.0, 0.5 );

	int cascade = count - 1;
	for ( int i = 0; i < 4; ++i ) {
		if ( i >= count ) {
			break;
		}
		if ( z < ShadowContract_CascadeFar( splits, i ) ) {
			cascade = i;
			break;
		}
	}

	GpuShadowGpuRecord rec = rec0;
	if ( cascade == 1 ) {
		rec = rec1;
	} else if ( cascade == 2 ) {
		rec = rec2;
	} else if ( cascade >= 3 ) {
		rec = rec3;
	}

	float vis = ShadowContract_SampleCascadeRaw( rec, shadowMap, worldPos );
	if ( vis < 0.0 ) {
		vis = 1.0;
	}

	if ( blendFrac > 1e-4 && cascade + 1 < count ) {
		float splitFar = ShadowContract_CascadeFar( splits, cascade );
		float splitNear = ( cascade == 0 ) ? max( nearZ, 0.1 ) : ShadowContract_CascadeFar( splits, cascade - 1 );
		float range = max( splitFar - splitNear, 1e-3 );
		float edge = ( z - ( splitFar - blendFrac * range ) ) / max( blendFrac * range, 1e-3 );
		edge = clamp( edge, 0.0, 1.0 );
		if ( edge > 0.0 ) {
			GpuShadowGpuRecord recNext = rec1;
			int c1 = cascade + 1;
			if ( c1 == 2 ) {
				recNext = rec2;
			} else if ( c1 >= 3 ) {
				recNext = rec3;
			}
			float vis1 = ShadowContract_SampleCascadeRaw( recNext, shadowMap, worldPos );
			if ( vis1 >= 0.0 ) {
				vis = mix( vis, vis1, edge );
			}
		}
	}

	return mix( 1.0, vis, clamp( strength, 0.0, 1.0 ) );
}

/*
 * Best-fit cascade without CPU splits: try fine→coarse, first in-frustum wins.
 * Used by WBOIT when push space cannot carry split distances.
 */
float ShadowContract_SampleCSM_BestFit(
	GpuShadowGpuRecord rec0,
	GpuShadowGpuRecord rec1,
	GpuShadowGpuRecord rec2,
	GpuShadowGpuRecord rec3,
	sampler2D shadowMap,
	vec3 worldPos,
	float strength,
	uint cascadeCount )
{
	if ( strength <= 0.0 || cascadeCount == 0u ) {
		return 1.0;
	}
	int count = int( clamp( float( cascadeCount ), 1.0, 4.0 ) );
	float vis = -1.0;
	for ( int i = 0; i < 4; ++i ) {
		if ( i >= count ) {
			break;
		}
		GpuShadowGpuRecord rec = rec0;
		if ( i == 1 ) {
			rec = rec1;
		} else if ( i == 2 ) {
			rec = rec2;
		} else if ( i >= 3 ) {
			rec = rec3;
		}
		float v = ShadowContract_SampleCascadeRaw( rec, shadowMap, worldPos );
		if ( v >= 0.0 ) {
			vis = v;
			break;
		}
	}
	if ( vis < 0.0 ) {
		return 1.0;
	}
	return mix( 1.0, vis, clamp( strength, 0.0, 1.0 ) );
}

#endif
