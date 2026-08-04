/*
 * Canonical material-boundary representation shared by Forward+, Deferred,
 * supported transparency, shadow alpha tests, and material debug views.
 * Inputs and outputs are scene-linear; roughness is perceptual at this seam.
 */
#ifndef SURFACE_MATERIAL_DECODE_GLSL
#define SURFACE_MATERIAL_DECODE_GLSL

/* These defaults are part of the legacy material ABI.  Every consumer that
 * does not have a physical map uses the same values; do not duplicate them in
 * a lighting path. */
const float SURFACE_LEGACY_ROUGHNESS = 0.72;
const float SURFACE_LEGACY_METALLIC = 0.0;
const float SURFACE_LEGACY_AO = 1.0;
const vec3 SURFACE_LEGACY_EMISSIVE = vec3( 0.0 );

const uint SURFACE_ALPHA_TEST_NONE = 0u;
const uint SURFACE_ALPHA_TEST_GT = 1u;
const uint SURFACE_ALPHA_TEST_GE = 2u;
/* Legacy alphaFunc encoding used by the generated raster pipelines. */
const uint SURFACE_ALPHA_TEST_NE = 3u;
const uint SURFACE_ALPHA_TEST_LT = 4u;
const uint SURFACE_ALPHA_TEST_LE = 5u;

bool SurfaceAlphaTestPass( float alpha, float threshold, uint mode )
{
	if ( mode == SURFACE_ALPHA_TEST_NONE ) {
		return true;
	}
	if ( mode == SURFACE_ALPHA_TEST_NE ) {
		return alpha != threshold;
	}
	if ( mode == SURFACE_ALPHA_TEST_LT ) {
		return alpha < threshold;
	}
	if ( mode == SURFACE_ALPHA_TEST_LE ) {
		return alpha <= threshold;
	}
	return ( mode == SURFACE_ALPHA_TEST_GE ) ? alpha >= threshold : alpha > threshold;
}

bool SurfaceAlphaTestPassLegacy( float alpha, float threshold, int func )
{
	if ( func == 1 ) return SurfaceAlphaTestPass( alpha, threshold, SURFACE_ALPHA_TEST_NE );
	if ( func == 2 ) return SurfaceAlphaTestPass( alpha, threshold, SURFACE_ALPHA_TEST_LT );
	if ( func == 3 ) return SurfaceAlphaTestPass( alpha, threshold, SURFACE_ALPHA_TEST_GE );
	return true;
}

const uint OPAQUE_OWNER_INVALID = 0u;
const uint OPAQUE_OWNER_DEFERRED = 1u;
const uint OPAQUE_OWNER_FORWARD_PLUS = 2u;
const uint OPAQUE_OWNER_LIGHTMAP_ONLY = 3u;
const uint OPAQUE_OWNER_EXPLICIT_FULLBRIGHT = 4u;
const uint OPAQUE_OWNER_SPECIALIZED = 5u;

struct SurfaceMaterial {
	vec3 baseColor;
	vec3 normalWS;
	float perceptualRoughness;
	float metallic;
	float ambientOcclusion;
	vec3 emissive;
	float opacity;
	float clearcoat;
	float clearcoatRoughness;
	float sheen;
	uint materialFlags;
	uint shadingModel;
	uint lightingOwner;
	uint lightmapIndex;
};

SurfaceMaterial SurfaceMaterialDecodeCanonical(
	vec3 baseColor, float opacity, vec3 normalWS,
	float perceptualRoughness, float metallic, float ambientOcclusion,
	vec3 emissive, float clearcoat, float clearcoatRoughness, float sheen,
	uint materialFlags, uint shadingModel, uint lightingOwner, uint lightmapIndex )
{
	SurfaceMaterial m;
	m.baseColor = max( baseColor, vec3( 0.0 ) );
	m.opacity = clamp( opacity, 0.0, 1.0 );
	m.normalWS = dot( normalWS, normalWS ) > 1e-8
		? normalize( normalWS ) : vec3( 0.0, 0.0, 1.0 );
	m.perceptualRoughness = clamp( perceptualRoughness, 0.04, 1.0 );
	m.metallic = clamp( metallic, 0.0, 1.0 );
	m.ambientOcclusion = clamp( ambientOcclusion, 0.0, 1.0 );
	m.emissive = max( emissive, vec3( 0.0 ) );
	m.clearcoat = clamp( clearcoat, 0.0, 1.0 );
	m.clearcoatRoughness = clamp( clearcoatRoughness, 0.04, 1.0 );
	m.sheen = max( sheen, 0.0 );
	m.materialFlags = materialFlags;
	m.shadingModel = shadingModel;
	m.lightingOwner = lightingOwner;
	m.lightmapIndex = lightmapIndex;
	return m;
}

SurfaceMaterial SurfaceMaterialDecodeLegacy(
	vec3 baseColor, float opacity, vec3 normalWS,
	vec3 emissive, uint materialFlags, uint shadingModel,
	uint lightingOwner, uint lightmapIndex )
{
	return SurfaceMaterialDecodeCanonical( baseColor, opacity, normalWS,
		SURFACE_LEGACY_ROUGHNESS, SURFACE_LEGACY_METALLIC, SURFACE_LEGACY_AO,
		emissive, 0.0, SURFACE_LEGACY_ROUGHNESS, 0.0,
		materialFlags, shadingModel, lightingOwner, lightmapIndex );
}

#endif
