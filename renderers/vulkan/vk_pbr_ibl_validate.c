/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Startup logging / sanity check for PBR IBL resources (BRDF LUT, cubemap fallbacks).
Called from tr_init.c after pipeline creation.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"

void vk_validate_pbr_ibl_resources( void )
{
	int i;

	if ( !vk.pbrActive ) {
		return;
	}

	vk.pbr_ibl_using_hdr_fallback = qfalse;
	vk.pbr_ibl_has_ready_local_cubemap = qfalse;
	vk.pbr_ibl_ready_cubemap_count = 0;
	vk.pbr_ibl_incomplete_cubemap_count = 0;

	for ( i = 0; i < tr.numCubemaps; i++ ) {
		const cubemap_t *cube = &tr.cubemaps[i];
		const qboolean ready = ( cube->prefiltered_image &&
			cube->irradiance_image &&
			cube->prefiltered_image->handle != VK_NULL_HANDLE &&
			cube->prefiltered_image->view != VK_NULL_HANDLE &&
			cube->prefiltered_image->descriptor != VK_NULL_HANDLE &&
			cube->irradiance_image->handle != VK_NULL_HANDLE &&
			cube->irradiance_image->view != VK_NULL_HANDLE &&
			cube->irradiance_image->descriptor != VK_NULL_HANDLE );

		if ( ready ) {
			vk.pbr_ibl_ready_cubemap_count++;
			vk.pbr_ibl_has_ready_local_cubemap = qtrue;
		} else {
			vk.pbr_ibl_incomplete_cubemap_count++;
		}
	}

	{
		const qboolean brdfLutReady = ( vk.brdflut_image != VK_NULL_HANDLE &&
			vk.brdflut_image_view != VK_NULL_HANDLE &&
			vk.brdflut_image_descriptor != VK_NULL_HANDLE );
		const qboolean emptyCubemapReady = ( tr.emptyCubemap != NULL &&
			tr.emptyCubemap->descriptor != VK_NULL_HANDLE );

		ri.Printf( PRINT_ALL, "[VK] PBR IBL: BRDF LUT %s, empty cubemap fallback %s\n",
			brdfLutReady ? "ready" : "missing",
			emptyCubemapReady ? "ready" : "missing" );

#ifdef VK_CUBEMAP
		ri.Printf( PRINT_ALL, "[VK] PBR IBL: runtime cubemap path %s\n",
			vk.cubemapActive ? "enabled" : "disabled" );
		if ( vk.cubemapActive && tr.numCubemaps == 0 ) {
			ri.Printf( PRINT_ALL, "[VK] PBR IBL: no map cubemaps loaded at startup, using fallback until available\n" );
		} else if ( vk.cubemapActive ) {
			ri.Printf( PRINT_ALL, "[VK] PBR IBL: local cubemaps ready=%d incomplete=%d\n",
				vk.pbr_ibl_ready_cubemap_count, vk.pbr_ibl_incomplete_cubemap_count );
		}
#endif

		if ( !brdfLutReady ) {
			ri.Printf( PRINT_WARNING, "PBR IBL: BRDF LUT resources are incomplete, split-sum specular may fallback\n" );
		}
		if ( !emptyCubemapReady ) {
			ri.Printf( PRINT_WARNING, "PBR IBL: empty cubemap fallback is missing\n" );
		}
	}
}
