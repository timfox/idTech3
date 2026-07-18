#pragma once

#include "../common/vulkan/vulkan.h"

#ifdef USE_VULKAN_RTX

void vk_rtx_init( void );
void vk_rtx_shutdown( void );
void vk_rtx_frame_begin( void );
void vk_rtx_record_demo_pass( VkCommandBuffer cmd );
qboolean vk_rtx_scene_ready( void );
void vk_rtx_scene_prepare( void );
void vk_rtx_scene_extent( uint32_t *w, uint32_t *h );
void vk_rtx_bind_tlas_descriptor( VkDescriptorSet set );
void vk_rtx_bind_world_albedo_ssbo( VkDescriptorSet set, uint32_t binding );
uint32_t vk_rtx_world_albedo_count( void );
void vk_rtx_bind_world_normal_ssbo( VkDescriptorSet set, uint32_t binding );
uint32_t vk_rtx_world_normal_count( void );
void vk_rtx_bind_prim_uv_ssbo( VkDescriptorSet set, uint32_t binding );
uint32_t vk_rtx_world_uv_count( void );
void vk_rtx_bind_entity_albedo_ssbo( VkDescriptorSet set, uint32_t binding );
uint32_t vk_rtx_entity_albedo_count( void );
void vk_rtx_bind_entity_normal_ssbo( VkDescriptorSet set, uint32_t binding );
uint32_t vk_rtx_entity_normal_count( void );

#else

void vk_rtx_init( void );
void vk_rtx_shutdown( void );
void vk_rtx_frame_begin( void );
void vk_rtx_record_demo_pass( VkCommandBuffer cmd );
qboolean vk_rtx_scene_ready( void );
void vk_rtx_scene_prepare( void );
void vk_rtx_scene_extent( uint32_t *w, uint32_t *h );
void vk_rtx_bind_tlas_descriptor( VkDescriptorSet set );
void vk_rtx_bind_world_albedo_ssbo( VkDescriptorSet set, uint32_t binding );
uint32_t vk_rtx_world_albedo_count( void );
void vk_rtx_bind_world_normal_ssbo( VkDescriptorSet set, uint32_t binding );
uint32_t vk_rtx_world_normal_count( void );
void vk_rtx_bind_prim_uv_ssbo( VkDescriptorSet set, uint32_t binding );
uint32_t vk_rtx_world_uv_count( void );
void vk_rtx_bind_entity_albedo_ssbo( VkDescriptorSet set, uint32_t binding );
uint32_t vk_rtx_entity_albedo_count( void );
void vk_rtx_bind_entity_normal_ssbo( VkDescriptorSet set, uint32_t binding );
uint32_t vk_rtx_entity_normal_count( void );

#endif
