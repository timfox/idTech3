/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Miscellaneous client console commands (info, fs lists, UI open, etc.).
===========================================================================
*/

#include "client.h"
#include "cl_cmds.h"
#include "cl_ref.h"
#include "../../qcommon/script_emit.h"

#include <string.h>

static qboolean CL_SetActiveMenuByName( const char *name ) {
	int menu = -1;

	if ( !uivm || !name || !name[0] ) {
		return qfalse;
	}

	if ( !Q_stricmp( name, "none" ) || !Q_stricmp( name, "close" ) ) {
		menu = UIMENU_NONE;
	} else if ( !Q_stricmp( name, "main" ) || !Q_stricmp( name, "menu" ) || !Q_stricmp( name, "home" ) ) {
		menu = UIMENU_MAIN;
	} else if ( !Q_stricmp( name, "ingame" ) || !Q_stricmp( name, "pause" ) ) {
		menu = UIMENU_INGAME;
	} else if ( !Q_stricmp( name, "need_cd" ) || !Q_stricmp( name, "needcd" ) ) {
		menu = UIMENU_NEED_CD;
	} else if ( !Q_stricmp( name, "bad_cd_key" ) || !Q_stricmp( name, "badcdkey" ) ) {
		menu = UIMENU_BAD_CD_KEY;
	} else if ( !Q_stricmp( name, "team" ) ) {
		menu = UIMENU_TEAM;
	} else if ( !Q_stricmp( name, "postgame" ) ) {
		menu = UIMENU_POSTGAME;
	}

	if ( menu < 0 ) {
		return qfalse;
	}

	VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, menu );
	CL_JsNotifyMenuChanged( menu );
	return qtrue;
}

static void CL_Open_f( void ) {
	char target[MAX_TOKEN_CHARS];

	if ( Cmd_Argc() < 2 ) {
		if ( !CL_SetActiveMenuByName( "main" ) ) {
			Com_Printf( "open: UI is not available\n" );
		}
		return;
	}

	Q_strncpyz( target, Cmd_Argv( 1 ), sizeof( target ) );
	Q_CleanStr( target );

	if ( !target[0] ) {
		Com_Printf( "open: empty target\n" );
		return;
	}

	/* First handle direct menu ids locally. */
	if ( CL_SetActiveMenuByName( target ) ) {
		Cvar_Set( "ui_open_tab", "" );
		return;
	}

	/* Preserve the VM-driven UI path for custom menu commands/scripts. */
	if ( uivm && UI_GameCommand() ) {
		return;
	}

	/* Keep legacy JS/C# "open <tab>" flows alive by advertising a menu_changed event. */
	Com_ScriptEmitEvent( "menu_changed", target, NULL, -1, 0 );

	/* Common tabs live under main menu in most UI scripts. Set ui_open_tab
	 * so the UI can switch to the requested tab when main menu opens. */
	if ( !Q_stricmp( target, "credits" ) || !Q_stricmp( target, "audio" ) || !Q_stricmp( target, "gameplay" ) ) {
		Cvar_Set( "ui_open_tab", target );
		CL_SetActiveMenuByName( "main" );
		return;
	}

	Cvar_Set( "ui_open_tab", "" );
	Com_Printf( "open: unhandled target '%s'\n", target );
}
static void CL_SetPlayerName_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: %s <name>\n", Cmd_Argv( 0 ) );
		return;
	}

	Cvar_Set( "name", Cmd_ArgsFrom( 1 ) );
}
/*
=================
CL_Snd_Restart_f

Restart the sound subsystem
The cgame and game must also be forced to restart because
handles will be invalid
=================
*/
static void CL_Snd_Restart_f( void )
{
	S_Shutdown();

	// sound will be reinitialized by vid_restart
	CL_Ref_VidRestart( REF_KEEP_CONTEXT /*REF_KEEP_WINDOW*/ );
}


/*
==================
CL_PK3List_f
==================
*/
static void CL_OpenedPK3List_f( void ) {
	Com_Printf("Opened PK3 Names: %s\n", FS_LoadedPakNames());
}


/*
==================
CL_PureList_f
==================
*/
static void CL_ReferencedPK3List_f( void ) {
	Com_Printf( "Referenced PK3 Names: %s\n", FS_ReferencedPakNames() );
}


/*
==================
CL_Configstrings_f
==================
*/
static void CL_Configstrings_f( void ) {
	int		i;
	int		ofs;

	if ( cls.state != CA_ACTIVE ) {
		Com_Printf( "Not connected to a server.\n");
		return;
	}

	for ( i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		ofs = cl.gameState.stringOffsets[ i ];
		if ( !ofs ) {
			continue;
		}
		Com_Printf( "%4i: %s\n", i, cl.gameState.stringData + ofs );
	}
}


/*
==============
CL_Clientinfo_f
==============
*/
static void CL_Clientinfo_f( void ) {
	Com_Printf( "--------- Client Information ---------\n" );
	Com_Printf( "state: %i\n", cls.state );
	Com_Printf( "Server: %s\n", cls.servername );
	Com_Printf ("User info settings:\n");
	Info_Print( Cvar_InfoString( CVAR_USERINFO, NULL ) );
	Com_Printf( "--------------------------------------\n" );
}


/*
==============
CL_Serverinfo_f
==============
*/
static void CL_Serverinfo_f( void ) {
	int		ofs;

	ofs = cl.gameState.stringOffsets[ CS_SERVERINFO ];
	if ( !ofs )
		return;

	Com_Printf( "Server info settings:\n" );
	Info_Print( cl.gameState.stringData + ofs );
}


/*
===========
CL_Systeminfo_f
===========
*/
static void CL_Systeminfo_f( void ) {
	int ofs;

	ofs = cl.gameState.stringOffsets[ CS_SYSTEMINFO ];
	if ( !ofs )
		return;

	Com_Printf( "System info settings:\n" );
	Info_Print( cl.gameState.stringData + ofs );
}
static void CL_SetModel_f( void ) {
	const char *arg;
	char name[ MAX_CVAR_VALUE_STRING ];

	arg = Cmd_Argv( 1 );
	if ( arg[0] ) {
		Cvar_Set( "model", arg );
		Cvar_Set( "headmodel", arg );
	} else {
		Cvar_VariableStringBuffer( "model", name, sizeof( name ) );
		Com_Printf( "model is set to %s\n", name );
	}
}

static void CL_FovAlias_f( void ) {
	if ( Cmd_Argc() > 1 ) {
		Cvar_Set( "cg_fov", Cmd_Argv( 1 ) );
		return;
	}

	Com_Printf( "cg_fov is \"%s\"\n", Cvar_VariableString( "cg_fov" ) );
}
static void CL_ShowIP_f( void ) {
	Sys_ShowIP();
}


void CL_Cmds_Init( void ) {
	Cmd_AddCommand( "configstrings", CL_Configstrings_f );
	Cmd_AddCommand( "clientinfo", CL_Clientinfo_f );
	Cmd_AddCommand( "snd_restart", CL_Snd_Restart_f );
	Cmd_AddCommand( "cinematic", CL_PlayCinematic_f );
	Cmd_AddCommand( "showip", CL_ShowIP_f );
	Cmd_AddCommand( "fs_openedList", CL_OpenedPK3List_f );
	Cmd_AddCommand( "fs_referencedList", CL_ReferencedPK3List_f );
	Cmd_AddCommand( "model", CL_SetModel_f );
	Cmd_AddCommand( "r_fov", CL_FovAlias_f );
	Cmd_AddCommand( "serverinfo", CL_Serverinfo_f );
	Cmd_AddCommand( "systeminfo", CL_Systeminfo_f );
	Cmd_AddCommand( "playername", CL_SetPlayerName_f );
	Cmd_AddCommand( "setname", CL_SetPlayerName_f );
	Cmd_AddCommand( "open", CL_Open_f );
}

void CL_Cmds_Shutdown( void ) {
	Cmd_RemoveCommand( "configstrings" );
	Cmd_RemoveCommand( "userinfo" );
	Cmd_RemoveCommand( "clientinfo" );
	Cmd_RemoveCommand( "snd_restart" );
	Cmd_RemoveCommand( "cinematic" );
	Cmd_RemoveCommand( "showip" );
	Cmd_RemoveCommand( "fs_openedList" );
	Cmd_RemoveCommand( "fs_referencedList" );
	Cmd_RemoveCommand( "model" );
	Cmd_RemoveCommand( "r_fov" );
	Cmd_RemoveCommand( "serverinfo" );
	Cmd_RemoveCommand( "systeminfo" );
	Cmd_RemoveCommand( "open" );
}
