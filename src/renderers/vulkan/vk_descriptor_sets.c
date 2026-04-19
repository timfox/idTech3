/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Descriptor set allocation and image/buffer binding updates (split from vk.c).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_descriptor_sets.h"
#include "vk_forward_plus.h"
#include "vk_postfx.h"
#include "vk_postfx_params.h"
#include "vk_post_fog.h"
#include "vk_descriptors.h"
#include "vk_attachments.h"
#include "vk_volumetric_params.h"

void vk_update_attachment_descriptors( void ) {
	uint32_t i;

	if ( vk.color_image_view )
	{
		VkDescriptorImageInfo info;
		VkWriteDescriptorSet desc;
		Vk_Sampler_Def sd;

		Com_Memset( &sd, 0, sizeof( sd ) );
		// Post-process source should stay linear-filtered; Panini magnifies edges and nearest exacerbates aliasing.
		sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.max_lod_1_0 = qtrue;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );
		info.imageView = vk.color_image_view;
		info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		desc.dstBinding = 0;
		desc.dstArrayElement = 0;
		desc.descriptorCount = 1;
		desc.pNext = NULL;
		desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		desc.pImageInfo = &info;
		desc.pBufferInfo = NULL;
		desc.pTexelBufferView = NULL;
		for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
			desc.dstSet = vk.color_descriptor[i];
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			if ( vk.post_color_descriptor[i] != VK_NULL_HANDLE ) {
				VkDescriptorImageInfo post_info;
				VkWriteDescriptorSet post_desc;
				Vk_Sampler_Def post_sd;

				Com_Memset( &post_sd, 0, sizeof( post_sd ) );
				post_sd.gl_mag_filter = post_sd.gl_min_filter = GL_LINEAR;
				post_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
				post_sd.max_lod_1_0 = qtrue;
				post_sd.noAnisotropy = qtrue;

				post_info.sampler = vk_find_sampler( &post_sd );
				post_info.imageView = vk.color_image_view;
				post_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				post_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				post_desc.dstSet = vk.post_color_descriptor[i];
				post_desc.dstBinding = 0;
				post_desc.dstArrayElement = 0;
				post_desc.descriptorCount = 1;
				post_desc.pNext = NULL;
				post_desc.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				post_desc.pImageInfo = &post_info;
				post_desc.pBufferInfo = NULL;
				post_desc.pTexelBufferView = NULL;

				qvkUpdateDescriptorSets( vk.device, 1, &post_desc, 0, NULL );
			}
		}
		/* Ensure post-fog and luminance descriptors are initialized for gamma/eye-adaptation. */
		vk_update_post_fog_descriptors( vk.color_image_view );

		// screenmap
		sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
		/* screenMap UVs routinely go out of range on reflective surfaces.
		 * Clamp-to-edge smears the screen border across water/refraction planes,
		 * which shows up as the wide bright/dark bands in-game. */
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sd.max_lod_1_0 = qfalse;
		sd.noAnisotropy = qtrue;

		info.sampler = vk_find_sampler( &sd );

		info.imageView = vk.screenMap.color_image_view;
		desc.dstSet = vk.screenMap.color_descriptor;

		qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

		if ( r_ssao && r_ssao->integer )
		{
			// depth sampling for SSAO (use depth-only view when available for VUID-01976)
			sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;
			info.sampler = vk_find_sampler( &sd );
			info.imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
			info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
				desc.dstSet = vk.depth_descriptor[i];
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}

			// ssao output
			sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;
			info.sampler = vk_find_sampler( &sd );
			info.imageView = vk.ssao_image_view;
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			desc.dstSet = vk.ssao_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

			// ssao blur output
			info.imageView = vk.ssao_blur_image_view;
			desc.dstSet = vk.ssao_blur_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			if ( vk.fog_scene_image_view ) {
				info.imageView = vk.fog_scene_image_view;
				desc.dstSet = vk.ssao_scene_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}
		if ( r_oit && r_oit->integer ) {
			if ( vk.fog_scene_image_view ) {
				info.imageView = vk.fog_scene_image_view;
				desc.dstSet = vk.oit_opaque_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
			if ( vk.oit_accum_image_view ) {
				info.imageView = vk.oit_accum_image_view;
				desc.dstSet = vk.oit_accum_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
			if ( vk.oit_reveal_image_view ) {
				info.imageView = vk.oit_reveal_image_view;
				desc.dstSet = vk.oit_reveal_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
			if ( vk.oit_depth_descriptor ) {
				VkImageView depth_view = VK_NULL_HANDLE;
				VkImageLayout depth_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
				sd.max_lod_1_0 = qtrue;
				sd.noAnisotropy = qtrue;
				info.sampler = vk_find_sampler( &sd );
				if ( vk.msaaActive && vk.volumetric_depth_view != VK_NULL_HANDLE ) {
					depth_view = vk.volumetric_depth_view;
					depth_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				} else {
					depth_view = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
					depth_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				}
				if ( depth_view != VK_NULL_HANDLE ) {
					info.imageView = depth_view;
					info.imageLayout = depth_layout;
					desc.dstSet = vk.oit_depth_descriptor;
					qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
				}
			}
		}

		if ( PostFX_SSR_IsEnabled() && vk.ssr_image_view )
		{
			// ssr set 0: color texture
			sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;
			info.sampler = vk_find_sampler( &sd );
			info.imageView = vk.color_image_view;
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			desc.dstSet = vk.ssr_descriptor[0];
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

			// ssr set 1: depth texture (use depth-only view when available for VUID-01976)
			sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
			info.sampler = vk_find_sampler( &sd );
			info.imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
			info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			desc.dstSet = vk.ssr_descriptor[1];
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
		}

		// bloom images
		if ( r_bloom->integer )
		{
			uint32_t j;

			sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;
			info.sampler = vk_find_sampler( &sd );
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			for ( j = 0; j < ARRAY_LEN( vk.bloom_image_descriptor ); j++ )
			{
				info.imageView = vk.bloom_image_view[j];
				desc.dstSet = vk.bloom_image_descriptor[j];

				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}

		if ( vk.smaaActive )
		{
			sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
			sd.max_lod_1_0 = qtrue;
			sd.noAnisotropy = qtrue;
			info.sampler = vk_find_sampler( &sd );
			info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			if ( vk.smaa_edge_image_view )
			{
				info.imageView = vk.color_image_view;
				desc.dstSet = vk.smaa_edge_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
			if ( vk.smaa_edge_image_view )
			{
				info.imageView = vk.smaa_edge_image_view;
				desc.dstSet = vk.smaa_blend_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
			if ( vk.smaa_blend_image_view )
			{
				info.imageView = vk.smaa_blend_image_view;
				desc.dstSet = vk.smaa_compose_descriptor;
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}
		for ( i = 0; i < 2; i++ ) {
			if ( vk.taa_history_image_view[i] ) {
				info.imageView = vk.taa_history_image_view[i];
				desc.dstSet = vk.taa_history_descriptor[i];
				qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );
			}
		}

#ifdef VK_PBR_BRDFLUT
		if( vk.pbrActive )
		{
			// brdf
			info.imageView = vk.brdflut_image_view;
			desc.dstSet = vk.brdflut_image_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );	
		
			// cubemap
			info.imageView = vk.cubeMap.color_image_view[0];
			desc.dstSet = vk.cubeMap.color_descriptor;
			qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );	
		}
#endif
	}
}

void vk_update_volumetric_descriptors( void )
{
	VkDescriptorBufferInfo params_buffer;
	VkImageView volumetric_depth_view;
	VkImageLayout volumetric_depth_layout;

	if ( vk.volumetric_params_buffer == VK_NULL_HANDLE ) {
		return;
	}

	params_buffer.buffer = vk.volumetric_params_buffer;
	params_buffer.offset = 0;
	params_buffer.range = sizeof( volumetric_params_t );

	volumetric_depth_view = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	volumetric_depth_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	if ( vk.msaaActive && vk.volumetric_depth_view != VK_NULL_HANDLE ) {
		volumetric_depth_view = vk.volumetric_depth_view;
		volumetric_depth_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	if ( vk.volumetric_compute_descriptor != VK_NULL_HANDLE &&
		vk.froxel_volume_view && vk.froxel_history_view && vk.froxel_light_view &&
		vk.froxel_extinction_view && vk.froxel_clamp_view &&
		volumetric_depth_view && vk.fog_noise_view && vk.sun_shadow_sample_view &&
		vk.local_spot_shadow_atlas_sample_view && vk.local_point_shadow_array_sample_view && vk.motion_vector_view &&
		vk.fluid_velocity_views[0] && vk.fluid_velocity_views[1] &&
		vk.fluid_density_views[0] && vk.fluid_density_views[1] &&
		vk.volumetric_telemetry_view )
	{
		VkDescriptorImageInfo storage_info[5];
		VkDescriptorImageInfo telemetry_info;
		VkDescriptorImageInfo depth_info;
		VkDescriptorImageInfo noise_info;
		VkDescriptorImageInfo shadow_info;
		VkDescriptorImageInfo local_spot_shadow_info;
		VkDescriptorImageInfo local_point_shadow_info;
		VkDescriptorImageInfo motion_info;
		VkDescriptorImageInfo fluid_info[4];
		VkWriteDescriptorSet writes[17];
		Vk_Sampler_Def depth_sd;
		Vk_Sampler_Def noise_sd;
		Vk_Sampler_Def shadow_sd;
		Vk_Sampler_Def motion_sd;
		Vk_Sampler_Def fluid_sd;

		Com_Memset( storage_info, 0, sizeof( storage_info ) );
		storage_info[0].imageView = vk.froxel_volume_view;
		storage_info[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		storage_info[1].imageView = vk.froxel_history_view;
		storage_info[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		storage_info[2].imageView = vk.froxel_light_view;
		storage_info[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		storage_info[3].imageView = vk.froxel_extinction_view;
		storage_info[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		storage_info[4].imageView = vk.froxel_clamp_view;
		storage_info[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( &depth_sd, 0, sizeof( depth_sd ) );
		depth_sd.gl_mag_filter = depth_sd.gl_min_filter = GL_NEAREST;
		depth_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		depth_sd.noAnisotropy = qtrue;
		vk.froxel_depth_sampler = vk_find_sampler( &depth_sd );

		Com_Memset( &depth_info, 0, sizeof( depth_info ) );
		depth_info.sampler = vk.froxel_depth_sampler;
		depth_info.imageView = volumetric_depth_view;
		depth_info.imageLayout = volumetric_depth_layout;
		Com_Memset( &noise_sd, 0, sizeof( noise_sd ) );
		noise_sd.gl_mag_filter = noise_sd.gl_min_filter = GL_LINEAR;
		noise_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		noise_sd.noAnisotropy = qtrue;
		vk.fog_noise_sampler = vk_find_sampler( &noise_sd );

		Com_Memset( &noise_info, 0, sizeof( noise_info ) );
		noise_info.sampler = vk.fog_noise_sampler;
		noise_info.imageView = vk.fog_noise_view;
		noise_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		Com_Memset( &shadow_sd, 0, sizeof( shadow_sd ) );
		shadow_sd.gl_mag_filter = shadow_sd.gl_min_filter = GL_NEAREST;
		shadow_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		shadow_sd.noAnisotropy = qtrue;
		vk.sun_shadow_sampler = vk_find_sampler( &shadow_sd );

		Com_Memset( &shadow_info, 0, sizeof( shadow_info ) );
		shadow_info.sampler = vk.sun_shadow_sampler;
		shadow_info.imageView = vk.sun_shadow_sample_view;
		shadow_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		Com_Memset( &local_spot_shadow_info, 0, sizeof( local_spot_shadow_info ) );
		local_spot_shadow_info.sampler = vk.sun_shadow_sampler;
		local_spot_shadow_info.imageView = vk.local_spot_shadow_atlas_sample_view;
		local_spot_shadow_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		Com_Memset( &local_point_shadow_info, 0, sizeof( local_point_shadow_info ) );
		local_point_shadow_info.sampler = vk.sun_shadow_sampler;
		local_point_shadow_info.imageView = vk.local_point_shadow_array_sample_view;
		local_point_shadow_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		Com_Memset( &motion_sd, 0, sizeof( motion_sd ) );
		motion_sd.gl_mag_filter = motion_sd.gl_min_filter = GL_LINEAR;
		motion_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		motion_sd.noAnisotropy = qtrue;

		Com_Memset( &motion_info, 0, sizeof( motion_info ) );
		motion_info.sampler = vk_find_sampler( &motion_sd );
		motion_info.imageView = vk.motion_vector_view;
		motion_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		Com_Memset( &telemetry_info, 0, sizeof( telemetry_info ) );
		telemetry_info.imageView = vk.volumetric_telemetry_view;
		telemetry_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( &fluid_sd, 0, sizeof( fluid_sd ) );
		fluid_sd.gl_mag_filter = fluid_sd.gl_min_filter = GL_LINEAR;
		fluid_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		fluid_sd.noAnisotropy = qtrue;
		const VkSampler fluid_sampler = vk_find_sampler( &fluid_sd );

		Com_Memset( fluid_info, 0, sizeof( fluid_info ) );
		fluid_info[0].sampler = fluid_sampler;
		fluid_info[0].imageView = vk.fluid_velocity_views[0];
		fluid_info[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		fluid_info[1].sampler = fluid_sampler;
		fluid_info[1].imageView = vk.fluid_velocity_views[1];
		fluid_info[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		fluid_info[2].sampler = fluid_sampler;
		fluid_info[2].imageView = vk.fluid_density_views[0];
		fluid_info[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		fluid_info[3].sampler = fluid_sampler;
		fluid_info[3].imageView = vk.fluid_density_views[1];
		fluid_info[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		for ( int i = 0; i < 2; i++ ) {
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = vk.volumetric_compute_descriptor;
			writes[i].dstBinding = i;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writes[i].pImageInfo = &storage_info[i];
		}

		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = vk.volumetric_compute_descriptor;
		writes[2].dstBinding = 2;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[2].pImageInfo = &depth_info;

		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = vk.volumetric_compute_descriptor;
		writes[3].dstBinding = 3;
		writes[3].descriptorCount = 1;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[3].pBufferInfo = &params_buffer;

		writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[4].dstSet = vk.volumetric_compute_descriptor;
		writes[4].dstBinding = 4;
		writes[4].descriptorCount = 1;
		writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[4].pImageInfo = &noise_info;

		writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[5].dstSet = vk.volumetric_compute_descriptor;
		writes[5].dstBinding = 5;
		writes[5].descriptorCount = 1;
		writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[5].pImageInfo = &shadow_info;

		writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[6].dstSet = vk.volumetric_compute_descriptor;
		writes[6].dstBinding = 6;
		writes[6].descriptorCount = 1;
		writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[6].pImageInfo = &storage_info[2];

		writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[7].dstSet = vk.volumetric_compute_descriptor;
		writes[7].dstBinding = 7;
		writes[7].descriptorCount = 1;
		writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[7].pImageInfo = &storage_info[3];

		writes[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[8].dstSet = vk.volumetric_compute_descriptor;
		writes[8].dstBinding = 8;
		writes[8].descriptorCount = 1;
		writes[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[8].pImageInfo = &storage_info[4];

		writes[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[9].dstSet = vk.volumetric_compute_descriptor;
		writes[9].dstBinding = 9;
		writes[9].descriptorCount = 1;
		writes[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[9].pImageInfo = &motion_info;

		writes[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[10].dstSet = vk.volumetric_compute_descriptor;
		writes[10].dstBinding = 10;
		writes[10].descriptorCount = 1;
		writes[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[10].pImageInfo = &local_spot_shadow_info;

		writes[11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[11].dstSet = vk.volumetric_compute_descriptor;
		writes[11].dstBinding = 11;
		writes[11].descriptorCount = 1;
		writes[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[11].pImageInfo = &local_point_shadow_info;

		for ( int i = 0; i < 4; i++ ) {
			writes[12 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[12 + i].dstSet = vk.volumetric_compute_descriptor;
			writes[12 + i].dstBinding = 12 + i;
			writes[12 + i].descriptorCount = 1;
			writes[12 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[12 + i].pImageInfo = &fluid_info[i];
		}

		writes[16].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[16].dstSet = vk.volumetric_compute_descriptor;
		writes[16].dstBinding = 16;
		writes[16].descriptorCount = 1;
		writes[16].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[16].pImageInfo = &telemetry_info;

		qvkUpdateDescriptorSets( vk.device, ARRAY_LEN( writes ), writes, 0, NULL );
	}

	if ( vk.volumetric_composite_descriptor != VK_NULL_HANDLE &&
		vk.fog_scene_image_view && volumetric_depth_view && vk.froxel_volume_view && vk.froxel_extinction_view &&
		vk.motion_vector_view && vk.local_spot_shadow_atlas_sample_view && vk.local_point_shadow_array_sample_view &&
		vk.volumetric_telemetry_view )
	{
		VkDescriptorImageInfo composite_info[8];
		VkWriteDescriptorSet writes[9];
		Vk_Sampler_Def color_sd;
		Vk_Sampler_Def volume_sd;
		Vk_Sampler_Def motion_sd;
		Vk_Sampler_Def shadow_sd;
		Vk_Sampler_Def telemetry_sd;

		Com_Memset( &color_sd, 0, sizeof( color_sd ) );
		color_sd.gl_mag_filter = color_sd.gl_min_filter = GL_LINEAR;
		color_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		color_sd.noAnisotropy = qtrue;

		Com_Memset( &volume_sd, 0, sizeof( volume_sd ) );
		volume_sd.gl_mag_filter = volume_sd.gl_min_filter = GL_LINEAR;
		volume_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		volume_sd.noAnisotropy = qtrue;
		vk.froxel_sampler = vk_find_sampler( &volume_sd );

		Com_Memset( &motion_sd, 0, sizeof( motion_sd ) );
		motion_sd.gl_mag_filter = motion_sd.gl_min_filter = GL_LINEAR;
		motion_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		motion_sd.noAnisotropy = qtrue;

		Com_Memset( &shadow_sd, 0, sizeof( shadow_sd ) );
		shadow_sd.gl_mag_filter = shadow_sd.gl_min_filter = GL_NEAREST;
		shadow_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		shadow_sd.noAnisotropy = qtrue;

		Com_Memset( &telemetry_sd, 0, sizeof( telemetry_sd ) );
		telemetry_sd.gl_mag_filter = telemetry_sd.gl_min_filter = GL_NEAREST;
		telemetry_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		telemetry_sd.noAnisotropy = qtrue;

		Com_Memset( composite_info, 0, sizeof( composite_info ) );

		// sceneColor (binding 0)
		composite_info[0].sampler = vk_find_sampler( &color_sd );
		composite_info[0].imageView = vk.fog_scene_image_view;
		composite_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// depthTexture (binding 1)
		composite_info[1].sampler = vk.froxel_depth_sampler;
		composite_info[1].imageView = volumetric_depth_view;
		composite_info[1].imageLayout = volumetric_depth_layout;

		// froxelScattering (binding 2)
		composite_info[2].sampler = vk.froxel_sampler;
		composite_info[2].imageView = vk.froxel_volume_view;
		composite_info[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// froxelExtinction (binding 3)
		composite_info[3].sampler = vk.froxel_sampler;
		composite_info[3].imageView = vk.froxel_extinction_view;
		composite_info[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// motionTexture (binding 5)
		composite_info[4].sampler = vk_find_sampler( &motion_sd );
		composite_info[4].imageView = vk.motion_vector_view;
		composite_info[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		// localSpotShadowMap (binding 6)
		composite_info[5].sampler = vk_find_sampler( &shadow_sd );
		composite_info[5].imageView = vk.local_spot_shadow_atlas_sample_view;
		composite_info[5].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		// localPointShadowMap (binding 7)
		composite_info[6].sampler = vk_find_sampler( &shadow_sd );
		composite_info[6].imageView = vk.local_point_shadow_array_sample_view;
		composite_info[6].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		// telemetryTexture (binding 8)
		composite_info[7].sampler = vk_find_sampler( &telemetry_sd );
		composite_info[7].imageView = vk.volumetric_telemetry_view;
		composite_info[7].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		for ( int i = 0; i < 4; i++ ) {
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = vk.volumetric_composite_descriptor;
			writes[i].dstBinding = i;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[i].pImageInfo = &composite_info[i];
		}

		writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[4].dstSet = vk.volumetric_composite_descriptor;
		writes[4].dstBinding = 4;
		writes[4].descriptorCount = 1;
		writes[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[4].pBufferInfo = &params_buffer;

		for ( int i = 0; i < 3; i++ ) {
			writes[5 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[5 + i].dstSet = vk.volumetric_composite_descriptor;
			writes[5 + i].dstBinding = 5 + i;
			writes[5 + i].descriptorCount = 1;
			writes[5 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[5 + i].pImageInfo = &composite_info[4 + i];
		}

		writes[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[8].dstSet = vk.volumetric_composite_descriptor;
		writes[8].dstBinding = 8;
		writes[8].descriptorCount = 1;
		writes[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[8].pImageInfo = &composite_info[7];

		qvkUpdateDescriptorSets( vk.device, ARRAY_LEN( writes ), writes, 0, NULL );
	}

	if ( vk.volumetric_depth_resolve_descriptor != VK_NULL_HANDLE &&
		vk.msaaActive && vk.depth_image_view && vk.volumetric_depth_view )
	{
		VkDescriptorImageInfo resolve_info[2];
		VkWriteDescriptorSet resolve_writes[2];
		Vk_Sampler_Def depth_sd;

		if ( vk.froxel_depth_sampler == VK_NULL_HANDLE ) {
			Com_Memset( &depth_sd, 0, sizeof( depth_sd ) );
			depth_sd.gl_mag_filter = depth_sd.gl_min_filter = GL_NEAREST;
			depth_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			depth_sd.noAnisotropy = qtrue;
			vk.froxel_depth_sampler = vk_find_sampler( &depth_sd );
		}

		Com_Memset( resolve_info, 0, sizeof( resolve_info ) );
		resolve_info[0].sampler = vk.froxel_depth_sampler;
		resolve_info[0].imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
		resolve_info[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		resolve_info[1].imageView = vk.volumetric_depth_view;
		resolve_info[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( resolve_writes, 0, sizeof( resolve_writes ) );
		resolve_writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		resolve_writes[0].dstSet = vk.volumetric_depth_resolve_descriptor;
		resolve_writes[0].dstBinding = 0;
		resolve_writes[0].descriptorCount = 1;
		resolve_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		resolve_writes[0].pImageInfo = &resolve_info[0];

		resolve_writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		resolve_writes[1].dstSet = vk.volumetric_depth_resolve_descriptor;
		resolve_writes[1].dstBinding = 1;
		resolve_writes[1].descriptorCount = 1;
		resolve_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		resolve_writes[1].pImageInfo = &resolve_info[1];

		qvkUpdateDescriptorSets( vk.device, ARRAY_LEN( resolve_writes ), resolve_writes, 0, NULL );
	}

	if ( vk.volumetric_fluid_descriptor != VK_NULL_HANDLE &&
		vk.fluid_velocity_views[0] && vk.fluid_velocity_views[1] &&
		vk.fluid_density_views[0] && vk.fluid_density_views[1] &&
		vk.fluid_pressure_views[0] && vk.fluid_pressure_views[1] &&
		vk.fluid_divergence_view && vk.volumetric_params_buffer != VK_NULL_HANDLE &&
		vk.volumetric_telemetry_view )
	{
		VkDescriptorImageInfo info[15];
		VkWriteDescriptorSet writes[16];
		Vk_Sampler_Def sd_linear;
		Vk_Sampler_Def sd_nearest;
			VkSampler sampler_linear;
			VkSampler sampler_nearest;

		Com_Memset( &sd_linear, 0, sizeof( sd_linear ) );
		sd_linear.gl_mag_filter = sd_linear.gl_min_filter = GL_LINEAR;
		sd_linear.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd_linear.noAnisotropy = qtrue;

		Com_Memset( &sd_nearest, 0, sizeof( sd_nearest ) );
		sd_nearest.gl_mag_filter = sd_nearest.gl_min_filter = GL_NEAREST;
		sd_nearest.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd_nearest.noAnisotropy = qtrue;

		sampler_linear = vk_find_sampler( &sd_linear );
		sampler_nearest = vk_find_sampler( &sd_nearest );

		Com_Memset( info, 0, sizeof( info ) );
		info[0].sampler = sampler_linear;
		info[0].imageView = vk.fluid_velocity_views[0];
		info[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[1].sampler = sampler_linear;
		info[1].imageView = vk.fluid_velocity_views[1];
		info[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[2].imageView = vk.fluid_velocity_views[0];
		info[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[3].imageView = vk.fluid_velocity_views[1];
		info[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[4].sampler = sampler_linear;
		info[4].imageView = vk.fluid_density_views[0];
		info[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[5].sampler = sampler_linear;
		info[5].imageView = vk.fluid_density_views[1];
		info[5].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[6].imageView = vk.fluid_density_views[0];
		info[6].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[7].imageView = vk.fluid_density_views[1];
		info[7].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[8].sampler = sampler_nearest;
		info[8].imageView = vk.fluid_pressure_views[0];
		info[8].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[9].sampler = sampler_nearest;
		info[9].imageView = vk.fluid_pressure_views[1];
		info[9].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[10].imageView = vk.fluid_pressure_views[0];
		info[10].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[11].imageView = vk.fluid_pressure_views[1];
		info[11].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[12].imageView = vk.fluid_divergence_view;
		info[12].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[13].sampler = sampler_nearest;
		info[13].imageView = vk.fluid_divergence_view;
		info[13].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		info[14].imageView = vk.volumetric_telemetry_view;
		info[14].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		Com_Memset( writes, 0, sizeof( writes ) );
		for ( uint32_t i = 0; i < 15; i++ ) {
			writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[i].dstSet = vk.volumetric_fluid_descriptor;
			writes[i].dstBinding = ( i == 14 ) ? 15 : i;
			writes[i].descriptorCount = 1;
			writes[i].descriptorType =
				(i == 2 || i == 3 || i == 6 || i == 7 || i == 10 || i == 11 || i == 12 || i == 14) ?
				VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[i].pImageInfo = &info[i];
		}

		writes[15].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[15].dstSet = vk.volumetric_fluid_descriptor;
		writes[15].dstBinding = 14;
		writes[15].descriptorCount = 1;
		writes[15].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[15].pBufferInfo = &params_buffer;

		qvkUpdateDescriptorSets( vk.device, ARRAY_LEN( writes ), writes, 0, NULL );
	}

	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL,
			"[VK][fog] descriptors computeSet=0x%llx storage=GENERAL history=GENERAL light=GENERAL ext=GENERAL clamp=GENERAL noiseView=0x%llx sunShadow=0x%llx localSpot=0x%llx localPoint=0x%llx motionView=0x%llx depthView=0x%llx compositeSet=0x%llx sceneView=0x%llx\n",
			(unsigned long long)(uintptr_t)vk.volumetric_compute_descriptor,
			(unsigned long long)(uintptr_t)vk.fog_noise_view,
			(unsigned long long)(uintptr_t)vk.sun_shadow_sample_view,
			(unsigned long long)(uintptr_t)vk.local_spot_shadow_atlas_sample_view,
			(unsigned long long)(uintptr_t)vk.local_point_shadow_array_sample_view,
			(unsigned long long)(uintptr_t)vk.motion_vector_view,
			(unsigned long long)(uintptr_t)volumetric_depth_view,
			(unsigned long long)(uintptr_t)vk.volumetric_composite_descriptor,
			(unsigned long long)(uintptr_t)vk.fog_scene_image_view );
	}
}


void vk_init_descriptors( void )
{
	VkDescriptorSetAllocateInfo alloc;
	VkDescriptorBufferInfo info;
	VkWriteDescriptorSet desc;
	uint32_t i;

	vk_create_postfx_params_buffers();

	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.pNext = NULL;
	alloc.descriptorPool = vk.descriptor_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.set_layout_storage;

	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.storage.descriptor ) );

	info.buffer = vk.storage.buffer;
	info.offset = 0;
	info.range = sizeof( uint32_t );

	desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	desc.dstSet = vk.storage.descriptor;
	desc.dstBinding = 0;
	desc.dstArrayElement = 0;
	desc.descriptorCount = 1;
	desc.pNext = NULL;
	desc.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	desc.pImageInfo = NULL;
	desc.pBufferInfo = &info;
	desc.pTexelBufferView = NULL;

	qvkUpdateDescriptorSets( vk.device, 1, &desc, 0, NULL );

	// allocated and update descriptor set
	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_uniform;

		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.tess[i].uniform_descriptor ) );

		vk_update_uniform_descriptor( vk.tess[ i ].uniform_descriptor, vk.tess[ i ].vertex_buffer );

		SET_OBJECT_NAME( vk.tess[ i ].uniform_descriptor, va( "uniform descriptor %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT );
	}

	if ( vk.color_image_view )
	{
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.pNext = NULL;
		alloc.descriptorPool = vk.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.set_layout_sampler;

			for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.color_descriptor[i] ) );
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.post_color_descriptor[i] ) );
			}
		for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.depth_descriptor[i] ) );

		if ( r_ssao && r_ssao->integer ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssao_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssao_blur_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssao_scene_descriptor ) );
		}
		if ( r_oit && r_oit->integer ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.oit_opaque_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.oit_accum_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.oit_reveal_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.oit_depth_descriptor ) );
		}

		if ( PostFX_SSR_IsEnabled() ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssr_descriptor[0] ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.ssr_descriptor[1] ) );
		}

		if ( r_bloom->integer )
		{
			for ( i = 0; i < ARRAY_LEN( vk.bloom_image_descriptor ); i++ )
			{
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.bloom_image_descriptor[i] ) );
			}
		}

		if ( vk.smaaActive )
		{
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa_edge_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa_blend_descriptor ) );
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.smaa_compose_descriptor ) );
		}
		for ( i = 0; i < 2; i++ ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.taa_history_descriptor[i] ) );
		}

		alloc.descriptorSetCount = 1;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.screenMap.color_descriptor ) ); // screenmap

#ifdef VK_PBR_BRDFLUT
		if( vk.pbrActive )
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.brdflut_image_descriptor ) );
#endif

		// cubemap
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.cubeMap.color_descriptor ) );

		alloc.pSetLayouts = &vk.set_layout_postfx_uniform;
		for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
			VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.postfx_params_descriptor[i] ) );
		}

		alloc.pSetLayouts = &vk.volumetric_compute_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.volumetric_compute_descriptor ) );

		alloc.pSetLayouts = &vk.volumetric_composite_layout;
		VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.volumetric_composite_descriptor ) );

			if ( vk.volumetric_depth_resolve_layout != VK_NULL_HANDLE ) {
				alloc.pSetLayouts = &vk.volumetric_depth_resolve_layout;
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.volumetric_depth_resolve_descriptor ) );
			}
			if ( vk.luminance_layout != VK_NULL_HANDLE ) {
				alloc.pSetLayouts = &vk.luminance_layout;
				for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
					VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.luminance_descriptor[i] ) );
			}
			if ( vk.volumetric_fluid_layout != VK_NULL_HANDLE ) {
				alloc.pSetLayouts = &vk.volumetric_fluid_layout;
				VK_CHECK( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.volumetric_fluid_descriptor ) );
			}

			for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
				VkDescriptorBufferInfo postfx_info;
				VkWriteDescriptorSet postfx_desc;

				postfx_info.buffer = vk.postfx_params_buffer[i];
				postfx_info.offset = 0;
				postfx_info.range = sizeof( VkPostFXParams );

				Com_Memset( &postfx_desc, 0, sizeof( postfx_desc ) );
				postfx_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				postfx_desc.dstSet = vk.postfx_params_descriptor[i];
				postfx_desc.dstBinding = 0;
				postfx_desc.descriptorCount = 1;
				postfx_desc.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				postfx_desc.pBufferInfo = &postfx_info;
				qvkUpdateDescriptorSets( vk.device, 1, &postfx_desc, 0, NULL );
			}

			vk_update_attachment_descriptors();
			vk_update_volumetric_descriptors();

			vk_forward_plus_init_graphics_descriptors();
		}
	}
