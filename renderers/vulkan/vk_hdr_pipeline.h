#pragma once

#ifdef USE_VULKAN

#include "../common/tr_types.h"

typedef enum {
	VK_HDR_STAGE_SCENE = 0,
	VK_HDR_STAGE_EXPOSURE,
	VK_HDR_STAGE_BLOOM,
	VK_HDR_STAGE_TONEMAP,
	VK_HDR_STAGE_GAMMA,
	VK_HDR_STAGE_COUNT
} vkHdrStage_t;

void vk_hdr_pipeline_register( void );
void vk_hdr_pipeline_begin_frame( void );
void vk_hdr_pipeline_note_stage( vkHdrStage_t stage, const char *passName );

#endif /* USE_VULKAN */
