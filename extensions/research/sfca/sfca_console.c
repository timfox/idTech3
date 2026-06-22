/*
===========================================================================
SFCA console commands.
===========================================================================
*/

#include "sfca/sfca.h"

#include "qcommon/qcommon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cvar_t *cl_sfca_enable;
static qboolean sfca_console_registered = qfalse;

static void SFCA_Cmd_Info_f( void )
{
	Com_Printf( "[SFCA] Separable-Field Cellular Automaton (Shi & Huang)\n" );
	Com_Printf( "[SFCA] q = n/nmax, n = R_j C_i, survival S and birth B intervals\n" );
	Com_Printf( "[SFCA] cvars: cl_sfca_enable\n" );
	Com_Printf( "[SFCA] commands: sfca_info sfca_run sfca_batch sfca_phase sfca_transition\n" );
	Com_Printf( "[SFCA]            sfca_fingerprints sfca_survival sfca_damage\n" );
}

static void SFCA_Cmd_Run_f( void )
{
	sfca_run_params_t p;
	sfca_run_result_t r;
	const char *outcomeName;

	if ( !cl_sfca_enable || !cl_sfca_enable->integer ) {
		Com_Printf( "[SFCA] disabled (cl_sfca_enable 0)\n" );
		return;
	}

	memset( &p, 0, sizeof( p ) );
	p.height = 75;
	p.width = 100;
	p.rho0 = 0.25f;
	p.maxGenerations = 2000;
	p.seed = 0x5FCAu;
	SFCA_DefaultRepresentativeRule( &p.intervals );

	if ( Cmd_Argc() >= 2 ) {
		p.maxGenerations = atoi( Cmd_Argv( 1 ) );
	}

	SFCA_Run( &p, &r );

	switch ( r.outcome ) {
	case SFCA_OUTCOME_EXTINCTION: outcomeName = "extinction"; break;
	case SFCA_OUTCOME_FIXED_POINT: outcomeName = "fixed"; break;
	case SFCA_OUTCOME_CYCLE: outcomeName = "cycle"; break;
	default: outcomeName = "long_transient"; break;
	}

	Com_Printf( "[SFCA] outcome=%s gen=%d period=%d rho=%.4f chi=%.4f stripe=%.4f geom=%d\n",
		outcomeName, r.terminalGeneration, r.period, r.finalDensity, r.meanChangeRate, r.stripeScore,
		(int)SFCA_IntervalGeometry( &p.intervals ) );
}

static void SFCA_Cmd_Batch_f( void )
{
	sfca_run_params_t p;
	sfca_outcome_stats_t stats;
	int n = 1000;

	if ( !cl_sfca_enable || !cl_sfca_enable->integer ) {
		Com_Printf( "[SFCA] disabled (cl_sfca_enable 0)\n" );
		return;
	}

	memset( &p, 0, sizeof( p ) );
	p.height = 30;
	p.width = 40;
	p.rho0 = 0.25f;
	p.maxGenerations = 2000;
	p.seed = 0xCAFEu;
	SFCA_DefaultRepresentativeRule( &p.intervals );

	if ( Cmd_Argc() >= 2 ) {
		n = atoi( Cmd_Argv( 1 ) );
	}

	SFCA_RunBatch( &p, n, &stats, NULL );
	Com_Printf( "[SFCA] batch n=%d rep rule: ext=%.1f%% fix=%.1f%% cyc=%.1f%% lt=%.1f%%\n",
		stats.numRuns, stats.extinction * 100.0f, stats.fixedPoint * 100.0f,
		stats.cycle * 100.0f, stats.longTransient * 100.0f );
}

static void SFCA_Cmd_Phase_f( void )
{
	sfca_run_params_t p;
	sfca_outcome_stats_t stats;
	int wS;
	int wB;

	if ( !cl_sfca_enable || !cl_sfca_enable->integer ) {
		return;
	}

	memset( &p, 0, sizeof( p ) );
	p.height = 20;
	p.width = 30;
	p.rho0 = 0.25f;
	p.maxGenerations = 500;
	p.seed = 0xFACEu;

	Com_Printf( "[SFCA] coarse phase sample (deltaLow=5/18, sLow=1/18, 8 runs/rule)\n" );
	Com_Printf( "  wS wB  ext%%  fix%%  cyc%%  LT%%  geom\n" );
	for ( wS = 2; wS <= 8; wS += 2 ) {
		for ( wB = 2; wB <= 8; wB += 2 ) {
			sfca_intervals_t iv;
			SFCA_GeometryWidthRule( wS, wB, 5, 1, &iv );
			p.intervals = iv;
			p.seed = (unsigned)( 0x1000u + (unsigned)wS * 31u + (unsigned)wB );
			SFCA_ScanWidthCellBatch( &p, wS, wB, 8, &stats );
			Com_Printf( "  %2d %2d %5.1f %5.1f %5.1f %5.1f  %d\n",
				wS, wB, stats.extinction * 100.0f, stats.fixedPoint * 100.0f,
				stats.cycle * 100.0f, stats.longTransient * 100.0f,
				(int)SFCA_IntervalGeometry( &iv ) );
		}
	}
}

static void SFCA_Cmd_Transition_f( void )
{
	sfca_run_params_t p;
	sfca_transition_scan_t scan[16];
	int n;
	int i;

	if ( !cl_sfca_enable || !cl_sfca_enable->integer ) {
		return;
	}

	memset( &p, 0, sizeof( p ) );
	p.height = 20;
	p.width = 30;
	p.rho0 = 0.25f;
	p.maxGenerations = 500;
	p.seed = 1u;

	SFCA_ScanTransitionAxis( &p, 45, 75, 5, 120, scan, 16 );
	n = 7;

	Com_Printf( "[SFCA] canonical transition axis: wS -> cycle%% / LT%%\n" );
	for ( i = 0; i < n; i++ ) {
		Com_Printf( "  wS=%3d/180  cycle=%5.1f%%  LT=%5.1f%%  geom=%d\n",
			scan[i].wSLvl, scan[i].outcomes.cycle * 100.0f,
			scan[i].outcomes.longTransient * 100.0f, (int)scan[i].geometry );
	}
}

static void SFCA_Cmd_Fingerprints_f( void )
{
	sfca_run_params_t p;
	sfca_cycle_fingerprint_t dense;
	sfca_cycle_fingerprint_t sparse;

	if ( !cl_sfca_enable || !cl_sfca_enable->integer ) {
		return;
	}

	memset( &p, 0, sizeof( p ) );
	p.height = 20;
	p.width = 30;
	p.rho0 = 0.25f;
	p.maxGenerations = 800;
	p.seed = 0xDEu;

	SFCA_CanonicalTransitionRule( 55.0f / (float)SFCA_FINE_LEVELS, &p.intervals );
	SFCA_CycleFingerprints( &p, 300, &dense );

	p.seed = 0x510u;
	SFCA_CanonicalTransitionRule( 70.0f / (float)SFCA_FINE_LEVELS, &p.intervals );
	SFCA_CycleFingerprints( &p, 300, &sparse );

	Com_Printf( "[SFCA] cycle fingerprints (low wS=55 vs high wS=70, reduced grid)\n" );
	Com_Printf( "  dense  n=%d rho=%.3f chi=%.3f stripe=%.3f\n",
		dense.count, dense.meanDensity, dense.meanChangeRate, dense.meanStripe );
	Com_Printf( "  sparse n=%d rho=%.3f chi=%.3f stripe=%.3f\n",
		sparse.count, sparse.meanDensity, sparse.meanChangeRate, sparse.meanStripe );
}

static void SFCA_Cmd_Survival_f( void )
{
	sfca_run_params_t p;
	const int gens[] = { 50, 100, 200, 400, 800 };
	float surv[5];
	int wIdx;
	int t;

	if ( !cl_sfca_enable || !cl_sfca_enable->integer ) {
		return;
	}

	memset( &p, 0, sizeof( p ) );
	p.height = 16;
	p.width = 20;
	p.rho0 = 0.25f;
	p.maxGenerations = 800;
	p.seed = 0xAB0u;

	Com_Printf( "[SFCA] Kaplan-Meier S(t) for wS=50/60/70 (120 runs each)\n" );
	for ( wIdx = 0; wIdx < 3; wIdx++ ) {
		const int wS = ( wIdx == 0 ) ? 50 : ( ( wIdx == 1 ) ? 60 : 70 );
		SFCA_CanonicalTransitionRule( (float)wS / (float)SFCA_FINE_LEVELS, &p.intervals );
		p.seed = (unsigned)( 0xAB00u + (unsigned)wS );
		SFCA_KaplanMeier( &p, 120, gens, 5, surv );
		Com_Printf( "  wS=%3d/180:", wS );
		for ( t = 0; t < 5; t++ ) {
			Com_Printf( " S(%d)=%.2f", gens[t], surv[t] );
		}
		Com_Printf( "\n" );
	}
}

static void SFCA_Cmd_Damage_f( void )
{
	sfca_run_params_t p;
	sfca_damage_scan_t scan[16];
	int n;
	int i;
	float dMax = 0.0f;
	int wPeak = 0;

	memset( &p, 0, sizeof( p ) );
	p.height = 20;
	p.width = 30;
	p.rho0 = 0.25f;
	p.maxGenerations = 500;
	p.seed = 0xDA0u;

	SFCA_ScanDamageAxis( &p, 45, 75, 5, 40, scan, 16 );
	n = 7;

	Com_Printf( "[SFCA] damage plateau dH vs wS (40 pairs/point)\n" );
	for ( i = 0; i < n; i++ ) {
		Com_Printf( "  wS=%3d/180  dH=%.4f\n", scan[i].wSLvl, scan[i].plateauHamming );
		if ( scan[i].plateauHamming > dMax ) {
			dMax = scan[i].plateauHamming;
			wPeak = scan[i].wSLvl;
		}
	}
	Com_Printf( "[SFCA] peak separation near wS=%d/180 (dH=%.4f)\n", wPeak, dMax );
}

void SFCA_ConsoleInit( void )
{
	if ( sfca_console_registered ) {
		return;
	}

	cl_sfca_enable = Cvar_Get( "cl_sfca_enable", "0", CVAR_ARCHIVE );
	Cmd_AddCommand( "sfca_info", SFCA_Cmd_Info_f );
	Cmd_AddCommand( "sfca_run", SFCA_Cmd_Run_f );
	Cmd_AddCommand( "sfca_batch", SFCA_Cmd_Batch_f );
	Cmd_AddCommand( "sfca_phase", SFCA_Cmd_Phase_f );
	Cmd_AddCommand( "sfca_transition", SFCA_Cmd_Transition_f );
	Cmd_AddCommand( "sfca_fingerprints", SFCA_Cmd_Fingerprints_f );
	Cmd_AddCommand( "sfca_survival", SFCA_Cmd_Survival_f );
	Cmd_AddCommand( "sfca_damage", SFCA_Cmd_Damage_f );

	Com_Printf( "[SFCA] Separable-Field CA module (cl_sfca_enable %d)\n", cl_sfca_enable->integer );
	sfca_console_registered = qtrue;
}

void SFCA_ConsoleShutdown( void )
{
	sfca_console_registered = qfalse;
}
