/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Arc Blanc — real-time ocean framework (Algis et al. 2025, arXiv:2503.03326).
Tessendorf FFT free surface, depth velocity, fluid-solid coupling.
===========================================================================
*/

#pragma once

#include "../../qcommon/q_shared.h"

#define ARC_BLANC_GPU_MAX_CASCADES 3
#define ARC_BLANC_VELOCITY_SAMPLES 8

typedef struct arcBlancComplex_s {
	float re;
	float im;
} arcBlancComplex_t;

typedef struct arcBlancHeightExport_s {
	const float *heights;
	int gridN;
	float tileSize;
	vec3_t origin;
	float minHeight;
	float maxHeight;
} arcBlancHeightExport_t;

typedef struct arcBlancGpuParams_s {
	int gridN;
	int cascadeCount;
	float time;
	float tileLength[ARC_BLANC_GPU_MAX_CASCADES];
	float minHeight;
	float maxHeight;
	const arcBlancComplex_t *h0[ARC_BLANC_GPU_MAX_CASCADES];
	const arcBlancComplex_t *h0conj[ARC_BLANC_GPU_MAX_CASCADES];
	const float *omega[ARC_BLANC_GPU_MAX_CASCADES];
	const float *kMag[ARC_BLANC_GPU_MAX_CASCADES];
	float *outCombinedHeight;
	float *outCombinedDispX;
	float *outCombinedDispZ;
	byte *outRgba;
	int rgbaMaxBytes;
	int *outWidth;
	int *outHeight;
	float depthSamples[ARC_BLANC_VELOCITY_SAMPLES];
	float *outVelocitySlice[ARC_BLANC_VELOCITY_SAMPLES];
	qboolean updateVelocityGpu;
} arcBlancGpuParams_t;

void     ArcBlanc_Init( void );
void     ArcBlanc_Shutdown( void );

qboolean ArcBlanc_Enabled( void );
qboolean ArcBlanc_GpuWanted( void );
void     ArcBlanc_Frame( float dt );

typedef qboolean ( *arcBlancGpuStepFn )( const arcBlancGpuParams_t *params );
void     ArcBlanc_SetGpuStepFn( arcBlancGpuStepFn fn );
void     ArcBlanc_FillGpuParams( arcBlancGpuParams_t *out, byte *rgbaBuf, int rgbaMax,
	int *outW, int *outH );

float    ArcBlanc_SampleHeight( float worldX, float worldZ );
void     ArcBlanc_SampleVelocity( float worldX, float worldY, float worldZ, vec3_t outVel );

int      ArcBlanc_RegisterBoxHull( int physBody, const vec3_t origin, const vec3_t mins, const vec3_t maxs );
void     ArcBlanc_UnregisterHull( int hullId );
void     ArcBlanc_SetHullVelocity( int hullId, const vec3_t velocity );

void     ArcBlanc_GetHeightExport( arcBlancHeightExport_t *out );
void     ArcBlanc_BuildHeightRGBA( byte *rgba, int maxBytes, int *width, int *height );

void     ArcBlanc_Status_f( void );
void     ArcBlanc_Reseed_f( void );
void     ArcBlanc_Sample_f( void );

void     ArcBlanc_ResetForTest( void );
