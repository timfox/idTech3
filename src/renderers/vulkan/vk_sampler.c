/*
============================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan sampler creation and management. Extracted from vk.c for modularization.
============================================================================
*/

#include "tr_local.h"
#include "vk.h"

VkSampler vk_find_sampler( const Vk_Sampler_Def *def )
{
	VkSamplerAddressMode address_mode;
	VkSamplerCreateInfo desc;
	VkSampler sampler;
	VkFilter mag_filter;
	VkFilter min_filter;
	VkSamplerMipmapMode mipmap_mode;
	float maxLod;
	int i;

	for ( i = 0; i < vk.samplers.count; i++ ) {
		const Vk_Sampler_Def *cur_def = &vk.samplers.def[i];
		if ( memcmp( cur_def, def, sizeof( *def ) ) == 0 ) {
			return vk.samplers.handle[i];
		}
	}

	if ( vk.samplers.count >= MAX_VK_SAMPLERS ) {
		ri.Error( ERR_DROP, "vk_find_sampler: MAX_VK_SAMPLERS hit\n" );
	}

	address_mode = def->address_mode;

	if ( def->gl_mag_filter == GL_NEAREST ) {
		mag_filter = VK_FILTER_NEAREST;
	} else if ( def->gl_mag_filter == GL_LINEAR ) {
		mag_filter = VK_FILTER_LINEAR;
	} else {
		ri.Error( ERR_FATAL, "vk_find_sampler: invalid gl_mag_filter" );
		return VK_NULL_HANDLE;
	}

	maxLod = vk.maxLod;

	if ( def->gl_min_filter == GL_NEAREST ) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f;
	} else if ( def->gl_min_filter == GL_LINEAR ) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		maxLod = 0.25f;
	} else if ( def->gl_min_filter == GL_NEAREST_MIPMAP_NEAREST ) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if ( def->gl_min_filter == GL_LINEAR_MIPMAP_NEAREST ) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	} else if ( def->gl_min_filter == GL_NEAREST_MIPMAP_LINEAR ) {
		min_filter = VK_FILTER_NEAREST;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else if ( def->gl_min_filter == GL_LINEAR_MIPMAP_LINEAR ) {
		min_filter = VK_FILTER_LINEAR;
		mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	} else {
		ri.Error( ERR_FATAL, "vk_find_sampler: invalid gl_min_filter" );
		return VK_NULL_HANDLE;
	}

	if ( def->max_lod_1_0 ) {
		maxLod = 1.0f;
	}

	desc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.magFilter = mag_filter;
	desc.minFilter = min_filter;
	desc.mipmapMode = mipmap_mode;
	desc.addressModeU = address_mode;
	desc.addressModeV = address_mode;
	desc.addressModeW = address_mode;
	desc.mipLodBias = r_mipLodBias->value;

	if ( def->noAnisotropy || mipmap_mode == VK_SAMPLER_MIPMAP_MODE_NEAREST || mag_filter == VK_FILTER_NEAREST ) {
		desc.anisotropyEnable = VK_FALSE;
		desc.maxAnisotropy = 1.0f;
	} else {
		desc.anisotropyEnable = ( r_ext_texture_filter_anisotropic->integer && vk.samplerAnisotropy ) ? VK_TRUE : VK_FALSE;
		if ( desc.anisotropyEnable ) {
			desc.maxAnisotropy = MIN( r_ext_max_anisotropy->integer, vk.maxAnisotropy );
		}
	}

	desc.compareEnable = VK_FALSE;
	desc.compareOp = VK_COMPARE_OP_ALWAYS;
	desc.minLod = 0.0f;
	desc.maxLod = ( maxLod == vk.maxLod ) ? VK_LOD_CLAMP_NONE : maxLod;
	desc.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	desc.unnormalizedCoordinates = VK_FALSE;

	VK_CHECK( qvkCreateSampler( vk.device, &desc, NULL, &sampler ) );

	vk_set_object_name( (uint64_t)sampler, va( "image sampler %i", vk.samplers.count ), VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT );

	vk.samplers.def[vk.samplers.count] = *def;
	vk.samplers.handle[vk.samplers.count] = sampler;
	vk.samplers.count++;

	return sampler;
}


void vk_destroy_samplers( void )
{
	int i;

	if ( vk.device == VK_NULL_HANDLE || qvkDestroySampler == NULL )
		return;

	for ( i = 0; i < vk.samplers.count; i++ ) {
		qvkDestroySampler( vk.device, vk.samplers.handle[i], NULL );
		Com_Memset( &vk.samplers.def[i], 0, sizeof( vk.samplers.def[i] ) );
		vk.samplers.handle[i] = VK_NULL_HANDLE;
	}

	vk.samplers.count = 0;
}
