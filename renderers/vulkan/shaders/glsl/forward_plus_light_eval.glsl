/* Shared Forward+ / OIT light evaluation (Burley diffuse + GGX specular).
 * Expects: fp_lights, fp_tiles, fp_params SSBOs and CLUSTER_LIST_CELLS + cluster_light_list.glsl.
 * Core BRDF: pbr_brdf_core.glsl (shared with deferred_lighting_common.glsl).
 */
#ifndef FORWARD_PLUS_LIGHT_EVAL_GLSL
#define FORWARD_PLUS_LIGHT_EVAL_GLSL

#include "pbr_brdf_core.glsl"
#include "surface_material_decode.glsl"

#ifndef FP_EVAL_PI
#define FP_EVAL_PI PBR_BRDF_PI
#endif

float FpEval_Pow5( float x )
{
	return PbrPow5( x );
}

vec3 FpEval_Diffuse_Burley( vec3 diffuseColor, float NE, float NL, float LH, float roughness )
{
	return PbrDiffuseBurley( diffuseColor, NE, NL, LH, roughness );
}

float FpEval_D_GGX( float NH, float alpha )
{
	return PbrD_GGX( NH, alpha );
}

float FpEval_Visibility( float NL, float NE, float alpha )
{
	return PbrVisibilitySmithGGX( NL, NE, alpha );
}

vec3 FpEval_FresnelSchlick( float cosTheta, vec3 F0 )
{
	return PbrFresnelSchlick( cosTheta, F0 );
}

/* lightingDebug: 0=full, 1=diffuse, 2=specular, 3=direct, 6=cluster count heat */
vec3 FpEval_ForwardPlusAdd(
	vec3 albedo,
	vec3 N,
	vec3 V,
	vec3 worldPos,
	float roughness,
	float metalness,
	int lightingDebug,
	out bool clusterOob,
	out uint lightCountOut )
{
	SurfaceMaterial surfaceMaterial = SurfaceMaterialDecodeCanonical(
		albedo, 1.0, N, roughness, metalness, 1.0, vec3( 0.0 ),
		0.0, roughness, 0.0, 0u, 0u, OPAQUE_OWNER_FORWARD_PLUS, 0u );
	albedo = surfaceMaterial.baseColor;
	N = surfaceMaterial.normalWS;
	roughness = surfaceMaterial.perceptualRoughness;
	metalness = surfaceMaterial.metallic;
	vec3 fpAdd = vec3( 0.0 );
	vec3 diffAccum = vec3( 0.0 );
	vec3 specAccum = vec3( 0.0 );
	clusterOob = false;
	lightCountOut = 0u;

	uint tilesX = fp_params.fp_tiles_xy_viewport.x;
	uint tilesY = fp_params.fp_tiles_xy_viewport.y;
	float vw = float( fp_params.fp_tiles_xy_viewport.z );
	float vh = float( fp_params.fp_tiles_xy_viewport.w );
	if ( tilesX == 0u || tilesY == 0u || vw <= 1.0 || vh <= 1.0 ) {
		tilesX = uint( fp_lights.fp_light_data[1].x + 0.5 );
		tilesY = uint( fp_lights.fp_light_data[1].y + 0.5 );
		vw = fp_lights.fp_light_data[1].z;
		vh = fp_lights.fp_light_data[1].w;
	}
	if ( tilesX == 0u || tilesY == 0u || vw <= 1.0 || vh <= 1.0 ) {
		return fpAdd;
	}

	float tilePxX = vw / max( float( tilesX ), 1.0 );
	float tilePxY = vh / max( float( tilesY ), 1.0 );
	vec4 wc = fp_params.fp_clip_from_world * vec4( worldPos, 1.0 );
	if ( abs( wc.w ) <= 1e-5 ) {
		return fpAdd;
	}
	vec3 ndc = wc.xyz / wc.w;
	if ( ndc.z < -1.0 || ndc.z > 1.0 || ndc.x < -1.05 || ndc.x > 1.05 || ndc.y < -1.05 || ndc.y > 1.05 ) {
		return fpAdd;
	}
	vec2 px;
	px.x = 0.5 * ( 1.0 + ndc.x ) * vw;
	px.y = 0.5 * ( 1.0 + ndc.y ) * vh;
	/* Odd extents: clamp tile indices; never assume even width/height. */
	uint tx = min( uint( clamp( px.x / tilePxX, 0.0, float( tilesX ) - 1e-4 ) ), tilesX - 1u );
	uint ty = min( uint( clamp( px.y / tilePxY, 0.0, float( tilesY ) - 1e-4 ) ), tilesY - 1u );

	uint zSlices = max( fp_params.fp_cluster_meta.x, 1u );
	uint zMode = fp_params.fp_cluster_meta.y;
	uint compactLists = fp_params.fp_cluster_meta.z;
	uint clusterCount = max( fp_params.fp_cluster_meta.w, tilesX * tilesY * zSlices );
	float zNear = max( fp_params.fp_cluster_z_range.x, 1e-3 );
	float zFar = max( fp_params.fp_cluster_z_range.y, zNear + 1e-3 );
	uint slice = fp_view_depth_to_slice( abs( wc.w ), zSlices, zMode, zNear, zFar );
	uint tileId = fp_cluster_index( tx, ty, tilesX, tilesY, slice, zSlices );
	uint legacyMax = 8u;
	if ( tileId >= clusterCount ) {
		clusterOob = true;
		return fpAdd;
	}

	float nLights = fp_lights.fp_light_data[0].x;
	uint maxPerTile = uint( max( fp_lights.fp_light_data[0].z + 0.5, 1.0 ) );
	maxPerTile = min( maxPerTile, compactLists != 0u ? 32u : legacyMax );

	roughness = clamp( roughness, 0.04, 1.0 );
	metalness = clamp( metalness, 0.0, 1.0 );
	vec3 F0 = mix( vec3( 0.04 ), albedo, metalness );
	vec3 diffuseColor = albedo * ( 1.0 - metalness );
	float alpha = max( roughness * roughness, 0.0004 );
	vec3 Nv = normalize( N );
	vec3 Vv = normalize( V );
	float NE = max( dot( Nv, Vv ), 0.0 );

	for ( uint k = 0u; k < maxPerTile; k++ ) {
		uint li = Cluster_FetchLightIndex( tileId, k, compactLists, legacyMax, clusterCount );
		if ( li == 0xFFFFFFFFu ) {
			continue;
		}
		if ( float( li ) + 0.5 >= nLights ) {
			continue;
		}
		uint b0 = 2u + li * 4u;
		vec3 lpos = fp_lights.fp_light_data[ b0 ].xyz;
		float rad = max( fp_lights.fp_light_data[ b0 ].w, 1e-3 );
		vec4 lc = fp_lights.fp_light_data[ b0 + 1u ];
		vec4 lpack = fp_lights.fp_light_data[ b0 + 2u ];
		vec3 Ldir;
		float att = 0.0;
		if ( lc.w < 0.5 ) {
			vec3 Lw = lpos - worldPos;
			float dist = length( Lw );
			if ( dist > rad ) {
				continue;
			}
			float dr = dist / max( rad, 1e-4 );
			att = clamp( 1.0 - dr * dr, 0.0, 1.0 );
			Ldir = Lw / max( dist, 1e-4 );
		} else {
			/* Spot: cone from packed axis + cosOuter/Inner in lpack.w / lc extras when present. */
			vec3 axis = normalize( vec3( lpack.x, lpack.y, lpack.z ) );
			float cosOuter = clamp( lpack.w, -1.0, 1.0 );
			float cosInner = clamp( fp_lights.fp_light_data[ b0 + 3u ].x, cosOuter, 1.0 );
			vec3 toPos = worldPos - lpos;
			float segLen = max( fp_lights.fp_light_data[ b0 + 3u ].y, 0.0 );
			float tAx = dot( toPos, axis );
			float tCl = clamp( tAx, 0.0, segLen );
			vec3 closest = lpos + axis * tCl;
			float dPerp = length( worldPos - closest );
			if ( dPerp > rad ) {
				continue;
			}
			vec3 wDir = toPos / max( length( toPos ), 1e-4 );
			float cosAng = dot( wDir, axis );
			float cone = smoothstep( cosOuter, cosInner, cosAng );
			if ( cone <= 1e-4 ) {
				continue;
			}
			float axAtt = 1.0 - smoothstep( segLen * 0.85, max( segLen, 1e-4 ), tCl );
			float rp = dPerp / max( rad, 1e-4 );
			att = cone * axAtt * clamp( 1.0 - rp * rp, 0.0, 1.0 );
			Ldir = -wDir;
		}
		float NL = max( dot( Nv, Ldir ), 0.0 );
		if ( att <= 0.0 || NL <= 0.0 ) {
			continue;
		}
		lightCountOut++;
		vec3 H = normalize( Ldir + Vv );
		float NH = max( dot( Nv, H ), 0.0 );
		float LH = max( dot( Ldir, H ), 0.0 );
		vec3 Fd = FpEval_Diffuse_Burley( diffuseColor, NE, NL, LH, roughness );
		vec3 F = FpEval_FresnelSchlick( LH, F0 );
		vec3 kD = ( vec3( 1.0 ) - F ) * ( 1.0 - metalness );
		float D = FpEval_D_GGX( NH, alpha );
		float Vis = FpEval_Visibility( NL, NE, alpha );
		vec3 Fr = D * Vis * F;
		float fpAdditive = fp_lights.fp_light_data[ b0 + 3u ].z;
		float addBoost = mix( 1.0, 1.25, step( 0.5, fpAdditive ) );
		vec3 lightRgb = lc.rgb * att * addBoost;
		diffAccum += Fd * kD * NL * lightRgb;
		specAccum += Fr * NL * lightRgb;
	}

	if ( lightingDebug == 1 ) {
		return diffAccum;
	}
	if ( lightingDebug == 2 ) {
		return specAccum;
	}
	if ( lightingDebug == 6 ) {
		float t = float( lightCountOut ) / 8.0;
		return vec3( t, 1.0 - t, 0.0 );
	}
	fpAdd = diffAccum + specAccum;
	if ( lightingDebug == 3 ) {
		return fpAdd;
	}
	return fpAdd;
}

#endif /* FORWARD_PLUS_LIGHT_EVAL_GLSL */
