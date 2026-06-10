#pragma once

/*
===========================================================================
DaX (大象) — general pathology representations across scales.
Zhao et al., arXiv:2606.06983 (DINOv3-style ViT-L + benchboard evaluation).
===========================================================================
*/

#include "qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DAX_NUM_BENCHMARK_TASKS   161
#define DAX_NUM_BENCHMARK_DATASETS 44
#define DAX_NUM_LEVEL1_DOMAINS    4
#define DAX_NUM_LEVEL2_CATEGORIES 9
#define DAX_NUM_CV_FOLDS          20
#define DAX_NUM_ANCHOR_MAGS       4
#define DAX_EMBED_DIM             1024
#define DAX_PATCH_PX              1920
#define DAX_MPP_AT_20X            0.5f
#define DAX_OVERLAP_PX            640
#define DAX_TISSUE_RATIO_MIN      0.6666667f

typedef enum {
	DAX_DOMAIN_DIAGNOSTIC = 0,
	DAX_DOMAIN_BIOMARKER,
	DAX_DOMAIN_SPECIMEN,
	DAX_DOMAIN_PROGNOSIS
} dax_domain_t;

typedef enum {
	DAX_CAT_DISEASE_ENTITY = 0,
	DAX_CAT_HISTOLOGIC_GRADING,
	DAX_CAT_ANATOMIC_STAGING,
	DAX_CAT_GENOMIC_ALTERATIONS,
	DAX_CAT_IHC_RECEPTOR,
	DAX_CAT_COMPOSITE_IMMUNE,
	DAX_CAT_TISSUE_ORIGIN,
	DAX_CAT_TREATMENT_RESPONSE,
	DAX_CAT_SURVIVAL_OUTCOMES
} dax_category_t;

typedef enum {
	DAX_MODEL_DAX = 0,
	DAX_MODEL_DAX_BASE,
	DAX_MODEL_H_OPTIMUS_1,
	DAX_MODEL_UNI2,
	DAX_MODEL_VIRCHOW2,
	DAX_MODEL_UNI,
	DAX_MODEL_CONCH,
	DAX_MODEL_GPFM,
	DAX_MODEL_MUSK,
	DAX_MODEL_DINOV3_VITL,
	DAX_MODEL_RESNET50,
	DAX_MODEL_COUNT
} dax_model_id_t;

typedef enum {
	DAX_AGG_MEAN = 0,
	DAX_AGG_ABMIL
} dax_aggregation_t;

typedef struct {
	int num_tasks;
	int num_datasets;
	int num_patients;
	int num_slides;
	int pretrain_wsis;
} dax_benchmark_stats_t;

typedef struct {
	dax_domain_t domain;
	dax_category_t category;
	int task_count;
} dax_category_row_t;

typedef struct {
	dax_model_id_t id;
	const char *name;
	const char *architecture;
	int params_m;
	int pretrain_wsis;
	float mean_benchmark_score;
} dax_model_row_t;

typedef struct {
	float fold_scores[DAX_NUM_CV_FOLDS];
	int num_folds;
	float mean;
} dax_fold_result_t;

const char *Dax_DomainName( dax_domain_t domain );
const char *Dax_CategoryName( dax_category_t cat );
const char *Dax_ModelName( dax_model_id_t id );

dax_benchmark_stats_t Dax_BenchmarkStats( void );
const dax_category_row_t *Dax_CategoryTable( int *count );
const dax_model_row_t *Dax_ModelTable( int *count );

float Dax_AnchorMagnification( int index );
void Dax_Stage2CropPair( int index, int *global_px, int *local_px );

int Dax_BuildPatientFolds( int num_patients, int num_folds, int *out_fold_assignments );
int Dax_StatisticalRankScore( const dax_fold_result_t *results, int num_models,
	const dax_fold_result_t *model_results, int model_index, float alpha );

void Dax_FoldResultInit( dax_fold_result_t *r );
void Dax_FoldResultPush( dax_fold_result_t *r, float score );
float Dax_FoldResultMean( const dax_fold_result_t *r );

float Dax_GramMatrixFrobeniusDiff( const float *a, const float *b, int tokens, int dim );

void Dax_ModelLookup( dax_model_id_t id, dax_model_row_t *out );

#ifdef __cplusplus
}
#endif
