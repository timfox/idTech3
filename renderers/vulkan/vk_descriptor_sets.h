/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Descriptor pool allocation and writes for main render targets, postfx,
and volumetric/froxel paths (split from vk.c).
===========================================================================
*/

#ifndef VK_DESCRIPTOR_SETS_H
#define VK_DESCRIPTOR_SETS_H

void vk_init_descriptors( void );
void vk_update_attachment_descriptors( void );
/* Swapchain / attachment rebuild paths call this after froxel views exist */
void vk_update_volumetric_descriptors( void );

#endif
