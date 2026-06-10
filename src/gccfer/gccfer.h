#pragma once

/*
===========================================================================
GCC-FER / CA-FER — culture-aware dynamic facial expression recognition.
Singh et al., arXiv:2606.07063 (GCC-FER dataset + CA-FER framework).
===========================================================================
*/

#include "qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GCCFER_NUM_EXPRESSIONS   7
#define GCCFER_NUM_CULTURES      4
#define GCCFER_NUM_AUS           20
#define GCCFER_AU_STATS          4
#define GCCFER_AU_FEATURE_DIM    ( GCCFER_NUM_AUS * GCCFER_AU_STATS )
#define GCCFER_FRAMES_PER_VIDEO  16
#define GCCFER_INPUT_SIZE        224
#define GCCFER_EMBED_DIM         128
#define GCCFER_LATENT_DIM        768

typedef enum {
	GCCFER_EXPR_ANGRY = 0,
	GCCFER_EXPR_DISGUST,
	GCCFER_EXPR_FEAR,
	GCCFER_EXPR_HAPPY,
	GCCFER_EXPR_NEUTRAL,
	GCCFER_EXPR_SAD,
	GCCFER_EXPR_SURPRISE
} gccfer_expression_t;

typedef enum {
	GCCFER_CULTURE_CAUCASIAN = 0,
	GCCFER_CULTURE_EAST_ASIAN,
	GCCFER_CULTURE_SOUTH_ASIAN,
	GCCFER_CULTURE_AFRICAN,
	GCCFER_CULTURE_UNKNOWN = -1,
	GCCFER_CULTURE_GLOBAL  = -2
} gccfer_culture_t;

typedef enum {
	GCCFER_METHOD_CULTURE_AGNOSTIC = 0,
	GCCFER_METHOD_RANDOM_EMBED,
	GCCFER_METHOD_ONEHOT,
	GCCFER_METHOD_SEPARATE_HEADS,
	GCCFER_METHOD_AU_CONCAT_FIXED,
	GCCFER_METHOD_CAFER
} gccfer_method_t;

typedef struct {
	int angry;
	int disgust;
	int fear;
	int happy;
	int neutral;
	int sad;
	int surprise;
	int total;
} gccfer_expr_counts_t;

typedef struct {
	gccfer_expr_counts_t by_expr;
	float pct_of_total;
} gccfer_culture_row_t;

typedef struct {
	float uar;
	float war;
} gccfer_metrics_t;

typedef struct {
	gccfer_method_t method;
	gccfer_metrics_t metrics;
	const char *label;
} gccfer_benchmark_row_t;

typedef struct {
	float mean[GCCFER_NUM_AUS];
	float stddev[GCCFER_NUM_AUS];
	float maxv[GCCFER_NUM_AUS];
	float freq[GCCFER_NUM_AUS];
} gccfer_au_stats_t;

typedef struct {
	float W_proj[GCCFER_EMBED_DIM * GCCFER_NUM_AUS];
	float Wa[GCCFER_LATENT_DIM * GCCFER_EMBED_DIM];
	float Wb[GCCFER_LATENT_DIM * GCCFER_EMBED_DIM];
	float embeddings[GCCFER_NUM_CULTURES][GCCFER_EMBED_DIM];
	float global_embedding[GCCFER_EMBED_DIM];
	qboolean initialized;
} gccfer_cafer_params_t;

const char *Gccfer_ExpressionName( gccfer_expression_t expr );
const char *Gccfer_CultureName( gccfer_culture_t culture );
const char *Gccfer_MethodName( gccfer_method_t method );

int Gccfer_TotalSamples( void );
const gccfer_culture_row_t *Gccfer_DatasetTable( void );
const gccfer_expr_counts_t *Gccfer_ExpressionTotals( void );
int Gccfer_CountFor( gccfer_culture_t culture, gccfer_expression_t expr );

void Gccfer_AuStatsFromSequence( const float *au_matrix, int num_frames,
	gccfer_au_stats_t *out );
void Gccfer_AuStatsToVector( const gccfer_au_stats_t *stats, float *out80 );

void Gccfer_MinMaxNormalizeProfile( float *profile, int dim,
	const float *mu_min, const float *mu_max );

void Gccfer_ProjectCultureEmbedding( const gccfer_cafer_params_t *params,
	const float *normalized_au20, float *out_embed );

void Gccfer_GenerateAdaptParams( const gccfer_cafer_params_t *params,
	gccfer_culture_t culture, float *out_a, float *out_b );

void Gccfer_AdaptLatent( const float *latent, const float *a, const float *b,
	int dim, float *out );

void Gccfer_CaferInitDefaults( gccfer_cafer_params_t *params, unsigned int seed );
void Gccfer_CaferInitFromProfiles( gccfer_cafer_params_t *params,
	const float culture_profiles[GCCFER_NUM_CULTURES][GCCFER_NUM_AUS] );

float Gccfer_FocalLoss( const float *logits, int num_classes, int target,
	float gamma, float alpha, float label_smoothing );

const gccfer_benchmark_row_t *Gccfer_GccferBenchmarks( int *count );
const gccfer_benchmark_row_t *Gccfer_DfewBenchmarks( int *count );
gccfer_metrics_t Gccfer_LookupPerCulture( gccfer_culture_t culture );

void Gccfer_ModelEvaluate( gccfer_method_t method, gccfer_metrics_t *out );

#ifdef __cplusplus
}
#endif
