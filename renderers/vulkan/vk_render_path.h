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

/* r_renderPathDebug / r_hybridCompare accessors (may be NULL before register). */
extern cvar_t *r_renderPathDebug;
extern cvar_t *r_hybridCompare;

#endif /* USE_VULKAN */
