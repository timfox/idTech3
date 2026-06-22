/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Iris Core benchmark model — Landvater & Balis, J Pathol Inform 16 (2025) 100414.
Fig. 7 TeFOV/TPT; Fig. 8 buffer-rate and 120 FPS trace.
===========================================================================
*/

#include "iris/iris_model.h"

#include <math.h>
#include <string.h>

/* Paper Fig. 7 — M1 MacBook Pro, 2774×1750, Leica SVS */
static const iris_perf_row_t perf_table[] = {
	{ IRIS_LAYER_LR, IRIS_DECODER_CODEC,      { 10.f,  9.f,  11.f }, { 0.16f, 0.12f, 0.21f } },
	{ IRIS_LAYER_HR, IRIS_DECODER_CODEC,      { 25.f, 20.f,  33.f }, { 0.10f, 0.08f, 0.12f } },
	{ IRIS_LAYER_LR, IRIS_DECODER_OPENSELIDE, { 56.f, 45.f,  87.f }, { 1.32f, 0.94f, 2.04f } },
	{ IRIS_LAYER_HR, IRIS_DECODER_OPENSELIDE, {124.f, 90.f, 209.f }, { 0.60f, 0.42f, 0.69f } },
};

/* Literature TFOV (less strict than TeFOV); paper Discussion § */
static const iris_literature_row_t literature_table[] = {
	{ "DziTileSource",    357.f },
	{ "FlexTileSource",   240.f },
	{ "Schuffler2022",    164.f },
	{ "AperioWebViewer",  2000.f /* ≥2000 μs/tile → ~500 tiles/s at 1 tile/ms scale */ },
};

static const iris_perf_row_t *Iris_Lookup( iris_layer_t layer, iris_decoder_t decoder )
{
	size_t i;

	for ( i = 0; i < sizeof( perf_table ) / sizeof( perf_table[0] ); i++ ) {
		if ( perf_table[i].layer == layer && perf_table[i].decoder == decoder ) {
			return &perf_table[i];
		}
	}
	return &perf_table[0];
}

const iris_perf_row_t *Iris_PerfTable( int *count )
{
	if ( count ) {
		*count = (int)( sizeof( perf_table ) / sizeof( perf_table[0] ) );
	}
	return perf_table;
}

const iris_literature_row_t *Iris_LiteratureTable( int *count )
{
	if ( count ) {
		*count = (int)( sizeof( literature_table ) / sizeof( literature_table[0] ) );
	}
	return literature_table;
}

const char *Iris_LayerName( iris_layer_t layer )
{
	return ( layer == IRIS_LAYER_HR ) ? "HR" : "LR";
}

const char *Iris_DecoderName( iris_decoder_t decoder )
{
	return ( decoder == IRIS_DECODER_OPENSELIDE ) ? "OpenSlide" : "IrisCodec";
}

float Iris_TeFOV( iris_layer_t layer, iris_decoder_t decoder )
{
	return Iris_Lookup( layer, decoder )->te_fov.median_ms;
}

float Iris_TPT( iris_layer_t layer, iris_decoder_t decoder )
{
	return Iris_Lookup( layer, decoder )->tpt_ms.median_ms;
}

float Iris_BufferRateGibS( void )
{
	return 1.36f; /* paper median 1.39 GiB/s; abstract cites 1.36 GB/s decompressed */
}

float Iris_SustainedFps( void )
{
	return 120.f;
}

void Iris_ModelBenchmark( iris_decoder_t decoder, iris_model_result_t *out )
{
	const iris_perf_row_t *lr;
	const iris_perf_row_t *hr;

	if ( !out ) {
		return;
	}

	lr = Iris_Lookup( IRIS_LAYER_LR, decoder );
	hr = Iris_Lookup( IRIS_LAYER_HR, decoder );

	out->decoder = decoder;
	out->lr_te_fov = lr->te_fov;
	out->hr_te_fov = hr->te_fov;
	out->lr_tpt = lr->tpt_ms;
	out->hr_tpt = hr->tpt_ms;
	out->buffer_rate_gib_s = Iris_BufferRateGibS();
	out->fps_median = Iris_SustainedFps();
}

float Iris_SpeedupVsLiterature( const char *name, iris_layer_t layer, iris_decoder_t decoder )
{
	size_t i;
	float iris_ms;

	if ( !name || !name[0] ) {
		return 0.f;
	}

	iris_ms = Iris_TeFOV( layer, decoder );
	for ( i = 0; i < sizeof( literature_table ) / sizeof( literature_table[0] ); i++ ) {
		if ( !strcmp( literature_table[i].name, name ) ) {
			if ( iris_ms <= 0.f ) {
				return 0.f;
			}
			return literature_table[i].tfov_ms / iris_ms;
		}
	}
	return 0.f;
}
