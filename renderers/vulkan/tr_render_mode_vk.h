/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan r_renderMode helpers (Spine 1.0–1.2).
===========================================================================
*/
#ifndef TR_RENDER_MODE_VK_H
#define TR_RENDER_MODE_VK_H

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct renderModeProfile_s {
	int mode;
	const char *name;
	const char *tier;
	qboolean wantsForwardPlus;
	qboolean wantsGBuffer;
	qboolean wantsDeferredLighting;
	qboolean wantsOpaqueTransparentSplit;
	qboolean wantsPathTracing;
	qboolean productionDefault;
} renderModeProfile_t;

void R_ApplyRenderModeLatch( void );

/* Product tiers (Spine 1.2):
 *  2 — Tier A Certified Raster (Forward+)
 *  3 — Unified Clustered raster
 *  4 — Tier B Selective Hybrid
 *  5 — Tier C Full Path-Traced Reference
 */
qboolean R_RenderMode_IsCertifiedRaster( void );      /* mode 2 */
qboolean R_RenderMode_IsUnifiedClustered( void );     /* mode 3 or 4 base raster */
qboolean R_RenderMode_IsSelectiveHybrid( void );      /* mode 4 */
qboolean R_RenderMode_IsPathTracedReference( void );  /* mode 5 */
qboolean R_RenderMode_WantsGBuffer( void );           /* 1,2,3,4,5 when sidecar allowed */
qboolean R_RenderMode_WantsDeferredLighting( void );  /* 1,3,4 (not 2; 5 optional scaffold) */
qboolean R_RenderMode_WantsOpaqueTransparentSplit( void ); /* 1/3/4 unified split */
const char *R_RenderMode_TierName( void );
const renderModeProfile_t *R_RenderMode_ProfileForValue( int mode );
const renderModeProfile_t *R_RenderMode_CurrentProfile( void );

/* Present-time adaptive reconstruction policy (no frame generation / no added latency). */
qboolean R_PresentAdaptiveRecon_Allowed( void );

#ifdef __cplusplus
}
#endif

#endif /* TR_RENDER_MODE_VK_H */
