#ifndef VK_AMBIENT_VISIBILITY_H
#define VK_AMBIENT_VISIBILITY_H

#include "vk.h"

void vk_ambient_visibility_init( void );
void vk_ambient_visibility_shutdown( void );
void vk_ambient_visibility_frame_begin( void );
void vk_ambient_visibility_apply_after_geometry( void );
void vk_ambient_visibility_reset_history( void );
qboolean vk_ambient_visibility_blocks_legacy_post( void );
qboolean vk_ambient_visibility_active( void );
qboolean vk_ambient_visibility_available( void );
VkImageView vk_ambient_visibility_view( void );
float vk_ambient_visibility_strength( void );

#endif
