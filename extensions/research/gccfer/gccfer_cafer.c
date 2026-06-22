/*
===========================================================================
CA-FER culture-aware latent adaptation (Algorithm 1, Eqs. 2–15).
===========================================================================
*/

#include "gccfer/gccfer.h"

#include <math.h>
#include <string.h>

static unsigned int gccfer_lcg( unsigned int *state )
{
	*state = *state * 1664525u + 1013904223u;
	return *state;
}

static float gccfer_rand_unit( unsigned int *state )
{
	return (float)( gccfer_lcg( state ) & 0xFFFFu ) / 65535.0f - 0.5f;
}

void Gccfer_AuStatsFromSequence( const float *au_matrix, int num_frames,
	gccfer_au_stats_t *out )
{
	int a;
	int t;
	const int T = num_frames > 0 ? num_frames : GCCFER_FRAMES_PER_VIDEO;
	const float thresh = 0.1f;

	if ( !au_matrix || !out ) {
		return;
	}

	memset( out, 0, sizeof( *out ) );

	for ( a = 0; a < GCCFER_NUM_AUS; a++ ) {
		float sum = 0.0f;
		float sumsq = 0.0f;
		float maxv = 0.0f;
		int active = 0;

		for ( t = 0; t < T; t++ ) {
			const float v = au_matrix[t * GCCFER_NUM_AUS + a];
			sum += v;
			sumsq += v * v;
			if ( v > maxv ) {
				maxv = v;
			}
			if ( v >= thresh ) {
				active++;
			}
		}

		out->mean[a] = sum / (float)T;
		{
			const float var = sumsq / (float)T - out->mean[a] * out->mean[a];
			out->stddev[a] = var > 0.0f ? sqrtf( var ) : 0.0f;
		}
		out->maxv[a] = maxv;
		out->freq[a] = (float)active / (float)T;
	}
}

void Gccfer_AuStatsToVector( const gccfer_au_stats_t *stats, float *out80 )
{
	if ( !stats || !out80 ) {
		return;
	}
	memcpy( out80, stats->mean, sizeof( stats->mean ) );
	memcpy( out80 + GCCFER_NUM_AUS, stats->stddev, sizeof( stats->stddev ) );
	memcpy( out80 + 2 * GCCFER_NUM_AUS, stats->maxv, sizeof( stats->maxv ) );
	memcpy( out80 + 3 * GCCFER_NUM_AUS, stats->freq, sizeof( stats->freq ) );
}

void Gccfer_MinMaxNormalizeProfile( float *profile, int dim,
	const float *mu_min, const float *mu_max )
{
	int i;

	if ( !profile || dim <= 0 ) {
		return;
	}

	for ( i = 0; i < dim; i++ ) {
		const float denom = mu_max[i] - mu_min[i];
		if ( denom > 1e-8f ) {
			profile[i] = ( profile[i] - mu_min[i] ) / denom;
		} else {
			profile[i] = 0.0f;
		}
	}
}

void Gccfer_ProjectCultureEmbedding( const gccfer_cafer_params_t *params,
	const float *normalized_au20, float *out_embed )
{
	int i;
	int j;

	if ( !params || !normalized_au20 || !out_embed ) {
		return;
	}

	for ( i = 0; i < GCCFER_EMBED_DIM; i++ ) {
		float sum = 0.0f;
		for ( j = 0; j < GCCFER_NUM_AUS; j++ ) {
			sum += params->W_proj[i * GCCFER_NUM_AUS + j] * normalized_au20[j];
		}
		out_embed[i] = sum;
	}
}

void Gccfer_GenerateAdaptParams( const gccfer_cafer_params_t *params,
	gccfer_culture_t culture, float *out_a, float *out_b )
{
	const float *ec;
	int i;
	int j;

	if ( !params || !out_a || !out_b ) {
		return;
	}

	if ( culture == GCCFER_CULTURE_GLOBAL || culture == GCCFER_CULTURE_UNKNOWN ) {
		ec = params->global_embedding;
	} else if ( culture >= 0 && culture < GCCFER_NUM_CULTURES ) {
		ec = params->embeddings[culture];
	} else {
		ec = params->global_embedding;
	}

	for ( i = 0; i < GCCFER_LATENT_DIM; i++ ) {
		float sa = 0.0f;
		float sb = 0.0f;
		for ( j = 0; j < GCCFER_EMBED_DIM; j++ ) {
			sa += params->Wa[i * GCCFER_EMBED_DIM + j] * ec[j];
			sb += params->Wb[i * GCCFER_EMBED_DIM + j] * ec[j];
		}
		out_a[i] = sa;
		out_b[i] = sb;
	}
}

void Gccfer_AdaptLatent( const float *latent, const float *a, const float *b,
	int dim, float *out )
{
	int i;
	const int n = dim > 0 ? dim : GCCFER_LATENT_DIM;

	if ( !latent || !a || !b || !out ) {
		return;
	}

	for ( i = 0; i < n; i++ ) {
		out[i] = a[i] * latent[i] + b[i];
	}
}

static void Gccfer_InitMatrixSmall( float *m, int rows, int cols, unsigned int *seed, float scale )
{
	int i;
	const int n = rows * cols;
	for ( i = 0; i < n; i++ ) {
		m[i] = gccfer_rand_unit( seed ) * scale;
	}
}

void Gccfer_CaferInitDefaults( gccfer_cafer_params_t *params, unsigned int seed )
{
	unsigned int rng = seed ? seed : 1u;
	int c;
	int i;

	if ( !params ) {
		return;
	}

	memset( params, 0, sizeof( *params ) );
	Gccfer_InitMatrixSmall( params->W_proj, GCCFER_EMBED_DIM, GCCFER_NUM_AUS, &rng, 0.05f );
	Gccfer_InitMatrixSmall( params->Wa, GCCFER_LATENT_DIM, GCCFER_EMBED_DIM, &rng, 0.02f );
	Gccfer_InitMatrixSmall( params->Wb, GCCFER_LATENT_DIM, GCCFER_EMBED_DIM, &rng, 0.01f );

	for ( c = 0; c < GCCFER_NUM_CULTURES; c++ ) {
		for ( i = 0; i < GCCFER_EMBED_DIM; i++ ) {
			params->embeddings[c][i] = gccfer_rand_unit( &rng ) * 0.1f;
		}
	}
	for ( i = 0; i < GCCFER_EMBED_DIM; i++ ) {
		params->global_embedding[i] =
			( params->embeddings[0][i] + params->embeddings[1][i] +
			  params->embeddings[2][i] + params->embeddings[3][i] ) * 0.25f;
	}
	params->initialized = qtrue;
}

void Gccfer_CaferInitFromProfiles( gccfer_cafer_params_t *params,
	const float culture_profiles[GCCFER_NUM_CULTURES][GCCFER_NUM_AUS] )
{
	float mu_min[GCCFER_NUM_AUS];
	float mu_max[GCCFER_NUM_AUS];
	float normalized[GCCFER_NUM_AUS];
	int a;
	int c;

	if ( !params || !culture_profiles ) {
		return;
	}

	Gccfer_CaferInitDefaults( params, 42u );

	for ( a = 0; a < GCCFER_NUM_AUS; a++ ) {
		mu_min[a] = culture_profiles[0][a];
		mu_max[a] = culture_profiles[0][a];
		for ( c = 1; c < GCCFER_NUM_CULTURES; c++ ) {
			if ( culture_profiles[c][a] < mu_min[a] ) {
				mu_min[a] = culture_profiles[c][a];
			}
			if ( culture_profiles[c][a] > mu_max[a] ) {
				mu_max[a] = culture_profiles[c][a];
			}
		}
	}

	for ( c = 0; c < GCCFER_NUM_CULTURES; c++ ) {
		memcpy( normalized, culture_profiles[c], sizeof( normalized ) );
		Gccfer_MinMaxNormalizeProfile( normalized, GCCFER_NUM_AUS, mu_min, mu_max );
		Gccfer_ProjectCultureEmbedding( params, normalized, params->embeddings[c] );
	}

	for ( a = 0; a < GCCFER_EMBED_DIM; a++ ) {
		params->global_embedding[a] =
			( params->embeddings[0][a] + params->embeddings[1][a] +
			  params->embeddings[2][a] + params->embeddings[3][a] ) * 0.25f;
	}
	params->initialized = qtrue;
}

float Gccfer_FocalLoss( const float *logits, int num_classes, int target,
	float gamma, float alpha, float label_smoothing )
{
	float max_logit = logits[0];
	float sum_exp = 0.0f;
	float log_prob;
	float pt;
	float ce;
	int i;

	if ( !logits || num_classes <= 0 || target < 0 || target >= num_classes ) {
		return 0.0f;
	}

	for ( i = 1; i < num_classes; i++ ) {
		if ( logits[i] > max_logit ) {
			max_logit = logits[i];
		}
	}
	for ( i = 0; i < num_classes; i++ ) {
		sum_exp += expf( logits[i] - max_logit );
	}
	log_prob = logits[target] - max_logit - logf( sum_exp );
	pt = expf( log_prob );
	ce = -log_prob;
	if ( label_smoothing > 0.0f ) {
		ce = ce * ( 1.0f - label_smoothing ) + label_smoothing * (float)num_classes / (float)num_classes;
	}
	return alpha * powf( 1.0f - pt, gamma ) * ce;
}
