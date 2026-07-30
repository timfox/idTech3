/*
===========================================================================
Native RTS session shell.

The simulation module owns selection and orders. This client adapter exposes
native console commands and draws a deliberately small 0 A.D.-inspired
session HUD. JavaScript gets the scriptable UI surface from idtech3.rts.
===========================================================================
*/

#include "client.h"
#include "cl_rts_gui.h"
#include "../../../modules/rts/rts_public.h"

static cvar_t *cl_rtsHud;

static void CL_RTSGui_SelectAll_f( void ) {
	const int selected = RTS_GuiSelectRect( RTS_OWNER_PLAYER1, -16384, -16384, 16384, 16384 );
	Com_Printf( "rts_gui: selected %d player-1 entities\n", selected );
}

static void CL_RTSGui_Clear_f( void ) {
	RTS_GuiClearSelection( RTS_OWNER_PLAYER1 );
	Com_Printf( "rts_gui: selection cleared\n" );
}

static void CL_RTSGui_Move_f( void ) {
	int posted;

	if ( Cmd_Argc() != 3 ) {
		Com_Printf( "usage: rts_gui_move <x> <y>\n" );
		return;
	}
	posted = RTS_GuiIssueMoveSelected( RTS_OWNER_PLAYER1, atoi( Cmd_Argv( 1 ) ), atoi( Cmd_Argv( 2 ) ) );
	Com_Printf( "rts_gui: posted %d move orders\n", posted );
}

static void CL_RTSGui_Status_f( void ) {
	rtsGuiState_t state;

	if ( !RTS_GuiGetState( RTS_OWNER_PLAYER1, &state ) ) {
		Com_Printf( "rts_gui: unavailable\n" );
		return;
	}
	Com_Printf( "rts_gui: turn=%d entities=%d selected=%d primary=%d hp=%d resources=%d pending=%d\n",
		state.currentTurn, state.entityCount, state.selectedCount, state.primarySelection,
		state.primaryHitpoints, state.playerResources, state.pendingCommands );
}

void CL_RTSGui_Init( void ) {
	cl_rtsHud = Cvar_Get( "cl_rtsHud", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_rtsHud, "Draw the native RTS session HUD when the local RTS simulation has entities." );

	Cmd_RemoveCommand( "rts_gui_select_all" );
	Cmd_RemoveCommand( "rts_gui_clear" );
	Cmd_RemoveCommand( "rts_gui_move" );
	Cmd_RemoveCommand( "rts_gui_status" );
	Cmd_AddCommand( "rts_gui_select_all", CL_RTSGui_SelectAll_f );
	Cmd_AddCommand( "rts_gui_clear", CL_RTSGui_Clear_f );
	Cmd_AddCommand( "rts_gui_move", CL_RTSGui_Move_f );
	Cmd_AddCommand( "rts_gui_status", CL_RTSGui_Status_f );
}

void CL_RTSGui_Render( void ) {
	rtsGuiState_t state;
	const vec4_t panel = { 0.035f, 0.055f, 0.060f, 0.88f };
	const vec4_t border = { 0.48f, 0.35f, 0.12f, 0.9f };
	const vec4_t text = { 0.92f, 0.89f, 0.74f, 1.0f };
	const vec4_t health = { 0.25f, 0.78f, 0.31f, 0.95f };
	char line[128];
	float healthWidth;

	if ( !cl_rtsHud || !cl_rtsHud->integer || !RTS_GuiGetState( RTS_OWNER_PLAYER1, &state ) || !state.entityCount ) {
		return;
	}

	SCR_FillRect( 8.0f, 8.0f, 254.0f, 42.0f, panel );
	SCR_FillRect( 8.0f, 8.0f, 254.0f, 1.0f, border );
	Com_sprintf( line, sizeof( line ), "Resources %d    Turn %d    Units %d", state.playerResources, state.currentTurn, state.entityCount );
	SCR_DrawStringExt( 16, 18, 12.0f, line, text, qtrue, qtrue );

	SCR_FillRect( 8.0f, 402.0f, 258.0f, 70.0f, panel );
	SCR_FillRect( 8.0f, 402.0f, 258.0f, 1.0f, border );
	if ( !state.selectedCount ) {
		SCR_DrawStringExt( 16, 420, 12.0f, "No selection  [rts_gui_select_all]", text, qtrue, qtrue );
		return;
	}

	Com_sprintf( line, sizeof( line ), "Selected %d    Entity %d", state.selectedCount, state.primarySelection );
	SCR_DrawStringExt( 16, 414, 12.0f, line, text, qtrue, qtrue );
	SCR_FillRect( 16.0f, 438.0f, 232.0f, 10.0f, border );
	healthWidth = 232.0f * ( state.primaryHitpoints < 0 ? 0.0f : ( state.primaryHitpoints > 100 ? 1.0f : state.primaryHitpoints / 100.0f ) );
	SCR_FillRect( 16.0f, 438.0f, healthWidth, 10.0f, health );
	Com_sprintf( line, sizeof( line ), "HP %d     Move: rts_gui_move <x> <y>", state.primaryHitpoints );
	SCR_DrawStringExt( 16, 454, 10.0f, line, text, qtrue, qtrue );
}
