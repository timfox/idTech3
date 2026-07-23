#pragma once

/*
 * Phase 2.6A — certification metrics over float RGBA buffers.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

typedef enum {
	CERT_MASK_ALL = 0,
	CERT_MASK_EMPTY_OIT,
	CERT_MASK_TRANSPARENT,
	CERT_MASK_INTERSECTION,
	CERT_MASK_EDGE,
	CERT_MASK_FOG,
	CERT_MASK_ADDITIVE,
	CERT_MASK_BACKGROUND
} certMetricsMask_t;

typedef struct certMetrics_s {
	uint32_t pixelCount;
	uint32_t validPixelCount;
	uint32_t nanCount;
	uint32_t infCount;
	double meanAbsRgb;
	double maxAbsRgb;
	double rmse;
	double meanRelLum;
	double maxRelLum;
	double meanHue;
	double maxHue;
	uint32_t modifiedEmptyPixels;
	double maxEmptyPixelError;
	double meanEmptyPixelError;
	double revealageError;
	double weightMin;
	double weightMax;
	double weightMean;
	uint32_t weightLowClamps;
	uint32_t weightHighClamps;
	uint32_t weightInvalid;
	double permutationVariance;
	double depthError;
	double fogTransmittanceError;
	double fogInscatterError;
	double additiveRevealageDelta;
	uint32_t blackFrameCorruption;
} certMetrics_t;

void vk_cert_metrics_clear( certMetrics_t *m );

float vk_cert_metrics_luminance( float r, float g, float b );

/* Compare two RGBA float images (a vs b). mask optional (1=include), length = w*h. */
void vk_cert_metrics_compare_rgba( const float *a, const float *b, uint32_t w, uint32_t h,
	const uint8_t *mask, certMetrics_t *out );

/*
 * Empty-pixel gate: for pixels with accumWeight≈0 and revealage≈1,
 * resolved must match fog_scene within eps.
 */
void vk_cert_metrics_empty_pixels( const float *fogSceneRgba, const float *accumRgba,
	const float *revealR, const float *resolvedRgba, uint32_t w, uint32_t h,
	float eps, certMetrics_t *out );

/* Revealage product check: gpuReveal[i] vs expected product. */
void vk_cert_metrics_revealage( const float *gpuReveal, const float *expected, uint32_t count,
	certMetrics_t *out );

/* Weight sample stats vs contract bounds. */
void vk_cert_metrics_weights( const float *weights, uint32_t count,
	float minW, float maxW, certMetrics_t *out );

#endif /* USE_VULKAN */
