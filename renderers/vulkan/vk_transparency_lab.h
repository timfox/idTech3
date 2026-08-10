#pragma once

/*
 * Color Pipeline Phase 2.6 — frozen transparency laboratory / reference compare.
 * Sorted-alpha reference is certification-only, not a production WBOIT replacement.
 */


#include "../common/tr_types.h"

typedef enum {
	TRANSPARENCY_REF_DISABLED = 0,
	TRANSPARENCY_REF_SORTED_ALPHA = 1,
	TRANSPARENCY_REF_WBOIT_VS_SORTED = 2,
	TRANSPARENCY_REF_MBOIT_VS_SORTED = 3,
	TRANSPARENCY_REF_SPECIALIZED = 4
} transparencyReferenceMode_t;

typedef struct {
	float meanAbsRgb;
	float maxAbsRgb;
	float relativeLuminance;
	float hueError;
	float edgeOnlyError;
	float permutationVariance;
	uint32_t invalidPixelCount;
} transparencyCompareMetrics_t;

void vk_transparency_lab_register( void );
void vk_transparency_lab_begin_frame( void );

qboolean vk_transparency_lab_frozen( void );
transparencyReferenceMode_t vk_transparency_lab_mode( void );
const transparencyCompareMetrics_t *vk_transparency_lab_last_metrics( void );

/* CPU / unit helpers for lab metrics. */
float vk_transparency_lab_relative_luminance( float r, float g, float b );
float vk_transparency_lab_hue_error( float r0, float g0, float b0, float r1, float g1, float b1 );
float vk_transparency_lab_fresnel_schlick( float cosTheta, float f0 );
void vk_transparency_lab_beer_lambert( const float color[3], float distance, float absorptionDistance,
	float outTransmittance[3] );
float vk_transparency_lab_refraction_offset_bound( float offsetPx, float maxOffsetPx );

