/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

MSAA sample count state and optional min sample shading rate for main passes.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_raster_samples.h"

static int vkSamples = VK_SAMPLE_COUNT_1_BIT;
static int vkMaxSamples = VK_SAMPLE_COUNT_1_BIT;

float vk_get_msaa_min_sample_shading( void )
{
	if ( !vk.msaaSampleShading ) {
		return 1.0f;
	}

	return Com_Clamp( 0.25f, 1.0f,
		r_msaa_sample_shading_rate ? r_msaa_sample_shading_rate->value : 0.5f );
}

VkSampleCountFlagBits vk_get_main_rasterization_max_samples( void )
{
	return (VkSampleCountFlagBits)vkMaxSamples;
}

VkSampleCountFlagBits vk_get_main_rasterization_samples( void )
{
	return (VkSampleCountFlagBits)vkSamples;
}

void vk_raster_samples_configure( const VkPhysicalDeviceProperties *props, qboolean msaaActive )
{
	vkMaxSamples = MIN( props->limits.sampledImageColorSampleCounts, props->limits.sampledImageDepthSampleCounts );

	if ( msaaActive ) {
		VkSampleCountFlags mask = vkMaxSamples;
		int req = r_ext_multisample->integer;
		if ( req < 2 ) req = 2;
		else if ( req == 3 || req == 5 || req == 6 || req == 7 ) req = ( req <= 4 ) ? 4 : 8;
		else if ( req > 16 ) req = 16;
		vkSamples = MAX( log2pad( req, 1 ), VK_SAMPLE_COUNT_2_BIT );
		while ( (VkSampleCountFlags)vkSamples > mask )
			vkSamples >>= 1;
		vk.msaaSampleShading = ( r_msaa_sample_shading && r_msaa_sample_shading->integer ) ? qtrue : qfalse;
		if ( vk.msaaSampleShading ) {
			ri.Printf( PRINT_ALL, "...using %ix MSAA (sample shading %.2f)\n",
				vkSamples, vk_get_msaa_min_sample_shading() );
		} else {
			ri.Printf( PRINT_ALL, "...using %ix MSAA\n", vkSamples );
		}
	} else {
		vkSamples = VK_SAMPLE_COUNT_1_BIT;
		vk.msaaSampleShading = qfalse;
	}
}
