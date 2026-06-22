#pragma once

/*
===========================================================================
Domany–Kinzel quasi-stationary distribution (bond directed-percolation line).

Lee, Harada, Kawashima — arXiv:2606.11885 (2026); MPS power iteration on
projected transfer matrix; bipartite mutual information / flock diagnostics.
===========================================================================
*/

#include "qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DK_QSD_CHI_MAX_DEFAULT 120

typedef enum {
	DK_QSD_METHOD_DENSE = 0,
	DK_QSD_METHOD_MPS
} dk_qsd_method_t;

typedef struct dk_qsd_state_s dk_qsd_state_t;

typedef struct {
	float meanActive;
	float r11;
	float halfChainMI;
	float flockExtentMean;
	float flockFillMean;
	float singleClusterFrac;
	float lambda1;
} dk_qsd_observables_t;

float DK_Qsd_Pc( void );
float DK_Qsd_P2( float p );

dk_qsd_state_t *DK_Qsd_Solve( int N, float p, dk_qsd_method_t method, int chiMax,
	int maxIter, float tol );
void DK_Qsd_Free( dk_qsd_state_t *state );

float DK_Qsd_Lambda1( const dk_qsd_state_t *state );
int DK_Qsd_ChainLength( const dk_qsd_state_t *state );
float DK_Qsd_ParamP( const dk_qsd_state_t *state );
qboolean DK_Qsd_Converged( const dk_qsd_state_t *state );

void DK_Qsd_ComputeObservables( const dk_qsd_state_t *state, int numSamples, unsigned seed,
	dk_qsd_observables_t *obs );

int DK_Qsd_SampleConfig( const dk_qsd_state_t *state, byte *sites, int N, unsigned *rng );

float DK_Qsd_BinaryEntropy( float x );
float DK_Qsd_BondMI_UniformFlock( float cutFraction, int N, float kEff );

void DK_Qsd_ConsoleInit( void );
void DK_Qsd_ConsoleShutdown( void );

#ifdef __cplusplus
}
#endif
