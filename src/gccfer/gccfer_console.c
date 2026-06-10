/*
===========================================================================
GCC-FER / CA-FER console commands.
===========================================================================
*/

#include "gccfer/gccfer_console.h"
#include "gccfer/gccfer.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cvar_t *cl_gccfer_enable;
static cvar_t *cl_gccfer_repo;
static cvar_t *cl_gccfer_python;
static cvar_t *cl_gccfer_checkpoint;
static qboolean gccfer_console_registered = qfalse;

static gccfer_culture_t Gccfer_ParseCulture( const char *s )
{
	if ( !s || !s[0] ) {
		return GCCFER_CULTURE_UNKNOWN;
	}
	if ( !Q_stricmp( s, "caucasian" ) || !Q_stricmp( s, "cauc" ) ) {
		return GCCFER_CULTURE_CAUCASIAN;
	}
	if ( !Q_stricmp( s, "east_asian" ) || !Q_stricmp( s, "east" ) ) {
		return GCCFER_CULTURE_EAST_ASIAN;
	}
	if ( !Q_stricmp( s, "south_asian" ) || !Q_stricmp( s, "south" ) ) {
		return GCCFER_CULTURE_SOUTH_ASIAN;
	}
	if ( !Q_stricmp( s, "african" ) || !Q_stricmp( s, "africa" ) ) {
		return GCCFER_CULTURE_AFRICAN;
	}
	if ( !Q_stricmp( s, "global" ) ) {
		return GCCFER_CULTURE_GLOBAL;
	}
	return GCCFER_CULTURE_UNKNOWN;
}

static void Gccfer_Cmd_Info_f( void )
{
	gccfer_metrics_t cafer;

	Com_Printf( "[GCC-FER] Global Cross-Cultural DFER dataset + CA-FER (arXiv:2606.07063)\n" );
	Com_Printf( "[GCC-FER] %d videos | %d cultures | %d expressions | %d frames @ %dx%d\n",
		Gccfer_TotalSamples(), GCCFER_NUM_CULTURES, GCCFER_NUM_EXPRESSIONS,
		GCCFER_FRAMES_PER_VIDEO, GCCFER_INPUT_SIZE, GCCFER_INPUT_SIZE );
	Gccfer_ModelEvaluate( GCCFER_METHOD_CAFER, &cafer );
	Com_Printf( "[GCC-FER] CA-FER GCC-FER: %.2f UAR / %.2f WAR | DFEW: 63.93 UAR\n",
		cafer.uar, cafer.war );
	Com_Printf( "[GCC-FER] cvars: cl_gccfer_enable cl_gccfer_repo cl_gccfer_python cl_gccfer_checkpoint\n" );
	Com_Printf( "[GCC-FER] commands: gccfer_info gccfer_dataset gccfer_benchmark gccfer_adapt_test\n" );
	Com_Printf( "[GCC-FER] python: gccfer_infer <video_or_frames_dir> [culture]\n" );
}

static void Gccfer_Cmd_Dataset_f( void )
{
	const gccfer_culture_row_t *rows = Gccfer_DatasetTable();
	int c;
	int e;

	if ( !cl_gccfer_enable || !cl_gccfer_enable->integer ) {
		Com_Printf( "[GCC-FER] disabled (cl_gccfer_enable 0)\n" );
		return;
	}

	Com_Printf( "[GCC-FER] Table II distribution (culture x expression):\n" );
	Com_Printf( "%-12s %6s %6s %6s %6s %6s %6s %6s %6s\n",
		"Culture", "Angry", "Disg", "Fear", "Happy", "Neut", "Sad", "Surp", "Total" );

	for ( c = 0; c < GCCFER_NUM_CULTURES; c++ ) {
		const gccfer_culture_row_t *row = &rows[c];
		Com_Printf( "%-12s %6d %6d %6d %6d %6d %6d %6d %6d (%.1f%%)\n",
			Gccfer_CultureName( (gccfer_culture_t)c ),
			row->by_expr.angry, row->by_expr.disgust, row->by_expr.fear,
			row->by_expr.happy, row->by_expr.neutral, row->by_expr.sad,
			row->by_expr.surprise, row->by_expr.total, row->pct_of_total );
	}

	Com_Printf( "[GCC-FER] Per-culture CA-FER UAR (Table V):\n" );
	for ( c = 0; c < GCCFER_NUM_CULTURES; c++ ) {
		const gccfer_metrics_t m = Gccfer_LookupPerCulture( (gccfer_culture_t)c );
		Com_Printf( "  %-12s UAR=%.1f%% WAR=%.1f%%\n",
			Gccfer_CultureName( (gccfer_culture_t)c ), m.uar, m.war );
	}

	Com_Printf( "[GCC-FER] Expression totals:\n" );
	for ( e = 0; e < GCCFER_NUM_EXPRESSIONS; e++ ) {
		int sum = 0;
		for ( c = 0; c < GCCFER_NUM_CULTURES; c++ ) {
			sum += Gccfer_CountFor( (gccfer_culture_t)c, (gccfer_expression_t)e );
		}
		Com_Printf( "  %-10s %d\n", Gccfer_ExpressionName( (gccfer_expression_t)e ), sum );
	}
}

static void Gccfer_Cmd_Benchmark_f( void )
{
	const gccfer_benchmark_row_t *gcc;
	const gccfer_benchmark_row_t *dfew;
	int n;
	int i;
	const char *which;

	which = ( Cmd_Argc() >= 2 ) ? Cmd_Argv( 1 ) : "gccfer";

	if ( !Q_stricmp( which, "dfew" ) ) {
		dfew = Gccfer_DfewBenchmarks( &n );
		Com_Printf( "[GCC-FER] DFEW benchmark (Table IV):\n" );
		for ( i = 0; i < n; i++ ) {
			Com_Printf( "  %-28s UAR=%5.2f WAR=%5.2f\n",
				dfew[i].label, dfew[i].metrics.uar, dfew[i].metrics.war );
		}
		return;
	}

	gcc = Gccfer_GccferBenchmarks( &n );
	Com_Printf( "[GCC-FER] GCC-FER benchmark (Table III):\n" );
	for ( i = 0; i < n; i++ ) {
		Com_Printf( "  %-28s UAR=%5.2f WAR=%5.2f\n",
			gcc[i].label, gcc[i].metrics.uar, gcc[i].metrics.war );
	}
}

static void Gccfer_Cmd_AdaptTest_f( void )
{
	gccfer_cafer_params_t params;
	float latent[GCCFER_LATENT_DIM];
	float adapted[GCCFER_LATENT_DIM];
	float a[GCCFER_LATENT_DIM];
	float b[GCCFER_LATENT_DIM];
	float au_seq[GCCFER_FRAMES_PER_VIDEO * GCCFER_NUM_AUS];
	gccfer_au_stats_t stats;
	float au80[GCCFER_AU_FEATURE_DIM];
	gccfer_culture_t culture;
	int i;
	unsigned int seed = 7u;

	if ( !cl_gccfer_enable || !cl_gccfer_enable->integer ) {
		Com_Printf( "[GCC-FER] disabled\n" );
		return;
	}

	culture = ( Cmd_Argc() >= 2 ) ? Gccfer_ParseCulture( Cmd_Argv( 1 ) ) : GCCFER_CULTURE_GLOBAL;

	for ( i = 0; i < GCCFER_FRAMES_PER_VIDEO * GCCFER_NUM_AUS; i++ ) {
		seed = seed * 1664525u + 1013904223u;
		au_seq[i] = (float)( seed & 0xFFu ) / 255.0f;
	}
	for ( i = 0; i < GCCFER_LATENT_DIM; i++ ) {
		seed = seed * 1664525u + 1013904223u;
		latent[i] = (float)( seed & 0xFFFFu ) / 65535.0f - 0.5f;
	}

	Gccfer_AuStatsFromSequence( au_seq, GCCFER_FRAMES_PER_VIDEO, &stats );
	Gccfer_AuStatsToVector( &stats, au80 );
	Gccfer_CaferInitDefaults( &params, 42u );
	Gccfer_GenerateAdaptParams( &params, culture, a, b );
	Gccfer_AdaptLatent( latent, a, b, GCCFER_LATENT_DIM, adapted );

	Com_Printf( "[GCC-FER] CA-FER adapt test culture=%s f[0]=%.4f -> f'[0]=%.4f\n",
		Gccfer_CultureName( culture ), latent[0], adapted[0] );
}

static void Gccfer_Cmd_Infer_f( void )
{
	const char *input;
	const char *culture_str;
	const char *repo;
	const char *python;
	char cmd[2048];

	if ( !cl_gccfer_enable || !cl_gccfer_enable->integer ) {
		Com_Printf( "[GCC-FER] disabled (cl_gccfer_enable 0)\n" );
		return;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: gccfer_infer <video_or_frame_dir> [culture]\n" );
		return;
	}

	input = Cmd_Argv( 1 );
	culture_str = ( Cmd_Argc() >= 3 ) ? Cmd_Argv( 2 ) : "global";
	repo = cl_gccfer_repo && cl_gccfer_repo->string[0] ? cl_gccfer_repo->string : ".";
	python = cl_gccfer_python && cl_gccfer_python->string[0] ? cl_gccfer_python->string : "python3";

	if ( cl_gccfer_checkpoint && cl_gccfer_checkpoint->string[0] ) {
		Com_sprintf( cmd, sizeof( cmd ),
			"cd \"%s/tools/gccfer\" && \"%s\" infer_cafer.py --input \"%s\" --culture %s --checkpoint \"%s\"",
			repo, python, input, culture_str, cl_gccfer_checkpoint->string );
	} else {
		Com_sprintf( cmd, sizeof( cmd ),
			"cd \"%s/tools/gccfer\" && \"%s\" infer_cafer.py --input \"%s\" --culture %s",
			repo, python, input, culture_str );
	}
	Com_Printf( "[GCC-FER] %s\n", cmd );
	if ( system( cmd ) != 0 ) {
		Com_Printf( S_COLOR_YELLOW "[GCC-FER] inference failed (install tools/gccfer/requirements.txt)\n" );
	}
}

void Gccfer_ConsoleInit( void )
{
	if ( gccfer_console_registered ) {
		return;
	}

	cl_gccfer_enable = Cvar_Get( "cl_gccfer_enable", "1", CVAR_ARCHIVE );
	cl_gccfer_repo = Cvar_Get( "cl_gccfer_repo", "", CVAR_ARCHIVE );
	cl_gccfer_python = Cvar_Get( "cl_gccfer_python", "python3", CVAR_ARCHIVE );
	cl_gccfer_checkpoint = Cvar_Get( "cl_gccfer_checkpoint", "", CVAR_ARCHIVE );

	Cvar_SetDescription( cl_gccfer_enable,
		"Enable GCC-FER / CA-FER console tools (startup log when 1)." );
	Cvar_SetDescription( cl_gccfer_repo,
		"Path to idtech3 repo root for tools/gccfer Python pipeline." );
	Cvar_SetDescription( cl_gccfer_python,
		"Python interpreter for gccfer_infer (PyTorch + transformers)." );
	Cvar_SetDescription( cl_gccfer_checkpoint,
		"Default CA-FER checkpoint path forwarded to gccfer_infer / infer_cafer.py." );

	Cmd_AddCommand( "gccfer_info", Gccfer_Cmd_Info_f );
	Cmd_AddCommand( "gccfer_dataset", Gccfer_Cmd_Dataset_f );
	Cmd_AddCommand( "gccfer_benchmark", Gccfer_Cmd_Benchmark_f );
	Cmd_AddCommand( "gccfer_adapt_test", Gccfer_Cmd_AdaptTest_f );
	Cmd_AddCommand( "gccfer_infer", Gccfer_Cmd_Infer_f );

	gccfer_console_registered = qtrue;

	if ( cl_gccfer_enable->integer ) {
		Com_Printf( "[GCC-FER] enabled (%d samples, CA-FER latent dim %d)\n",
			Gccfer_TotalSamples(), GCCFER_LATENT_DIM );
	}
}
