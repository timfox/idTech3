#pragma once

/*
===========================================================================
How Dark is Dark — black-material reflectance scaffold.

Filip & Vávra, arXiv:2601.05094 (2026). Calibrated summary metrics from
paper Figs. 4–6 and 8 — not measured EXR BRDF evaluation.
===========================================================================
*/

#include "qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HOWDARK_MATERIAL_COUNT 6
#define HOWDARK_THETA_SAMPLES  8

typedef enum {
	HOWDARK_CLASS_ULTRA_BLACK = 0,
	HOWDARK_CLASS_FABRIC,
	HOWDARK_CLASS_COATING
} howdark_class_t;

typedef struct {
	int id;
	const char *name;
	const char *shortName;
	howdark_class_t classId;
	const char *className;
	float albedo;           /* effective albedo A (eq. 2), relative */
	float lumP1;            /* relative BRDF luminance percentiles */
	float lumP50;
	float lumP99;
	float tisMean;          /* mean TIS over sampled θi (Fig. 6) */
} howdark_material_t;

int HowDark_MaterialCount( void );
const howdark_material_t *HowDark_GetMaterial( int id );
int HowDark_FindMaterial( const char *nameOrId );

float HowDark_Albedo( int id );
void HowDark_LuminancePercentiles( int id, float *p1, float *p50, float *p99 );

/* θi in degrees; returns calibrated THR / TIS / Rs for that illumination. */
float HowDark_THR( int id, float thetaI_deg );
float HowDark_TIS( int id, float thetaI_deg );
float HowDark_Specular( int id, float thetaI_deg );

/* Perceived darkness 0–100 (Fig. 8). intensityScale must be 1, 10, or 100. */
float HowDark_PerceivedDarkness( int id, int intensityScale );

/* outIds[HOWDARK_MATERIAL_COUNT] ordered darkest-first. Returns count written. */
int HowDark_RankByDarkness( int intensityScale, int *outIds, int outCap );

/* useCase: optical | calibration | stray | aesthetic (and aliases). */
const char *HowDark_SelectAdvice( const char *useCase );

void HowDark_ConsoleInit( void );
void HowDark_ConsoleShutdown( void );

#ifdef __cplusplus
}
#endif
