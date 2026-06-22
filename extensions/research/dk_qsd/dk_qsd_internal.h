#pragma once

#include "dk_qsd/dk_qsd.h"

#include <stddef.h>

#define DK_QSD_PC 0.644700185f
#define DK_QSD_EPS_CUT 1e-15f
#define DK_QSD_DENSE_MAX_N 14

typedef struct dk_mps_site_s {
	float *t[2];
	int chiL;
	int chiR;
} dk_mps_site_t;

typedef struct dk_mps_s {
	int N;
	int chiMax;
	dk_mps_site_t *sites;
} dk_mps_t;

struct dk_qsd_state_s {
	dk_qsd_method_t method;
	int N;
	float p;
	float lambda1;
	int iterations;
	float overlap;
	qboolean converged;

	float *prob;
	dk_mps_t mps;
};

void DK_Kernels_Fill( float p, float W[2][2][2], float V[2][2] );
float DK_Kernels_W( float p, int s0, int s1, int out );
float DK_Kernels_V( float p, int s, int out );

int DK_Dense_ConfigIndex( const byte *x, int N );
void DK_Dense_ConfigFromIndex( int idx, int N, byte *x );
int DK_Dense_StateCount( int N );

void DK_Dense_BuildTransfer( int N, float p, float *T );
void DK_Dense_PowerIterate( int N, float p, float *prob, int maxIter, float tol,
	float *outLambda, int *outIter, float *outOverlap, qboolean *outConverged );

void DK_Mps_Init( dk_mps_t *mps, int N, int chiMax );
void DK_Mps_Free( dk_mps_t *mps );
void DK_Mps_InitRandomNonAbsorbing( dk_mps_t *mps, unsigned seed );

float DK_Mps_FlatNorm( const dk_mps_t *mps );
float DK_Mps_AmplitudeAllZero( const dk_mps_t *mps );
void DK_Mps_ProjectAbsorbing( dk_mps_t *mps );
void DK_Mps_NormalizeFlat( dk_mps_t *mps );
void DK_Mps_ApplyTransfer( dk_mps_t *mps, float p, int chiMax, float epsCut );

void DK_Mps_PowerIterate( int N, float p, int chiMax, float epsCut, int maxIter, float tol,
	dk_mps_t *mps, float *outLambda, int *outIter, float *outOverlap, qboolean *outConverged );

int DK_Mps_Sample( const dk_mps_t *mps, byte *x, unsigned *rng );

void DK_Obs_FromProb( int N, const float *prob, dk_qsd_observables_t *obs );
void DK_Obs_FromMps( const dk_mps_t *mps, int numSamples, unsigned seed, dk_qsd_observables_t *obs );

float DK_Entropy_Bits( const float *p, int n );
float DK_Entropy_BinomialHalf( int k );
