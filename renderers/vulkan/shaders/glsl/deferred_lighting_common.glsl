/*
 * Shared BRDF / light eval for deferred lighting and VRCS variant.
 * Expects: DEF_PI, MAX_PER_TILE, REC_VEC4S, lights, tiles, pc push with lighting fields.
 *
 * Diffuse matches Forward+ Disney/Burley Fd (gen_frag.tmpl Diffuse_Burley + Fresnel kD).
 */

#ifndef DEFERRED_LIGHTING_COMMON_GLSL
#define DEFERRED_LIGHTING_COMMON_GLSL

#include "forward_plus_cluster.glsl"

float viewZFromDepth( float depth ) {
	return -pc.projInfo.w / max( depth + pc.projInfo.z, 1e-6 );
}

vec3 reconstructViewPos( vec2 uv, float depth ) {
	float viewZ = viewZFromDepth( depth );
	float negZ = -viewZ;
	vec2 ndc = uv * 2.0 - 1.0;
	return vec3( ndc.x * negZ * pc.projInfo.x, ndc.y * negZ * pc.projInfo.y, viewZ );
}

vec3 toViewDir( vec3 worldDir ) {
	return normalize( ( pc.viewMatrix * vec4( worldDir, 0.0 ) ).xyz );
}

vec3 safeNormalizeOr( vec3 v, vec3 fallbackDir ) {
	float lenSq = dot( v, v );
	if ( lenSq <= 1e-8 ) {
		return fallbackDir;
	}
	return v * inversesqrt( lenSq );
}

float Pow5( float x ) {
	float x2 = x * x;
	return x2 * x2 * x;
}

/* Disney 2012 diffuse — same formulation as Forward+ Diffuse_Burley. */
vec3 Diffuse_Burley( vec3 diffuseColor, float NE, float NL, float LH, float roughness ) {
	float FD90 = 0.5 + 2.0 * LH * LH * roughness;
	float lightScatter = 1.0 + ( FD90 - 1.0 ) * Pow5( 1.0 - NL );
	float viewScatter = 1.0 + ( FD90 - 1.0 ) * Pow5( 1.0 - NE );
	return diffuseColor * ( 1.0 / DEF_PI ) * lightScatter * viewScatter;
}

float D_GGX( float NH, float alpha ) {
	float alphaSq = alpha * alpha;
	float d = ( NH * alphaSq - NH ) * NH + 1.0;
	return alphaSq / ( DEF_PI * d * d );
}

float CalcVisibility( float NL, float NE, float alpha ) {
	float alphaSq = alpha * alpha;
	float lambdaE = NL * sqrt( ( -NE * alphaSq + NE ) * NE + alphaSq );
	float lambdaL = NE * sqrt( ( -NL * alphaSq + NL ) * NL + alphaSq );
	return 0.5 / max( lambdaE + lambdaL, 1e-7 );
}

/* Attenuation only (no N·L) — matches Forward+ att * Fd * NL separation. */
float attenPointLight( vec3 lView, vec3 viewPos, float rad, out vec3 L ) {
	vec3 toLight = lView - viewPos;
	float dist = length( toLight );
	if ( dist > rad ) {
		L = vec3( 0.0 );
		return 0.0;
	}
	L = toLight / max( dist, 1e-4 );
	float dr = dist / rad;
	return clamp( 1.0 - dr * dr, 0.0, 1.0 );
}

float attenSpotLight( vec3 lView, vec3 viewPos, float rad,
	vec3 axisView, float cosOuter, float cosInner, float segLen, out vec3 L ) {
	vec3 v = viewPos - lView;
	float tAx = dot( v, axisView );
	float tCl = clamp( tAx, 0.0, segLen );
	vec3 closest = lView + axisView * tCl;
	float dPerp = length( viewPos - closest );
	if ( dPerp > rad ) {
		L = vec3( 0.0 );
		return 0.0;
	}
	vec3 wDir = v / max( length( v ), 1e-4 );
	float cosAng = dot( wDir, axisView );
	float cone = smoothstep( cosOuter, cosInner, cosAng );
	if ( cone <= 1e-4 ) {
		L = vec3( 0.0 );
		return 0.0;
	}
	float axAtt = 1.0 - smoothstep( segLen * 0.85, segLen, tCl );
	float rp = dPerp / max( rad, 1e-4 );
	float radAtt = clamp( 1.0 - rp * rp, 0.0, 1.0 );
	L = -wDir;
	return cone * axAtt * radAtt;
}

const uint CLASS_EMPTY = 0u;
const uint CLASS_SIMPLE_OPAQUE = 1u;
const uint CLASS_LAYERED = 2u;
const uint CLASS_TRANSMISSION = 3u;
const uint CLASS_EMISSIVE = 4u;
const uint CLASS_ALPHA_TEST = 5u;

float ApplyDeferredSpecularAA( float roughness, vec2 uv, ivec2 pix )
{
	if ( pc.specularAA <= 0.0 ) {
		return roughness;
	}
	/* Screen-space normal variance (compute path; Toksvig-style inflate, Ultra 1.12). */
	ivec2 sz = textureSize( normalTex, 0 );
	ivec2 px = clamp( pix, ivec2( 0 ), sz - ivec2( 1 ) );
	vec3 nC = texture( normalTex, uv ).xyz;
	vec3 nX = texelFetch( normalTex, clamp( px + ivec2( 1, 0 ), ivec2( 0 ), sz - ivec2( 1 ) ), 0 ).xyz;
	vec3 nY = texelFetch( normalTex, clamp( px + ivec2( 0, 1 ), ivec2( 0 ), sz - ivec2( 1 ) ), 0 ).xyz;
	vec3 dndx = nX - nC;
	vec3 dndy = nY - nC;
	float variance = min( dot( dndx, dndx ) + dot( dndy, dndy ), 0.5 );
	float toksvig = variance / ( 1.0 + variance );
	float alpha = max( roughness * roughness, 0.0004 );
	alpha = clamp( alpha + toksvig * pc.specularAA, 0.0004, 1.0 );
	return clamp( sqrt( alpha ), 0.02, 1.0 );
}

vec3 shadeDeferredPixel( uvec2 pix ) {
	vec2 uv = ( vec2( pix ) + 0.5 ) / vec2( pc.extent );
	float depth = texture( depthTex, uv ).r;

	if ( depth <= 0.0 || depth >= 1.0 ) {
		return vec3( 0.0 );
	}

	uint matClass = CLASS_SIMPLE_OPAQUE;
	float classSpecScale = 1.0;
	float classDiffScale = 1.0;
	if ( pc.useMaterialClass != 0u ) {
		matClass = texelFetch( classMap, ivec2( pix ), 0 ).r;
		if ( matClass == CLASS_EMPTY ) {
			return vec3( 0.0 );
		}
		if ( matClass == CLASS_ALPHA_TEST ) {
			matClass = CLASS_SIMPLE_OPAQUE;
		}
		if ( matClass == CLASS_EMISSIVE && pc.additive != 0u ) {
			return vec3( 0.0 );
		}
		if ( matClass == CLASS_LAYERED ) {
			classSpecScale = 1.12;
		} else if ( matClass == CLASS_TRANSMISSION ) {
			classSpecScale = 0.75;
			classDiffScale = 0.85;
		}
	}

	vec3 viewPos = reconstructViewPos( uv, depth );
	vec3 Nsamp = texture( normalTex, uv ).xyz;
	float normalConfidence = clamp( texture( normalTex, uv ).w, 0.0, 1.0 );
	vec3 N;
	if ( pc.normalsAreWorld != 0u ) {
		N = safeNormalizeOr( ( pc.viewMatrix * vec4( Nsamp, 0.0 ) ).xyz, vec3( 0.0, 0.0, 1.0 ) );
	} else {
		N = safeNormalizeOr( Nsamp, vec3( 0.0, 0.0, 1.0 ) );
	}
	vec3 albedo = texture( albedoTex, uv ).rgb;
	vec4 material = texture( materialTex, uv );
	float materialConfidence = 1.0; /* clearcoat packed in material.a; confidence from normal.w */
	float shadingConfidence = min( normalConfidence, materialConfidence );
	float metalness = mix( 0.0, clamp( material.r, 0.0, 1.0 ), shadingConfidence );
	float roughness = mix( 0.92, clamp( material.g, 0.04, 1.0 ), shadingConfidence );
	roughness = ApplyDeferredSpecularAA( roughness, uv, ivec2( pix ) );
	float materialAO = clamp( material.b, 0.0, 1.0 );
	float clearcoat = clamp( material.a, 0.0, 1.0 );
	float aoCoupling = mix( 1.0, materialAO, clamp( pc.aoStrength, 0.0, 1.0 ) * shadingConfidence );
	vec3 V = safeNormalizeOr( -viewPos, vec3( 0.0, 0.0, 1.0 ) );
	N = safeNormalizeOr( mix( vec3( 0.0, 0.0, 1.0 ), N, max( shadingConfidence, 0.15 ) ), vec3( 0.0, 0.0, 1.0 ) );
	vec3 F0 = mix( vec3( 0.04 ), albedo, metalness );
	float NE = max( dot( N, V ), 0.0 );

	uint tilesX = max( pc.tileGrid.x, 1u );
	uint tilesY = max( pc.tileGrid.y, 1u );
	uint zSlices = max( pc.zSlices, 1u );
	float tilePxX = float( pc.extent.x ) / float( tilesX );
	float tilePxY = float( pc.extent.y ) / float( tilesY );
	uint tx = min( uint( float( pix.x ) / tilePxX ), tilesX - 1u );
	uint ty = min( uint( float( pix.y ) / tilePxY ), tilesY - 1u );
	uint slice = fp_view_depth_to_slice( abs( viewPos.z ), zSlices, pc.zSliceMode, pc.zNear, pc.zFar );
	uint tileId = fp_cluster_index( tx, ty, tilesX, tilesY, slice, zSlices );
	uint clusterCount = max( pc.clusterCount, tilesX * tilesY * zSlices );
	uint legacyMax = 8u;

	uint nPack = uint( max( lights.data[0].x + 0.5, 0.0 ) );
	uint nLights = min( pc.numLights, nPack );
	uint maxPer = min( pc.maxPerTile, MAX_PER_TILE );
	if ( pc.compactLists == 0u ) {
		maxPer = min( maxPer, legacyMax );
	}

	vec3 diffuseAcc = vec3( 0.0 );
	vec3 specularAcc = vec3( 0.0 );

	for ( uint k = 0u; k < maxPer; k++ ) {
		uint li = Cluster_FetchLightIndex( tileId, k, pc.compactLists, legacyMax, clusterCount );
		if ( li == 0xFFFFFFFFu ) {
			break;
		}
		if ( li >= nLights ) {
			continue;
		}
		uint b0 = 2u + li * REC_VEC4S;
		vec3 lWorld = lights.data[ b0 ].xyz;
		float rad = max( lights.data[ b0 ].w, 1e-3 );
		vec4 lc = lights.data[ b0 + 1u ];
		vec4 lpack = lights.data[ b0 + 2u ];
		vec4 ltail = lights.data[ b0 + 3u ];
		vec3 lView = ( pc.viewMatrix * vec4( lWorld, 1.0 ) ).xyz;
		vec3 L;
		float att;
		float addBoost = mix( 1.0, 1.25, step( 0.5, ltail.z ) );

#ifdef USE_LTC_AREA_LIGHT
		if ( fpLightIsArea( lc.w ) ) {
			vec3 halfU = ( pc.viewMatrix * vec4( lpack.xyz, 0.0 ) ).xyz;
			vec3 halfV = ( pc.viewMatrix * vec4( ltail.xyz, 0.0 ) ).xyz;
			float addBoostA = mix( 1.0, 1.25, step( 0.5, lpack.w ) );
			vec3 areaLit = EvalRectAreaLight(
				N, V, viewPos, lView, halfU, halfV,
				lc.rgb * addBoostA, albedo, F0, metalness, roughness,
				ltcMatTex, ltcAmpTex );
			diffuseAcc += areaLit * aoCoupling * classDiffScale * shadingConfidence;
			continue;
		}
#else
		if ( lc.w >= 1.5 ) {
			/* Area light without LTC bindings (e.g. VRCS path) — skip rather than mis-shade as spot. */
			continue;
		}
#endif
		if ( lc.w < 0.5 ) {
			att = attenPointLight( lView, viewPos, rad, L );
		} else {
			vec3 axisView = toViewDir( lpack.xyz );
			att = attenSpotLight( lView, viewPos, rad, axisView, lpack.w, ltail.x, ltail.y, L );
		}
		float NL = max( dot( N, L ), 0.0 );
		if ( att <= 0.0 || NL <= 0.0 ) {
			continue;
		}

		vec3 H = normalize( L + V );
		float LH = max( dot( L, H ), 0.0 );
		float VH = max( dot( V, H ), 0.0 );
		vec3 F = F0 + ( 1.0 - F0 ) * Pow5( 1.0 - VH );
		vec3 kD = ( vec3( 1.0 ) - F ) * ( 1.0 - metalness );
		/* Forward+ Fd: Burley * kD, then * (att * NL). */
		vec3 Fd = Diffuse_Burley( albedo, NE, NL, LH, roughness ) * kD;
		diffuseAcc += lc.rgb * addBoost * Fd * ( att * NL ) * aoCoupling * classDiffScale * shadingConfidence;

		if ( pc.specular != 0u ) {
			float NH = max( dot( N, H ), 0.0 );
			float alpha = max( roughness * roughness, 0.04 );
			float D = D_GGX( NH, alpha );
			float Vis = CalcVisibility( NL, NE, alpha );
			specularAcc += lc.rgb * addBoost * att * ( D * Vis * F * NL ) *
				mix( 1.0, aoCoupling, 0.5 ) * pc.specularStrength * classSpecScale * shadingConfidence;

			/* Clearcoat: dielectric F0=0.04 lobe; attenuate base (Forward+ parity). */
			if ( clearcoat > 1e-4 ) {
				float ccRough = mix( 0.08, 0.35, roughness );
				float ccAlpha = max( ccRough * ccRough, 0.001 );
				float ccD = D_GGX( NH, ccAlpha );
				float ccVis = CalcVisibility( NL, NE, ccAlpha );
				vec3 ccF = vec3( 0.04 ) + ( vec3( 1.0 ) - vec3( 0.04 ) ) * Pow5( 1.0 - VH );
				specularAcc *= ( 1.0 - clearcoat * 0.85 );
				specularAcc += lc.rgb * addBoost * att * ( ccD * ccVis * ccF * NL ) *
					clearcoat * pc.specularStrength * shadingConfidence;
			}
		}
	}

	/*
	 * Soft-cap specular energy relative to diffuse. Bright GGX peaks otherwise land in
	 * color_image ahead of TAA and create highlight trails (no separate specular history).
	 */
	{
		const vec3 lumaW = vec3( 0.2126, 0.7152, 0.0722 );
		float specLuma = max( dot( specularAcc, lumaW ), 0.0 );
		float diffLuma = max( dot( diffuseAcc, lumaW ), 0.0 );
		float specCap = max( diffLuma * 2.5, 0.28 ) * max( pc.specularStrength, 0.01 );
		if ( specLuma > specCap ) {
			specularAcc *= specCap / max( specLuma, 1e-4 );
		}
	}

	float roughMod = mix( 1.0, 0.85, roughness );
	vec3 lit;
	if ( pc.additive != 0u ) {
		/* Additive: Fd already includes albedo × kD (Forward+ parity). */
		lit = ( diffuseAcc + specularAcc ) * roughMod * pc.strength;
	} else {
		/* Multiply/legacy: ambient floor on albedo + Burley dynamic (albedo already in Fd). */
		lit = ( albedo * vec3( 0.04 ) + diffuseAcc + specularAcc ) * roughMod * pc.strength;
	}
	return lit;
}

#endif
