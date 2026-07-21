/*
===========================================================================
Temporal reactive mask: R8 coverage stamped by OIT / transparent / stochastic
draws and sampled by Temporal Reconstruction (max with heuristics).
===========================================================================
*/

#ifndef VK_REACTIVE_MASK_H
#define VK_REACTIVE_MASK_H

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

qboolean vk_reactive_mask_wanted( void );
qboolean vk_reactive_mask_active( void );
qboolean vk_reactive_mask_stamp_enabled( void );

void vk_reactive_mask_clear( void );
void vk_barrier_reactive_mask_for_sampling( const char *reason );
void vk_barrier_reactive_mask_for_storage( const char *reason );
void vk_reactive_mask_stamp_from_reveal( void );

/* After deferred weapon flush: MAX-blend high reactivity on DEPTH_RANGE_WEAPON. */
void vk_reactive_mask_stamp_weapon_from_depth( void );

void vk_reactive_mask_update_taa_descriptors( void );
void vk_reactive_mask_update_storage_descriptor( void );

void vk_create_reactive_mask_pipeline( void );
void vk_destroy_reactive_mask_pipeline( void );

#ifdef __cplusplus
}
#endif

#endif
