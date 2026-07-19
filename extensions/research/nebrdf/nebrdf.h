#pragma once

/*
===========================================================================
Neural Enhancement of Analytical Appearance Models scaffold.

Shen, Ma, Zhou & Wu, arXiv:2604.24081 (2026). Paper graph / MLP sizes /
hypercube search / fit metrics — not trained weights or GLSL MLP eval.
===========================================================================
*/

#ifdef __cplusplus
extern "C" {
#endif

#define NEBRDF_NODE_COUNT           11
#define NEBRDF_ENHANCE_ORDER_LEN    4
#define NEBRDF_COMPARE_COUNT        5

typedef enum {
	NEBRDF_KIND_NODE = 0,
	NEBRDF_KIND_OP
} nebrdf_kind_t;

typedef struct {
	int id;
	const char *symbol;     /* M, S, D, F, G, 1/E, +, * */
	const char *name;
	nebrdf_kind_t kind;
	int neuralInFinal;      /* 1 if hat in final enhanced GGX */
} nebrdf_node_t;

typedef struct {
	int analyticalParams;   /* 12 */
	int neuralParams;       /* 27 */
	int totalParams;        /* 39 */
	int weightApprox;       /* ~7000 */
	float sizeKB;           /* 26.45 */
	int mlpH0;              /* 16 */
	int mlpH1;              /* 32 */
	int mlpH2;              /* 16 */
	int mlpLayers;          /* 4 FC */
} nebrdf_param_counts_t;

typedef struct {
	const char *materialName;
	float enhancedSsim;
	float enhancedDeItp;    /* ×10^3 as reported */
	float ggxSsim;
	float ggxDeItp;
} nebrdf_compare_row_t;

int NeBrdf_NodeCount( void );
const nebrdf_node_t *NeBrdf_GetNode( int id );

/* Bit i set => node i is neural in final state. */
unsigned int NeBrdf_FinalStateMask( void );
int NeBrdf_IsNeural( int nodeId );

/* Enhancement order Fig. 1: E, G, mul(F×G), F — returns count written. */
int NeBrdf_EnhancementOrder( int *outNodeIds, int outCap );

void NeBrdf_ParamCounts( nebrdf_param_counts_t *out );

/* Hamming distance ≤ maxHamming from a state; for maxHamming==1 returns N+1. */
int NeBrdf_HypercubeNeighbors( int nodeCount, int maxHamming );

float NeBrdf_FitTimeSec( int enhanced );           /* 1=enhanced, 0=vanilla GGX */
float NeBrdf_RenderRaysPerSec( int enhanced );     /* ×10^6 as paper float */

int NeBrdf_CompareCount( void );
const nebrdf_compare_row_t *NeBrdf_CompareRow( int id );

/* useCase: fit | render | edit | limit */
const char *NeBrdf_SelectAdvice( const char *useCase );

const char *NeBrdf_FinalFormula( void );
int NeBrdf_EpochsBetweenStateChanges( void );

void NeBrdf_ConsoleInit( void );
void NeBrdf_ConsoleShutdown( void );

#ifdef __cplusplus
}
#endif
