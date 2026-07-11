/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Miscellaneous client console commands (info, fs lists, UI open, etc.).
===========================================================================
*/

#include "client.h"
#include "cl_cmds.h"
#include "cl_ref.h"
#include "cl_p2p_session.h"
#include "../../qcommon/net_p2p.h"
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

static void CL_P2PStatus_f( void ) {
	char address[MAX_STRING_CHARS];

	Com_Printf( "P2P backend: %s\n", NET_P2P_BackendName() );
	Com_Printf( "P2P supported: %s\n", NET_P2P_IsSupported() ? "yes" : "no" );
	Com_Printf( "P2P enabled: %s\n", NET_P2P_IsEnabled() ? "yes" : "no" );
	Com_Printf( "P2P ready: %s\n", NET_P2P_IsReady() ? "yes" : "no" );

	if ( NET_P2P_GetLocalAddressString( address, sizeof( address ) ) ) {
		Com_Printf( "P2P address: %s\n", address );
	} else {
		Com_Printf( "P2P address: unavailable\n" );
	}

	if ( NET_P2P_IsSupported() ) {
		Com_Printf( "P2P notes: enable with net_p2p 1; use net_p2pBackend auto|steam_sdr|direct_udp. direct_udp can advertise via net_p2pAdvertiseAddress.\n" );
	}
}

static void CL_P2PAddress_f( void ) {
	char address[MAX_STRING_CHARS];

	if ( !NET_P2P_IsSupported() ) {
		Com_Printf( "P2P is not compiled in\n" );
		return;
	}

	if ( !NET_P2P_GetLocalAddressString( address, sizeof( address ) ) ) {
		Com_Printf( "P2P address unavailable; make sure net_p2p is enabled and configure Steam or net_p2pAdvertiseAddress for direct_udp\n" );
		return;
	}

	Com_Printf( "%s\n", address );
}

static void CL_P2PConnect_f( void ) {
	const char *peer;
	char address[MAX_STRING_CHARS];

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: p2p_connect <steamid|steam:steamid|host:port|udp:host:port>\n" );
		return;
	}

	if ( !NET_P2P_IsSupported() ) {
		Com_Printf( "P2P is not compiled in\n" );
		return;
	}

	if ( !NET_P2P_IsEnabled() ) {
		Com_Printf( "P2P is disabled; set net_p2p 1\n" );
		return;
	}

	peer = Cmd_Argv( 1 );
	if ( !NET_P2P_NormalizeAddressString( peer, address, sizeof( address ) ) ) {
		Com_Printf( "p2p_connect: expected a SteamID64, steam:SteamID64, host:port, or udp:host:port\n" );
		return;
	}

	NET_P2P_BeginConnectPath( address );
	CL_P2P_SessionOnConnect( "", address, "reconnect", 45 );
	Cbuf_AddText( va( "connect %s\n", address ) );
}

static void CL_P2PPunch_f( void ) {
	char address[MAX_STRING_CHARS];

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "usage: p2p_punch <host:port|udp:host:port>\n" );
		return;
	}

	if ( !NET_P2P_IsEnabled() ) {
		Com_Printf( "P2P is disabled; set net_p2p 1\n" );
		return;
	}

	if ( !NET_P2P_NormalizeAddressString( Cmd_Argv( 1 ), address, sizeof( address ) ) ||
	     Q_stricmpn( address, "udp:", 4 ) != 0 ) {
		Com_Printf( "p2p_punch: expected host:port or udp:host:port\n" );
		return;
	}

	NET_P2P_BeginPunchForAddress( address );
}

static void CL_P2PPunchStatus_f( void ) {
	NET_P2P_PrintPunchStatus();
}

static void CL_P2PCandidates_f( void ) {
	NET_P2P_PrintIceCandidates();
}

static serverInfo_t *CL_P2PBrowserServer( const char *source, int index, int *count ) {
	if ( count ) {
		*count = 0;
	}

	if ( !Q_stricmp( source, "local" ) ) {
		if ( count ) {
			*count = cls.numlocalservers;
		}
		if ( index >= 0 && index < cls.numlocalservers ) {
			return &cls.localServers[index];
		}
	} else if ( !Q_stricmp( source, "global" ) ) {
		if ( count ) {
			*count = cls.numglobalservers;
		}
		if ( index >= 0 && index < cls.numglobalservers ) {
			return &cls.globalServers[index];
		}
	} else if ( !Q_stricmp( source, "favorites" ) ) {
		if ( count ) {
			*count = cls.numfavoriteservers;
		}
		if ( index >= 0 && index < cls.numfavoriteservers ) {
			return &cls.favoriteServers[index];
		}
	}

	return NULL;
}

static qboolean CL_P2PServerMatches( const serverInfo_t *server ) {
	if ( !server ) {
		return qfalse;
	}
	if ( !server->adr.port && !server->p2pAddr[0] ) {
		return qfalse;
	}
	if ( !server->p2pAvailable && !server->p2pAddr[0] ) {
		return qfalse;
	}
	return qtrue;
}

static int CL_P2PServerSortPing( const serverInfo_t *server ) {
	if ( !server ) {
		return 999999;
	}
	if ( server->ping <= 0 ) {
		return 999999;
	}
	return server->ping;
}

static int CL_P2PServerBestIndex( serverInfo_t *servers, int count, qboolean *used ) {
	int bestIndex;
	int i;

	bestIndex = -1;

	for ( i = 0; i < count; i++ ) {
		if ( used[i] || !CL_P2PServerMatches( &servers[i] ) ) {
			continue;
		}

		if ( bestIndex < 0 ) {
			bestIndex = i;
			continue;
		}

		if ( CL_P2PServerSortPing( &servers[i] ) < CL_P2PServerSortPing( &servers[bestIndex] ) ) {
			bestIndex = i;
			continue;
		}

		if ( CL_P2PServerSortPing( &servers[i] ) == CL_P2PServerSortPing( &servers[bestIndex] ) &&
			Q_stricmp( servers[i].hostName, servers[bestIndex].hostName ) < 0 ) {
			bestIndex = i;
		}
	}

	return bestIndex;
}

static void CL_P2PListSource( const char *source, serverInfo_t *servers, int count ) {
	qboolean *used;
	int bestIndex;
	int matches;

	if ( !servers || count <= 0 ) {
		Com_Printf( "%-9s no P2P-capable cached servers\n", source );
		return;
	}

	used = (qboolean *)Z_Malloc( count * sizeof( *used ) );
	matches = 0;

	while ( ( bestIndex = CL_P2PServerBestIndex( servers, count, used ) ) >= 0 ) {
		serverInfo_t *server = &servers[bestIndex];
		used[bestIndex] = qtrue;

		Com_Printf(
			"%-9s %3d  ping:%4d  map:%-16s  host:%s\n",
			source,
			bestIndex,
			server->ping,
			server->mapName[0] ? server->mapName : "<unknown>",
			server->hostName[0] ? server->hostName : "<unnamed>"
		);
		Com_Printf(
			"           p2p:%s  udp:%s\n",
			server->p2pAddr[0] ? server->p2pAddr : "<unavailable>",
			NET_AdrToStringwPort( &server->adr )
		);
		Com_Printf(
			"           session:%s  proto:%d  reconnect:%ds  migrate:%s  secure:%s  failover:%s\n",
			server->p2pSessionId[0] ? server->p2pSessionId : "<auto>",
			server->protocol,
			server->p2pReconnectWindow,
			server->p2pHostMigration ? "yes" : "no",
			server->p2pAntiCheat[0] ? server->p2pAntiCheat : "unknown",
			server->p2pFailover[0] ? server->p2pFailover : "unknown"
		);
		matches++;
	}

	if ( !matches ) {
		Com_Printf( "%-9s no P2P-capable cached servers\n", source );
	} else {
		Com_Printf( "%-9s listed %d P2P-capable cached server%s (sorted by ping)\n",
			source, matches, matches == 1 ? "" : "s" );
	}

	Z_Free( used );
}

static void CL_P2PList_f( void ) {
	const char *source;

	if ( Cmd_Argc() > 2 ) {
		Com_Printf( "usage: p2p_list [local|global|favorites|master]\n" );
		return;
	}

	source = ( Cmd_Argc() == 2 ) ? Cmd_Argv( 1 ) : "all";

	if ( !Q_stricmp( source, "all" ) ) {
		CL_P2PListSource( "local", cls.localServers, cls.numlocalservers );
		CL_P2PListSource( "global", cls.globalServers, cls.numglobalservers );
		CL_P2PListSource( "favorites", cls.favoriteServers, cls.numfavoriteservers );
		return;
	}

	if ( !Q_stricmp( source, "local" ) ) {
		CL_P2PListSource( source, cls.localServers, cls.numlocalservers );
		return;
	}

	if ( !Q_stricmp( source, "global" ) ) {
		CL_P2PListSource( source, cls.globalServers, cls.numglobalservers );
		return;
	}

	if ( !Q_stricmp( source, "favorites" ) ) {
		CL_P2PListSource( source, cls.favoriteServers, cls.numfavoriteservers );
		return;
	}

	if ( !Q_stricmp( source, "master" ) ) {
		NET_P2P_BeginMasterList( NULL );
		return;
	}

	Com_Printf( "p2p_list: unknown source '%s'\n", source );
}

static void CL_P2PSessionInfo_f( void ) {
	serverInfo_t *server;
	int index;
	int count;
	const char *source;

	if ( Cmd_Argc() != 3 ) {
		Com_Printf( "usage: p2p_sessioninfo <local|global|favorites> <index>\n" );
		return;
	}

	source = Cmd_Argv( 1 );
	index = atoi( Cmd_Argv( 2 ) );
	server = CL_P2PBrowserServer( source, index, &count );
	if ( !server || index < 0 || index >= count ) {
		Com_Printf( "p2p_sessioninfo: no server at %s %d\n", source, index );
		return;
	}

	Com_Printf( "P2P session %s %d\n", source, index );
	Com_Printf( "  host: %s\n", server->hostName[0] ? server->hostName : "<unnamed>" );
	Com_Printf( "  map: %s\n", server->mapName[0] ? server->mapName : "<unknown>" );
	Com_Printf( "  p2p address: %s\n", server->p2pAddr[0] ? server->p2pAddr : "<unavailable>" );
	Com_Printf( "  session id: %s\n", server->p2pSessionId[0] ? server->p2pSessionId : "<auto>" );
	Com_Printf( "  protocol: %d\n", server->protocol );
	Com_Printf( "  reconnect window: %d seconds\n", server->p2pReconnectWindow );
	Com_Printf( "  host migration: %s\n", server->p2pHostMigration ? "supported" : "not advertised" );
	Com_Printf( "  anti-cheat posture: %s\n", server->p2pAntiCheat[0] ? server->p2pAntiCheat : "unknown" );
	Com_Printf( "  failure recovery: %s\n", server->p2pFailover[0] ? server->p2pFailover : "unknown" );
}

static void CL_P2PConnectBrowser_f( void ) {
	serverInfo_t *server;
	int index;
	const char *source;

	if ( Cmd_Argc() != 3 ) {
		Com_Printf( "usage: p2p_connect_browser <local|global|favorites> <index>\n" );
		return;
	}

	if ( !NET_P2P_IsEnabled() ) {
		Com_Printf( "P2P is disabled; set net_p2p 1\n" );
		return;
	}

	source = Cmd_Argv( 1 );
	index = atoi( Cmd_Argv( 2 ) );
	server = CL_P2PBrowserServer( source, index, NULL );
	if ( !server || ( server->adr.port == 0 && !server->p2pAddr[0] ) ) {
		Com_Printf( "p2p_connect_browser: no server at %s %d\n", source, index );
		return;
	}

	if ( server->p2pAddr[0] ) {
		CL_P2P_SessionOnConnect(
			server->p2pSessionId,
			server->p2pAddr,
			server->p2pFailover,
			server->p2pReconnectWindow );
		NET_P2P_BeginConnectPath( server->p2pAddr );
		Cbuf_AddText( va( "connect %s\n", server->p2pAddr ) );
		return;
	}

	CL_P2P_SessionOnConnect(
		server->p2pSessionId,
		NET_AdrToStringwPort( &server->adr ),
		server->p2pFailover,
		server->p2pReconnectWindow );
	NET_P2P_BeginConnectPath( NET_AdrToStringwPort( &server->adr ) );
	Cbuf_AddText( va( "connect %s\n", NET_AdrToStringwPort( &server->adr ) ) );
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
	Cmd_AddCommand( "p2p_status", CL_P2PStatus_f );
	Cmd_AddCommand( "p2p_address", CL_P2PAddress_f );
	Cmd_AddCommand( "p2p_connect", CL_P2PConnect_f );
	Cmd_AddCommand( "p2p_punch", CL_P2PPunch_f );
	Cmd_AddCommand( "p2p_punch_status", CL_P2PPunchStatus_f );
	Cmd_AddCommand( "p2p_candidates", CL_P2PCandidates_f );
	Cmd_AddCommand( "p2p_list", CL_P2PList_f );
	Cmd_AddCommand( "p2p_sessioninfo", CL_P2PSessionInfo_f );
	Cmd_AddCommand( "p2p_connect_browser", CL_P2PConnectBrowser_f );
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
	Cmd_RemoveCommand( "p2p_status" );
	Cmd_RemoveCommand( "p2p_address" );
	Cmd_RemoveCommand( "p2p_connect" );
	Cmd_RemoveCommand( "p2p_punch" );
	Cmd_RemoveCommand( "p2p_punch_status" );
	Cmd_RemoveCommand( "p2p_candidates" );
	Cmd_RemoveCommand( "p2p_list" );
	Cmd_RemoveCommand( "p2p_sessioninfo" );
	Cmd_RemoveCommand( "p2p_connect_browser" );
}
