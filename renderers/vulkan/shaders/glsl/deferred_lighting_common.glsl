/*
 * Shared BRDF / light eval for deferred lighting and VRCS variant.
 * Expects: DEF_PI, MAX_PER_TILE, REC_VEC4S, lights, tiles, pc push with lighting fields.
 *
 * Core BRDF: pbr_brdf_core.glsl (shared with Forward+ / OIT).
 */

#ifndef DEFERRED_LIGHTING_COMMON_GLSL
#define DEFERRED_LIGHTING_COMMON_GLSL

#include "forward_plus_cluster.glsl"
#include "pbr_brdf_core.glsl"
#include "surface_material_decode.glsl"
#include "gbuffer_octahedral.glsl"
#include "lightmap_decode.glsl"

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
	return PbrPow5( x );
}

/* Disney 2012 diffuse — same formulation as Forward+ Diffuse_Burley. */
vec3 Diffuse_Burley( vec3 diffuseColor, float NE, float NL, float LH, float roughness ) {
	return PbrDiffuseBurley( diffuseColor, NE, NL, LH, roughness );
}

float D_GGX( float NH, float alpha ) {
	return PbrD_GGX( NH, alpha );
}

float CalcVisibility( float NL, float NE, float alpha ) {
	return PbrVisibilitySmithGGX( NL, NE, alpha );
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

vec3 SampleDeferredNormal( vec2 uv, vec4 material, out float normalConfidence )
{
	vec4 nSamp = texture( normalTex, uv );
	vec3 Nsamp;
	if ( pc.gbufferCompact != 0u ) {
		/* Compact dual-write: octahedral in material.ba; normal.a holds AO (confidence = 1). */
		Nsamp = GbufDecodeOctahedral( material.ba );
		normalConfidence = 1.0;
	} else {
		Nsamp = nSamp.xyz;
		normalConfidence = ( pc.mixedMaterial != 0u ) ? 1.0 : clamp( nSamp.w, 0.0, 1.0 );
	}
	return Nsamp;
}

float ApplyDeferredSpecularAA( float roughness, vec2 uv, ivec2 pix, vec3 nC )
{
	if ( pc.specularAA <= 0.0 ) {
		return roughness;
	}
	/* Screen-space normal variance (compute path; Toksvig + geometric floor). */
	ivec2 sz = textureSize( normalTex, 0 );
	ivec2 px = clamp( pix, ivec2( 0 ), sz - ivec2( 1 ) );
	vec3 nX;
	vec3 nY;
	if ( pc.gbufferCompact != 0u ) {
		vec4 mX = texelFetch( materialTex, clamp( px + ivec2( 1, 0 ), ivec2( 0 ), sz - ivec2( 1 ) ), 0 );
		vec4 mY = texelFetch( materialTex, clamp( px + ivec2( 0, 1 ), ivec2( 0 ), sz - ivec2( 1 ) ), 0 );
		nX = GbufDecodeOctahedral( mX.ba );
		nY = GbufDecodeOctahedral( mY.ba );
	} else {
		nX = texelFetch( normalTex, clamp( px + ivec2( 1, 0 ), ivec2( 0 ), sz - ivec2( 1 ) ), 0 ).xyz;
		nY = texelFetch( normalTex, clamp( px + ivec2( 0, 1 ), ivec2( 0 ), sz - ivec2( 1 ) ), 0 ).xyz;
	}
	vec3 dndx = nX - nC;
	vec3 dndy = nY - nC;
	float variance = dot( dndx, dndx ) + dot( dndy, dndy );
	float r = PbrSpecularAARoughness( roughness, variance, 0.0, pc.specularAA );
	float depth = texture( depthTex, uv ).r;
	vec3 viewPos = reconstructViewPos( uv, depth );
	vec3 V = safeNormalizeOr( -viewPos, vec3( 0.0, 0.0, 1.0 ) );
	float NV = clamp( abs( dot( normalize( nC ), V ) ), 0.0, 1.0 );
	return PbrGlancingRoughness( r, NV );
}

#ifdef DEFERRED_HAS_IBL
/*
 * Sky split-sum IBL (prefilter + BRDF LUT + irradiance). Local probe pick is v2.
 * skipDiffuse: lightmap already owns static diffuse for this pixel.
 */
vec3 DeferredEvalSkyIBL( vec3 Nview, vec3 Vview, vec3 albedoIn, vec3 F0in,
	float metalIn, float roughIn, float aoIn, bool skipDiffuse )
{
	mat4 invView = inverse( pc.viewMatrix );
	vec3 Nw = safeNormalizeOr( ( invView * vec4( Nview, 0.0 ) ).xyz, vec3( 0.0, 0.0, 1.0 ) );
	vec3 Vw = safeNormalizeOr( ( invView * vec4( Vview, 0.0 ) ).xyz, vec3( 0.0, 0.0, 1.0 ) );
	vec3 Rw = reflect( -Vw, Nw );
	Rw.y *= -1.0;
	vec3 Nsample = Nw;
	Nsample.y *= -1.0;
	float NdotV = clamp( abs( dot( Nw, Vw ) ), 0.0, 1.0 );
	uint envLevels = textureQueryLevels( prefilterCube );
	float maxLod = ( envLevels > 0u ) ? float( envLevels - 1u ) : 0.0;
	float envLod = min( clamp( roughIn, 0.02, 1.0 ) * 6.0, maxLod );
	vec3 prefilter = textureLod( prefilterCube, Rw, envLod ).rgb;
	vec2 envBrdf = texture( brdfLutTex, vec2( NdotV, 1.0 - clamp( roughIn, 0.0, 1.0 ) ) ).rg;
	vec3 FssEss = F0in * envBrdf.x + vec3( envBrdf.y );
	vec3 specIbl = prefilter * FssEss;
	vec3 diffIbl = vec3( 0.0 );
	if ( !skipDiffuse ) {
		vec3 irr = texture( irradianceCube, Nsample ).rgb;
		diffIbl = irr * albedoIn * ( 1.0 - metalIn ) * aoIn;
	}
	return diffIbl + specIbl;
}
#endif

vec4 shadeDeferredPixel( uvec2 pix ) {
	vec2 uv = ( vec2( pix ) + 0.5 ) / vec2( pc.extent );
	float depth = texture( depthTex, uv ).r;

	if ( depth <= 0.0 || depth >= 1.0 ) {
		return vec4( 0.0 );
	}

	uint matClass = CLASS_SIMPLE_OPAQUE;
	float classSpecScale = 1.0;
	float classDiffScale = 1.0;
	if ( pc.useMaterialClass != 0u ) {
		matClass = texelFetch( classMap, ivec2( pix ), 0 ).r;
		if ( matClass == CLASS_EMPTY ) {
			return vec4( 0.0 );
		}
		if ( matClass == CLASS_TRANSMISSION ) {
			/* Transmission/refraction stay Forward+ owned; deferred cannot reconstruct the required layer state. */
			return vec4( 0.0 );
		}
		if ( matClass == CLASS_ALPHA_TEST ) {
			matClass = CLASS_SIMPLE_OPAQUE;
		}
		if ( matClass == CLASS_EMISSIVE && pc.additive != 0u ) {
			return vec4( 0.0 );
		}
		if ( matClass == CLASS_LAYERED ) {
			classSpecScale = 1.12;
		}
	}

	vec3 viewPos = reconstructViewPos( uv, depth );
	vec4 material = texture( materialTex, uv );
	float normalConfidence = 1.0;
	vec3 Nsamp = SampleDeferredNormal( uv, material, normalConfidence );
	vec3 N;
	if ( pc.normalsAreWorld != 0u ) {
		N = safeNormalizeOr( ( pc.viewMatrix * vec4( Nsamp, 0.0 ) ).xyz, vec3( 0.0, 0.0, 1.0 ) );
	} else {
		N = safeNormalizeOr( Nsamp, vec3( 0.0, 0.0, 1.0 ) );
	}
	vec3 albedo = texture( albedoTex, uv ).rgb;
	float materialConfidence = 1.0;
	float shadingConfidence = min( normalConfidence, materialConfidence );
	float metalness = mix( 0.0, clamp( material.r, 0.0, 1.0 ), shadingConfidence );
	float roughness = mix( 0.92, clamp( material.g, 0.04, 1.0 ), shadingConfidence );
	roughness = ApplyDeferredSpecularAA( roughness, uv, ivec2( pix ), Nsamp );

	/* MIXED_MATERIAL_DEFERRED: ownership + lightmap from GBufferSurfaceData. */
	bool mixedOwned = false;
	vec3 lightmapIrr = vec3( 1.0 );
	if ( pc.mixedMaterial != 0u ) {
#ifdef DEFERRED_HAS_SURFACE
		vec4 surf = texture( surfaceTex, uv );
		mixedOwned = ( surf.a > 0.5 );
		if ( !mixedOwned ) {
			return vec4( 0.0 );
		}
		lightmapIrr = max( surf.rgb, vec3( 0.0 ) );
#else
		/* Paths without SurfaceData cannot own mixed pixels. */
		return vec4( 0.0 );
#endif
	}

	/* Compact: AO in normal.a; clearcoat defaults (material.ba is octahedral). */
	float materialAO;
	float clearcoat;
	if ( pc.gbufferCompact != 0u ) {
		materialAO = clamp( texture( normalTex, uv ).a, 0.0, 1.0 );
		clearcoat = 0.0;
	} else {
		materialAO = clamp( material.b, 0.0, 1.0 );
		/* Direct MRT: material.a = clearcoat. Depth-fill packs confidence here — treat as 0 coat. */
		clearcoat = ( pc.normalsAreWorld != 0u ) ? clamp( material.a, 0.0, 1.0 ) : 0.0;
	}
	float aoCoupling = mix( 1.0, materialAO, clamp( pc.aoStrength, 0.0, 1.0 ) * shadingConfidence );
	SurfaceMaterial surfaceMaterial = SurfaceMaterialDecodeCanonical(
		albedo, 1.0, Nsamp, roughness, metalness, materialAO,
		vec3( 0.0 ), clearcoat, mix( 0.08, 0.35, roughness ), 0.0,
		0u, 0u, mixedOwned ? OPAQUE_OWNER_DEFERRED : OPAQUE_OWNER_FORWARD_PLUS, 0u );
	albedo = surfaceMaterial.baseColor;
	roughness = surfaceMaterial.perceptualRoughness;
	metalness = surfaceMaterial.metallic;
	materialAO = surfaceMaterial.ambientOcclusion;
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

			/* Clearcoat: shared lobe (Forward+ / pbr_brdf_core parity). */
			if ( clearcoat > 1e-4 ) {
				float ccRough = mix( 0.08, 0.35, roughness );
				specularAcc *= ( 1.0 - clearcoat * 0.85 );
				specularAcc += lc.rgb * addBoost * att *
					clearcoat_lobe( NH, NL, NE, VH, clearcoat, ccRough ) * NL *
					pc.specularStrength * shadingConfidence;
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

	float roughMod = 1.0; /* IQ P1-O: was mix(1,0.85,roughness) — deferred/Forward+ seam */

	/* Milestone 3: full directional sun BRDF (Burley + GGX + multiscatter + clearcoat). */
	vec3 sunDiffuse = vec3( 0.0 );
	vec3 sunSpecular = vec3( 0.0 );
	uint sunFlags = uint( pc.sunRadiance.w + 0.5 );
	bool sunBrdf = ( sunFlags & 1u ) != 0u;
	bool lmOwnsDiffuse = ( sunFlags & 2u ) != 0u;
	if ( sunBrdf && ( pc.mixedMaterial == 0u || mixedOwned ) ) {
		vec3 Lsun = safeNormalizeOr( pc.sunDir.xyz, vec3( 0.0, 0.0, 1.0 ) );
		if ( pc.normalsAreWorld != 0u ) {
			/* N is view-space above; transform sun L into view for N·L. */
			Lsun = safeNormalizeOr( ( pc.viewMatrix * vec4( Lsun, 0.0 ) ).xyz, Lsun );
		}
		float NLsun = max( dot( N, Lsun ), 0.0 );
		if ( NLsun > 1e-5 ) {
			vec3 Hsun = normalize( Lsun + V );
			float LHsun = max( dot( Lsun, Hsun ), 0.0 );
			float VHsun = max( dot( V, Hsun ), 0.0 );
			float NHsun = max( dot( N, Hsun ), 0.0 );
			vec3 Fsun = F0 + ( 1.0 - F0 ) * Pow5( 1.0 - VHsun );
			vec3 kDsun = ( vec3( 1.0 ) - Fsun ) * ( 1.0 - metalness );
			vec3 sunRad = max( pc.sunRadiance.rgb, vec3( 0.0 ) );
			/* When lightmap owns static diffuse, skip sun Burley to avoid double baking. */
			if ( !( pc.mixedMaterial != 0u && mixedOwned && lmOwnsDiffuse &&
					( lightmapIrr.r + lightmapIrr.g + lightmapIrr.b ) > 1e-4 ) ) {
				sunDiffuse = sunRad * Diffuse_Burley( albedo, NE, NLsun, LHsun, roughness ) *
					kDsun * NLsun * aoCoupling;
			}
			if ( pc.specular != 0u ) {
				float alpha = max( roughness * roughness, 0.04 );
				float D = D_GGX( NHsun, alpha );
				float Vis = CalcVisibility( NLsun, NE, alpha );
				vec3 ms = PbrEnergyCompensation( F0, roughness );
				sunSpecular = sunRad * ( D * Vis * Fsun * NLsun ) * ms *
					mix( 1.0, aoCoupling, 0.5 ) * pc.specularStrength;
				if ( clearcoat > 1e-4 ) {
					float ccRough = mix( 0.08, 0.35, roughness );
					sunSpecular *= ( 1.0 - clearcoat * 0.85 );
					sunSpecular += sunRad * clearcoat_lobe( NHsun, NLsun, NE, VHsun, clearcoat, ccRough ) *
						NLsun * pc.specularStrength;
				}
			}
		}
	}
	vec3 sunTerm = sunDiffuse + sunSpecular;

	float sunVis = 1.0;
#ifdef DEFERRED_HAS_SHADOW_CONTRACT
	if ( ( pc.shadowFlags & 1u ) != 0u && pc.shadowStrength > 0.0 ) {
		mat4 invView = inverse( pc.viewMatrix );
		vec3 worldPos = ( invView * vec4( viewPos, 1.0 ) ).xyz;
		float viewDist = max( length( viewPos ), max( pc.shadowNear, 0.1 ) );
		sunVis = ShadowContract_SampleCSM(
			shadows.records[0], shadows.records[1], shadows.records[2], shadows.records[3],
			sunShadowMap, worldPos, viewDist, pc.shadowStrength,
			pc.shadowCascadeCount, pc.shadowSplits, pc.shadowNear, pc.shadowBlend );
	}
#endif

	vec3 iblTerm = vec3( 0.0 );
#ifdef DEFERRED_HAS_IBL
	if ( ( pc.iblFlags & 1u ) != 0u && pc.specular != 0u &&
		( pc.mixedMaterial == 0u || mixedOwned ) ) {
		bool skipIblDiff = ( pc.mixedMaterial != 0u && mixedOwned &&
			( lightmapIrr.r + lightmapIrr.g + lightmapIrr.b ) > 1e-4 );
		iblTerm = DeferredEvalSkyIBL( N, V, albedo, F0, metalness, roughness, aoCoupling, skipIblDiff ) *
			max( pc.iblStrength, 0.0 );
	}
#endif

	vec3 lit;
	if ( pc.mixedMaterial != 0u && mixedOwned ) {
		/* Static LM + sun BRDF (CSM on primary only) + clustered dynamics (unshadowed by sun CSM). */
		vec3 staticTerm;
		if ( pc.lightmapMode != 0u ) {
			vec3 dominantL = safeNormalizeOr( pc.sunDir.xyz, vec3( 0.0, 0.0, 1.0 ) );
			if ( pc.normalsAreWorld != 0u ) {
				dominantL = safeNormalizeOr( ( pc.viewMatrix * vec4( dominantL, 0.0 ) ).xyz, dominantL );
			}
			staticTerm = DeferredStaticDiffuseFromDeluxeApprox( albedo, metalness, lightmapIrr,
				aoCoupling, N, dominantL, pc.lightmapDeluxeStrength );
			if ( pc.lightmapMode == 2u ) {
				vec3 irradianceOnly = DeferredStaticDiffuseFromLightmap( albedo, metalness, lightmapIrr, aoCoupling );
				staticTerm = mix( irradianceOnly, staticTerm, 0.5 );
			}
		} else {
			staticTerm = DeferredStaticDiffuseFromLightmap( albedo, metalness, lightmapIrr, aoCoupling );
		}
		vec3 primary = ( staticTerm + sunTerm ) * sunVis;
		lit = ( primary + diffuseAcc + specularAcc + iblTerm ) * roughMod * pc.strength;
	} else if ( pc.additive != 0u ) {
		/* Hybrid additive: dynamics only (SceneBaseLit already has primary + sunVis + IBL). */
		lit = ( diffuseAcc + specularAcc ) * roughMod * pc.strength;
	} else {
		lit = ( albedo * vec3( 0.04 ) + diffuseAcc + specularAcc + sunTerm * sunVis + iblTerm ) *
			roughMod * pc.strength;
	}

	/* .a = deferred pixel ownership for MIXED_MATERIAL_DEFERRED composite replace. */
	float ownerA = ( pc.mixedMaterial != 0u && mixedOwned ) ? 1.0 : 0.0;
	return vec4( lit, ownerA );
}

#endif
