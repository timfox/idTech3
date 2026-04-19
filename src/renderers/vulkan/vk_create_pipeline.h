/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Main-scene graphics pipeline factory (Vk_Pipeline_Def → VkPipeline).
Included from vk.h after VK_Pipeline_t.
===========================================================================
*/

#ifndef VK_CREATE_PIPELINE_H
#define VK_CREATE_PIPELINE_H

/* Included from vk.h after Vk_Pipeline_Def / renderPass_t / VkPipeline. */

struct Vk_Pipeline_FragSpecData {
	int32_t alpha_test_func;
	float   alpha_test_value;
	float   depth_fragment;
	int32_t alpha_to_coverage;
	int32_t color_mode;
	int32_t abs_light;
	int32_t tex_mode;
	int32_t discard_mode;
	float   identity_color;
	float	identity_alpha;
	int32_t	acff;
#ifdef USE_VK_PBR
	float   specularScale_x;
	float   specularScale_y;
	float   specularScale_z;
	float   specularScale_w;
	float   normalScale_x;
	float   normalScale_y;
	float   normalScale_z;
	float   normalScale_w;
	int32_t normal_texture_set;
	int32_t physical_texture_set;
	int32_t env_texture_set;
	int32_t lightmap_texture_set;
	int32_t irradiance_texture_set;
	int32_t emissive_texture_set;
	int32_t clearcoat_texture_set;
	int32_t sheen_texture_set;
	int32_t anisotropy_texture_set;
	int32_t transmission_texture_set;
	int32_t subsurface_texture_set;
	int32_t deluxe_mapping;
	float deluxe_specular_scale;
	float lightmap_scale;
	int32_t lightmap_srgb_decode;
	int32_t detail_texture_set;
	float   detail_scale;
	int32_t pom_height_source;
	int32_t pom_enabled;
	int32_t pom_max_steps;
	float   parallax_bias_shader;
	float   forward_plus_shade_strength;
#endif
};

VkPipeline vk_create_pipeline( const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index );
VkPipeline vk_gen_pipeline( uint32_t index );
uint32_t vk_find_pipeline_ext( uint32_t base, const Vk_Pipeline_Def *def, qboolean use );
void vk_get_pipeline_def( uint32_t pipeline, Vk_Pipeline_Def *def );

#endif
