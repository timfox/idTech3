/*
 * Arc Blanc internal types (Algis et al. 2025, arXiv:2503.03326).
 */
#pragma once

#include "../../qcommon/q_shared.h"

#define AB_GRAVITY           9.80665f
#define AB_CASCADE_COUNT     3
#define AB_DEFAULT_GRID_N    128
#define AB_MAX_GRID_N        256
#define AB_HEIGHT_ITER       4
#define AB_VELOCITY_SAMPLES  8
#define AB_MAX_HULLS         8
#define AB_FDM_GRID          128
#define AB_FDM_MARGIN        16
#define AB_MAX_WATERLINE     64

typedef struct abComplex_s {
	float re;
	float im;
} abComplex_t;

typedef struct abCascadeParams_s {
	float length;
	float kMin;
	float kMax;
} abCascadeParams_t;

typedef struct abSpectrumState_s {
	abComplex_t h0[AB_MAX_GRID_N * AB_MAX_GRID_N];
	abComplex_t h0conj[AB_MAX_GRID_N * AB_MAX_GRID_N];
	float omega[AB_MAX_GRID_N * AB_MAX_GRID_N];
	float kMag[AB_MAX_GRID_N * AB_MAX_GRID_N];
	qboolean valid;
} abSpectrumState_t;

typedef struct abCascadeField_s {
	abSpectrumState_t spec;
	float height[AB_MAX_GRID_N * AB_MAX_GRID_N];
	float dispX[AB_MAX_GRID_N * AB_MAX_GRID_N];
	float dispZ[AB_MAX_GRID_N * AB_MAX_GRID_N];
	float gradHx[AB_MAX_GRID_N * AB_MAX_GRID_N];
	float gradHz[AB_MAX_GRID_N * AB_MAX_GRID_N];
} abCascadeField_t;

typedef struct abOceanState_s {
	int gridN;
	float tileSize;
	float time;
	float windSpeed;
	float fetch;
	float windDirRad;
	float swell;
	float directional;
	float amplitudeScale;
	float heightScale;
	float chopScale;
	float waveSpeed;
	float spread;
	float gustStrength;
	float gustSpeed;
	abCascadeParams_t cascades[AB_CASCADE_COUNT];
	abCascadeField_t fields[AB_CASCADE_COUNT];
	float combinedHeight[AB_MAX_GRID_N * AB_MAX_GRID_N];
	float combinedDispX[AB_MAX_GRID_N * AB_MAX_GRID_N];
	float combinedDispZ[AB_MAX_GRID_N * AB_MAX_GRID_N];
	float velocitySlices[AB_VELOCITY_SAMPLES][AB_MAX_GRID_N * AB_MAX_GRID_N * 3];
	float depthSamples[AB_VELOCITY_SAMPLES];
	qboolean spectrumDirty;
} abOceanState_t;

typedef struct abVec3_s {
	float v[3];
} abVec3_t;

typedef struct abTriangle_s {
	abVec3_t v0, v1, v2;
	abVec3_t normal;
	float area;
	float submergedArea;
	float centroidDepth;
} abTriangle_t;

typedef struct abHull_s {
	qboolean active;
	int physBody;
	vec3_t origin;
	vec3_t fdmPrevOrigin;
	vec3_t fdmPrev2Origin;
	vec3_t velocity;
	vec3_t angles;
	vec3_t boundsMin;
	vec3_t boundsMax;
	float hullHeight;
	float hullVolume;
	float submergedVolume;
	vec3_t immersionCenter;
	vec3_t forceWater;
	vec3_t forceAir;
	vec3_t forceBuoyancy;
	abTriangle_t *triangles;
	int triangleCount;
	float fdmGrid[AB_FDM_GRID * AB_FDM_GRID];
	float fdmPrev[AB_FDM_GRID * AB_FDM_GRID];
	float fdmMask[AB_FDM_GRID * AB_FDM_GRID];
	float fdmZoneSize;
	float cdWater;
	float cdAir;
	float maskBw;
	abVec3_t waterline[AB_MAX_WATERLINE];
	int waterlineCount;
} abHull_t;

void     AB_FFT_Complex1D( abComplex_t *data, int n, qboolean inverse );
void     AB_FFT_IFFT2D( abComplex_t *grid, int n );
void     AB_FFT_IFFT2D_HermitianPair( abComplex_t *freqA, abComplex_t *freqB, float *outA, float *outB, int n );
void     AB_FFT_ApplyCheckerboard( float *realGrid, int n );

void     AB_Spectrum_TimeHt( const abSpectrumState_t *spec, int n, float tileLength, float t,
	abComplex_t *outH, abComplex_t *outDx, abComplex_t *outDz );
void     AB_Spectrum_TimeVelocityDrive( const abSpectrumState_t *spec, int n, float tileLength, float t,
	int idx, abComplex_t *outDrive );

void     AB_Spectrum_Seed( unsigned int seed );
float    AB_Spectrum_JONSWAP( float omega, float windSpeed, float fetch );
float    AB_Spectrum_Directional( float omega, float theta, float windDir, float swell, float directional,
	float spread );
void     AB_Spectrum_GenerateH0( abSpectrumState_t *spec, int n, float tileLength,
	float windSpeed, float fetch, float windDirRad, float swell, float directional,
	float spread, float kMin, float kMax );
int      AB_Spectrum_NegKIndex( int n, int ix, int iz );
float    AB_Spectrum_WaterDensity( float depthY );

void     AB_Ocean_InitDefaults( abOceanState_t *ocean, int gridN, float tileSize );
void     AB_Ocean_UpdateSpectrum( abOceanState_t *ocean );
void     AB_Ocean_UpdateTime( abOceanState_t *ocean, float dt );
void     AB_Ocean_CombineCascades( abOceanState_t *ocean );
void     AB_Ocean_FillDepthSamples( float *out, int count );
void     AB_Ocean_UpdateVelocitySlices( abOceanState_t *ocean );
float    AB_Ocean_MaxHeightGridErrorHermitian( const abOceanState_t *ocean, int cascadeIndex );

float    AB_Ocean_SampleHeightTile( const abOceanState_t *ocean, float localX, float localZ );
float    AB_Ocean_SampleHeightWorld( const abOceanState_t *ocean, float worldX, float worldZ );
void     AB_Ocean_SampleVelocityWorld( const abOceanState_t *ocean, float worldX, float worldY, float worldZ, vec3_t outVel );

void     AB_Coupling_UpdateHullGeometry( abHull_t *hull, const abOceanState_t *ocean );
void     AB_Coupling_ComputeForces( abHull_t *hull, const abOceanState_t *ocean );
void     AB_Coupling_StepFDM( abHull_t *hull, const abOceanState_t *ocean, float dt );
void     AB_Coupling_BuildMask( abHull_t *hull, const abOceanState_t *ocean );
float    AB_Coupling_SampleWakeHeight( const abHull_t *hull, float worldX, float worldZ );
void     AB_Coupling_ApplyWakesToHeightGrid( abOceanState_t *ocean, const abHull_t *hulls, int hullCount,
	float wakeScale );

#ifdef ARC_BLANC_UNIT_TEST
const abOceanState_t *ArcBlanc_GetOceanForTest( void );
#endif
