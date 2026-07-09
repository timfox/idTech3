/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

BubbleSH dataset console and compact metric tooling.
Ramesh et al., arXiv:2607.07275.
===========================================================================
*/

#include "bubblesh.h"
#include "bubblesh_model.h"

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

static cvar_t *cl_bubblesh;
static qboolean bubblesh_cmds_registered = qfalse;

static void BubbleSH_Status_f( void )
{
	Com_Printf( "[BubbleSH] cl_bubblesh=%d order=%d bubbles=%d meshNodes=%d-%d\n",
		cl_bubblesh ? cl_bubblesh->integer : 0,
		BUBBLESH_DEFAULT_ORDER,
		BUBBLESH_BUBBLE_COUNT,
		BUBBLESH_MESH_NODES_MIN,
		BUBBLESH_MESH_NODES_MAX );
	Com_Printf( "[BubbleSH] Commands: bubblesh_status, bubblesh_case, bubblesh_orders, bubblesh_metrics, bubblesh_w1_demo\n" );
	Com_Printf( "[BubbleSH] Dataset space: diameters 4/5/6 mm, void fractions 5%%..40%%, 24 configs, periodic box\n" );
}

static void BubbleSH_Case_f( void )
{
	bubblesh_config_t cfg;
	int diameter_mm;
	int void_pct;
	int sh_order;

	diameter_mm = ( Cmd_Argc() >= 2 ) ? atoi( Cmd_Argv( 1 ) ) : 5;
	void_pct = ( Cmd_Argc() >= 3 ) ? atoi( Cmd_Argv( 2 ) ) : 30;
	sh_order = ( Cmd_Argc() >= 4 ) ? atoi( Cmd_Argv( 3 ) ) : BUBBLESH_DEFAULT_ORDER;

	if ( !BubbleSH_FillConfig( diameter_mm, void_pct, sh_order, &cfg ) ) {
		Com_Printf( S_COLOR_YELLOW "[BubbleSH] Usage: bubblesh_case [diam_mm 4|5|6] [void_pct 5..40 step 5] [sh_order]\n" );
		return;
	}

	Com_Printf( "[BubbleSH] case d=%dmm eps=%d%% order=%d coeffs=%d stateDim=%d\n",
		cfg.diameter_mm, cfg.void_fraction_pct, cfg.sh_order, cfg.sh_coeff_count, cfg.state_dim );
	Com_Printf( "[BubbleSH]   bubbles=%d dt=%.4g s box=%.2f mm coordCompression=%.2fx\n",
		cfg.bubble_count, cfg.dt_seconds, cfg.domain_size_mm, cfg.coord_compression_ratio );
}

static void BubbleSH_Orders_f( void )
{
	const int orders[] = { 3, 5, 14 };
	int i;

	for ( i = 0; i < 3; i++ ) {
		const int coeffs = BubbleSH_CoefficientCount( orders[i] );
		Com_Printf( "[BubbleSH] L=%d coeffs=%d stateDim=%d\n", orders[i], coeffs, 6 + coeffs );
	}
	Com_Printf( "[BubbleSH] Paper benchmark variants: L=3 (16 coeffs), L=5 (36 coeffs), dataset default L=14 (225 coeffs)\n" );
}

static void BubbleSH_Metrics_f( void )
{
	float mean_pos_err;
	float final_pos_err;
	float arc_length;
	float mean_chamfer;
	float surface_change;

	if ( Cmd_Argc() < 6 ) {
		Com_Printf( "Usage: bubblesh_metrics <mean_pos_err> <final_pos_err> <arc_length> <mean_chamfer> <surface_change>\n" );
		return;
	}

	mean_pos_err = (float)atof( Cmd_Argv( 1 ) );
	final_pos_err = (float)atof( Cmd_Argv( 2 ) );
	arc_length = (float)atof( Cmd_Argv( 3 ) );
	mean_chamfer = (float)atof( Cmd_Argv( 4 ) );
	surface_change = (float)atof( Cmd_Argv( 5 ) );

	Com_Printf( "[BubbleSH] R-ADE=%.4f R-FDE=%.4f R-ACD=%.4f\n",
		BubbleSH_RelativeAverageDisplacementError( mean_pos_err, arc_length ),
		BubbleSH_RelativeFinalDisplacementError( final_pos_err, arc_length ),
		BubbleSH_RelativeAverageChamferDistance( mean_chamfer, surface_change ) );
}

static void BubbleSH_W1Demo_f( void )
{
	static const float truth[] = { 0.0f, 0.5f, 1.0f, 2.0f, 3.0f, 3.5f };
	static const float pred[] =  { 0.3f, 0.9f, 1.5f, 2.2f, 2.9f, 4.1f };

	Com_Printf( "[BubbleSH] demo W1=%.4f normalizedW1=%.4f (IQR-normalized)\n",
		BubbleSH_Wasserstein1( pred, truth, 6 ),
		BubbleSH_NormalizedWasserstein1( pred, truth, 6 ) );
}

void BubbleSH_Init( void )
{
	cl_bubblesh = Cvar_Get( "cl_bubblesh", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_bubblesh,
		"Enable BubbleSH dataset characterization commands (Ramesh et al., arXiv:2607.07275)." );

	if ( !cl_bubblesh->integer ) {
		return;
	}

	if ( !bubblesh_cmds_registered ) {
		Cmd_AddCommand( "bubblesh_status", BubbleSH_Status_f );
		Cmd_AddCommand( "bubblesh_case", BubbleSH_Case_f );
		Cmd_AddCommand( "bubblesh_orders", BubbleSH_Orders_f );
		Cmd_AddCommand( "bubblesh_metrics", BubbleSH_Metrics_f );
		Cmd_AddCommand( "bubblesh_w1_demo", BubbleSH_W1Demo_f );
		bubblesh_cmds_registered = qtrue;
	}

	Com_Printf( "[BubbleSH] Dataset tools enabled (cl_bubblesh 1)\n" );
}
