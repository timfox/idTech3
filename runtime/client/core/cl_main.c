/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// cl_main.c  -- client globals, reliable commands, init glue

#include "client.h"
#include "cl_cvars.h"
#include "cl_lifecycle.h"
#include "cl_gameframe.h"
#include "cl_emoji.h"
#include "cl_osp.h"
#include "cl_voip.h"
#include "cl_mumble.h"
#include "cl_superhud.h"
#include "cl_websocket.h"
#include "cl_steam.h"
#include "cl_menuvideo.h"
#include "cl_sdf_font.h"
#include "cl_vector_font.h"
#include "cl_usd.h"
#include "cl_district.h"
#include "cl_openworld.h"
#include "cl_proc.h"
#include "cl_genetic_gan.h"
#include "cl_ml_worker.h"
#include "cl_flux.h"
#include "cl_trellis.h"
#include "cl_generative.h"
#include "cl_vuda.h"
#include "cl_emulator.h"
#include "cl_serverbrowser.h"
#include "cl_ref.h"
#include "cl_connect.h"
#include "cl_download.h"
#include "cl_demo.h"
#include "cl_cmds.h"
#include "../../qcommon/script_emit.h"
#ifdef USE_LUA
#include "../../qcommon/lua_debug.h"
#include "g_lua_bindings.h"
#include "cl_app_crdt.h"
#include "lua_compat.h"

static void CL_LuaRegisterAll( void *luaState )
{
	LuaBindings_RegisterAll( luaState );
	CL_AppCrdt_RegisterLua( (lua_State *)luaState );
}
#endif

void CL_JsNotifyMenuChanged( int menu ) {
	const char *menuName = "unknown";

	switch ( menu ) {
		case UIMENU_NONE: menuName = "none"; break;
		case UIMENU_MAIN: menuName = "main"; break;
		case UIMENU_INGAME: menuName = "ingame"; break;
		case UIMENU_NEED_CD: menuName = "need_cd"; break;
		case UIMENU_BAD_CD_KEY: menuName = "bad_cd_key"; break;
		case UIMENU_TEAM: menuName = "team"; break;
		case UIMENU_POSTGAME: menuName = "postgame"; break;
		default: break;
	}

	Com_ScriptEmitEvent( "menu_changed", menuName, NULL, menu, 0 );
	Com_ScriptSetCurrentMenu( menu );
}

clientActive_t		cl;
clientConnection_t	clc;
clientStatic_t		cls;
vm_t				*cgvm = NULL;

netadr_t			rcon_address;

char				cl_oldGame[ MAX_QPATH ];
qboolean			cl_oldGameSet;

void CL_CDDialog( void ) {
	cls.cddialog = qtrue;
}

void CL_AddReliableCommand( const char *cmd, qboolean isDisconnectCmd ) {
	int		index;
	int		unacknowledged = clc.reliableSequence - clc.reliableAcknowledge;

	if ( clc.serverAddress.type == NA_BAD ) {
		return;
	}

	if ( ( isDisconnectCmd && unacknowledged > MAX_RELIABLE_COMMANDS ) ||
		( !isDisconnectCmd && unacknowledged >= MAX_RELIABLE_COMMANDS ) ) {
		if ( com_errorEntered ) {
			return;
		}
		Com_Error( ERR_DROP, "Client command overflow" );
	}

	clc.reliableSequence++;
	index = clc.reliableSequence & ( MAX_RELIABLE_COMMANDS - 1 );
	Q_strncpyz( clc.reliableCommands[ index ], cmd, sizeof( clc.reliableCommands[ index ] ) );
}

void CL_ClearState( void ) {
	Com_Memset( &cl, 0, sizeof( cl ) );
}

void CL_UpdateGUID( const char *prefix, int prefix_len )
{
#ifdef USE_Q3KEY
	fileHandle_t f;
	int len;

	len = FS_SV_FOpenFileRead( QKEY_FILE, &f );
	FS_FCloseFile( f );

	if ( len != QKEY_SIZE ) {
		Cvar_Set( "cl_guid", "" );
	} else {
		Cvar_Set( "cl_guid", Com_MD5File( QKEY_FILE, QKEY_SIZE, prefix, prefix_len ) );
	}
#else
	Cvar_Set( "cl_guid", Com_MD5Buf( &cl_cdkey[0], sizeof(cl_cdkey), prefix, prefix_len ) );
#endif
}

void CL_ResetOldGame( void )
{
	cl_oldGameSet = qfalse;
	cl_oldGame[0] = '\0';
}

#ifdef USE_Q3KEY
static void CL_GenerateQKey( void )
{
	int len = 0;
	unsigned char buff[ QKEY_SIZE ];
	fileHandle_t f;

	len = FS_SV_FOpenFileRead( QKEY_FILE, &f );
	FS_FCloseFile( f );
	if ( len == QKEY_SIZE ) {
		Com_Printf( "QKEY found.\n" );
		return;
	}

	if ( len > 0 ) {
		Com_Printf( "QKEY file size != %d, regenerating\n", QKEY_SIZE );
	}

	Com_Printf( "QKEY building random string\n" );
	Com_RandomBytes( buff, sizeof(buff) );

	f = FS_SV_FOpenFileWrite( QKEY_FILE );
	if ( !f ) {
		Com_Printf( "QKEY could not open %s for write\n", QKEY_FILE );
		return;
	}
	FS_Write( buff, sizeof(buff), f );
	FS_FCloseFile( f );
	Com_Printf( "QKEY generated\n" );
}
#endif

void CL_Init( void ) {
	Com_Printf( "----- Client Initialization -----\n" );

	Con_Init();

	CL_ClearState();
	cls.state = CA_DISCONNECTED;

	CL_ResetOldGame();

	cls.realtime = 0;

	CL_InitInput();

	CL_InitCvars();

	CL_Connect_Init();
	CL_Cmds_Init();
	CL_Demo_Init();

	Cvar_Set( "cl_running", "1" );
#ifdef USE_MD5
	CL_GenerateQKey();
#endif
	Cvar_Get( "cl_guid", "", CVAR_USERINFO | CVAR_ROM | CVAR_PROTECTED );
	CL_UpdateGUID( NULL, 0 );

	CL_InitGameSystems();

	CL_Emoji_Init();
	CL_OSP_Init();
	CL_VoIP_Init();
	CL_Mumble_Init();
	SHUD_Init();
	WS_Init();
	Steam_Init();
	MenuVideo_Init();
	SDF_Init();
	VectorFont_Init();
	CL_USD_Init();
	CL_District_Init();
	CL_OpenWorld_Init();
	CL_Proc_Init();
	CL_ServerBrowser_Init();
	CL_Download_Init();
	CL_Ref_Init();
#ifdef USE_FLUX
	CL_Flux_Init();
#endif
#ifdef USE_TRELLIS
	CL_Trellis_Init();
#endif
#if defined( USE_FLUX ) || defined( USE_TRELLIS ) || defined( USE_GENETIC_GAN )
	CL_MlWorker_Init();
#endif
#ifdef USE_GENETIC_GAN
	CL_GeneticGan_Init();
#endif
#ifdef USE_VUDA
	CL_VUDA_Init();
#endif
	CL_Emulator_Init();

#ifdef USE_LUA
	LuaDebug_SetEngineRegisterCallback( CL_LuaRegisterAll );
	CL_AppCrdt_Init();
#endif

	Com_Printf( "----- Client Initialization Complete -----\n" );
}
