#pragma once

/*
 * Renderer IQ P0-A — authoritative SceneHDR writer ownership.
 *
 * Tracks who last wrote vk.color_image (SCENE_LINEAR_HDR) and refuses
 * illegal replace/compose orders (e.g. Raster GI after WBOIT resolve).
 * See docs/COLOR_PIPELINE.md.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

typedef enum sceneHdrStage_e {
	SCENE_HDR_NONE = 0,
	SCENE_HDR_OPAQUE,
	SCENE_HDR_SKY_ATMOSPHERE,
	SCENE_HDR_GI,
	SCENE_HDR_WBOIT_RESOLVE,
	SCENE_HDR_ADDITIVE,
	SCENE_HDR_REFRACTION,
	SCENE_HDR_SPECIAL_BLEND,
	SCENE_HDR_WEAPON,
	SCENE_HDR_VOLUMETRIC,
	SCENE_HDR_BLOOM_SOURCE,
	SCENE_HDR_STAGE_COUNT
} sceneHdrStage_t;

typedef enum sceneHdrWriteMode_e {
	SCENE_HDR_WRITE_REPLACE = 0, /* full-screen / storage overwrite of SceneHDR */
	SCENE_HDR_WRITE_COMPOSE      /* additive / blend over existing SceneHDR */
} sceneHdrWriteMode_t;

typedef struct sceneHdrOwnership_s {
	sceneHdrStage_t lastWriter;
	uint32_t generation;
	uint64_t frameNumber;

	uint32_t colorSpace;     /* vkColorSpace_t */
	uint32_t exposureState;  /* 0 = pre-exposure scene linear */
	uint32_t extentWidth;
	uint32_t extentHeight;

	uint32_t writerContractHash;
	char writerName[64];
	uint32_t blockedWrites;
	uint32_t allowedWrites;
} sceneHdrOwnership_t;

void vk_scene_hdr_ownership_register( void );
void vk_scene_hdr_ownership_begin_frame( void );

const sceneHdrOwnership_t *vk_scene_hdr_ownership_get( void );
const char *vk_scene_hdr_stage_name( sceneHdrStage_t stage );

/*
 * Returns qtrue if the write is allowed. On success, updates ownership.
 * mode REPLACE after WBOIT/weapon/bloom stages is rejected for GI.
 */
qboolean vk_scene_hdr_note_writer( sceneHdrStage_t stage, const char *writerName,
	sceneHdrWriteMode_t mode );

/* True when geometry-time GI may still write SceneHDR (pre-OIT only). */
qboolean vk_scene_hdr_allows_pre_oit_gi( void );

/* One-shot warning when a GI pass is skipped for ownership. */
void vk_scene_hdr_log_gi_blocked( const char *passName );

void vk_scene_hdr_ownership_status_f( void );

#endif /* USE_VULKAN */
