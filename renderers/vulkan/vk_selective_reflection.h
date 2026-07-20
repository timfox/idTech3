/*
===========================================================================
Selective Hybrid Reflections 1.0 — exclusive specular reflection ownership.

Waterfall under one subsystem: RT → SSR → probe/sky.
Sources never add at full strength. Path tracing disables this composite.
===========================================================================
*/
#ifndef VK_SELECTIVE_REFLECTION_H
#define VK_SELECTIVE_REFLECTION_H

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	VK_SHR_OWNER_OFF = 0,
	VK_SHR_OWNER_PROBE = 1,
	VK_SHR_OWNER_SSR = 2,
	VK_SHR_OWNER_RT = 3,
	VK_SHR_OWNER_PATH_TRACER = 4,
	VK_SHR_OWNER_FALLBACK = 5
} vkShrReflectionOwner_t;

/* Fail-inject bitmask (r_shrFailInject). */
enum {
	VK_SHR_FAIL_TLAS = 1,
	VK_SHR_FAIL_RT_PIPELINE = 2,
	VK_SHR_FAIL_DESCRIPTOR = 4,
	VK_SHR_FAIL_HISTORY = 8,
	VK_SHR_FAIL_SSR = 16
};

void vk_shr_init( void );
void vk_shr_shutdown( void );
void vk_shr_frame_begin( void );

qboolean vk_shr_active( void );
vkShrReflectionOwner_t vk_shr_owner( void );
qboolean vk_shr_rt_owns( void );
qboolean vk_shr_ssr_allowed( void ); /* SSR may run as fallback / secondary */
qboolean vk_shr_suppress_gen_frag_ibl_spec( void ); /* RT owns: no gen_frag IBL specular add */
qboolean vk_shr_pathtrace_blocks( void );

const char *vk_shr_owner_name( void );
const char *vk_shr_fallback_reason( void );
uint32_t vk_shr_fail_inject( void );
qboolean vk_shr_fail_inject_active( int bit );

void vk_shr_note_rt_pipeline_ok( qboolean ok );
void vk_shr_note_descriptor_ok( qboolean ok );
void vk_shr_note_history_ok( qboolean ok );
void vk_shr_note_tlas_ok( qboolean ok );
void vk_shr_note_ssr_ok( qboolean ok );
void vk_shr_invalidate_history( const char *reason );

int vk_shr_debug_mode( void );
int vk_shr_composite_debug_mode( void ); /* maps r_shrDebug → hybrid1_composite codes */
float vk_shr_roughness_rt_max( void ); /* skip RT above this roughness → probe */
float vk_shr_temporal_alpha_floor( void );
uint32_t vk_shr_max_history_age( void );

/* Specular occlusion for probe/IBL fallback only (0..1). Valid RT must not use this. */
float vk_shr_probe_specular_occlusion_strength( void );

#ifdef __cplusplus
}
#endif

#endif /* VK_SELECTIVE_REFLECTION_H */
