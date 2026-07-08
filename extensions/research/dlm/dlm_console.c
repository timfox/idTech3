/*
===========================================================================
Deep-layered machine console commands.
===========================================================================
*/

#include "dlm/dlm.h"

#include "qcommon/qcommon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cvar_t *cl_dlm_enable;
static qboolean dlm_console_registered = qfalse;

static void DLM_Cmd_Info_f( void )
{
	Com_Printf( "[DLM] Deep-layered machines — exact output Hamming-weight distribution\n" );
	Com_Printf( "[DLM] Fink arXiv:2606.11965 | q(n) from transition matrix A, ell=2^k\n" );
	Com_Printf( "[DLM] cvars: cl_dlm_enable\n" );
	Com_Printf( "[DLM] commands: dlm_info dlm_q dlm_enum dlm_sample dlm_eigen\n" );
}

static void DLM_Cmd_Q_f( void )
{
	int k = 2;
	int depth = 3;
	dlm_distribution_t *dist;
	int w;

	if ( !cl_dlm_enable || !cl_dlm_enable->integer ) {
		Com_Printf( "[DLM] disabled (cl_dlm_enable 0)\n" );
		return;
	}

	if ( Cmd_Argc() >= 2 ) {
		k = atoi( Cmd_Argv( 1 ) );
	}
	if ( Cmd_Argc() >= 3 ) {
		depth = atoi( Cmd_Argv( 2 ) );
	}

	dist = DLM_ComputeExact( k, depth );
	if ( !dist ) {
		Com_Printf( S_COLOR_RED "[DLM] exact q failed (k<=%d, 2^k functions)\n", DLM_MAX_K );
		return;
	}

	Com_Printf( "[DLM] q(%d) for k=%d ell=%d:\n", depth, k, dist->ell );
	for ( w = 0; w <= dist->ell; w++ ) {
		Com_Printf( "  w=%d  q=%.8f\n", w, dist->q[w] );
	}
	Com_Printf( "[DLM] P(true)+P(false)=%.6f | n_c~%.1f\n",
		dist->endpointProb, DLM_CriticalDepthEstimate( k ) );
	DLM_Free( dist );
}

static void DLM_Cmd_Enum_f( void )
{
	int k = 2;
	int depth = 2;
	int counts[257];
	int total;
	int w;

	if ( Cmd_Argc() >= 2 ) {
		k = atoi( Cmd_Argv( 1 ) );
	}
	if ( Cmd_Argc() >= 3 ) {
		depth = atoi( Cmd_Argv( 2 ) );
	}

	total = DLM_EnumerateWeightCounts( k, depth, counts, (int)( sizeof( counts ) / sizeof( counts[0] ) ) );
	if ( total <= 0 ) {
		Com_Printf( S_COLOR_RED "[DLM] enumeration failed or too large\n" );
		return;
	}

	Com_Printf( "[DLM] enumerated %d configs k=%d n=%d:\n", total, k, depth );
	for ( w = 0; w <= DLM_Ell( k ); w++ ) {
		if ( counts[w] > 0 ) {
			Com_Printf( "  w=%d  freq=%.8f (%d/%d)\n", w, (float)counts[w] / (float)total, counts[w], total );
		}
	}
}

static void DLM_Cmd_Sample_f( void )
{
	int k = 3;
	int depth = 3;
	int samples = 100000;
	int counts[257];
	int w;
	int s;

	if ( Cmd_Argc() >= 2 ) {
		k = atoi( Cmd_Argv( 1 ) );
	}
	if ( Cmd_Argc() >= 3 ) {
		depth = atoi( Cmd_Argv( 2 ) );
	}
	if ( Cmd_Argc() >= 4 ) {
		samples = atoi( Cmd_Argv( 3 ) );
	}

	s = DLM_SampleWeightHistogram( k, depth, samples, 0xF1u, counts,
		(int)( sizeof( counts ) / sizeof( counts[0] ) ) );
	if ( s <= 0 ) {
		Com_Printf( S_COLOR_RED "[DLM] sampling failed\n" );
		return;
	}

	Com_Printf( "[DLM] sampled %d configs k=%d n=%d:\n", s, k, depth );
	for ( w = 0; w <= DLM_Ell( k ); w++ ) {
		if ( counts[w] > 0 ) {
			Com_Printf( "  w=%d  freq=%.8f\n", w, (float)counts[w] / (float)s );
		}
	}
}

static void DLM_Cmd_Eigen_f( void )
{
	int k = 2;
	int j;

	if ( Cmd_Argc() >= 2 ) {
		k = atoi( Cmd_Argv( 1 ) );
	}

	Com_Printf( "[DLM] eigenvalues lambda_j = (ell)_j / ell^j, k=%d ell=%d:\n", k, DLM_Ell( k ) );
	for ( j = 0; j <= DLM_Ell( k ) && j < 8; j++ ) {
		Com_Printf( "  j=%d  lambda=%.8f\n", j, DLM_Eigenvalue( k, j ) );
	}
}

void DLM_ConsoleInit( void )
{
	if ( dlm_console_registered ) {
		return;
	}

	cl_dlm_enable = Cvar_Get( "cl_dlm_enable", "0", CVAR_ARCHIVE );
	Cmd_AddCommand( "dlm_info", DLM_Cmd_Info_f );
	Cmd_AddCommand( "dlm_q", DLM_Cmd_Q_f );
	Cmd_AddCommand( "dlm_enum", DLM_Cmd_Enum_f );
	Cmd_AddCommand( "dlm_sample", DLM_Cmd_Sample_f );
	Cmd_AddCommand( "dlm_eigen", DLM_Cmd_Eigen_f );

	Com_Printf( "[DLM] deep-layered machine module (cl_dlm_enable %d)\n", cl_dlm_enable->integer );
	dlm_console_registered = qtrue;
}

void DLM_ConsoleShutdown( void )
{
	dlm_console_registered = qfalse;
}
