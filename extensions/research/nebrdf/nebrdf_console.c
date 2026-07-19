/*
===========================================================================
NEBRDF console — Shen et al. arXiv:2604.24081.
===========================================================================
*/

#include "nebrdf/nebrdf.h"

#include "qcommon/qcommon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cvar_t *cl_nebrdf_enable;
static qboolean nebrdf_console_registered = qfalse;

static qboolean NeBrdf_Enabled( void )
{
	if ( !cl_nebrdf_enable || !cl_nebrdf_enable->integer ) {
		Com_Printf( "[nebrdf] disabled (cl_nebrdf_enable 0)\n" );
		return qfalse;
	}
	return qtrue;
}

static void NeBrdf_Cmd_Paper_f( void )
{
	if ( !NeBrdf_Enabled() ) {
		return;
	}
	Com_Printf( "[nebrdf] Shen, Ma, Zhou & Wu — Neural Enhancement of Analytical\n" );
	Com_Printf( "[nebrdf] Appearance Models, arXiv:2604.24081\n" );
	Com_Printf( "[nebrdf] Replace bottleneck GGX nodes/ops with small MLPs via\n" );
	Com_Printf( "[nebrdf] hypercube search. Scaffold: graph/metrics only — no weights.\n" );
}

static void NeBrdf_Cmd_Status_f( void )
{
	Com_Printf( "[nebrdf] cl_nebrdf_enable=%d nodes=%d formula=%s\n",
		cl_nebrdf_enable ? cl_nebrdf_enable->integer : 0,
		NeBrdf_NodeCount(),
		NeBrdf_FinalFormula() );
	Com_Printf( "[nebrdf] commands: nebrdf_paper nebrdf_graph nebrdf_search nebrdf_mlp\n" );
	Com_Printf( "[nebrdf]           nebrdf_perf nebrdf_compare nebrdf_advice nebrdf_status\n" );
}

static void NeBrdf_Cmd_Graph_f( void )
{
	int i;
	unsigned int mask;

	if ( !NeBrdf_Enabled() ) {
		return;
	}

	mask = NeBrdf_FinalStateMask();
	Com_Printf( "[nebrdf] final: %s\n", NeBrdf_FinalFormula() );
	Com_Printf( "[nebrdf] state mask=0x%x  id  sym   kind   neural?\n", mask );
	for ( i = 0; i < NeBrdf_NodeCount(); i++ ) {
		const nebrdf_node_t *n = NeBrdf_GetNode( i );
		Com_Printf( "  %2d  %-4s  %-4s  %s\n",
			n->id, n->symbol,
			n->kind == NEBRDF_KIND_OP ? "op" : "node",
			n->neuralInFinal ? "HAT" : "analytic" );
	}
}

static void NeBrdf_Cmd_Search_f( void )
{
	int order[NEBRDF_ENHANCE_ORDER_LEN];
	int n, i;
	int neighbors;

	if ( !NeBrdf_Enabled() ) {
		return;
	}

	neighbors = NeBrdf_HypercubeNeighbors( NeBrdf_NodeCount(), 1 );
	n = NeBrdf_EnhancementOrder( order, NEBRDF_ENHANCE_ORDER_LEN );
	Com_Printf( "[nebrdf] hypercube N=%d Hamming≤1 candidates=%d (N+1)\n",
		NeBrdf_NodeCount(), neighbors );
	Com_Printf( "[nebrdf] epochs between state changes=%d\n",
		NeBrdf_EpochsBetweenStateChanges() );
	Com_Printf( "[nebrdf] Fig.1 enhancement order:\n" );
	for ( i = 0; i < n; i++ ) {
		const nebrdf_node_t *node = NeBrdf_GetNode( order[i] );
		Com_Printf( "  #%d  id=%d  %s (%s)\n",
			i + 1, order[i], node->symbol, node->name );
	}
}

static void NeBrdf_Cmd_Mlp_f( void )
{
	nebrdf_param_counts_t p;

	if ( !NeBrdf_Enabled() ) {
		return;
	}
	NeBrdf_ParamCounts( &p );
	Com_Printf( "[nebrdf] MLP: %d-layer FC hidden {%d,%d,%d} + leaky ReLU\n",
		p.mlpLayers, p.mlpH0, p.mlpH1, p.mlpH2 );
	Com_Printf( "[nebrdf] params: analytical=%d neural=%d total=%d\n",
		p.analyticalParams, p.neuralParams, p.totalParams );
	Com_Printf( "[nebrdf] ~%d trainable weights during enhancement; footprint≈%.2f KB\n",
		p.weightApprox, p.sizeKB );
}

static void NeBrdf_Cmd_Perf_f( void )
{
	if ( !NeBrdf_Enabled() ) {
		return;
	}
	Com_Printf( "[nebrdf] fit 1e5 samples: enhanced=%.1fs  GGX=%.1fs\n",
		NeBrdf_FitTimeSec( 1 ), NeBrdf_FitTimeSec( 0 ) );
	Com_Printf( "[nebrdf] Mitsuba rays/s (×1e6): enhanced=%.2f  GGX=%.2f\n",
		NeBrdf_RenderRaysPerSec( 1 ), NeBrdf_RenderRaysPerSec( 0 ) );
}

static void NeBrdf_Cmd_Compare_f( void )
{
	int i;

	if ( !NeBrdf_Enabled() ) {
		return;
	}
	Com_Printf( "[nebrdf] Fig.5 subset SSIM / ΔE_ITP(×1e3) — enhanced vs GGX:\n" );
	Com_Printf( "  material                 enhSSIM  enhΔE   ggxSSIM  ggxΔE\n" );
	for ( i = 0; i < NeBrdf_CompareCount(); i++ ) {
		const nebrdf_compare_row_t *r = NeBrdf_CompareRow( i );
		Com_Printf( "  %-22s  %.3f   %.2f   %.3f   %.2f\n",
			r->materialName, r->enhancedSsim, r->enhancedDeItp,
			r->ggxSsim, r->ggxDeItp );
	}
	Com_Printf( "[nebrdf] Paper: enhanced GGX ≈ NBRDF quality with fixed net +\n" );
	Com_Printf( "[nebrdf] per-material params; stronger than NLBRDF on highlights.\n" );
}

static void NeBrdf_Cmd_Advice_f( void )
{
	const char *useCase = "fit";

	if ( !NeBrdf_Enabled() ) {
		return;
	}
	if ( Cmd_Argc() >= 2 ) {
		useCase = Cmd_Argv( 1 );
	}
	Com_Printf( "[nebrdf] advice (%s): %s\n", useCase, NeBrdf_SelectAdvice( useCase ) );
}

void NeBrdf_ConsoleInit( void )
{
	if ( nebrdf_console_registered ) {
		return;
	}

	cl_nebrdf_enable = Cvar_Get( "cl_nebrdf_enable", "0", CVAR_ARCHIVE );
	Cmd_AddCommand( "nebrdf_paper", NeBrdf_Cmd_Paper_f );
	Cmd_AddCommand( "nebrdf_graph", NeBrdf_Cmd_Graph_f );
	Cmd_AddCommand( "nebrdf_search", NeBrdf_Cmd_Search_f );
	Cmd_AddCommand( "nebrdf_mlp", NeBrdf_Cmd_Mlp_f );
	Cmd_AddCommand( "nebrdf_perf", NeBrdf_Cmd_Perf_f );
	Cmd_AddCommand( "nebrdf_compare", NeBrdf_Cmd_Compare_f );
	Cmd_AddCommand( "nebrdf_advice", NeBrdf_Cmd_Advice_f );
	Cmd_AddCommand( "nebrdf_status", NeBrdf_Cmd_Status_f );

	Com_Printf( "[nebrdf] Shen et al. neural-enhanced BRDF (cl_nebrdf_enable %d)\n",
		cl_nebrdf_enable->integer );
	nebrdf_console_registered = qtrue;
}

void NeBrdf_ConsoleShutdown( void )
{
	nebrdf_console_registered = qfalse;
}
