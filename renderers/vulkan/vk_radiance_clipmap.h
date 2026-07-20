/*
===========================================================================
Raster Ultra 1.13 — Camera-centered clipmapped radiance cache (raster-only).
No BLAS/TLAS/ray queries/RT pipelines. See docs/RASTER_ULTRA_1.13.md.
===========================================================================
*/
#ifndef VK_RADIANCE_CLIPMAP_H
#define VK_RADIANCE_CLIPMAP_H

#include "../common/tr_types.h"
#include <vulkan/vulkan.h>

void vk_radiance_clipmap_init( void );
void vk_radiance_clipmap_shutdown( void );
void vk_radiance_clipmap_frame_begin( void );
void vk_radiance_clipmap_on_map_load( void );
void vk_radiance_clipmap_invalidate( void );

qboolean vk_radiance_clipmap_active( void );
qboolean vk_radiance_clipmap_ready( void );

/* CPU update: scroll, inject, propagate (budgeted). Call before GPU sample. */
void vk_radiance_clipmap_update( const trRefdef_t *refdef );

/* Dispatch sample compute into cache irradiance / meta images. */
void vk_radiance_clipmap_dispatch_sample( VkCommandBuffer cmd,
	VkImageView depthView, VkImageView normalView,
	const float invView[16], const float projInfo[4], uint32_t normalsAreWorld,
	uint32_t width, uint32_t height );

VkImageView vk_radiance_clipmap_irradiance_view( void );
VkImageView vk_radiance_clipmap_meta_view( void );
VkImageLayout vk_radiance_clipmap_sample_layout( void );

/* Explicit emissive GI fixture (material-flagged or console). Analytic light wins if both. */
void vk_radiance_clipmap_register_emissive( const vec3_t origin, const vec3_t color,
	float intensity, float radius, float importance );

/* Metrics for status / validation (no invented GPU timings). */
void vk_radiance_clipmap_metrics( int *levelsOut, int *cellsOut, int *updatedOut,
	int *invalidatedOut, int *reusedOut, int *injectedOut, size_t *bytesOut );

#endif /* VK_RADIANCE_CLIPMAP_H */
