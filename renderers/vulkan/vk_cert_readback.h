#pragma once

/*
 * Phase 2.6A/2.6C — certification GPU readback + deferred OIT frame snapshots.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"
#include <vulkan/vulkan.h>

typedef enum {
	CERT_RB_FOG_SCENE = 0,
	CERT_RB_SCENE_DEPTH,
	CERT_RB_OIT_ACCUM,
	CERT_RB_OIT_REVEALAGE,
	CERT_RB_OIT_ADDITIVE,
	CERT_RB_RESOLVED_WBOIT,
	CERT_RB_SORTED_REFERENCE,
	CERT_RB_BLOOM_SOURCE,
	CERT_RB_TONEMAP_INPUT,
	CERT_RB_FINAL_DISPLAY,
	CERT_RB_BLOOM_EXTRACT,
	CERT_RB_GBUFFER_ALBEDO,
	CERT_RB_GBUFFER_NORMAL,
	CERT_RB_MOTION_VECTORS,
	CERT_RB_COUNT
} certReadbackResource_t;

typedef struct certReadbackCapture_s {
	certReadbackResource_t resource;
	VkFormat format;
	uint32_t width;
	uint32_t height;
	uint32_t rowPitchBytes;
	uint64_t frameNumber;
	uint32_t generation;
	uint32_t pipelineStage;
	char colorSpace[32];
	qboolean preExposed;
	uint32_t oitContractHash;
	uint32_t weightContractHash;
	uint32_t resolveContractHash;
	/* Host float RGBA (decoded). Owned by readback module until next overwrite of that slot. */
	float *rgba;
	uint32_t pixelCount;
	qboolean valid;
} certReadbackCapture_t;

typedef struct certOitSnapshot_s {
	qboolean valid;
	uint64_t frameNumber;
	uint32_t generation;
	certReadbackCapture_t fog;
	certReadbackCapture_t accum;
	certReadbackCapture_t reveal;
	certReadbackCapture_t resolved;
} certOitSnapshot_t;

/* Phase 1.5 — IQ snapshot: bloom source/extract + optional G-buffer/motion. */
typedef struct certIqSnapshot_s {
	qboolean valid;
	uint64_t frameNumber;
	uint32_t generation;
	certReadbackCapture_t bloomSource;
	certReadbackCapture_t bloomExtract;
	certReadbackCapture_t gbufferAlbedo;
	certReadbackCapture_t gbufferNormal;
	certReadbackCapture_t motion;
} certIqSnapshot_t;

void vk_cert_readback_register( void );
void vk_cert_readback_shutdown( void );

const char *vk_cert_readback_resource_name( certReadbackResource_t r );
float vk_cert_half_to_float( uint16_t h );

/* Blocking capture of a named resource into the last-capture slot (overwrites shared scratch). */
qboolean vk_cert_readback_capture( certReadbackResource_t resource, certReadbackCapture_t *out );
void vk_cert_readback_flush( void );
const certReadbackCapture_t *vk_cert_readback_last( void );

/*
 * Phase 2.6C — record fog/accum/reveal/resolved copies into the CURRENT command buffer
 * immediately after WBOIT resolve (before bloom/TAA overwrite color). Finalize after the
 * frame's rendering_finished_fence (begin_frame wait).
 */
qboolean vk_cert_readback_record_oit_snapshot( VkCommandBuffer cmd, int cmdIndex );
qboolean vk_cert_readback_oit_snapshot_pending( int cmdIndex );
qboolean vk_cert_readback_finalize_oit_snapshot( int cmdIndex, certOitSnapshot_t *out );
const certOitSnapshot_t *vk_cert_readback_last_oit_snapshot( void );

/*
 * Phase 1.5 — record bloom source + extract (+ optional G-buffer/motion) into the
 * CURRENT command buffer after bloom extract. Finalize after rendering_finished_fence.
 */
qboolean vk_cert_readback_record_iq_snapshot( VkCommandBuffer cmd, int cmdIndex );
qboolean vk_cert_readback_iq_snapshot_pending( int cmdIndex );
qboolean vk_cert_readback_finalize_iq_snapshot( int cmdIndex, certIqSnapshot_t *out );
const certIqSnapshot_t *vk_cert_readback_last_iq_snapshot( void );

qboolean vk_cert_readback_decode_to_rgba( VkFormat format, uint32_t width, uint32_t height,
	uint32_t rowPitchBytes, const void *src, float *dstRgba );

#endif /* USE_VULKAN */
