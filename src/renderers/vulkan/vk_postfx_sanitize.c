#include "vk_postfx_sanitize.h"
#include <math.h>

static float vk_postfx_clamp( float min_value, float max_value, float value )
{
	if ( value < min_value ) {
		return min_value;
	}
	if ( value > max_value ) {
		return max_value;
	}
	return value;
}

void vk_postfx_sanitize_taa_params( int has_valid_history,
	float stationary_feedback,
	float motion_feedback,
	float sharpen,
	float out_taa_params[4] )
{
	if ( !out_taa_params ) {
		return;
	}

	out_taa_params[0] = has_valid_history ? 1.0f : 0.0f;
	out_taa_params[1] = vk_postfx_clamp( 0.0f, 0.99f, stationary_feedback );
	out_taa_params[2] = vk_postfx_clamp( 0.0f, 0.99f, motion_feedback );
	out_taa_params[3] = vk_postfx_clamp( 0.0f, 1.0f, sharpen );
}

void vk_postfx_sanitize_auto_exposure_params( int has_valid_luminance,
	float filtered_avg_log_luminance,
	float auto_target_luminance,
	float manual_exposure,
	float min_exposure,
	float max_exposure,
	float out_auto_exposure_params[4] )
{
	const float safe_manual_exposure = fmaxf( manual_exposure, 1e-4f );
	const float safe_target = fmaxf( auto_target_luminance, 1e-4f );
	const float safe_min_exposure = fmaxf( min_exposure, 0.01f );
	const float safe_max_exposure = fmaxf( max_exposure, safe_min_exposure );

	if ( !out_auto_exposure_params ) {
		return;
	}

	if ( has_valid_luminance ) {
		out_auto_exposure_params[0] = filtered_avg_log_luminance;
	} else {
		out_auto_exposure_params[0] = log2f( safe_target / safe_manual_exposure );
	}
	out_auto_exposure_params[1] = safe_target;
	out_auto_exposure_params[2] = safe_min_exposure;
	out_auto_exposure_params[3] = safe_max_exposure;
}
