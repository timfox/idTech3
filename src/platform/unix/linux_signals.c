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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#ifdef _DEBUG
#include <execinfo.h>
#endif

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#ifndef DEDICATED
#include "../../renderers/opengl/tr_local.h"
#endif
#include "linux_local.h"

static qboolean signalcaught = qfalse;
static pthread_t mainThreadId;

extern void NORETURN Sys_Exit( int code );

static const char *signal_name( int sig )
{
	switch ( sig ) {
		case SIGHUP:  return "SIGHUP (hangup)";
		case SIGQUIT: return "SIGQUIT (quit)";
		case SIGILL:  return "SIGILL (illegal instruction)";
		case SIGTRAP: return "SIGTRAP (trace trap)";
		case SIGABRT: return "SIGABRT (abort)";
		case SIGBUS:  return "SIGBUS (bus error)";
		case SIGFPE:  return "SIGFPE (floating point exception)";
		case SIGSEGV: return "SIGSEGV (segmentation fault)";
		case SIGTERM: return "SIGTERM (termination)";
		default:      return "unknown signal";
	}
}

static void signal_handler( int sig )
{
	char msg[256];
	qboolean isMainThread = pthread_equal( pthread_self(), mainThreadId );

	if ( signalcaught == qtrue )
	{
		fprintf( stderr, "DOUBLE SIGNAL FAULT: Received signal %d (%s), forcing exit...\n",
			sig, signal_name( sig ) );
		Sys_Exit( 1 );
	}

	fprintf( stderr, "\n" );
	fprintf( stderr, "========================================\n" );
	fprintf( stderr, " ENGINE CRASH: signal %d (%s)\n", sig, signal_name( sig ) );
	fprintf( stderr, " Thread: %s\n", isMainThread ? "main" : "worker/driver" );
	fprintf( stderr, "========================================\n" );

	if ( sig == SIGABRT && !isMainThread ) {
		fprintf( stderr, "\n" );
		fprintf( stderr, " This abort occurred on a background thread, most likely\n" );
		fprintf( stderr, " the GPU driver's shader compiler (NVIDIA LLVM backend).\n" );
		fprintf( stderr, "\n" );
		fprintf( stderr, " Possible causes:\n" );
		fprintf( stderr, "   - GPU driver bug with a specific SPIR-V shader\n" );
		fprintf( stderr, "   - Stale shader cache (try deleting ~/.nv/GLCache/)\n" );
		fprintf( stderr, "   - Driver version incompatibility\n" );
		fprintf( stderr, "\n" );
		fprintf( stderr, " Try these workarounds:\n" );
		fprintf( stderr, "   1. Clear shader cache:  rm -rf ~/.nv/GLCache/\n" );
		fprintf( stderr, "   2. Use OpenGL renderer: +set cl_renderer opengl\n" );
		fprintf( stderr, "   3. Disable post-process: +set r_bloom 0 +set r_ssao 0\n" );
		fprintf( stderr, "   4. Update GPU driver to the latest version\n" );
		fprintf( stderr, "   5. Set r_volumetricFog 0 to disable volumetric fog\n" );
		fprintf( stderr, "\n" );
	} else if ( sig == SIGSEGV ) {
		fprintf( stderr, "\n" );
		fprintf( stderr, " Segmentation fault — the engine accessed invalid memory.\n" );
		fprintf( stderr, " This is a bug. Please report it with:\n" );
		fprintf( stderr, "   - The full console output above this message\n" );
		fprintf( stderr, "   - Your GPU, driver version, and OS\n" );
		fprintf( stderr, "   - The map/mod you were running\n" );
		fprintf( stderr, "\n" );
	} else if ( sig == SIGABRT ) {
		fprintf( stderr, "\n" );
		fprintf( stderr, " The engine called abort(). This usually means:\n" );
		fprintf( stderr, "   - A fatal assertion failed\n" );
		fprintf( stderr, "   - Memory allocation failed (out of memory)\n" );
		fprintf( stderr, "   - A library encountered an unrecoverable error\n" );
		fprintf( stderr, "\n" );
		fprintf( stderr, " Try: +set com_hunkMegs 256 (increase memory)\n" );
		fprintf( stderr, "\n" );
	}

#ifdef _DEBUG
	if ( sig == SIGSEGV || sig == SIGILL || sig == SIGBUS || sig == SIGABRT )
	{
		void *syms[20];
		const size_t size = backtrace( syms, 20 );
		fprintf( stderr, " Backtrace (%zu frames):\n", size );
		backtrace_symbols_fd( syms, size, STDERR_FILENO );
		fprintf( stderr, "\n" );
	}
#endif

	fprintf( stderr, "========================================\n\n" );

	signalcaught = qtrue;
	sprintf( msg, "Signal caught (%d: %s)", sig, signal_name( sig ) );
	VM_Forced_Unload_Start();
#ifndef DEDICATED
	CL_Shutdown( msg, qtrue );
#endif
	SV_Shutdown( msg );
	VM_Forced_Unload_Done();
	Sys_Exit( 0 );
}


void InitSig( void )
{
	mainThreadId = pthread_self();

	signal( SIGINT, SIG_IGN );
	signal( SIGHUP, signal_handler );
	signal( SIGQUIT, signal_handler );
	signal( SIGILL, signal_handler );
	signal( SIGTRAP, signal_handler );
	signal( SIGIOT, signal_handler );
	signal( SIGBUS, signal_handler );
	signal( SIGFPE, signal_handler );
	signal( SIGSEGV, signal_handler );
	signal( SIGTERM, signal_handler );
}
