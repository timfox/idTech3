/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Iris console — Landvater & Balis, J Pathol Inform 16 (2025) 100414.
===========================================================================
*/

#include "iris/iris_console.h"
#include "iris/iris_model.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

static cvar_t *cl_iris_model;
static qboolean iris_console_registered = qfalse;

static iris_decoder_t Iris_ParseDecoder( const char *name )
{
	if ( !name || !name[0] ) {
		return IRIS_DECODER_CODEC;
	}
	if ( !Q_stricmp( name, "openslide" ) || !Q_stricmp( name, "os" ) || !Q_stricmp( name, "svs" ) ) {
		return IRIS_DECODER_OPENSELIDE;
	}
	return IRIS_DECODER_CODEC;
}

static iris_layer_t Iris_ParseLayer( const char *name )
{
	if ( name && !Q_stricmp( name, "hr" ) ) {
		return IRIS_LAYER_HR;
	}
	return IRIS_LAYER_LR;
}

static void Iris_Cmd_ModelStatus_f( void )
{
	Com_Printf( "[Iris] cl_iris_model=%d\n", cl_iris_model ? cl_iris_model->integer : 0 );
	Com_Printf( "[Iris] Commands: iris_api, iris_model, iris_teFOV, iris_compare\n" );
	Com_Printf( "[Iris] Runtime (renderer): iris_status, iris_pan, iris_load, iris_save, iris_spd_step, iris_reset\n" );
	Com_Printf( "[Iris] See docs/IRIS.md — doi:10.1016/j.jpi.2024.100414\n" );
}

static void Iris_Cmd_Api_f( void )
{
	Com_Printf( "[Iris] Digital pathology WSI renderer (Vulkan Core):\n" );
	Com_Printf( "  256×256 tiles; LR (>1:1) + HR (≤1:1) dual-pass scope view\n" );
	Com_Printf( "  RTBS microtransactions; SPD Laplacian mipmap enhancement\n" );
	Com_Printf( "  Pull-style threads: render, buffer, loaders; atomic tile wrappers\n" );
	Com_Printf( "[Iris] Runtime: r_iris 1 + vid_restart; iris_pan, iris_load/save, r_iris_decoder\n" );
}

static void Iris_Cmd_Model_f( void )
{
	iris_model_result_t codec;
	iris_model_result_t os;
	int i;
	int count;
	const iris_perf_row_t *rows;

	Iris_ModelBenchmark( IRIS_DECODER_CODEC, &codec );
	Iris_ModelBenchmark( IRIS_DECODER_OPENSELIDE, &os );

	Com_Printf( "[Iris] Paper metrics (M1 MacBook Pro, Fig. 7):\n" );
	Com_Printf( "  IrisCodec  LR TeFOV %.0f ms (%.0f–%.0f)  TPT %.2f ms\n",
		codec.lr_te_fov.median_ms, codec.lr_te_fov.p25_ms, codec.lr_te_fov.p75_ms,
		codec.lr_tpt.median_ms );
	Com_Printf( "  IrisCodec  HR TeFOV %.0f ms (%.0f–%.0f)  TPT %.2f ms\n",
		codec.hr_te_fov.median_ms, codec.hr_te_fov.p25_ms, codec.hr_te_fov.p75_ms,
		codec.hr_tpt.median_ms );
	Com_Printf( "  OpenSlide  LR TeFOV %.0f ms  HR TeFOV %.0f ms\n",
		os.lr_te_fov.median_ms, os.hr_te_fov.median_ms );
	Com_Printf( "  Buffer-rate median %.2f GiB/s; sustained %.0f FPS (Fig. 8)\n",
		codec.buffer_rate_gib_s, codec.fps_median );

	rows = Iris_PerfTable( &count );
	Com_Printf( "[Iris] Full table (%d rows):\n", count );
	for ( i = 0; i < count; i++ ) {
		Com_Printf( "  %s %s  TeFOV=%.0f ms  TPT=%.2f ms\n",
			Iris_DecoderName( rows[i].decoder ), Iris_LayerName( rows[i].layer ),
			rows[i].te_fov.median_ms, rows[i].tpt_ms.median_ms );
	}
}

static void Iris_Cmd_TeFOV_f( void )
{
	iris_decoder_t dec;
	iris_layer_t layer;

	dec = ( Cmd_Argc() >= 2 ) ? Iris_ParseDecoder( Cmd_Argv( 1 ) ) : IRIS_DECODER_CODEC;
	layer = ( Cmd_Argc() >= 3 ) ? Iris_ParseLayer( Cmd_Argv( 2 ) ) : IRIS_LAYER_LR;

	Com_Printf( "[Iris] TeFOV %s %s: %.1f ms (TPT %.2f ms/tile)\n",
		Iris_DecoderName( dec ), Iris_LayerName( layer ),
		Iris_TeFOV( layer, dec ), Iris_TPT( layer, dec ) );
	Com_Printf( "[Iris] Tile payload %u bytes (256×256 RGBA8)\n", (unsigned)IRIS_TILE_BYTES_RGBA );
}

static void Iris_Cmd_Compare_f( void )
{
	const iris_literature_row_t *lit;
	int count;
	int i;

	lit = Iris_LiteratureTable( &count );
	Com_Printf( "[Iris] Speedup vs literature (HR TeFOV, IrisCodec baseline):\n" );
	for ( i = 0; i < count; i++ ) {
		float speedup = Iris_SpeedupVsLiterature( lit[i].name, IRIS_LAYER_HR, IRIS_DECODER_CODEC );
		Com_Printf( "  %s TFOV %.0f ms → %.1fx vs Iris HR TeFOV %.0f ms\n",
			lit[i].name, lit[i].tfov_ms, speedup, Iris_TeFOV( IRIS_LAYER_HR, IRIS_DECODER_CODEC ) );
	}
}

void Iris_ConsoleInit( void )
{
	cl_iris_model = Cvar_Get( "cl_iris_model", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_iris_model,
		"Enable Iris digital pathology benchmark commands (J Pathol Inform 2025)." );

	if ( !cl_iris_model->integer ) {
		return;
	}

	if ( !iris_console_registered ) {
		Cmd_AddCommand( "iris_model_status", Iris_Cmd_ModelStatus_f );
		Cmd_AddCommand( "iris_api", Iris_Cmd_Api_f );
		Cmd_AddCommand( "iris_model", Iris_Cmd_Model_f );
		Cmd_AddCommand( "iris_teFOV", Iris_Cmd_TeFOV_f );
		Cmd_AddCommand( "iris_compare", Iris_Cmd_Compare_f );
		iris_console_registered = qtrue;
	}

	Com_Printf( "[Iris] Model commands enabled (cl_iris_model 1)\n" );
}
