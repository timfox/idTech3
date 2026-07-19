/*
===========================================================================
CEM console — Xie et al. arXiv:2508.04076 Crack Element Method scaffold.
===========================================================================
*/

#include "cem/cem.h"

#include "qcommon/qcommon.h"

#include <stdio.h>
#include <string.h>

static cvar_t *cl_cem_enable;
static qboolean cem_console_registered = qfalse;

static qboolean Cem_Enabled( void )
{
	if ( !cl_cem_enable || !cl_cem_enable->integer ) {
		Com_Printf( "[cem] disabled (cl_cem_enable 0)\n" );
		return qfalse;
	}
	return qtrue;
}

static const char *Cem_StatusName( cem_status_t s )
{
	switch ( s ) {
	case CEM_STATUS_PRESENT:
		return "present";
	case CEM_STATUS_PARTIAL:
		return "partial";
	default:
		return "absent";
	}
}

static void Cem_Cmd_Paper_f( void )
{
	if ( !Cem_Enabled() ) {
		return;
	}
	Com_Printf( "[cem] %s\n", Cem_PaperCite() );
	Com_Printf( "[cem] ES-FEM element-split + topology-based fracture energy G.\n" );
	Com_Printf( "[cem] Scaffold + CPU G_I/G_II — not a world solver (see docs/CEM.md).\n" );
}

static void Cem_Cmd_Status_f( void )
{
	Com_Printf( "[cem] cl_cem_enable=%d stages=%d patterns=%d gaps=%d\n",
		cl_cem_enable ? cl_cem_enable->integer : 0,
		Cem_StageCount(), Cem_PatternCount(), Cem_GapCount() );
	Com_Printf( "[cem] commands: cem_paper cem_pipeline cem_patterns\n" );
	Com_Printf( "[cem]           cem_gaps cem_advice cem_status\n" );
}

static void Cem_Cmd_Pipeline_f( void )
{
	int i;

	if ( !Cem_Enabled() ) {
		return;
	}
	Com_Printf( "[cem] arXiv:2508.04076 pipeline:\n" );
	for ( i = 0; i < Cem_StageCount(); i++ ) {
		const cem_stage_t *s = Cem_GetStage( i );
		Com_Printf( "  %2d  %-10s  %s\n", s->id, s->name, s->summary );
	}
}

static void Cem_Cmd_Patterns_f( void )
{
	int i;

	if ( !Cem_Enabled() ) {
		return;
	}
	Com_Printf( "[cem] crack patterns (tet I–II, hex III–VI):\n" );
	for ( i = 0; i < Cem_PatternCount(); i++ ) {
		const cem_pattern_t *p = Cem_GetPattern( i );
		Com_Printf( "  %-3s  %-3s  %s\n", p->name, p->element, p->summary );
	}
	Com_Printf( "[cem] Kalthoff Gc=%.3e J/m^2  E=%.0f GPa  branch-plate Gc=%.1f\n",
		Cem_KalthoffGc(), Cem_KalthoffYoungGPa(), Cem_BranchingPlateGc() );
	Com_Printf( "[cem] Neumann branching mesh floor ≈ %d tets\n",
		Cem_NeumannBranchMeshFloor() );
}

static void Cem_Cmd_Gaps_f( void )
{
	int i;

	if ( !Cem_Enabled() ) {
		return;
	}
	Com_Printf( "[cem] CEM feature vs engine (PHYSICS.md):\n" );
	for ( i = 0; i < Cem_GapCount(); i++ ) {
		const cem_gap_t *g = Cem_GetGap( i );
		Com_Printf( "  %-32s  %-7s  %s\n",
			g->feature, Cem_StatusName( g->status ), g->engineNote );
	}
}

static void Cem_Cmd_Advice_f( void )
{
	const char *useCase = "branch";

	if ( !Cem_Enabled() ) {
		return;
	}
	if ( Cmd_Argc() >= 2 ) {
		useCase = Cmd_Argv( 1 );
	}
	Com_Printf( "[cem] advice (%s): %s\n", useCase, Cem_SelectAdvice( useCase ) );
}

void Cem_ConsoleInit( void )
{
	if ( cem_console_registered ) {
		return;
	}

	cl_cem_enable = Cvar_Get( "cl_cem_enable", "0", CVAR_ARCHIVE );
	Cmd_AddCommand( "cem_paper", Cem_Cmd_Paper_f );
	Cmd_AddCommand( "cem_pipeline", Cem_Cmd_Pipeline_f );
	Cmd_AddCommand( "cem_patterns", Cem_Cmd_Patterns_f );
	Cmd_AddCommand( "cem_gaps", Cem_Cmd_Gaps_f );
	Cmd_AddCommand( "cem_advice", Cem_Cmd_Advice_f );
	Cmd_AddCommand( "cem_status", Cem_Cmd_Status_f );

	Com_Printf( "[cem] Xie et al. arXiv:2508.04076 CEM (cl_cem_enable %d)\n",
		cl_cem_enable->integer );
	cem_console_registered = qtrue;
}

void Cem_ConsoleShutdown( void )
{
	cem_console_registered = qfalse;
}
