/*
===========================================================================
Domany–Kinzel QSD console commands.
===========================================================================
*/

#include "dk_qsd/dk_qsd.h"
#include "dk_qsd/dk_qsd_internal.h"

#include "qcommon/qcommon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cvar_t *cl_dk_qsd_enable;
static dk_qsd_state_t *dk_qsd_cached;
static qboolean dk_qsd_console_registered = qfalse;

static void DK_Qsd_Cmd_Info_f( void )
{
	Com_Printf( "[DK-QSD] Domany–Kinzel quasi-stationary distribution (bond DP line)\n" );
	Com_Printf( "[DK-QSD] Lee/Harada/Kawashima arXiv:2606.11885 — MPS projected transfer + MI diagnostics\n" );
	Com_Printf( "[DK-QSD] p_c=%.9f | dense N<=%d | MPS chi_max=%d\n",
		DK_Qsd_Pc(), DK_QSD_DENSE_MAX_N, DK_QSD_CHI_MAX_DEFAULT );
	Com_Printf( "[DK-QSD] cvars: cl_dk_qsd_enable\n" );
	Com_Printf( "[DK-QSD] commands: dk_qsd_info dk_qsd_solve dk_qsd_obs dk_qsd_sample dk_qsd_mi\n" );
}

static void DK_Qsd_Cmd_Solve_f( void )
{
	int N = 32;
	float p = 0.60f;
	int chi = DK_QSD_CHI_MAX_DEFAULT;
	dk_qsd_method_t method = DK_QSD_METHOD_MPS;

	if ( !cl_dk_qsd_enable || !cl_dk_qsd_enable->integer ) {
		Com_Printf( "[DK-QSD] disabled (cl_dk_qsd_enable 0)\n" );
		return;
	}

	if ( Cmd_Argc() >= 2 ) {
		N = atoi( Cmd_Argv( 1 ) );
	}
	if ( Cmd_Argc() >= 3 ) {
		p = (float)atof( Cmd_Argv( 2 ) );
	}
	if ( Cmd_Argc() >= 4 ) {
		chi = atoi( Cmd_Argv( 3 ) );
	}
	if ( N <= DK_QSD_DENSE_MAX_N ) {
		method = DK_QSD_METHOD_DENSE;
	}

	if ( dk_qsd_cached ) {
		DK_Qsd_Free( dk_qsd_cached );
		dk_qsd_cached = NULL;
	}

	dk_qsd_cached = DK_Qsd_Solve( N, p, method, chi, 200, 1e-5f );
	if ( !dk_qsd_cached ) {
		Com_Printf( S_COLOR_RED "[DK-QSD] solve failed\n" );
	}
}

static void DK_Qsd_Cmd_Obs_f( void )
{
	dk_qsd_observables_t obs;

	if ( !cl_dk_qsd_enable || !cl_dk_qsd_enable->integer ) {
		Com_Printf( "[DK-QSD] disabled (cl_dk_qsd_enable 0)\n" );
		return;
	}
	if ( !dk_qsd_cached ) {
		Com_Printf( "[DK-QSD] run dk_qsd_solve first\n" );
		return;
	}

	DK_Qsd_ComputeObservables( dk_qsd_cached, 8192, 0x515u, &obs );
	Com_Printf( "[DK-QSD] N=%d p=%.3f lambda1=%.6f\n",
		DK_Qsd_ChainLength( dk_qsd_cached ), DK_Qsd_ParamP( dk_qsd_cached ), obs.lambda1 );
	Com_Printf( "[DK-QSD] <n>=%.4f R11=%.4f I_half=%.4f bits\n",
		obs.meanActive, obs.r11, obs.halfChainMI );
	Com_Printf( "[DK-QSD] flock: <extent>=%.2f <fill>=%.3f P(single)=%.3f\n",
		obs.flockExtentMean, obs.flockFillMean, obs.singleClusterFrac );
}

static void DK_Qsd_Cmd_Sample_f( void )
{
	byte x[256];
	int N;
	int i;
	unsigned rng = 0xABCu;

	if ( !dk_qsd_cached ) {
		Com_Printf( "[DK-QSD] run dk_qsd_solve first\n" );
		return;
	}

	N = DK_Qsd_ChainLength( dk_qsd_cached );
	if ( !DK_Qsd_SampleConfig( dk_qsd_cached, x, N, &rng ) ) {
		Com_Printf( S_COLOR_RED "[DK-QSD] sample failed\n" );
		return;
	}

	Com_Printf( "[DK-QSD] sample:" );
	for ( i = 0; i < N && i < 64; i++ ) {
		Com_Printf( "%d", x[i] & 1 );
	}
	if ( N > 64 ) {
		Com_Printf( "..." );
	}
	Com_Printf( "\n" );
}

static void DK_Qsd_Cmd_Mi_f( void )
{
	int N = DK_Qsd_ChainLength( dk_qsd_cached );
	int cut;
	float x;
	float theory;

	if ( !dk_qsd_cached ) {
		Com_Printf( "[DK-QSD] run dk_qsd_solve first\n" );
		return;
	}

	N = N > 0 ? N : 128;
	cut = N / 2;
	x = ( N > 0 ) ? ( (float)cut / (float)N ) : 0.5f;
	theory = DK_Qsd_BondMI_UniformFlock( x, N, 15.0f );
	Com_Printf( "[DK-QSD] h(%.3f)=%.4f bits | uniform-flock leading term at half-cut\n", x, DK_Qsd_BinaryEntropy( x ) );
	Com_Printf( "[DK-QSD] Eq.(4) leading+h.c. estimate=%.4f bits (K_eff=15)\n", theory );
}

void DK_Qsd_ConsoleInit( void )
{
	if ( dk_qsd_console_registered ) {
		return;
	}

	cl_dk_qsd_enable = Cvar_Get( "cl_dk_qsd_enable", "0", CVAR_ARCHIVE );
	Cmd_AddCommand( "dk_qsd_info", DK_Qsd_Cmd_Info_f );
	Cmd_AddCommand( "dk_qsd_solve", DK_Qsd_Cmd_Solve_f );
	Cmd_AddCommand( "dk_qsd_obs", DK_Qsd_Cmd_Obs_f );
	Cmd_AddCommand( "dk_qsd_sample", DK_Qsd_Cmd_Sample_f );
	Cmd_AddCommand( "dk_qsd_mi", DK_Qsd_Cmd_Mi_f );

	Com_Printf( "[DK-QSD] Domany–Kinzel QSD module (cl_dk_qsd_enable %d)\n",
		cl_dk_qsd_enable->integer );
	dk_qsd_console_registered = qtrue;
}

void DK_Qsd_ConsoleShutdown( void )
{
	if ( dk_qsd_cached ) {
		DK_Qsd_Free( dk_qsd_cached );
		dk_qsd_cached = NULL;
	}
	dk_qsd_console_registered = qfalse;
}
