/*
 * QEMU guest process launcher (out-of-process sandbox).
 */
#if !defined( _WIN32 ) && !defined( _DEFAULT_SOURCE )
#define _DEFAULT_SOURCE
#endif

#include "emulator_internal.h"

#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

static emulatorState_t s_state = EMULATOR_STATE_IDLE;
static int s_pid = -1;
static char s_qemuPath[MAX_OSPATH];
static char s_diskImage[MAX_OSPATH];

static qboolean Emulator_ResolveQemuPath( char *out, int outSize )
{
#ifndef IDTECH3_EMULATOR_DIR
#define IDTECH3_EMULATOR_DIR "third_party/idtech3-emulator"
#endif

	if ( !out || outSize < 1 ) {
		return qfalse;
	}

	Com_sprintf( out, outSize, "%s/build/qemu-system-x86_64", IDTECH3_EMULATOR_DIR );
	if ( access( out, X_OK ) == 0 ) {
		return qtrue;
	}

	Com_sprintf( out, outSize, "%s/build/qemu-system-x86_64", "third_party/idtech3-emulator" );
	if ( access( out, X_OK ) == 0 ) {
		return qtrue;
	}

	Q_strncpyz( out, "qemu-system-x86_64", outSize );
	return qtrue;
}

static void Emulator_ReapIfExited( void )
{
#ifndef _WIN32
	int status;

	if ( s_pid <= 0 ) {
		return;
	}

	if ( waitpid( s_pid, &status, WNOHANG ) == s_pid ) {
		if ( WIFEXITED( status ) || WIFSIGNALED( status ) ) {
			Com_Printf( "[emulator] guest exited (pid=%d)\n", s_pid );
		}
		s_pid = -1;
		s_state = EMULATOR_STATE_IDLE;
	}
#endif
}

void Emulator_Process_Init( void )
{
	s_state = EMULATOR_STATE_IDLE;
	s_pid = -1;
	s_qemuPath[0] = '\0';
	s_diskImage[0] = '\0';
	Emulator_ResolveQemuPath( s_qemuPath, sizeof( s_qemuPath ) );
}

void Emulator_Process_Shutdown( void )
{
	Emulator_Process_Stop();
}

qboolean Emulator_Process_Start( const char *diskImage )
{
#ifndef _WIN32
	char *argv[16];
	int argc = 0;

	Emulator_ReapIfExited();
	if ( s_pid > 0 ) {
		Com_Printf( S_COLOR_YELLOW "[emulator] guest already running (pid=%d)\n", s_pid );
		return qfalse;
	}

	if ( !Emulator_ResolveQemuPath( s_qemuPath, sizeof( s_qemuPath ) ) ) {
		Com_Printf( S_COLOR_YELLOW "[emulator] QEMU binary not found\n" );
		s_state = EMULATOR_STATE_ERROR;
		return qfalse;
	}

	if ( access( s_qemuPath, X_OK ) != 0 && Q_stricmp( s_qemuPath, "qemu-system-x86_64" ) != 0 ) {
		Com_Printf( S_COLOR_YELLOW "[emulator] QEMU not executable: %s (build submodule first)\n", s_qemuPath );
		s_state = EMULATOR_STATE_ERROR;
		return qfalse;
	}

	if ( diskImage && diskImage[0] ) {
		Q_strncpyz( s_diskImage, diskImage, sizeof( s_diskImage ) );
	} else {
		s_diskImage[0] = '\0';
	}

	s_state = EMULATOR_STATE_STARTING;

	argv[argc++] = s_qemuPath;
	argv[argc++] = "-machine";
	argv[argc++] = "microvm";
	argv[argc++] = "-m";
	argv[argc++] = "512";
	argv[argc++] = "-display";
	argv[argc++] = "none";
	argv[argc++] = "-serial";
	argv[argc++] = "mon:stdio";
	if ( s_diskImage[0] ) {
		static char driveArg[MAX_OSPATH];
		Com_sprintf( driveArg, sizeof( driveArg ), "file=%s,format=raw,if=virtio", s_diskImage );
		argv[argc++] = "-drive";
		argv[argc++] = driveArg;
	}
	argv[argc] = NULL;

	s_pid = fork();
	if ( s_pid < 0 ) {
		Com_Printf( S_COLOR_RED "[emulator] fork failed: %s\n", strerror( errno ) );
		s_state = EMULATOR_STATE_ERROR;
		return qfalse;
	}

	if ( s_pid == 0 ) {
		char wbuf[16];
		char hbuf[16];
		int gw = EMULATOR_DEFAULT_WIDTH;
		int gh = EMULATOR_DEFAULT_HEIGHT;

		/* Child: detach from controlling terminal noise in listen-server client. */
		setsid();
		Emulator_Frame_GetMetrics( &gw, &gh, NULL );
		Com_sprintf( wbuf, sizeof( wbuf ), "%d", gw );
		Com_sprintf( hbuf, sizeof( hbuf ), "%d", gh );
		setenv( "IDTECH3_EMULATOR_FRAME_SHM", EMULATOR_SHM_NAME, 1 );
		setenv( "IDTECH3_EMULATOR_INPUT_SHM", EMULATOR_INPUT_SHM_NAME, 1 );
		setenv( "IDTECH3_EMULATOR_WIDTH", wbuf, 1 );
		setenv( "IDTECH3_EMULATOR_HEIGHT", hbuf, 1 );
		execv( s_qemuPath, argv );
		fprintf( stderr, "[emulator] execv failed: %s\n", strerror( errno ) );
		_exit( 127 );
	}

	s_state = EMULATOR_STATE_RUNNING;
	Com_Printf( "[emulator] guest started pid=%d binary=%s\n", s_pid, s_qemuPath );
	return qtrue;
#else
	(void)diskImage;
	Com_Printf( S_COLOR_YELLOW "[emulator] guest launch not implemented on Windows yet\n" );
	s_state = EMULATOR_STATE_ERROR;
	return qfalse;
#endif
}

void Emulator_Process_Stop( void )
{
#ifndef _WIN32
	if ( s_pid <= 0 ) {
		s_state = EMULATOR_STATE_IDLE;
		return;
	}

	s_state = EMULATOR_STATE_STOPPING;
	kill( s_pid, SIGTERM );
	Sys_Sleep( 100 );
	if ( waitpid( s_pid, NULL, WNOHANG ) == 0 ) {
		kill( s_pid, SIGKILL );
		waitpid( s_pid, NULL, 0 );
	}
	Com_Printf( "[emulator] guest stopped (pid=%d)\n", s_pid );
	s_pid = -1;
#endif
	s_state = EMULATOR_STATE_IDLE;
}

void Emulator_Process_GetStatus( emulatorStatus_t *out )
{
	if ( !out ) {
		return;
	}

	Emulator_ReapIfExited();

	Com_Memset( out, 0, sizeof( *out ) );
	out->state = s_state;
	out->pid = s_pid;
	out->guestRunning = ( s_pid > 0 );
	Emulator_Frame_GetMetrics( &out->width, &out->height, &out->frameIndex );
}
