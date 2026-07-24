/*
 * Canonical material-boundary representation shared by Forward+, Deferred,
 * supported transparency, shadow alpha tests, and material debug views.
 * Inputs and outputs are scene-linear; roughness is perceptual at this seam.
 */
#ifndef SURFACE_MATERIAL_DECODE_GLSL
#define SURFACE_MATERIAL_DECODE_GLSL

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

#endif
