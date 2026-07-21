/*
===========================================================================
Dynamic-object identity buffer (R32_UINT ping-pong).

Each opaque dynamic entity fragment stamps a packed value
  (uint(reversedZ * 65535) << 16) | (stableId & 0xFFFF)
via imageAtomicMax so the frontmost object owns the pixel. World/background = 0.

The TAA resolve samples the current-frame id at the pixel and the previous-frame
id at the reprojected history UV. History is rejected whenever the two identities
differ (object moved, disoccluded background, entity-slot reuse, overlapping
object), removing trailing "echo" silhouettes with no blur/sharpen workaround.
===========================================================================
*/

#ifndef VK_OBJECT_ID_H
#define VK_OBJECT_ID_H

#include "../common/tr_types.h"

#ifdef __cplusplus
extern "C" {
#endif

qboolean vk_object_id_wanted( void );
qboolean vk_object_id_active( void );

/* Per-frame lifecycle (mirrors reactive-mask / temporal-class ping-pong). */
void vk_object_id_begin_frame( void );               /* pick + clear current slot -> GENERAL */
void vk_object_id_update_storage_descriptor( void ); /* set 18 binding 8 -> current slot view */
void vk_barrier_object_id_for_sampling( const char *reason );
void vk_object_id_update_taa_descriptors( void );    /* fixed per-slot sampler descriptors + stub */
void vk_object_id_commit( void );                    /* mark prev valid, flip ping-pong slot */
void vk_object_id_reset( void );

VkDescriptorSet vk_object_id_curr_descriptor( void ); /* current-frame stamp, sampled at pixel */
VkDescriptorSet vk_object_id_prev_descriptor( void ); /* previous frame, sampled at history UV */

#ifdef __cplusplus
}
#endif

#endif /* VK_OBJECT_ID_H */
