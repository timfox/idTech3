#pragma once

#include "tr_local.h"

#ifdef USE_VULKAN_RTX

/* Per-primitive material indirection (D2 Phase A). textureIndex 0xFFFFFFFF = SSBO RGB fallback. */
typedef struct {
	uint32_t textureIndex;
	uint16_t uvSet;
	uint16_t flags;
} RtxPrimMaterial;

#define RTX_PRIM_MATERIAL_INVALID 0xFFFFFFFFu
#define RTX_PRIM_MATERIAL_FLAG_THUMB 0x0001u
#define RTX_PRIM_MATERIAL_FLAG_SRGB  0x0002u

void vk_rtx_bindless_init( void );
void vk_rtx_bindless_shutdown( void );
qboolean vk_rtx_bindless_active( void );
uint32_t vk_rtx_bindless_texture_count( void );
uint32_t vk_rtx_bindless_cap( void );
int vk_rtx_bindless_mode( void );
qboolean vk_rtx_bindless_indexing_supported( void );

/*
 * Pack-time prim materials (Phase A.1): dense textureIndex into host staging,
 * then sync uploads [world | entity] to PrimMaterialSSBO. Sampling still off
 * until AS UVs + descriptor array (active() stays false).
 */
void vk_rtx_bindless_reset_texture_table( void );
void vk_rtx_bindless_prepare_capacity( uint32_t totalPrims );
void vk_rtx_bindless_clear_prims( uint32_t begin, uint32_t count );
void vk_rtx_bindless_set_entity_base( uint32_t worldPrimCount );
void vk_rtx_bindless_set_prim_from_image( uint32_t primIndex, image_t *img );
void vk_rtx_bindless_set_prim_from_shader( uint32_t primIndex, const shader_t *shader );
void vk_rtx_bindless_set_entity_prim_from_image( uint32_t entityPrimIndex, image_t *img );
void vk_rtx_bindless_set_entity_prim_from_shader( uint32_t entityPrimIndex, const shader_t *shader );

/* Upload host prim materials (world+entity); missing slots stay INVALID. */
void vk_rtx_bindless_sync_prim_materials( uint32_t worldPrimCount, uint32_t entityPrimCount );

void vk_rtx_bindless_bind_textures( VkDescriptorSet set, uint32_t binding );
void vk_rtx_bindless_bind_prim_material( VkDescriptorSet set, uint32_t binding );

void vk_rtx_bindless_status_line( void );

#else

void vk_rtx_bindless_init( void );
void vk_rtx_bindless_shutdown( void );
qboolean vk_rtx_bindless_active( void );
uint32_t vk_rtx_bindless_texture_count( void );
uint32_t vk_rtx_bindless_cap( void );
int vk_rtx_bindless_mode( void );
qboolean vk_rtx_bindless_indexing_supported( void );
void vk_rtx_bindless_reset_texture_table( void );
void vk_rtx_bindless_prepare_capacity( uint32_t totalPrims );
void vk_rtx_bindless_clear_prims( uint32_t begin, uint32_t count );
void vk_rtx_bindless_set_entity_base( uint32_t worldPrimCount );
void vk_rtx_bindless_set_prim_from_image( uint32_t primIndex, image_t *img );
void vk_rtx_bindless_set_prim_from_shader( uint32_t primIndex, const shader_t *shader );
void vk_rtx_bindless_set_entity_prim_from_image( uint32_t entityPrimIndex, image_t *img );
void vk_rtx_bindless_set_entity_prim_from_shader( uint32_t entityPrimIndex, const shader_t *shader );
void vk_rtx_bindless_sync_prim_materials( uint32_t worldPrimCount, uint32_t entityPrimCount );
void vk_rtx_bindless_bind_textures( VkDescriptorSet set, uint32_t binding );
void vk_rtx_bindless_bind_prim_material( VkDescriptorSet set, uint32_t binding );
void vk_rtx_bindless_status_line( void );

#endif
