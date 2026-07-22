#pragma once

#ifdef USE_VULKAN

/*
 * Deferred Honesty Milestone — explicit eligibility, hybrid architecture naming,
 * classic material translation, G-buffer validity counters.
 * See docs/DEFERRED_HONESTY.md.
 */

typedef enum {
	DEFERRED_ARCH_ADDITIVE_HYBRID = 0, /* SceneBaseLit + deferred dynamics (reference) */
	DEFERRED_ARCH_MIXED_MATERIAL = 1,  /* true G-buffer + deferred static/dynamic for eligible */
	DEFERRED_ARCH_STRICT_VALIDATION = 2, /* eligible deferred; invalid shown, no silent fallback */
	DEFERRED_ARCH_COMPARE = 3          /* Forward+ vs mixed deferred comparison */
} deferredArchitecture_t;

/* Backward-compatible alias (Milestone 1 name). */
#define DEFERRED_ARCH_MIXED_ELIGIBILITY DEFERRED_ARCH_MIXED_MATERIAL

/* G-buffer normal.a owner bias for MIXED_MATERIAL_DEFERRED lightmap packing. */
#define DEFERRED_OWNER_BIAS 1024.0f

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
	DEFERRED_REASON_COUNT
} deferredEligibilityReason_t;

typedef enum {
	PIXEL_OWNER_FORWARD_BASE = 0,
	PIXEL_OWNER_DEFERRED_FULL,
	PIXEL_OWNER_DEFERRED_APPROX,
	PIXEL_OWNER_FORWARD_FALLBACK,
	PIXEL_OWNER_SKY,
	PIXEL_OWNER_SPECIAL
} deferredPixelOwner_t;

typedef enum {
	GBUFFER_VALID_BASE_COLOR = ( 1u << 0 ),
	GBUFFER_VALID_NORMAL = ( 1u << 1 ),
	GBUFFER_VALID_MATERIAL = ( 1u << 2 ),
	GBUFFER_VALID_EMISSIVE = ( 1u << 3 ),
	GBUFFER_APPROXIMATED = ( 1u << 4 ),
	GBUFFER_TRANSLATED_CLASSIC = ( 1u << 5 ),
	GBUFFER_PBR_NATIVE = ( 1u << 6 ),
	GBUFFER_USING_LIT_SCENE_AS_BASE = ( 1u << 7 ) /* hybrid: SceneBaseLit reused as sample */
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
	int baseColorStage;
	int lightmapStage;
	int unsupportedStageCount;
	int stageCount;
	deferredEligibilityReason_t failReason;
	char summary[128];
} ClassicShaderMaterialInfo;

typedef struct DeferredEligibilityResult_s {
	deferredEligibility_t eligibility;
	deferredEligibilityReason_t reason;
	unsigned gbufferFlags;
	ClassicShaderMaterialInfo classic;
	const char *reasonName;
} DeferredEligibilityResult;

void vk_deferred_honesty_register( void );
void vk_deferred_honesty_begin_frame( void );

/* Authoritative classification for opaque deferred candidacy. */
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

ClassicShaderMaterialInfo R_TranslateClassicShaderToMaterial( const shader_t *shader );

void R_DeferredHonesty_NoteEligibility( const DeferredEligibilityResult *res );
void R_DeferredHonesty_NoteOpaque( void );
void R_DeferredHonesty_NoteDeferredExported( unsigned gbufferFlags );
void R_DeferredHonesty_NoteDefaultGBuffer( void );
void R_DeferredHonesty_NoteLitSceneAsBase( void );

qboolean R_DeferredHonesty_WantsDeferredPath( const DeferredEligibilityResult *res );

void R_DeferredStatus_f( void );
void R_MaterialTranslateStatus_f( void );

extern cvar_t *r_deferredArchitecture;
extern cvar_t *r_deferredCompositeMode;
extern cvar_t *r_deferredEligibilityDebug;
extern cvar_t *r_gbufferInvalidPolicy;

#endif /* USE_VULKAN */
