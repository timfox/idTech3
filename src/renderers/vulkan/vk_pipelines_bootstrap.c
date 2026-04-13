/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Initial pipeline allocation after device init (world base + volumetric).
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_pipelines_persistent.h"

void vk_create_pipelines( void )
{
	vk_alloc_persistent_pipelines();

	vk.pipelines_world_base = vk.pipelines_count;

#ifdef VK_PBR_BRDFLUT
	vk_create_brdflut_pipeline();
#endif
	vk_create_volumetric_pipelines();
}

#ifdef VK_PBR_BRDFLUT
void vk_create_brdflut_pipeline( void )
{
	if ( !vk.pbrActive )
		return;
	{
		uint32_t size = 512;
		vk_create_post_process_pipeline( 4, size, size );
	}
}
#endif
