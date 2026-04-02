#ifndef VK_POSTFX_SANITIZE_H
#define VK_POSTFX_SANITIZE_H

#ifdef __cplusplus
extern "C" {
#endif

void vk_postfx_sanitize_taa_params( int has_valid_history,
	float stationary_feedback,
	float motion_feedback,
	float sharpen,
	float out_taa_params[4] );

void vk_postfx_sanitize_auto_exposure_params( int has_valid_luminance,
	float filtered_avg_log_luminance,
	float auto_target_luminance,
	float manual_exposure,
	float min_exposure,
	float max_exposure,
	float out_auto_exposure_params[4] );

#ifdef __cplusplus
}
#endif

#endif
