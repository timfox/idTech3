/*
===========================================================================
VGS console — McGraw MIG 2024 Gram-Schmidt voxel scaffold.
===========================================================================
*/

#include "vgs/vgs.h"

#include "qcommon/qcommon.h"

#include <stdio.h>
#include <string.h>

static cvar_t *cl_vgs_enable;
static qboolean vgs_console_registered = qfalse;

static qboolean Vgs_Enabled( void )
{
	if ( !cl_vgs_enable || !cl_vgs_enable->integer ) {
		Com_Printf( "[vgs] disabled (cl_vgs_enable 0)\n" );
		return qfalse;
	}
	return qtrue;
}

static const char *Vgs_StatusName( vgs_status_t s )
{
	switch ( s ) {
	case VGS_STATUS_PRESENT:
		return "present";
	case VGS_STATUS_PARTIAL:
		return "partial";
	default:
		return "absent";
	}
}

static void Vgs_Cmd_Paper_f( void )
{
	if ( !Vgs_Enabled() ) {
		return;
	}
	Com_Printf( "[vgs] %s\n", Vgs_PaperCite() );
	Com_Printf( "[vgs] Voxel Gram-Schmidt + breakable face constraints for soft bodies.\n" );
	Com_Printf( "[vgs] Scaffold + CPU Alg.1 — not a world solver (see docs/VGS.md).\n" );
}

static void Vgs_Cmd_Status_f( void )
{
	Com_Printf( "[vgs] cl_vgs_enable=%d stages=%d gaps=%d\n",
		cl_vgs_enable ? cl_vgs_enable->integer : 0,
		Vgs_StageCount(), Vgs_GapCount() );
	Com_Printf( "[vgs] commands: vgs_paper vgs_pipeline vgs_constants\n" );
	Com_Printf( "[vgs]           vgs_gaps vgs_advice vgs_status\n" );
}

static void Vgs_Cmd_Pipeline_f( void )
{
	int i;

	if ( !Vgs_Enabled() ) {
		return;
	}
	Com_Printf( "[vgs] MIG'24 pipeline:\n" );
	for ( i = 0; i < Vgs_StageCount(); i++ ) {
		const vgs_stage_t *s = Vgs_GetStage( i );
		Com_Printf( "  %2d  %-10s  %s\n", s->id, s->name, s->summary );
	}
}

static void Vgs_Cmd_Constants_f( void )
{
	if ( !Vgs_Enabled() ) {
		return;
	}
	Com_Printf( "[vgs] α=%.2f (relaxed GS)  β=%.2f (edge blend)  vgs_it=%d\n",
		Vgs_DefaultAlpha(), Vgs_DefaultBeta(), Vgs_DefaultIters() );
	Com_Printf( "[vgs] face partitions=%d  VGS partitions=%d  face constraint=%d bytes\n",
		Vgs_FacePartitionCount(), Vgs_VgsPartitionCount(), Vgs_FaceConstraintBytes() );
	Com_Printf( "[vgs] particle radius r=L/4; no shared particles across voxels\n" );
}

static void Vgs_Cmd_Gaps_f( void )
{
	int i;

	if ( !Vgs_Enabled() ) {
		return;
	}
	Com_Printf( "[vgs] MIG'24 feature vs engine (PHYSICS.md):\n" );
	for ( i = 0; i < Vgs_GapCount(); i++ ) {
		const vgs_gap_t *g = Vgs_GetGap( i );
		Com_Printf( "  %-32s  %-7s  %s\n",
			g->feature, Vgs_StatusName( g->status ), g->engineNote );
	}
}

static void Vgs_Cmd_Advice_f( void )
{
	const char *useCase = "soft";

	if ( !Vgs_Enabled() ) {
		return;
	}
	if ( Cmd_Argc() >= 2 ) {
		useCase = Cmd_Argv( 1 );
	}
	Com_Printf( "[vgs] advice (%s): %s\n", useCase, Vgs_SelectAdvice( useCase ) );
}

void Vgs_ConsoleInit( void )
{
	if ( vgs_console_registered ) {
		return;
	}

	cl_vgs_enable = Cvar_Get( "cl_vgs_enable", "0", CVAR_ARCHIVE );
	Cmd_AddCommand( "vgs_paper", Vgs_Cmd_Paper_f );
	Cmd_AddCommand( "vgs_pipeline", Vgs_Cmd_Pipeline_f );
	Cmd_AddCommand( "vgs_constants", Vgs_Cmd_Constants_f );
	Cmd_AddCommand( "vgs_gaps", Vgs_Cmd_Gaps_f );
	Cmd_AddCommand( "vgs_advice", Vgs_Cmd_Advice_f );
	Cmd_AddCommand( "vgs_status", Vgs_Cmd_Status_f );

	Com_Printf( "[vgs] McGraw MIG 2024 VGS (cl_vgs_enable %d)\n",
		cl_vgs_enable->integer );
	vgs_console_registered = qtrue;
}

void Vgs_ConsoleShutdown( void )
{
	vgs_console_registered = qfalse;
}
