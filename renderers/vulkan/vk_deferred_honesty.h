#pragma once

#ifdef USE_VULKAN

/*
 * Deferred Honesty Milestone — eligibility, classic translation, G-buffer validity,
 * ownership, and architecture modes. See docs/DEFERRED_HONESTY.md.
 */

typedef enum {
	DEFERRED_ARCH_FORWARD_PLUS_REFERENCE = 0,
	DEFERRED_ARCH_ADDITIVE_HYBRID = 1,
	DEFERRED_ARCH_FULL_FIDELITY = 2,
	DEFERRED_ARCH_COMPARE = 3,
	DEFERRED_ARCH_STRICT_VALIDATION = 4
} deferredArchitecture_t;

#define DEFERRED_ARCH_MIXED_MATERIAL DEFERRED_ARCH_FULL_FIDELITY
#define DEFERRED_ARCH_MIXED_ELIGIBILITY DEFERRED_ARCH_FULL_FIDELITY

/* G-buffer normal.a owner bias for MIXED_MATERIAL_DEFERRED lightmap packing. */
/* Legacy: owner bias used when LM packed into normal.a (retired — SurfaceData.a is 0/1). */
#define DEFERRED_OWNER_BIAS 1024.0f
#define DEFERRED_SURFACE_OWNER_THRESHOLD 0.5f

qboolean R_DeferredMixedMaterialWanted( void );
qboolean R_DeferredStrictValidationWanted( void );

typedef enum {
	DEFERRED_COMPOSITE_ADDITIVE_HYBRID = 0,
	DEFERRED_COMPOSITE_FULL_REPLACE = 1,
	DEFERRED_COMPOSITE_SIDE_BY_SIDE = 2,
	DEFERRED_COMPOSITE_MATERIAL_VALIDATE = 3
} deferredCompositeMode_t;

typedef enum {
	DEFERRED_ELIGIBLE_FULL = 0,
	DEFERRED_ELIGIBLE_APPROXIMATE = 1,
	DEFERRED_FORWARD_FALLBACK = 2,
	DEFERRED_UNSUPPORTED = 3,
	DEFERRED_DEBUG_FORCED = 4
} deferredEligibility_t;

typedef enum {
	DEFERRED_REASON_NONE = 0,
	DEFERRED_REASON_NO_BASE_COLOR_EXPORT,
	DEFERRED_REASON_NO_NORMAL_EXPORT,
	DEFERRED_REASON_NO_MATERIAL_RESPONSE,
	DEFERRED_REASON_MULTISTAGE_CLASSIC_SHADER,
	DEFERRED_REASON_BLENDED_SURFACE,
	DEFERRED_REASON_DEFORMED_SURFACE,
	DEFERRED_REASON_PORTAL_OR_MIRROR,
	DEFERRED_REASON_SPECIAL_ENVIRONMENT_STAGE,
	DEFERRED_REASON_UNSUPPORTED_TCMOD,
	DEFERRED_REASON_UNSUPPORTED_ANIMATION,
	DEFERRED_REASON_TRANSMISSION_OR_REFRACTION,
	DEFERRED_REASON_FORWARD_ONLY_POLICY,
	DEFERRED_REASON_SKY,
	DEFERRED_REASON_WEAPON_OR_UI,
	DEFERRED_REASON_TRANSPARENT,
	DEFERRED_REASON_CLASSIC_OR_MODE0,
	DEFERRED_REASON_PATH_NOT_READY,
	DEFERRED_REASON_PBR_NATIVE,
	DEFERRED_REASON_CLASSIC_TRANSLATED,
	DEFERRED_REASON_BASE_COLOR_EXPORT_UNREPRESENTABLE,
	DEFERRED_REASON_COUNT
} deferredEligibilityReason_t;

/* Positive translation audit bits (classic → material). */
typedef enum {
	BASE_COLOR_STAGE_VALID = ( 1u << 0 ),
	BASE_COLOR_VERTEX_MODULATION_VALID = ( 1u << 1 ),
	BASE_COLOR_CONSTANT_MODULATION_VALID = ( 1u << 2 ),
	LIGHTMAP_STAGE_VALID = ( 1u << 3 ),
	BASE_COLOR_EXPORT_UNREPRESENTABLE = ( 1u << 4 )
} classicTranslateAudit_t;

typedef enum opaqueLightingOwner_e {
	OPAQUE_OWNER_INVALID = 0,
	OPAQUE_OWNER_DEFERRED,
	OPAQUE_OWNER_FORWARD_PLUS,
	OPAQUE_OWNER_LIGHTMAP_ONLY,
	OPAQUE_OWNER_EXPLICIT_FULLBRIGHT,
	OPAQUE_OWNER_SPECIALIZED
} opaqueLightingOwner_t;

typedef opaqueLightingOwner_t deferredPixelOwner_t;
#define PIXEL_OWNER_NONE OPAQUE_OWNER_INVALID
#define PIXEL_OWNER_DEFERRED_FULL OPAQUE_OWNER_DEFERRED
#define PIXEL_OWNER_DEFERRED_APPROX OPAQUE_OWNER_DEFERRED
#define PIXEL_OWNER_FORWARD_FALLBACK OPAQUE_OWNER_FORWARD_PLUS
#define PIXEL_OWNER_FORWARD_BASE OPAQUE_OWNER_FORWARD_PLUS
#define PIXEL_OWNER_SKY OPAQUE_OWNER_SPECIALIZED
#define PIXEL_OWNER_SPECIAL OPAQUE_OWNER_SPECIALIZED

typedef enum {
	GBUFFER_VALID_BASE_COLOR = ( 1u << 0 ),
	GBUFFER_VALID_NORMAL = ( 1u << 1 ),
	GBUFFER_VALID_MATERIAL = ( 1u << 2 ),
	GBUFFER_VALID_EMISSIVE = ( 1u << 3 ),
	GBUFFER_APPROXIMATED = ( 1u << 4 ),
	GBUFFER_TRANSLATED_CLASSIC = ( 1u << 5 ),
	GBUFFER_PBR_NATIVE = ( 1u << 6 ),
	GBUFFER_USING_LIT_SCENE_AS_BASE = ( 1u << 7 ), /* hybrid: SceneBaseLit reused as sample */
	GBUFFER_VALID_LIGHTMAP = ( 1u << 8 ),
	GBUFFER_VALID_OWNERSHIP = ( 1u << 9 ),
	GBUFFER_NATIVE_PBR = GBUFFER_PBR_NATIVE /* alias per DoD naming */
} gbufferValidityFlags_t;

typedef struct ClassicShaderMaterialInfo_s {
	qboolean valid;
	qboolean hasBaseColor;
	qboolean hasLightmap;
	qboolean hasNormalMap;
	qboolean hasSpecularOrPhysical;
	qboolean hasEmissive;
	qboolean alphaTested;
	qboolean twoSided;
	qboolean vertexColorModulation;
	qboolean constantColorModulation;
	qboolean materialResponseDefaulted;
	int baseColorStage;
	int lightmapStage;
	int unsupportedStageCount;
	int stageCount;
	int rgbGen; /* colorGen_t of base stage */
	int tcGen;  /* texCoordGen_t of base diffuse bundle */
	unsigned translateAudit;
	deferredEligibilityReason_t failReason;
	float legacyRoughness;
	float legacySpecularF0;
	char rgbGenName[32];
	char tcGenName[32];
	char summary[160];
} ClassicShaderMaterialInfo;

typedef struct DeferredEligibilityResult_s {
	deferredEligibility_t eligibility;
	deferredEligibilityReason_t reason;
	unsigned gbufferFlags;
	deferredPixelOwner_t owner;
	ClassicShaderMaterialInfo classic;
	const char *reasonName;
} DeferredEligibilityResult;

typedef struct deferredOwnershipSnapshot_s {
	uint32_t eligibleMaterials;
	uint32_t deferredOwnedDraws;
	uint32_t forwardOwnedDraws;
	uint32_t unsupportedMaterials;
	uint32_t invalidOwnerPixels;
	uint32_t doubleOwnerPixels;
	uint32_t fullbrightEscapeCount;
} deferredOwnershipSnapshot_t;

void vk_deferred_honesty_register( void );
void vk_deferred_honesty_begin_frame( void );

DeferredEligibilityResult R_GetDeferredEligibility(
	const shader_t *shader,
	const surfaceType_t *surface,
	unsigned drawSurfSortFlags,
	int viewClass );

void R_DeferredEligibility_DebugColor( deferredEligibility_t elig, float outRgb[3] );
const char *R_DeferredEligibility_Name( deferredEligibility_t elig );
const char *R_DeferredEligibilityReason_Name( deferredEligibilityReason_t reason );
const char *R_DeferredArchitecture_Name( deferredArchitecture_t arch );
const char *R_DeferredCompositeMode_Name( deferredCompositeMode_t mode );
const char *R_DeferredPixelOwner_Name( deferredPixelOwner_t owner );

ClassicShaderMaterialInfo R_TranslateClassicShaderToMaterial( const shader_t *shader );

void R_DeferredHonesty_NoteEligibility( const DeferredEligibilityResult *res );
void R_DeferredHonesty_NoteOpaque( void );
void R_DeferredHonesty_NoteDeferredExported( unsigned gbufferFlags );
void R_DeferredHonesty_NoteDefaultGBuffer( void );
void R_DeferredHonesty_NoteLitSceneAsBase( void );
void R_DeferredHonesty_NoteGBufferBaseColor( void );
void R_DeferredHonesty_NoteDeferredLightmap( void );
void R_DeferredHonesty_NoteForwardLightmap( void );
void R_DeferredHonesty_NoteDoubleShaded( void );
void R_DeferredHonesty_NoteUnowned( void );
void R_DeferredHonesty_NoteInvalidGBuffer( void );
void R_DeferredHonesty_GetOwnershipSnapshot( deferredOwnershipSnapshot_t *out );

qboolean R_DeferredHonesty_WantsDeferredPath( const DeferredEligibilityResult *res );

void R_DeferredStatus_f( void );
void R_MaterialTranslateStatus_f( void );

extern cvar_t *r_deferredArchitecture;
extern cvar_t *r_deferredCompositeMode;
extern cvar_t *r_deferredEligibilityDebug;
extern cvar_t *r_gbufferInvalidPolicy;
extern cvar_t *r_legacyDeferredRoughness;
extern cvar_t *r_legacyDeferredSpecular;
extern cvar_t *r_deferredLightmapMode;
extern cvar_t *r_deferredLightmapDebug;
extern cvar_t *r_deferredOwnershipDebug;
extern cvar_t *r_deferredCompositeDebug;
extern cvar_t *r_deferredArchitectureCompare;

#endif /* USE_VULKAN */
