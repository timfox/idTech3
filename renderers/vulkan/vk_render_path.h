#pragma once

#ifdef USE_VULKAN

/*
 * Clustered Hybrid M1 — authoritative surface render-path selection.
 * See docs/RENDERER_PATH_OWNERSHIP.md.
 */

typedef enum {
	RENDER_PATH_NONE = 0,
	RENDER_PATH_LEGACY_FORWARD,
	RENDER_PATH_DEFERRED_OPAQUE,
	RENDER_PATH_FORWARD_PLUS_OPAQUE,
	RENDER_PATH_FORWARD_PLUS_TRANSPARENT,
	RENDER_PATH_FORWARD_PLUS_WEAPON,
	RENDER_PATH_OIT,
	RENDER_PATH_SKY,
	RENDER_PATH_UI,
	RENDER_PATH_COUNT
} renderPath_t;

void R_RenderPath_RegisterCvars( void );
void R_RenderPath_BeginFrame( void );
void R_RenderPath_Note( renderPath_t path );
void R_RenderPath_Status_f( void );

const char *R_RenderPath_Name( renderPath_t path );
/* Stable HDR tint for r_renderPathDebug 1 (RGB 0..1). */
void R_RenderPath_DebugColor( renderPath_t path, float outRgb[3] );

/*
 * drawSurfSortFlags: optional bits already known at sort time.
 *   bit0 = first-person / depth-hack weapon candidate
 *   bit1 = force weapon path (RB_TryDeferWeaponDrawSurfs pre-filter)
 */
#define R_PATH_FLAG_WEAPON_CANDIDATE  1u
#define R_PATH_FLAG_FORCE_WEAPON      2u

renderPath_t R_SelectSurfaceRenderPath(
	const shader_t *shader,
	const surfaceType_t *surface,
	unsigned drawSurfSortFlags,
	int viewClass );

/* True when selected path hands opaque dynamics to deferred (skip Forward+ add). */
qboolean R_RenderPath_WantsDeferredHandoff( renderPath_t path );


/*
 * Material feature routing (Foundation Consolidation — docs/GBUFFER_2.md).
 */
#define R_MAT_FEAT_ANISOTROPY     (1u << 0)
#define R_MAT_FEAT_TRANSMISSION   (1u << 1)
#define R_MAT_FEAT_REFRACTION     (1u << 2)
#define R_MAT_FEAT_LAYERED        (1u << 3)
#define R_MAT_FEAT_SKIN           (1u << 4)
#define R_MAT_FEAT_WATER          (1u << 5)
#define R_MAT_FEAT_COMPLEX_COAT   (1u << 6)
#define R_MAT_FEAT_FORWARD_ONLY \
	( R_MAT_FEAT_ANISOTROPY | R_MAT_FEAT_TRANSMISSION | R_MAT_FEAT_REFRACTION | \
	  R_MAT_FEAT_LAYERED | R_MAT_FEAT_SKIN | R_MAT_FEAT_WATER | R_MAT_FEAT_COMPLEX_COAT )

renderPath_t R_SelectMaterialRenderPath(
	const shader_t *shader,
	unsigned materialFeatureFlags,
	const char **outReason );

void R_RenderPath_GetOpaqueCounts( uint32_t *outDeferred, uint32_t *outForwardPlus );

extern cvar_t *r_renderPathDebug;
extern cvar_t *r_hybridCompare;
extern cvar_t *r_materialPathDebug;
extern cvar_t *r_materialPathReason;
extern cvar_t *r_gbufferCompact;

#endif /* USE_VULKAN */
