#pragma once

/*
 * Production frame resource contract — SceneHDR/depth/G-buffer ownership.
 * See docs/RENDERER_FRAME_CONTRACT.md
 */


#include "../common/tr_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VK_FRAME_CONTRACT_MAX_READERS       8u
#define VK_FRAME_CONTRACT_NAME_LEN          32u
#define VK_FRAME_CONTRACT_WRITER_LEN        48u
#define VK_FRAME_CONTRACT_INVALIDATION_LEN  64u

typedef enum {
	VK_FRAME_RES_SCENE_HDR = 0,
	VK_FRAME_RES_SCENE_HDR_PREVIOUS,
	VK_FRAME_RES_SCENE_DEPTH,
	VK_FRAME_RES_SCENE_DEPTH_PREVIOUS,
	VK_FRAME_RES_GBUFFER_ALBEDO,
	VK_FRAME_RES_GBUFFER_NORMAL,
	VK_FRAME_RES_GBUFFER_MATERIAL,
	VK_FRAME_RES_GBUFFER_LIGHTING,
	VK_FRAME_RES_VELOCITY,
	VK_FRAME_RES_TEMPORAL_CLASS,
	VK_FRAME_RES_OIT_ACCUM,
	VK_FRAME_RES_OIT_REVEAL,
	VK_FRAME_RES_WEAPON_HDR,
	VK_FRAME_RES_WEAPON_HISTORY,
	VK_FRAME_RES_BLOOM_SOURCE,
	VK_FRAME_RES_TONEMAP_SOURCE,
	VK_FRAME_RES_FINAL_LDR,
	VK_FRAME_RES_COUNT
} vkFrameContractResource_t;

typedef struct {
	char     name[VK_FRAME_CONTRACT_NAME_LEN];
	uint64_t handle;
	uint32_t format;
	uint32_t extentW;
	uint32_t extentH;
	uint32_t generation;
	char     firstWriter[VK_FRAME_CONTRACT_WRITER_LEN];
	char     lastWriter[VK_FRAME_CONTRACT_WRITER_LEN];
	char     readers[VK_FRAME_CONTRACT_MAX_READERS][VK_FRAME_CONTRACT_WRITER_LEN];
	uint32_t readerCount;
	uint32_t lastValidFrame;
	char     invalidationReason[VK_FRAME_CONTRACT_INVALIDATION_LEN];
	qboolean writtenThisFrame;
	qboolean clearedAfterWrite;
} vkFrameResourceHistory_t;

typedef struct {
	uint32_t                 frame;
	int                      renderMode;
	uint32_t                 renderExtentW;
	uint32_t                 renderExtentH;
	uint32_t                 outputExtentW;
	uint32_t                 outputExtentH;
	uint32_t                 clusterListGeneration;
	uint32_t                 gpuSceneGeneration;
	uint32_t                 lightBufferGeneration;
	uint32_t                 materialBufferGeneration;
	uint32_t                 shadowGeneration;
	uint32_t                 probeGeneration;
	uint32_t                 exposureGeneration;
	uint32_t                 deferredGbufferGeneration;
	float                    adaptedExposure;
	vkFrameResourceHistory_t resources[VK_FRAME_RES_COUNT];
	uint32_t                 validateFailCount;
} vkFrameContractSnapshot_t;

void vk_frame_contract_register( void );
void vk_frame_contract_begin_frame( void );
void vk_frame_contract_note_writer( const char *resourceName, const char *passName );
void vk_frame_contract_note_reader( const char *resourceName, const char *passName );
void vk_frame_contract_set_meta( vkFrameContractResource_t res, uint64_t handle,
	uint32_t format, uint32_t extentW, uint32_t extentH, uint32_t generation );
void vk_frame_contract_invalidate( vkFrameContractResource_t res, const char *reason );
uint32_t vk_frame_contract_validate( qboolean printPass );
void vk_frame_contract_snapshot( vkFrameContractSnapshot_t *out );
void vk_frame_contract_status_f( void );
void vk_frame_contract_capture_f( void );

#ifdef __cplusplus
}
#endif
