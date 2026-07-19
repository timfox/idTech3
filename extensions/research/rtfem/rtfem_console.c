/*
===========================================================================
RTFEM console — Parker & O'Brien SCA 2009 scaffold.
===========================================================================
*/

#include "rtfem/rtfem.h"

#include "qcommon/qcommon.h"

#include <stdio.h>
#include <string.h>

static cvar_t *cl_rtfem_enable;
static qboolean rtfem_console_registered = qfalse;

static qboolean RtFem_Enabled( void )
{
	if ( !cl_rtfem_enable || !cl_rtfem_enable->integer ) {
		Com_Printf( "[rtfem] disabled (cl_rtfem_enable 0)\n" );
		return qfalse;
	}
	return qtrue;
}

static const char *RtFem_StatusName( rtfem_status_t s )
{
	switch ( s ) {
	case RTFEM_STATUS_PRESENT:
		return "present";
	case RTFEM_STATUS_PARTIAL:
		return "partial";
	default:
		return "absent";
	}
}

static void RtFem_Cmd_Paper_f( void )
{
	if ( !RtFem_Enabled() ) {
		return;
	}
	Com_Printf( "[rtfem] %s\n", RtFem_PaperCite() );
	Com_Printf( "[rtfem] Corotational tet FEM + fracture for real-time games.\n" );
	Com_Printf( "[rtfem] Scaffold only — no tet FEM solve (see docs/RTFEM.md).\n" );
}

static void RtFem_Cmd_Status_f( void )
{
	Com_Printf( "[rtfem] cl_rtfem_enable=%d stages=%d gaps=%d\n",
		cl_rtfem_enable ? cl_rtfem_enable->integer : 0,
		RtFem_StageCount(), RtFem_GapCount() );
	Com_Printf( "[rtfem] commands: rtfem_paper rtfem_pipeline rtfem_constants\n" );
	Com_Printf( "[rtfem]           rtfem_gaps rtfem_advice rtfem_status\n" );
}

static void RtFem_Cmd_Pipeline_f( void )
{
	int i;

	if ( !RtFem_Enabled() ) {
		return;
	}
	Com_Printf( "[rtfem] SCA09 pipeline:\n" );
	for ( i = 0; i < RtFem_StageCount(); i++ ) {
		const rtfem_stage_t *s = RtFem_GetStage( i );
		Com_Printf( "  %2d  %-10s  %s\n", s->id, s->name, s->summary );
	}
}

static void RtFem_Cmd_Constants_f( void )
{
	if ( !RtFem_Enabled() ) {
		return;
	}
	Com_Printf( "[rtfem] invert volume threshold=%.2f (QR switch)\n",
		RtFem_InvertVolumeThreshold() );
	Com_Printf( "[rtfem] CG relative error=%.4f\n", RtFem_CgRelError() );
	Com_Printf( "[rtfem] fracture min face-connected tets=%d\n", RtFem_FractureMinTets() );
	Com_Printf( "[rtfem] fast-object CCD move fraction=1/%.0f bbox diagonal\n",
		1.0f / RtFem_FastObjectMoveFraction() );
	Com_Printf( "[rtfem] large island: nodes>=60 and nodes > liveTotal/4\n" );
	Com_Printf( "[rtfem] example: island=80 live=200 → large=%d\n",
		RtFem_LargeIslandHeuristic( 80, 200 ) );
}

static void RtFem_Cmd_Gaps_f( void )
{
	int i;

	if ( !RtFem_Enabled() ) {
		return;
	}
	Com_Printf( "[rtfem] SCA09 feature vs engine (PHYSICS.md):\n" );
	for ( i = 0; i < RtFem_GapCount(); i++ ) {
		const rtfem_gap_t *g = RtFem_GetGap( i );
		Com_Printf( "  %-28s  %-7s  %s\n",
			g->feature, RtFem_StatusName( g->status ), g->engineNote );
	}
}

static void RtFem_Cmd_Advice_f( void )
{
	const char *useCase = "design";

	if ( !RtFem_Enabled() ) {
		return;
	}
	if ( Cmd_Argc() >= 2 ) {
		useCase = Cmd_Argv( 1 );
	}
	Com_Printf( "[rtfem] advice (%s): %s\n", useCase, RtFem_SelectAdvice( useCase ) );
}

void RtFem_ConsoleInit( void )
{
	if ( rtfem_console_registered ) {
		return;
	}

	cl_rtfem_enable = Cvar_Get( "cl_rtfem_enable", "0", CVAR_ARCHIVE );
	Cmd_AddCommand( "rtfem_paper", RtFem_Cmd_Paper_f );
	Cmd_AddCommand( "rtfem_pipeline", RtFem_Cmd_Pipeline_f );
	Cmd_AddCommand( "rtfem_constants", RtFem_Cmd_Constants_f );
	Cmd_AddCommand( "rtfem_gaps", RtFem_Cmd_Gaps_f );
	Cmd_AddCommand( "rtfem_advice", RtFem_Cmd_Advice_f );
	Cmd_AddCommand( "rtfem_status", RtFem_Cmd_Status_f );

	Com_Printf( "[rtfem] Parker & O'Brien SCA 2009 (cl_rtfem_enable %d)\n",
		cl_rtfem_enable->integer );
	rtfem_console_registered = qtrue;
}

void RtFem_ConsoleShutdown( void )
{
	rtfem_console_registered = qfalse;
}
