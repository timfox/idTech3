/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "client.h"
#include "cl_frame.h"
#include "cl_gameframe.h"
#include "cl_connect.h"
#include "cl_demo.h"
#include "cl_download.h"
#include "cl_voip.h"
#include "cl_websocket.h"
#include "cl_steam.h"
#include "cl_genetic_gan.h"
#include "cl_ml_worker.h"
#include "cl_flux.h"
#include "cl_trellis.h"
#include "cl_generative.h"
#include "cl_vuda.h"
#include "lua_debug.h"
#include "cl_app_crdt.h"

qboolean CL_CheckPaused( void )
{
	if ( cl_paused->integer || cl_paused->modified ) {
		return qtrue;
	}
	return qfalse;
}

qboolean CL_NoDelay( void )
{
	if ( CL_VideoRecording() || ( com_timedemo->integer && clc.demofile != FS_INVALID_HANDLE ) ) {
		return qtrue;
	}
	return qfalse;
}

static void CL_Steam_UpdateRichPresence( void ) {
	if ( !Steam_IsInitialized() ) {
		return;
	}
	if ( cls.state == CA_ACTIVE ) {
		if ( clc.demoplaying ) {
			Steam_SetRichPresence( "status", "Playing demo" );
		} else if ( cls.servername[0] ) {
			Steam_SetRichPresence( "status", cls.servername );
		} else {
			Steam_SetRichPresence( "status", "In game" );
		}
	} else if ( cls.state == CA_CONNECTING || cls.state == CA_CHALLENGING ) {
		Steam_SetRichPresence( "status", "Connecting..." );
	} else {
		Steam_SetRichPresence( "status", "In menu" );
	}
}

void CL_Frame( int msec, int realMsec ) {

	if ( CL_Download_Frame( msec, realMsec ) ) {
		return;
	}

	if ( !com_cl_running->integer ) {
		return;
	}

#if defined( USE_TRELLIS ) || defined( USE_GENETIC_GAN ) || defined( USE_FLUX )
	CL_GenerativeFrame();
#endif

	CL_MlWorker_Frame();

#ifdef USE_VUDA
	CL_VUDA_Frame();
#endif

	cls.realFrametime = realMsec;

	if ( cls.cddialog ) {
		cls.cddialog = qfalse;
		VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_NEED_CD );
		CL_JsNotifyMenuChanged( UIMENU_NEED_CD );
	} else if ( cls.state == CA_DISCONNECTED && !( Key_GetCatcher( ) & KEYCATCH_UI )
		&& !com_sv_running->integer && uivm ) {
		S_StopAllSounds();
		VM_Call( uivm, 1, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
		CL_JsNotifyMenuChanged( UIMENU_MAIN );
	}

	CL_Demo_Frame( &msec, &realMsec );

	cls.frametime = msec;
	cls.realtime += msec;

	if ( cl_timegraph->integer ) {
		SCR_DebugGraph( msec * 0.25f );
	}

	CL_Connect_Frame();

	CL_SendCmd();

	CL_SetCGameTime();

	cls.framecount++;
	SCR_UpdateScreen();

	SCR_RunCinematic();

	S_Update( realMsec );

	CL_GameFrame( (float)msec * 0.001f );

	CL_VoIP_Frame();
	WS_Frame();
	Steam_Frame();
	CL_Steam_UpdateRichPresence();

#ifdef USE_LUA
	LuaDebug_WatchTick( Sys_Milliseconds() );
	CL_AppCrdt_Frame();
#endif

	Con_RunConsole();
}
