/*
===========================================================================
Temporal classification buffer: WORLD / WEAPON / SKY for TAA history rejection.
===========================================================================
*/

#ifndef VK_TEMPORAL_CLASS_H
#define VK_TEMPORAL_CLASS_H

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Encoded into R8_UNORM as class/255. */
#define TEMPORAL_CLASS_WORLD  0
#define TEMPORAL_CLASS_WEAPON 1
#define TEMPORAL_CLASS_SKY    2

qboolean vk_temporal_class_wanted( void );
qboolean vk_temporal_class_active( void );

/* Clear current class to WORLD once per frame. */
void vk_temporal_class_clear( void );

/* After weapon depth is in the depth buffer: stamp WEAPON where depth is in
 * DEPTH_RANGE_WEAPON (reverse-Z near). Then promote current → previous. */
void vk_temporal_class_stamp_weapon_from_depth( void );

/* No weapon this frame: promote a cleared WORLD class so prev weapon silhouette ages out. */
void vk_temporal_class_commit_world_only( void );

void vk_barrier_temporal_class_for_sampling( const char *reason );
void vk_temporal_class_update_taa_descriptors( void );

void vk_create_temporal_class_pipeline( void );
void vk_destroy_temporal_class_pipeline( void );

/* Previous-class view for TAA (NULL if inactive). */
VkImageView vk_temporal_class_prev_view( void );

#ifdef __cplusplus
}
#endif

#endif
