/*
===========================================================================
Deep-layered machine evaluation and exhaustive / Monte Carlo sampling.
===========================================================================
*/

#include "dlm/dlm_internal.h"

#include "qcommon/qcommon.h"

#include <string.h>

dlm_truth_t DLM_FunctionApply( dlm_truth_t f, dlm_truth_t inputs, int k )
{
	const dlm_truth_t mask = ( k >= 32 ) ? 0xffffffffu : ( ( 1u << k ) - 1u );
	return ( f >> ( inputs & mask ) ) & 1u;
}

static dlm_truth_t DLM_PackLayer( const dlm_truth_t *layer, int k )
{
	dlm_truth_t pack = 0;
	int i;
	for ( i = 0; i < k; i++ ) {
		if ( layer[i] ) {
			pack |= ( 1u << i );
		}
	}
	return pack;
}

void DLM_EvalNetwork( int k, int depth, const dlm_truth_t *nodeFuncs,
	dlm_truth_t *outTruth )
{
	const int ell = DLM_Ell( k );
	dlm_truth_t layer[16];
	int assign;

	if ( !nodeFuncs || !outTruth || depth < 1 || k < 1 || k > 16 || ell <= 0 ) {
		return;
	}

	*outTruth = 0;

	for ( assign = 0; assign < ell; assign++ ) {
		int idx = 0;
		int lvl;
		int bit;
		dlm_truth_t F;

		if ( depth == 1 ) {
			F = DLM_FunctionApply( nodeFuncs[idx], (dlm_truth_t)assign, k );
		} else {
			for ( bit = 0; bit < k; bit++ ) {
				layer[bit] = DLM_FunctionApply( nodeFuncs[idx++], (dlm_truth_t)assign, k );
			}

			for ( lvl = 2; lvl < depth; lvl++ ) {
				const dlm_truth_t pack = DLM_PackLayer( layer, k );
				for ( bit = 0; bit < k; bit++ ) {
					layer[bit] = DLM_FunctionApply( nodeFuncs[idx++], pack, k );
				}
			}

			F = DLM_FunctionApply( nodeFuncs[idx], DLM_PackLayer( layer, k ), k );
		}

		if ( F ) {
			*outTruth |= ( 1u << assign );
		}
	}
}

static void DLM_AssignFromIndex( int k, int depth, int configIdx, dlm_truth_t *funcs )
{
	const int numF = DLM_NumFunctions( k );
	const int numNodes = DLM_NumNodes( k, depth );
	int n;

	if ( numF <= 0 || numNodes <= 0 ) {
		return;
	}

	for ( n = 0; n < numNodes; n++ ) {
		funcs[n] = (dlm_truth_t)( configIdx % numF );
		configIdx /= numF;
	}
}

int DLM_EnumerateWeightCounts( int k, int depth, int *weightCounts, int maxW )
{
	const int ell = DLM_Ell( k );
	const int numF = DLM_NumFunctions( k );
	const int numNodes = DLM_NumNodes( k, depth );
	int maxConfigs = 1;
	int cfg;
	dlm_truth_t *funcs;

	if ( !weightCounts || maxW < ell + 1 || k < 1 || depth < 1 || ell <= 0 || numF <= 0 ) {
		return 0;
	}

	{
		int i;
		for ( i = 0; i < numNodes; i++ ) {
			if ( maxConfigs > ( 0x7fffffff / numF ) ) {
				return -1;
			}
			maxConfigs *= numF;
		}
	}

	memset( weightCounts, 0, sizeof( int ) * (size_t)( ell + 1 ) );
	funcs = (dlm_truth_t *)Z_Malloc( sizeof( dlm_truth_t ) * (size_t)numNodes );

	for ( cfg = 0; cfg < maxConfigs; cfg++ ) {
		dlm_truth_t truth = 0;
		int w;

		DLM_AssignFromIndex( k, depth, cfg, funcs );
		DLM_EvalNetwork( k, depth, funcs, &truth );
		w = DLM_HammingWeight( truth, ell );
		weightCounts[w]++;
	}

	Z_Free( funcs );
	return maxConfigs;
}

int DLM_SampleWeightHistogram( int k, int depth, int numSamples, unsigned seed,
	int *weightCounts, int maxW )
{
	const int ell = DLM_Ell( k );
	const int numF = DLM_NumFunctions( k );
	const int numNodes = DLM_NumNodes( k, depth );
	dlm_truth_t *funcs;
	unsigned rng = seed ? seed : 1u;
	int s;

	if ( !weightCounts || maxW < ell + 1 || numSamples <= 0 || numF <= 0 ) {
		return 0;
	}

	memset( weightCounts, 0, sizeof( int ) * (size_t)( ell + 1 ) );
	funcs = (dlm_truth_t *)Z_Malloc( sizeof( dlm_truth_t ) * (size_t)numNodes );

	for ( s = 0; s < numSamples; s++ ) {
		int n;
		dlm_truth_t truth = 0;

		for ( n = 0; n < numNodes; n++ ) {
			rng = rng * 1664525u + 1013904223u;
			funcs[n] = (dlm_truth_t)( rng % (unsigned)numF );
		}
		DLM_EvalNetwork( k, depth, funcs, &truth );
		weightCounts[DLM_HammingWeight( truth, ell )]++;
	}

	Z_Free( funcs );
	return numSamples;
}
