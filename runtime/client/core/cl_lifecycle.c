/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "client.h"
#include "cl_lifecycle.h"
#include "cl_connect.h"
#include "cl_cmds.h"
#include "cl_demo.h"
#include "cl_download.h"
#include "cl_ref.h"
#include "cl_sdf_font.h"
#include "cl_superhud.h"
#include "cl_menuvideo.h"
#include "cl_serverbrowser.h"
#include "cl_streaming.h"
#include "cl_flux.h"
#include "cl_trellis.h"
#include "cl_genetic_gan.h"
#include "cl_ml_worker.h"
#include "cl_vuda.h"
#include "cl_emulator.h"
#ifdef USE_CURL
#include "cl_curl.h"
#endif

void CL_ShutdownVMs( void )
{
	CL_ShutdownCGame();
	CL_ShutdownUI();
}

void CL_ShutdownAll( void ) {

#ifdef USE_CURL
	CL_cURL_Shutdown();
#endif

	S_DisableSounds();

	CL_ShutdownVMs();

	if ( re.Shutdown ) {
		if ( CL_GameSwitch() ) {
			CL_Ref_Shutdown( REF_DESTROY_WINDOW );
		} else {
			re.Shutdown( REF_KEEP_CONTEXT );
		}
	}

	cls.rendererStarted = qfalse;
	cls.soundRegistered = qfalse;

	SCR_Done();
}

void CL_ClearMemory( void ) {
	if ( !com_sv_running->integer ) {
		Hunk_Clear();
		CM_ClearMap();
	} else {
		Hunk_ClearToMark();
	}
}

void CL_FlushMemory( void ) {
	CL_ShutdownAll();
	CL_ClearMemory();
	CL_StartHunkUsers();
}

void CL_MapLoading( void ) {
	if ( com_dedicated->integer ) {
		cls.state = CA_DISCONNECTED;
		Key_SetCatcher( KEYCATCH_CONSOLE );
		return;
	}

	if ( !com_cl_running->integer ) {
		return;
	}

	CL_TryEarlyStockBaseq3Profile();

	Con_Close();
	Key_SetCatcher( 0 );

	if ( cls.state >= CA_CONNECTED && !Q_stricmp( cls.servername, "localhost" ) ) {
		cls.state = CA_CONNECTED;
		Com_Memset( cls.updateInfoString, 0, sizeof( cls.updateInfoString ) );
		Com_Memset( clc.serverMessage, 0, sizeof( clc.serverMessage ) );
		Com_Memset( &cl.gameState, 0, sizeof( cl.gameState ) );
		clc.lastPacketSentTime = cls.realtime - 9999;
		cls.framecount++;
		SCR_UpdateScreen();
	} else {
		Cvar_Set( "nextmap", "" );
		CL_Disconnect( qtrue );
		Q_strncpyz( cls.servername, "localhost", sizeof(cls.servername) );
		cls.state = CA_CHALLENGING;
		Key_SetCatcher( 0 );
		cls.framecount++;
		SCR_UpdateScreen();
		clc.connectTime = -RETRANSMIT_TIMEOUT;
		NET_StringToAdr( cls.servername, &clc.serverAddress, NA_UNSPEC );
		CL_Connect_CheckForResend();
	}
}

void CL_StartHunkUsers( void ) {

	if ( !com_cl_running || !com_cl_running->integer ) {
		return;
	}

	if ( cls.state >= CA_LOADING ) {
		const char *info = cl.gameState.stringData + cl.gameState.stringOffsets[ CS_SERVERINFO ];
		const char *mapname = Info_ValueForKey( info, "mapname" );
		if ( mapname && *mapname != '\0' ) {
			const char *cmd = Cvar_VariableString( va( "cl_mapConfig_%s", mapname ) );
			if ( cmd && *cmd != '\0' ) {
				Cbuf_AddText( cmd );
				Cbuf_AddText( "\n" );
			} else {
				cmd = Cvar_VariableString( va( "cl_mapConfig_%s", "default" ) );
				if ( cmd && *cmd != '\0' ) {
					Cbuf_AddText( cmd );
					Cbuf_AddText( "\n" );
				}
			}

			{
				const char *postCfg = va( "maps/%s.post.cfg", mapname );
				if ( FS_FileExists( postCfg ) ) {
					Cbuf_AddText( va( "exec %s\n", postCfg ) );
				} else if ( FS_FileExists( "maps/default.post.cfg" ) ) {
					Cbuf_AddText( "exec maps/default.post.cfg\n" );
				}
			}
		}
	}

	if ( !cls.rendererStarted ) {
		cls.rendererStarted = qtrue;
		CL_Ref_InitRenderer();
	}

	if ( !cls.soundStarted ) {
		cls.soundStarted = qtrue;
		S_Init();
	}

	if ( !cls.soundRegistered ) {
		cls.soundRegistered = qtrue;
		S_BeginRegistration();
	}

	if ( !cls.uiStarted ) {
		cls.uiStarted = qtrue;
		CL_InitUI();
	}
}

void CL_Shutdown( const char *finalmsg, qboolean quit ) {
	static qboolean recursive = qfalse;

	if ( !( com_cl_running && com_cl_running->integer ) ) {
		return;
	}

	Com_Printf( "----- Client Shutdown (%s) -----\n", finalmsg );

	if ( recursive ) {
		Com_Printf( "WARNING: Recursive CL_Shutdown()\n" );
		return;
	}
	recursive = qtrue;

	CL_Connect_SetShutdownQuit( quit );
	CL_Disconnect( qfalse );
	CL_Streaming_Shutdown();
	SDF_Shutdown();
	SHUD_Shutdown();
	MenuVideo_Shutdown();
#ifdef USE_VUDA
	CL_VUDA_Shutdown();
#endif
	CL_Emulator_Shutdown();

	S_DisableSounds();

	CL_ShutdownVMs();

	CL_Ref_Shutdown( quit ? REF_UNLOAD_DLL : REF_DESTROY_WINDOW );

	Con_Shutdown();

	CL_ServerBrowser_Shutdown();
	CL_Ref_ShutdownCommands();

#ifdef USE_FLUX
	CL_Flux_Shutdown();
#endif
#ifdef USE_TRELLIS
	CL_Trellis_Shutdown();
#endif

#ifdef USE_GENETIC_GAN
	CL_GeneticGan_Shutdown();
#endif
#if defined( USE_FLUX ) || defined( USE_TRELLIS ) || defined( USE_GENETIC_GAN )
	CL_MlWorker_Shutdown();
#endif

	CL_Connect_ShutdownCommands();
	CL_Cmds_Shutdown();
	CL_Demo_Shutdown();

	CL_Download_Shutdown();

	CL_ClearInput();

	Cvar_Set( "cl_running", "0" );

	recursive = qfalse;

	Com_Memset( &cls, 0, sizeof( cls ) );
	Key_SetCatcher( 0 );
	Com_Printf( "-----------------------\n" );
}
