#pragma once

/*
 * Unified shadow consumer contract (pre-virtualization).
 * See docs/SHADOW_CONTRACT.md
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"
#include "../common/vulkan/vulkan.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VK_SHADOW_CONTRACT_MAX_RECORDS 16u
#define VK_SHADOW_CONTRACT_NAME_LEN    32u

typedef enum {
	VK_SHADOW_TYPE_NONE = 0,
	VK_SHADOW_TYPE_CSM_CASCADE,
	VK_SHADOW_TYPE_SPOT,
	VK_SHADOW_TYPE_POINT_FACE,
	VK_SHADOW_TYPE_ATLAS_PAGE
} vkShadowType_t;

typedef struct {
	uint32_t type;
	uint32_t textureIndex;
	uint32_t layerOrPage;
	uint32_t flags;

	float    worldToShadow[16];
	float    atlasScaleBias[4];
	float    depthBiasParams[4];
	float    filterParams[4];

	uint32_t slot;
	uint32_t cascade;
	uint32_t generation;
	uint32_t extentW;
	uint32_t extentH;
	uint64_t depthHandle;
	char     consumer[VK_SHADOW_CONTRACT_NAME_LEN];
	qboolean allocated;
} GpuShadowRecord;

void vk_shadow_contract_register( void );
void vk_shadow_contract_begin_frame( void );
void vk_shadow_contract_end_frame( void );
void vk_shadow_contract_shutdown( void );
GpuShadowRecord *vk_shadow_contract_alloc( uint32_t slot, uint32_t cascade );
void vk_shadow_contract_note_consumer( uint32_t slot, const char *consumer );
void vk_shadow_contract_set_transform( uint32_t slot, const float *worldToShadow16 );
void vk_shadow_contract_set_extent( uint32_t slot, uint32_t w, uint32_t h );
void vk_shadow_contract_set_atlas( uint32_t slot, float tileScaleX, float tileScaleY,
	float offsetX, float offsetY );
void vk_shadow_contract_set_bias_filter( uint32_t slot, float depthBias, float pcfRadius );
uint32_t vk_shadow_contract_generation( void );
const GpuShadowRecord *vk_shadow_contract_record( uint32_t slot );
qboolean vk_shadow_contract_upload_ssbo( void );
VkBuffer vk_shadow_contract_ssbo( void );
/* WBOIT/MBOIT: SSBO + sun atlas descriptor set (binding 0 storage, 1 sampler). */
VkDescriptorSetLayout vk_shadow_contract_oit_set_layout( void );
VkDescriptorSet vk_shadow_contract_oit_descriptor( void );
void vk_shadow_contract_oit_update_descriptors( void );
void vk_shadow_contract_status_f( void );

#ifdef __cplusplus
}
#endif

#endif /* USE_VULKAN */
