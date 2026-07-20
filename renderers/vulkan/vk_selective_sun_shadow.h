/*
===========================================================================
Selective Hybrid Shadows 1.0 — sun shadow signal ownership router.

Sun visibility has exactly one owner: raster cascade or Hybrid1/RQ RT.
Local lights remain raster. Path tracing disables this composition.
===========================================================================
*/
#ifndef VK_SELECTIVE_SUN_SHADOW_H
#define VK_SELECTIVE_SUN_SHADOW_H

#include "../common/tr_types.h"
#include "../common/vulkan/vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	VK_SHS_OWNER_RASTER = 0,
	VK_SHS_OWNER_RT = 1
} vkShsSunOwner_t;

/* Fail-inject bitmask (r_shsFailInject). */
enum {
	VK_SHS_FAIL_TLAS = 1,
	VK_SHS_FAIL_RT_PIPELINE = 2,
	VK_SHS_FAIL_DESCRIPTOR = 4,
	VK_SHS_FAIL_HISTORY = 8
};

void vk_shs_init( void );
void vk_shs_shutdown( void );
void vk_shs_frame_begin( void );

/* Resolved this frame after vk_shs_frame_begin(). */
vkShsSunOwner_t vk_shs_sun_owner( void );
qboolean vk_shs_rt_owns_sun( void );
qboolean vk_shs_raster_owns_sun( void );
qboolean vk_shs_active( void ); /* feature requested (cvar / selective hybrid) */
qboolean vk_shs_sun_only_rt( void ); /* RT sun only; no RT local-light shadows */
qboolean vk_shs_prefer_ray_query( void );
qboolean vk_shs_pathtrace_blocks( void );

const char *vk_shs_owner_name( void );
const char *vk_shs_fallback_reason( void );
uint32_t vk_shs_fail_inject( void );
qboolean vk_shs_fail_inject_active( int bit );

/* Hybrid1 hooks: report pipeline/descriptor/history health for ownership. */
void vk_shs_note_rt_pipeline_ok( qboolean ok );
void vk_shs_note_descriptor_ok( qboolean ok );
void vk_shs_note_history_ok( qboolean ok );
void vk_shs_note_tlas_ok( qboolean ok );
void vk_shs_invalidate_history( const char *reason );

int vk_shs_debug_mode( void );
/* Maps r_shsDebug / r_hybrid1_debug into hybrid1_composite debug codes. */
int vk_shs_composite_debug_mode( void );
float vk_shs_temporal_alpha_floor( void );
uint32_t vk_shs_max_history_age( void );

/* Prefer ray-query raw sun visibility into Hybrid1 raw_shadow when ready. */
qboolean vk_shs_record_raw_ray_query( VkCommandBuffer cmd, VkImageView rawView, uint32_t w, uint32_t h );

#ifdef __cplusplus
}
#endif

#endif /* VK_SELECTIVE_SUN_SHADOW_H */
