/*
===========================================================================
DaX console commands (benchboard + Python pipeline launcher).
===========================================================================
*/

#include "dax/dax_console.h"
#include "dax/dax.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cvar_t *cl_dax_enable;
static cvar_t *cl_dax_repo;
static cvar_t *cl_dax_python;
static cvar_t *cl_dax_encoder;
static cvar_t *cl_dax_weights;
static qboolean dax_console_registered = qfalse;

static void Dax_Cmd_Info_f( void )
{
	dax_benchmark_stats_t stats = Dax_BenchmarkStats();
	dax_model_row_t dax;

	Dax_ModelLookup( DAX_MODEL_DAX, &dax );
	Com_Printf( "[DaX] General pathology representations across scales (arXiv:2606.06983)\n" );
	Com_Printf( "[DaX] pretrain: %d WSIs | benchmark: %d tasks / %d datasets\n",
		stats.pretrain_wsis, stats.num_tasks, stats.num_datasets );
	Com_Printf( "[DaX] benchmark cohort: %d patients | %d slides\n",
		stats.num_patients, stats.num_slides );
	Com_Printf( "[DaX] encoder: %s (%dM params) | embed dim %d\n",
		dax.architecture, dax.params_m, DAX_EMBED_DIM );
	Com_Printf( "[DaX] anchor mags: 2.5x 5x 10x 20x | patch %d px @ %.1f um/px (20x)\n",
		DAX_PATCH_PX, DAX_MPP_AT_20X );
	Com_Printf( "[DaX] cvars: cl_dax_enable cl_dax_repo cl_dax_python cl_dax_encoder cl_dax_weights\n" );
	Com_Printf( "[DaX] commands: dax_info dax_benchmark dax_models dax_eval_test\n" );
	Com_Printf( "[DaX] python: dax_extract dax_eval dax_pretrain_stage1 dax_pretrain_stage2\n" );
}

static void Dax_Cmd_Benchmark_f( void )
{
	const dax_category_row_t *rows;
	int count;
	int i;

	if ( !cl_dax_enable || !cl_dax_enable->integer ) {
		Com_Printf( "[DaX] disabled (cl_dax_enable 0)\n" );
		return;
	}

	rows = Dax_CategoryTable( &count );
	Com_Printf( "[DaX] Level-2 task categories (161 tasks total):\n" );
	Com_Printf( "%-6s %-42s %5s\n", "Domain", "Category", "Tasks" );
	for ( i = 0; i < count; i++ ) {
		Com_Printf( "  %-40s %-42s %5d\n",
			Dax_DomainName( rows[i].domain ),
			Dax_CategoryName( rows[i].category ),
			rows[i].task_count );
	}
}

static void Dax_Cmd_Models_f( void )
{
	const dax_model_row_t *models;
	int count;
	int i;

	models = Dax_ModelTable( &count );
	Com_Printf( "[DaX] Table 2 foundation models (mean benchmark score):\n" );
	Com_Printf( "%-18s %-12s %6s %8s %6s\n", "Model", "Arch", "Params", "WSIs", "Mean" );
	for ( i = 0; i < count; i++ ) {
		Com_Printf( "  %-16s %-12s %4dM %8d %5.1f\n",
			models[i].name, models[i].architecture, models[i].params_m,
			models[i].pretrain_wsis, models[i].mean_benchmark_score );
	}
}

static void Dax_Cmd_EvalTest_f( void )
{
	dax_fold_result_t dax_res;
	dax_fold_result_t uni_res;
	dax_fold_result_t models[2];
	int rank;

	Dax_FoldResultInit( &dax_res );
	Dax_FoldResultInit( &uni_res );
	Dax_FoldResultPush( &dax_res, 0.82f );
	Dax_FoldResultPush( &dax_res, 0.79f );
	Dax_FoldResultPush( &dax_res, 0.81f );
	Dax_FoldResultPush( &dax_res, 0.80f );
	Dax_FoldResultPush( &uni_res, 0.74f );
	Dax_FoldResultPush( &uni_res, 0.72f );
	Dax_FoldResultPush( &uni_res, 0.73f );
	Dax_FoldResultPush( &uni_res, 0.71f );

	models[0] = dax_res;
	models[1] = uni_res;
	rank = Dax_StatisticalRankScore( models, 2, models, 0, 0.05f );

	Com_Printf( "[DaX] eval smoke: DaX mean=%.3f UNI mean=%.3f rank_score=%d\n",
		Dax_FoldResultMean( &dax_res ), Dax_FoldResultMean( &uni_res ), rank );
}

static void Dax_RunPython( const char *script, int first_arg )
{
	char cmd[1024];
	const char *repo;
	const char *py;
	int argc = Cmd_Argc();
	int i;
	int pos;

	if ( !cl_dax_enable || !cl_dax_enable->integer ) {
		Com_Printf( "[DaX] disabled (cl_dax_enable 0)\n" );
		return;
	}

	repo = ( cl_dax_repo && cl_dax_repo->string[0] ) ? cl_dax_repo->string : ".";
	py = ( cl_dax_python && cl_dax_python->string[0] ) ? cl_dax_python->string : "python3";

	pos = Com_sprintf( cmd, sizeof( cmd ), "cd \"%s\" && \"%s\" tools/dax/%s",
		repo, py, script );
	if ( pos <= 0 ) {
		Com_Printf( S_COLOR_YELLOW "[DaX] command too long\n" );
		return;
	}
	if ( !Q_stricmp( script, "evaluate_benchmark.py" ) && cl_dax_weights && cl_dax_weights->string[0] ) {
		Q_strcat( cmd, sizeof( cmd ), va( " --weights \"%s\"", cl_dax_weights->string ) );
	}
	if ( !Q_stricmp( script, "evaluate_benchmark.py" ) ) {
		Q_strcat( cmd, sizeof( cmd ),
			" --manifest tools/dax/fixtures/mini_bench/tasks.json --bench-root tools/dax/fixtures/mini_bench" );
	}
	for ( i = first_arg; i < argc; i++ ) {
		pos = (int)strlen( cmd );
		if ( pos >= (int)sizeof( cmd ) - 2 ) {
			break;
		}
		Q_strcat( cmd, sizeof( cmd ), " \"" );
		Q_strcat( cmd, sizeof( cmd ), Cmd_Argv( i ) );
		Q_strcat( cmd, sizeof( cmd ), "\"" );
	}

	pos = (int)strlen( cmd );
	if ( pos <= 0 || pos >= (int)sizeof( cmd ) ) {
		Com_Printf( S_COLOR_YELLOW "[DaX] command too long\n" );
		return;
	}

	Com_Printf( "[DaX] %s\n", cmd );
	if ( system( cmd ) != 0 ) {
		Com_Printf( S_COLOR_YELLOW "[DaX] python pipeline failed (see log)\n" );
	}
}

static void Dax_Cmd_Extract_f( void )
{
	Dax_RunPython( "extract_features.py", 1 );
}

static void Dax_Cmd_Eval_f( void )
{
	Dax_RunPython( "evaluate_benchmark.py", 1 );
}

static void Dax_Cmd_PretrainStage1_f( void )
{
	Dax_RunPython( "pretrain_stage1.py", 1 );
}

static void Dax_Cmd_PretrainStage2_f( void )
{
	Dax_RunPython( "pretrain_stage2.py", 1 );
}

void Dax_ConsoleInit( void )
{
	if ( dax_console_registered ) {
		return;
	}

	cl_dax_enable = Cvar_Get( "cl_dax_enable", "1", CVAR_ARCHIVE );
	cl_dax_repo = Cvar_Get( "cl_dax_repo", "", CVAR_ARCHIVE );
	cl_dax_python = Cvar_Get( "cl_dax_python", "python3", CVAR_ARCHIVE );
	cl_dax_encoder = Cvar_Get( "cl_dax_encoder", "ViT-L/16", CVAR_ARCHIVE );
	cl_dax_weights = Cvar_Get( "cl_dax_weights", "", CVAR_ARCHIVE );

	Cvar_SetDescription( cl_dax_enable,
		"Enable DaX pathology foundation model console tools." );
	Cvar_SetDescription( cl_dax_repo,
		"Path to idtech3 repo root for tools/dax Python pipeline." );
	Cvar_SetDescription( cl_dax_python,
		"Python interpreter for DaX extract/eval/pretrain scripts." );
	Cvar_SetDescription( cl_dax_encoder,
		"Encoder architecture label (DaX default ViT-L/16, DINOv3 init)." );
	Cvar_SetDescription( cl_dax_weights,
		"Public DaX weight source (path, huggingface:, or url) for extract/eval." );

	Cmd_AddCommand( "dax_info", Dax_Cmd_Info_f );
	Cmd_AddCommand( "dax_benchmark", Dax_Cmd_Benchmark_f );
	Cmd_AddCommand( "dax_models", Dax_Cmd_Models_f );
	Cmd_AddCommand( "dax_eval_test", Dax_Cmd_EvalTest_f );
	Cmd_AddCommand( "dax_extract", Dax_Cmd_Extract_f );
	Cmd_AddCommand( "dax_eval", Dax_Cmd_Eval_f );
	Cmd_AddCommand( "dax_pretrain_stage1", Dax_Cmd_PretrainStage1_f );
	Cmd_AddCommand( "dax_pretrain_stage2", Dax_Cmd_PretrainStage2_f );

	dax_console_registered = qtrue;

	if ( cl_dax_enable->integer ) {
		Com_Printf( "[DaX] enabled (%d benchmark tasks, ViT-L DINOv3 init)\n",
			DAX_NUM_BENCHMARK_TASKS );
	}
}
