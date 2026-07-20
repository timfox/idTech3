/*
===========================================================================
Raster Ultra 1.0 — raster-only high-end profile contract.

Locks ray tracing off when active. Does not change certified modern_vulkan.cfg.
===========================================================================
*/

#ifndef VK_RASTER_ULTRA_H
#define VK_RASTER_ULTRA_H

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void VK_RasterUltra_Init( void );
void VK_RasterUltra_Shutdown( void );

/* Latched r_rasterUltra != 0 after init / profile apply. */
qboolean VK_RasterUltra_Active( void );

/* Enforce RT-off contract (call after cvars load / profile exec / vid_restart). */
void VK_RasterUltra_Enforce( void );

/* Count of RT-related cvars that are non-zero (requested). */
int VK_RasterUltra_RTRequestedCount( void );

/* Count of RT systems that are effectively owning a signal this frame. */
int VK_RasterUltra_RTEffectiveCount( void );

/* Short completeness token: "complete" | "partial" | "rt_leak" | "inactive". */
const char *VK_RasterUltra_Completeness( void );

/* Human-readable ownership summary for havenrp_renderer_status. */
void VK_RasterUltra_PrintStatus( void );

#ifdef __cplusplus
}
#endif

#endif /* VK_RASTER_ULTRA_H */
