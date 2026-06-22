#pragma once

/*
===========================================================================
Deep-layered machines — exact Hamming-weight distribution of global output.

Thomas M. A. Fink, arXiv:2606.11965 (2026); transition matrix A, q(n)=A^(n-1)q(1).
===========================================================================
*/

#include "qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DLM_MAX_K 8

typedef struct {
	int k;
	int depth;
	int ell;
	int dim;
	float *q;
	float *p;
	float endpointProb;
	float interiorStdRel;
} dlm_distribution_t;

int DLM_Ell( int k );
int DLM_NumFunctions( int k );
int DLM_NumNodes( int k, int depth );

double DLM_Binom( int n, int r );
void DLM_TransitionEntry( int k, int i, int j, double *out );
void DLM_BuildTransition( int k, double *A );
void DLM_InitialQ( int k, float *q );

void DLM_EvolveQ( int k, int depth, float *qOut );
void DLM_QToP( int k, const float *q, float *p );

dlm_distribution_t *DLM_ComputeExact( int k, int depth );
void DLM_Free( dlm_distribution_t *dist );

double DLM_Eigenvalue( int k, int j );
float DLM_CriticalDepthEstimate( int k );

int DLM_EnumerateWeightCounts( int k, int depth, int *weightCounts, int maxW );
int DLM_SampleWeightHistogram( int k, int depth, int numSamples, unsigned seed,
	int *weightCounts, int maxW );

void DLM_ConsoleInit( void );
void DLM_ConsoleShutdown( void );

#ifdef __cplusplus
}
#endif
