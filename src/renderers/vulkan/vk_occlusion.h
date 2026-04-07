/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

GPU occlusion query path for entity bbox culling (split from vk.c).
===========================================================================
*/

#ifndef VK_OCCLUSION_H
#define VK_OCCLUSION_H

#include "tr_common.h"

/* Sized in vk_occlusion.c to MAX_REFENTITIES */
extern uint64_t vk_entity_occlusion_visibility[];

struct drawSurfsCommand_s;

void vk_occlusion_seed_visibility_all_visible( void );
void vk_occlusion_draw_entity_bboxes( const struct drawSurfsCommand_s *cmd );
void vk_occlusion_readback( void );
void vk_occlusion_pass( const struct drawSurfsCommand_s *cmd );
void vk_reset_occlusion_visibility( void );

#endif
