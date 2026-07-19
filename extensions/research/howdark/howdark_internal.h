#pragma once

/*
===========================================================================
Internal tables for How Dark is Dark scaffold (arXiv:2601.05094).
===========================================================================
*/

#include "howdark/howdark.h"

/* Illumination polar samples matching paper §3.3. */
extern const float howdark_theta_i[HOWDARK_THETA_SAMPLES];

typedef struct {
	float thr[HOWDARK_THETA_SAMPLES];
	float tis[HOWDARK_THETA_SAMPLES];
	float rs[HOWDARK_THETA_SAMPLES];
	float darkness[3]; /* intensity 1, 10, 100 */
} howdark_curves_t;

extern const howdark_material_t howdark_materials[HOWDARK_MATERIAL_COUNT];
extern const howdark_curves_t howdark_curves[HOWDARK_MATERIAL_COUNT];

float HowDark_InterpTheta( const float *samples, float thetaI_deg );
