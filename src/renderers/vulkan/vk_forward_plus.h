#pragma once

#ifdef USE_VULKAN

void vk_forward_plus_init( void );
void vk_forward_plus_shutdown( void );
/* Packs visible dynamic lights from backEnd.refdef into a host-visible SSBO (scaffolding). */
void vk_forward_plus_update_for_refdef( void );

#endif /* USE_VULKAN */
