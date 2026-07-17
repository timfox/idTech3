#pragma once

#include "../common/vulkan/vulkan.h"

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

/* Ensure prim-material SSBO covers world+entity prim counts; fills INVALID indices. */
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
void vk_rtx_bindless_sync_prim_materials( uint32_t worldPrimCount, uint32_t entityPrimCount );
void vk_rtx_bindless_bind_textures( VkDescriptorSet set, uint32_t binding );
void vk_rtx_bindless_bind_prim_material( VkDescriptorSet set, uint32_t binding );
void vk_rtx_bindless_status_line( void );

#endif
