/*
===========================================================================
DaX evaluation protocol: 5×4 patient-level CV and statistical ranking (Eq. 1).
===========================================================================
*/

#include "dax/dax.h"

#include <math.h>
#include <string.h>

void Dax_FoldResultInit( dax_fold_result_t *r )
{
	if ( !r ) {
		return;
	}
	memset( r, 0, sizeof( *r ) );
}

void Dax_FoldResultPush( dax_fold_result_t *r, float score )
{
	if ( !r || r->num_folds >= DAX_NUM_CV_FOLDS ) {
		return;
	}
	r->fold_scores[r->num_folds++] = score;
}

float Dax_FoldResultMean( const dax_fold_result_t *r )
{
	int i;
	float sum = 0.0f;

	if ( !r || r->num_folds <= 0 ) {
		return 0.0f;
	}
	for ( i = 0; i < r->num_folds; i++ ) {
		sum += r->fold_scores[i];
	}
	return sum / (float)r->num_folds;
}

int Dax_BuildPatientFolds( int num_patients, int num_folds, int *out_fold_assignments )
{
	int i;

	if ( !out_fold_assignments || num_patients <= 0 || num_folds <= 0 ) {
		return 0;
	}
	/*
	 * Spread patients across folds (deterministic hash). Full benchboard uses
	 * patient-disjoint splits from tools/dax/fixtures/mini_bench/splits.json;
	 * Python evaluate_benchmark.py is canonical for production eval.
	 */
	for ( i = 0; i < num_patients; i++ ) {
		const unsigned h = (unsigned)( i + 1 ) * 2654435761u;
		out_fold_assignments[i] = (int)( h % (unsigned)num_folds );
	}
	return num_folds;
}

static float dax_paired_significance( const dax_fold_result_t *a, const dax_fold_result_t *b )
{
	int n;
	int i;
	float mean_diff = 0.0f;
	float var_diff = 0.0f;
	float t;
	float p;

	if ( !a || !b ) {
		return 1.0f;
	}
	n = a->num_folds < b->num_folds ? a->num_folds : b->num_folds;
	if ( n <= 1 ) {
		return 1.0f;
	}

	for ( i = 0; i < n; i++ ) {
		mean_diff += a->fold_scores[i] - b->fold_scores[i];
	}
	mean_diff /= (float)n;

	for ( i = 0; i < n; i++ ) {
		const float d = ( a->fold_scores[i] - b->fold_scores[i] ) - mean_diff;
		var_diff += d * d;
	}
	if ( var_diff <= 1e-12f ) {
		return mean_diff > 0.0f ? 0.01f : 1.0f;
	}

	t = mean_diff / sqrtf( var_diff / (float)n );
	if ( t > 2.5f ) {
		return 0.02f;
	}
	if ( t > 2.0f ) {
		return 0.05f;
	}
	if ( t > 1.5f ) {
		return 0.15f;
	}
	p = 0.5f;
	return p;
}

int Dax_StatisticalRankScore( const dax_fold_result_t *results, int num_models,
	const dax_fold_result_t *model_results, int model_index, float alpha )
{
	int beaten = 0;
	int j;

	if ( !results || !model_results || model_index < 0 || model_index >= num_models ) {
		return 0;
	}

	{
		const float mean_m = Dax_FoldResultMean( &model_results[model_index] );
		for ( j = 0; j < num_models; j++ ) {
			float p;

			if ( j == model_index ) {
				continue;
			}
			if ( mean_m <= Dax_FoldResultMean( &model_results[j] ) ) {
				continue;
			}
			p = dax_paired_significance( &model_results[model_index], &model_results[j] );
			if ( p < alpha ) {
				beaten++;
			}
		}
	}
	return beaten;
}

float Dax_GramMatrixFrobeniusDiff( const float *a, const float *b, int tokens, int dim )
{
	int i;
	int j;
	int k;
	float sum = 0.0f;

	if ( !a || !b || tokens <= 0 || dim <= 0 ) {
		return 0.0f;
	}

	for ( i = 0; i < tokens; i++ ) {
		for ( j = 0; j < tokens; j++ ) {
			float dot_a = 0.0f;
			float dot_b = 0.0f;
			for ( k = 0; k < dim; k++ ) {
				dot_a += a[i * dim + k] * a[j * dim + k];
				dot_b += b[i * dim + k] * b[j * dim + k];
			}
			{
				const float d = dot_a - dot_b;
				sum += d * d;
			}
		}
	}
	return sqrtf( sum );
}
