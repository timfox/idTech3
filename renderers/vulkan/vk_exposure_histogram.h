#pragma once


/*
 * Raster Ultra 1.10 — histogram / metering exposure controller.
 * Hardens eye adaptation: metering modes, EV clamps, cut/map reset.
 */

typedef enum {
	VK_EXPOSURE_METER_AVERAGE = 0,
	VK_EXPOSURE_METER_CENTER,
	VK_EXPOSURE_METER_SPOT,
	VK_EXPOSURE_METER_HISTOGRAM
} vkExposureMeterMode_t;

typedef struct vkExposureHistogramState_s {
	vkExposureMeterMode_t meterMode;
	float currentEV;
	float targetEV;
	float preExposure;
	float compensationEV;
	float minEV;
	float maxEV;
	float lastAvgLogLum;
	qboolean fixedExposure;
	qboolean valid;
	uint32_t resets;
	uint32_t frames;
} vkExposureHistogramState_t;

void vk_exposure_histogram_register_cvars( void );
void vk_exposure_histogram_init( void );
void vk_exposure_histogram_shutdown( void );

qboolean vk_exposure_histogram_active( void );
const vkExposureHistogramState_t *vk_exposure_histogram_state( void );

void vk_exposure_histogram_on_camera_cut( void );
void vk_exposure_histogram_on_map_change( void );
void vk_exposure_histogram_on_focus_recovery( void );

/* Feed sparse luminance sample (log2) from existing luminance.comp readback. */
void vk_exposure_histogram_notify_luminance( float avgLogLum, qboolean valid );

/* Adjust auto-exposure target multiplier for metering mode (1.0 = unchanged). */
float vk_exposure_histogram_meter_scale( void );

void vk_exposure_histogram_status_f( void );

