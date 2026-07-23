#pragma once

/*
 * Phase 2.6A — certification GPU readback (blocking for explicit cert frames).
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
	/* Host float RGBA (decoded); size = width*height*4 floats. Caller must not free. */
	float *rgba;
	uint32_t pixelCount;
	qboolean valid;
} certReadbackCapture_t;

void vk_cert_readback_register( void );
void vk_cert_readback_shutdown( void );

const char *vk_cert_readback_resource_name( certReadbackResource_t r );
float vk_cert_half_to_float( uint16_t h );

/* Blocking capture of a named resource into the last-capture slot. */
qboolean vk_cert_readback_capture( certReadbackResource_t resource, certReadbackCapture_t *out );
void vk_cert_readback_flush( void );
const certReadbackCapture_t *vk_cert_readback_last( void );

/* Decode packed half/float bytes into float RGBA (allocates via ri.Hunk_AllocateTempMemory if needed externally). */
qboolean vk_cert_readback_decode_to_rgba( VkFormat format, uint32_t width, uint32_t height,
	uint32_t rowPitchBytes, const void *src, float *dstRgba );

#endif /* USE_VULKAN */
