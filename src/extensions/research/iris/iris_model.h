#pragma once

#include "qcommon/q_shared.h"

/* Iris Core analytical model — Landvater & Balis, J Pathol Inform 16 (2025) 100414 */

typedef enum {
	IRIS_LAYER_LR = 0,
	IRIS_LAYER_HR
} iris_layer_t;

typedef enum {
	IRIS_DECODER_CODEC = 0,
	IRIS_DECODER_OPENSELIDE
} iris_decoder_t;

typedef struct {
	float median_ms;
	float p25_ms;
	float p75_ms;
} iris_metric_t;

typedef struct {
	iris_layer_t layer;
	iris_decoder_t decoder;
	iris_metric_t te_fov;
	iris_metric_t tpt_ms;
} iris_perf_row_t;

typedef struct {
	const char *name;
	float tfov_ms;
} iris_literature_row_t;

typedef struct {
	iris_decoder_t decoder;
	iris_metric_t lr_te_fov;
	iris_metric_t hr_te_fov;
	iris_metric_t lr_tpt;
	iris_metric_t hr_tpt;
	float buffer_rate_gib_s;
	float fps_median;
} iris_model_result_t;

#define IRIS_TILE_PX          256
#define IRIS_TILE_BYTES_RGBA  ( IRIS_TILE_PX * IRIS_TILE_PX * 4 )

void Iris_ModelBenchmark( iris_decoder_t decoder, iris_model_result_t *out );

const iris_perf_row_t *Iris_PerfTable( int *count );
const iris_literature_row_t *Iris_LiteratureTable( int *count );

float Iris_TeFOV( iris_layer_t layer, iris_decoder_t decoder );
float Iris_TPT( iris_layer_t layer, iris_decoder_t decoder );
float Iris_BufferRateGibS( void );
float Iris_SustainedFps( void );

float Iris_SpeedupVsLiterature( const char *name, iris_layer_t layer, iris_decoder_t decoder );

const char *Iris_LayerName( iris_layer_t layer );
const char *Iris_DecoderName( iris_decoder_t decoder );
