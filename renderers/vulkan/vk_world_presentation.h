/*
 * Independently implemented from public behavioral specifications and
 * published graphics techniques. No proprietary third-party engine code used.
 */
#ifndef VK_WORLD_PRESENTATION_H
#define VK_WORLD_PRESENTATION_H

#ifdef USE_VULKAN

typedef enum worldPresentationFeature_e {
	WORLD_FEATURE_HDR_SKY            = 1u << 0,
	WORLD_FEATURE_SKY_ENVIRONMENT    = 1u << 1,
	WORLD_FEATURE_REFLECTION_PROBES  = 1u << 2,
	WORLD_FEATURE_WATER              = 1u << 3,
	WORLD_FEATURE_PROJECTED_LIGHTS   = 1u << 4,
	WORLD_FEATURE_DECALS             = 1u << 5,
	WORLD_FEATURE_DETAIL_TEXTURES    = 1u << 6,
	WORLD_FEATURE_LIGHTSTYLES        = 1u << 7,
	WORLD_FEATURE_LOCAL_FOG          = 1u << 8,
	WORLD_FEATURE_COLOR_CORRECTION   = 1u << 9,
	WORLD_FEATURE_MATERIAL_DRIVERS   = 1u << 10,
	WORLD_FEATURE_VISIBILITY_PORTALS = 1u << 11,
	WORLD_FEATURE_DISPLACEMENTS      = 1u << 12,
	WORLD_FEATURE_VIEWMODEL_LIGHTING = 1u << 13
} worldPresentationFeature_t;

typedef enum worldFeatureCert_e {
	WORLD_CERT_ABSENT = 0,
	WORLD_CERT_SCAFFOLD,
	WORLD_CERT_PARTIAL,
	WORLD_CERT_CERTIFIED
} worldFeatureCert_t;

typedef struct worldFeatureInfo_s {
	worldPresentationFeature_t bit;
	const char *name;
	const char *owner;
	const char *passPosition;
	const char *resourceInputs;
	const char *sceneHdrContribution;
	const char *exposureState;
	const char *depthConvention;
	const char *transparencyRoute;
	const char *perfCost;
	worldFeatureCert_t certification;
	qboolean enabled;
} worldFeatureInfo_t;

typedef struct worldExposureSettings_s {
	float minEV;
	float maxEV;
	float compensation;
	float brightenSpeed;
	float darkenSpeed;
	float lowPercentile;
	float highPercentile;
	float skyWeight;
	float centerWeight;
} worldExposureSettings_t;

typedef struct skyEnvironment_s {
	float origin[3];
	float scale;
	float fogColor[3];
	float fogStart;
	float fogEnd;
	float fogDensity;
	uint32_t flags;
} skyEnvironment_t;

typedef struct environmentProbe_s {
	float position[3];
	float influenceRadius;
	float boxMin[3];
	float boxMax[3];
	uint32_t radianceTexture;
	uint32_t irradianceTexture;
	uint32_t visibilityTexture;
	uint32_t priority;
	uint32_t flags;
} environmentProbe_t;

typedef struct reflectionMaterialExtension_s {
	float tint[3];
	float intensity;
	float fresnelMin;
	float fresnelMax;
	float fresnelExponent;
	float contrast;
	float saturation;
	uint32_t maskTexture;
	uint32_t flags;
} reflectionMaterialExtension_t;

typedef struct waterMaterial_s {
	float baseColor[3];
	float absorptionColor[3];
	float absorptionDistance;
	float refractiveIndex;
	float roughness;
	float normalScale;
	float reflectionStrength;
	float refractionStrength;
	float flowDirection[2];
	float flowSpeed;
	float waveScale[2];
	float waveSpeed[2];
	float foamDepth;
	float foamStrength;
	uint32_t normalTexture0;
	uint32_t normalTexture1;
	uint32_t flowTexture;
	uint32_t flags;
} waterMaterial_t;

/* projectedLight_t is owned by vk_flashlight.h — do not redefine here. */

typedef struct decalInstance_s {
	float transform[12];
	float color[4];
	float lifetime;
	float fadeTime;
	uint32_t material;
	uint32_t targetMask;
	uint32_t flags;
} decalInstance_t;

typedef struct lightStyle_s {
	float samples[64];
	uint32_t sampleCount;
	float sampleRate;
	float phase;
	uint32_t interpolation;
	uint32_t flags;
} lightStyle_t;

typedef struct localFogVolume_s {
	float transform[12];
	float color[3];
	float density;
	float startDistance;
	float endDistance;
	float heightFalloff;
	float anisotropy;
	uint32_t shape;
	uint32_t priority;
	uint32_t flags;
} localFogVolume_t;

typedef struct colorCorrectionVolume_s {
	float transform[12];
	float weight;
	float fadeDistance;
	uint32_t lutTexture;
	uint32_t priority;
	uint32_t flags;
} colorCorrectionVolume_t;

typedef struct materialParameterDriver_s {
	uint32_t operation;
	uint32_t inputA;
	uint32_t inputB;
	uint32_t outputParameter;
	float constants[4];
	uint32_t flags;
} materialParameterDriver_t;

typedef struct visibilityPortal_s {
	float plane[4];
	float boundsMin[3];
	float boundsMax[3];
	uint32_t areaA;
	uint32_t areaB;
	uint32_t open;
	uint32_t flags;
} visibilityPortal_t;

typedef struct occlusionRegion_s {
	float boundsMin[3];
	float boundsMax[3];
	uint32_t flags;
} occlusionRegion_t;

typedef struct terrainPatch_s {
	uint32_t baseSurface;
	uint32_t width;
	uint32_t height;
	uint32_t firstHeight;
	uint32_t firstNormal;
	uint32_t firstBlendWeight;
	float boundsMin[3];
	float boundsMax[3];
	uint32_t flags;
} terrainPatch_t;

typedef struct viewmodelLightingState_s {
	float keyDirection[3];
	float keyColor[3];
	float ambientColor[3];
	float exposureScale;
	uint32_t probeIndex;
	uint32_t flags;
} viewmodelLightingState_t;

void vk_world_presentation_register( void );
uint32_t vk_world_presentation_enabled_mask( void );
qboolean vk_world_presentation_feature_enabled( worldPresentationFeature_t bit );
void vk_world_presentation_set_feature( worldPresentationFeature_t bit, qboolean enable );
const worldFeatureInfo_t *vk_world_presentation_feature_info( worldPresentationFeature_t bit );

/* Exposure volume blend (single SceneHDR convention). */
void vk_world_exposure_settings_defaults( worldExposureSettings_t *out );
void vk_world_exposure_settings_apply( const worldExposureSettings_t *settings );
const worldExposureSettings_t *vk_world_exposure_settings_current( void );

#endif /* USE_VULKAN */
#endif
