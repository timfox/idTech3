#pragma once


/*
 * Raster Ultra 1.10 — capture / screenshot color-space policy.
 * Never silently write HDR into 8-bit SDR files.
 */

typedef enum {
	VK_CAPTURE_DISPLAY = 0,   /* post-tonemap SDR (what user saw) */
	VK_CAPTURE_PRE_TONEMAP,   /* HDR scene buffer intent */
	VK_CAPTURE_SCENE_LINEAR,  /* EXR-class intent */
	VK_CAPTURE_HDR_DISPLAY,   /* HDR10/scRGB present */
	VK_CAPTURE_COUNT
} vkCaptureColorSpace_t;

typedef struct vkCapturePipelineState_s {
	vkCaptureColorSpace_t colorSpace;
	qboolean includeUI;
	qboolean deterministicExposure;
	qboolean deterministicGrain;
	qboolean warnHdrToSdr;
	uint32_t captures;
	uint32_t blockedSilentHdr;
} vkCapturePipelineState_t;

void vk_capture_pipeline_register_cvars( void );
void vk_capture_pipeline_init( void );
void vk_capture_pipeline_shutdown( void );

qboolean vk_capture_pipeline_active( void );
const vkCapturePipelineState_t *vk_capture_pipeline_state( void );

/* Call before encoding an 8-bit screenshot; returns qfalse if blocked. */
qboolean vk_capture_pipeline_allow_sdr_encode( void );
void vk_capture_pipeline_note_capture( void );

void vk_capture_pipeline_status_f( void );

