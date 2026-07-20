#pragma once

#ifdef USE_VULKAN

/*
 * Raster Ultra 1.8 — compact material intermediate representation.
 * Mirrors shaderStage_t / PBR flags without unrestricted node graphs.
 * Classic Q3 stages remain the source of truth until translated.
 */

typedef enum {
	VK_MAT_DOMAIN_OPAQUE = 0,
	VK_MAT_DOMAIN_ALPHA_TEST,
	VK_MAT_DOMAIN_TRANSPARENT,
	VK_MAT_DOMAIN_WATER,
	VK_MAT_DOMAIN_GLASS,
	VK_MAT_DOMAIN_PARTICLE,
	VK_MAT_DOMAIN_DECAL,
	VK_MAT_DOMAIN_SKY,
	VK_MAT_DOMAIN_VOLUMETRIC,
	VK_MAT_DOMAIN_UI,
	VK_MAT_DOMAIN_TERRAIN,
	VK_MAT_DOMAIN_COUNT
} vkMaterialDomain_t;

typedef enum {
	VK_MAT_BLEND_OPAQUE = 0,
	VK_MAT_BLEND_ALPHA,
	VK_MAT_BLEND_ADD,
	VK_MAT_BLEND_MULTIPLY,
	VK_MAT_BLEND_PREMUL
} vkMaterialBlendMode_t;

typedef enum {
	VK_MAT_SHADE_CLASSIC = 0,
	VK_MAT_SHADE_PBR_METAL_ROUGH,
	VK_MAT_SHADE_UNLIT
} vkMaterialShadeModel_t;

/* Static feature bits — compile / pipeline specialization groups (bounded). */
enum {
	VK_MAT_FEAT_ALPHA_TEST     = ( 1u << 0 ),
	VK_MAT_FEAT_TRANSMISSION   = ( 1u << 1 ),
	VK_MAT_FEAT_CLEARCOAT      = ( 1u << 2 ),
	VK_MAT_FEAT_ANISOTROPY     = ( 1u << 3 ),
	VK_MAT_FEAT_SKINNING       = ( 1u << 4 ),
	VK_MAT_FEAT_POM            = ( 1u << 5 ),
	VK_MAT_FEAT_TRIPLANAR      = ( 1u << 6 ),
	VK_MAT_FEAT_TERRAIN        = ( 1u << 7 ),
	VK_MAT_FEAT_WATER          = ( 1u << 8 ),
	VK_MAT_FEAT_DECAL          = ( 1u << 9 ),
	VK_MAT_FEAT_HEIGHT_BLEND   = ( 1u << 10 ),
	VK_MAT_FEAT_SHEEN          = ( 1u << 11 ),
	VK_MAT_FEAT_FLOWMAP        = ( 1u << 12 ),
	VK_MAT_FEAT_EVOLUTION      = ( 1u << 13 ),
	VK_MAT_FEAT_FREQUENCY      = ( 1u << 14 )
};

/* Dynamic feature bits — instance / weather driven (no new SPIR-V). */
enum {
	VK_MAT_DYN_WETNESS  = ( 1u << 0 ),
	VK_MAT_DYN_SNOW     = ( 1u << 1 ),
	VK_MAT_DYN_DUST     = ( 1u << 2 ),
	VK_MAT_DYN_RUST     = ( 1u << 3 ),
	VK_MAT_DYN_SOOT     = ( 1u << 4 ),
	VK_MAT_DYN_MOSS     = ( 1u << 5 ),
	VK_MAT_DYN_DAMAGE   = ( 1u << 6 )
};

#define VK_MAT_IR_CACHE_VERSION 1
#define VK_MAT_MAX_LAYERS       8
#define VK_MAT_MAX_NAME         64

typedef struct vkMaterialLayerIR_s {
	float weight;
	float heightContrast;
	float color[3];
	float roughness;
	float metallic;
	float opacity;
} vkMaterialLayerIR_t;

typedef struct vkMaterialIR_s {
	char name[VK_MAT_MAX_NAME];
	vkMaterialDomain_t domain;
	vkMaterialBlendMode_t blendMode;
	vkMaterialShadeModel_t shadeModel;
	uint32_t staticFeatures;
	uint32_t dynamicFeatures;
	uint32_t permutationKey;
	uint32_t cacheVersion;
	uint32_t sourceShaderIndex; /* tr.shaders[] index or ~0u */
	int layerCount;
	vkMaterialLayerIR_t layers[VK_MAT_MAX_LAYERS];
	float baseColor[4];
	float roughness;
	float metallic;
	float emissive[3];
	float opacity;
	float wetness;
	float snow;
	float dust;
	float rust;
	float soot;
	float moss;
	float damage;
	float uvScale;
	float heightBlendSharpness;
	/* Raster Ultra 1.12 — optional frequency metadata (defaults when unset). */
	float expectedTexelDensity;
	float patternPeriod;
	float proceduralMaxFreq;
	float preferredAnisotropy;
	uint8_t detailFrequencyBand; /* 0 macro, 1 meso, 2 micro */
	uint8_t alphaCoveragePolicy; /* 0 default, 1 coverage-preserve, 2 stochastic-eligible */
	uint8_t antiMoireImportance; /* 0–3 */
	uint8_t stochasticEligible;
	qboolean fromClassic;
	qboolean valid;
} vkMaterialIR_t;

void vk_material_ir_register_cvars( void );
void vk_material_ir_init( void );
void vk_material_ir_shutdown( void );

/* Translate a registered Q3/PBR shader stage into IR (best-effort, classic-safe). */
qboolean vk_material_ir_from_shader( const shader_t *shader, vkMaterialIR_t *out );

/* Empty / fallback IR for missing materials. */
void vk_material_ir_reset( vkMaterialIR_t *ir );

uint32_t vk_material_ir_permutation_key( const vkMaterialIR_t *ir );

void vk_material_ir_status_f( void );

#endif /* USE_VULKAN */
