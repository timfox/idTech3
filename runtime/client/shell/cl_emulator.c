/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client-side idTech3 Emulator bridge: QEMU guest process + Vulkan screen texture.
===========================================================================
*/

#include "client.h"
#include "cl_emulator.h"

#ifdef USE_IDTECH3_EMULATOR

#include "../renderers/common/tr_public.h"
#include "emulator_internal.h"

static cvar_t *cl_emulator;
static cvar_t *cl_emulator_drawHud;
static byte *s_frameBuffer;
static int s_frameBufferBytes;

#define EMULATOR_CINEMATIC_SLOT 2

static void CL_Emulator_UploadFrame( int width, int height )
{
	if ( !re.EmulatorUploadFrame ) {
		return;
	}
	re.EmulatorUploadFrame( s_frameBuffer, width, height );
}

void CL_Emulator_Init( void )
{
	cl_emulator = Cvar_Get( "cl_emulator", "0", CVAR_ARCHIVE );
	cl_emulator_drawHud = Cvar_Get( "cl_emulator_drawHud", "0", CVAR_ARCHIVE );

	Cvar_CheckRange( cl_emulator, "0", "1", CV_INTEGER );
	Cvar_CheckRange( cl_emulator_drawHud, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_emulator,
		"Enable in-engine OS sandbox (QEMU guest + Vulkan screen texture)." );
	Cvar_SetDescription( cl_emulator_drawHud,
		"Draw emulator screen as HUD stretch pic (debug; set r_emulatorScreen 1)." );

	Emulator_Process_Init();
	Emulator_Frame_Init();
	Emulator_Input_Init();
	Emulator_Console_Register();

	s_frameBufferBytes = EMULATOR_MAX_WIDTH * EMULATOR_MAX_HEIGHT * 4;
	s_frameBuffer = Z_Malloc( s_frameBufferBytes );

	if ( cl_emulator->integer ) {
		Com_Printf( "[emulator] in-engine OS sandbox enabled (cl_emulator=1)\n" );
	} else {
		Com_Printf( "[emulator] in-engine OS sandbox disabled (cl_emulator=0)\n" );
	}
}

void CL_Emulator_Shutdown( void )
{
	Emulator_Console_Unregister();
	Emulator_Input_Shutdown();
	Emulator_Frame_Shutdown();
	Emulator_Process_Shutdown();

	if ( s_frameBuffer ) {
		Z_Free( s_frameBuffer );
		s_frameBuffer = NULL;
	}
	s_frameBufferBytes = 0;
}

void CL_Emulator_Frame( void )
{
	int width = 0;
	int height = 0;
	float sw, sh;

	if ( !cl_emulator || !cl_emulator->integer ) {
		return;
	}

	if ( !Emulator_Frame_Pump( s_frameBuffer, s_frameBufferBytes, &width, &height ) ) {
		return;
	}

	CL_Emulator_UploadFrame( width, height );

	if ( !cl_emulator_drawHud || !cl_emulator_drawHud->integer ) {
		return;
	}
	if ( !re.DrawStretchRaw || !re.UploadCinematic ) {
		return;
	}
	if ( !Cvar_VariableIntegerValue( "r_emulatorScreen" ) ) {
		return;
	}

	sw = (float)cls.glconfig.vidWidth * 0.35f;
	sh = sw * (float)height / (float)width;
	re.SetColor( NULL );
	re.UploadCinematic( width, height, width, height, s_frameBuffer, EMULATOR_CINEMATIC_SLOT, qtrue );
	re.DrawStretchRaw( 8, (int)( (float)cls.glconfig.vidHeight - sh - 8.0f ), (int)sw, (int)sh,
		width, height, s_frameBuffer, EMULATOR_CINEMATIC_SLOT, qtrue );
}

qboolean CL_Emulator_KeyEvent( int key, qboolean down )
{
	if ( !cl_emulator || !cl_emulator->integer ) {
		return qfalse;
	}
	if ( !Emulator_Input_CaptureActive() ) {
		return qfalse;
	}

	if ( key == K_ESCAPE && down ) {
		Emulator_Input_SetCapture( qfalse );
		Cvar_Set( "cl_emulator_captureKeys", "0" );
		return qtrue;
	}

	if ( Emulator_Input_Push( down ? EMULATOR_INPUT_KEY_DOWN : EMULATOR_INPUT_KEY_UP, key, 0 ) ) {
		return qtrue;
	}
	return qfalse;
}

qboolean CL_Emulator_CharEvent( int key )
{
	if ( !cl_emulator || !cl_emulator->integer ) {
		return qfalse;
	}
	if ( !Emulator_Input_CaptureActive() ) {
		return qfalse;
	}
	if ( key < 32 || key > 126 ) {
		return qfalse;
	}
	return Emulator_Input_Push( EMULATOR_INPUT_CHAR, 0, key );
}

#endif /* USE_IDTECH3_EMULATOR */
