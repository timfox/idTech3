/*
 * Console commands for the in-engine OS sandbox.
 */
#include "emulator_internal.h"

#include "client.h"

#include <stdlib.h>

static cvar_t *cl_emulator_width;
static cvar_t *cl_emulator_height;
static cvar_t *cl_emulator_captureKeys;

static void Emulator_Cmd_Capture_f( void )
{
	const int enable = ( Cmd_Argc() >= 2 ) ? atoi( Cmd_Argv( 1 ) ) : !Emulator_Input_CaptureActive();

	if ( !Cvar_VariableIntegerValue( "cl_emulator" ) ) {
		Com_Printf( S_COLOR_YELLOW "[emulator] set cl_emulator 1 first\n" );
		return;
	}

	Emulator_Input_SetCapture( enable ? qtrue : qfalse );
	if ( cl_emulator_captureKeys ) {
		Cvar_SetValue( cl_emulator_captureKeys->name, enable ? 1 : 0 );
	}
}

static void Emulator_Cmd_Start_f( void )
{
	const char *disk = ( Cmd_Argc() >= 2 ) ? Cmd_Argv( 1 ) : NULL;
	int w, h;

	if ( !Cvar_VariableIntegerValue( "cl_emulator" ) ) {
		Com_Printf( S_COLOR_YELLOW "[emulator] set cl_emulator 1 first\n" );
	}

	w = cl_emulator_width ? cl_emulator_width->integer : EMULATOR_DEFAULT_WIDTH;
	h = cl_emulator_height ? cl_emulator_height->integer : EMULATOR_DEFAULT_HEIGHT;
	Emulator_Frame_SetSize( w, h );

	(void)Emulator_Process_Start( disk );
}

static void Emulator_Cmd_Stop_f( void )
{
	Emulator_Process_Stop();
}

static void Emulator_Cmd_Status_f( void )
{
	emulatorStatus_t st;
	uint32_t inW = 0, inR = 0, inDrop = 0;

	Emulator_Process_GetStatus( &st );
	st.shmAttached = Emulator_Frame_ShmAttached();
	Emulator_Input_GetStatus( &inW, &inR, &inDrop );
	Com_Printf( "[emulator] cl_emulator=%d capture=%d state=%d pid=%d guest=%d shm=%d frame=%u size=%dx%d\n",
		Cvar_VariableIntegerValue( "cl_emulator" ),
		Emulator_Input_CaptureActive() ? 1 : 0,
		(int)st.state,
		st.pid,
		st.guestRunning ? 1 : 0,
		st.shmAttached ? 1 : 0,
		(unsigned)st.frameIndex,
		st.width,
		st.height );
	Com_Printf( "[emulator] input ring write=%u read=%u dropped=%u\n",
		(unsigned)inW, (unsigned)inR, (unsigned)inDrop );
	Com_Printf( "[emulator] renderer: r_emulatorScreen=%d (shader *emulator_screen)\n",
		Cvar_VariableIntegerValue( "r_emulatorScreen" ) );
}

void Emulator_Console_Register( void )
{
	cl_emulator_width = Cvar_Get( "cl_emulator_width", "640", CVAR_ARCHIVE );
	cl_emulator_height = Cvar_Get( "cl_emulator_height", "480", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_emulator_width, "64", "1920", CV_INTEGER );
	Cvar_CheckRange( cl_emulator_height, "64", "1080", CV_INTEGER );
	Cvar_SetDescription( cl_emulator_width, "Guest display width for emulator frame pump." );
	Cvar_SetDescription( cl_emulator_height, "Guest display height for emulator frame pump." );

	cl_emulator_captureKeys = Cvar_Get( "cl_emulator_captureKeys", "0", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_emulator_captureKeys, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_emulator_captureKeys,
		"Route keyboard input to guest via input shm (emulator_capture toggles)." );

	Cmd_AddCommand( "emulator_start", Emulator_Cmd_Start_f );
	Cmd_AddCommand( "emulator_stop", Emulator_Cmd_Stop_f );
	Cmd_AddCommand( "emulator_status", Emulator_Cmd_Status_f );
	Cmd_AddCommand( "emulator_capture", Emulator_Cmd_Capture_f );

	if ( cl_emulator_captureKeys && cl_emulator_captureKeys->integer ) {
		Emulator_Input_SetCapture( qtrue );
	}
}

void Emulator_Console_Unregister( void )
{
	Cmd_RemoveCommand( "emulator_start" );
	Cmd_RemoveCommand( "emulator_stop" );
	Cmd_RemoveCommand( "emulator_status" );
	Cmd_RemoveCommand( "emulator_capture" );
}
