/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Console commands for architecture-independent workload characterization.
===========================================================================
*/

#include "aiwc.h"
#include "aiwc_metrics.h"
#include "aiwc_matmul.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

static cvar_t *cl_aiwc;
static qboolean aiwc_cmds_registered = qfalse;

static void AIWC_PrintMetrics( const char *label, const aiwc_metrics_t *m )
{
	int n;

	if ( !m ) {
		return;
	}

	Com_Printf( "[AIWC] %s: accesses=%llu footprint=%u 90pct=%u rel_local=%.3f global_mae=%.2f\n",
		label,
		(unsigned long long)m->total_accesses,
		m->total_footprint,
		m->footprint_90pct,
		m->relative_local_usage,
		m->global_mae );

	Com_Printf( "[AIWC]   LMAE bits 0/3/10: %.2f / %.2f / %.2f\n",
		m->lmae[0], m->lmae[3], m->lmae[10] );
	Com_Printf( "[AIWC]   PSL bits 0/3/10:   %.2f / %.2f / %.2f\n",
		m->psl[0], m->psl[3], m->psl[10] );

	Com_Printf( "[AIWC]   PSL curve (bits 0-10): " );
	for ( n = 0; n < AIWC_BITS_LEVELS; n++ ) {
		Com_Printf( "%s%.2f", n ? " " : "", m->psl[n] );
	}
	Com_Printf( "\n" );
}

static void AIWC_RunMatmulVariant( aiwc_matmul_variant_t variant, int N )
{
	aiwc_recorder_t *rec;
	aiwc_metrics_t metrics;

	rec = AIWC_RecorderCreate();
	if ( !rec ) {
		Com_Printf( S_COLOR_RED "[AIWC] recorder allocation failed\n" );
		return;
	}

	AIWC_SimulateMatmul( variant, N, rec );
	AIWC_RecorderFinalize( rec, &metrics );
	AIWC_PrintMetrics( AIWC_MatmulVariantName( variant ), &metrics );
	AIWC_RecorderDestroy( rec );
}

static void AIWC_Matmul_f( void )
{
	const char *name;
	int N;
	aiwc_matmul_variant_t v;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: aiwc_matmul <simple|coalescedA|coalescedAB|coalescedABT|alignedABT> [N]\n" );
		return;
	}

	name = Cmd_Argv( 1 );
	N = ( Cmd_Argc() >= 3 ) ? atoi( Cmd_Argv( 2 ) ) : 256;
	if ( N <= 0 || ( N % AIWC_MATMUL_TILE_DIM ) != 0 ) {
		Com_Printf( S_COLOR_YELLOW "[AIWC] N must be a positive multiple of %d\n", AIWC_MATMUL_TILE_DIM );
		return;
	}

	for ( v = 0; v < AIWC_MATMUL_VARIANT_COUNT; v++ ) {
		if ( !Q_stricmp( name, AIWC_MatmulVariantName( v ) ) ) {
			AIWC_RunMatmulVariant( v, N );
			return;
		}
	}

	Com_Printf( S_COLOR_YELLOW "[AIWC] unknown variant '%s'\n", name );
}

static void AIWC_MatmulAll_f( void )
{
	int N;
	aiwc_matmul_variant_t v;

	N = ( Cmd_Argc() >= 2 ) ? atoi( Cmd_Argv( 1 ) ) : 256;
	if ( N <= 0 || ( N % 16 ) != 0 ) {
		Com_Printf( S_COLOR_YELLOW "[AIWC] N must be a positive multiple of 16\n" );
		return;
	}

	Com_Printf( "[AIWC] matrix multiply characterization N=%d (IWOCL'20 matmul suite)\n", N );
	for ( v = 0; v < AIWC_MATMUL_VARIANT_COUNT; v++ ) {
		AIWC_RunMatmulVariant( v, N );
	}
}

static void AIWC_Status_f( void )
{
	Com_Printf( "[AIWC] cl_aiwc=%d commands: aiwc_matmul, aiwc_matmul_all, aiwc_status\n",
		cl_aiwc ? cl_aiwc->integer : 0 );
	Com_Printf( "[AIWC] Metrics: footprint, MAE/LMAE, relative local usage, parallel spatial locality\n" );
	Com_Printf( "[AIWC] See docs/AIWC.md\n" );
}

void AIWC_Init( void )
{
	cl_aiwc = Cvar_Get( "cl_aiwc", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_aiwc,
		"Architecture-independent workload characterization (AIWC). Console: aiwc_matmul_all. See docs/AIWC.md." );

	if ( aiwc_cmds_registered ) {
		return;
	}

	Cmd_AddCommand( "aiwc_matmul", AIWC_Matmul_f );
	Cmd_AddCommand( "aiwc_matmul_all", AIWC_MatmulAll_f );
	Cmd_AddCommand( "aiwc_status", AIWC_Status_f );
	aiwc_cmds_registered = qtrue;

	if ( cl_aiwc && cl_aiwc->integer ) {
		Com_Printf( "[AIWC] Architecture-independent memory characterization ready (aiwc_matmul_all)\n" );
	}
}

void AIWC_Shutdown( void )
{
	if ( !aiwc_cmds_registered ) {
		return;
	}
	Cmd_RemoveCommand( "aiwc_matmul" );
	Cmd_RemoveCommand( "aiwc_matmul_all" );
	Cmd_RemoveCommand( "aiwc_status" );
	aiwc_cmds_registered = qfalse;
}
