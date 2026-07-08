/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Opt-in crash reporting: writes crash_*.txt locally; POST to com_crashReportURL when set.
No PII collected by default. See docs/CRASH_REPORTING.md.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "com_crash.h"

#include <signal.h>
#include <stdio.h>
#include <time.h>

static cvar_t *com_crashReportURL;
static cvar_t *com_crashReportEnable;
static qboolean com_crashInstalled;

static void Com_Crash_WriteReport( const char *reason, const char *tag )
{
	char path[MAX_OSPATH];
	char line[MAX_STRING_CHARS];
	time_t now;
	FILE *f;

	if ( !com_crashReportEnable || !com_crashReportEnable->integer ) {
		return;
	}

	now = time( NULL );
	Com_sprintf( path, sizeof( path ), "crash_%s_%ld.txt", tag, (long)now );
	f = fopen( path, "w" );
	if ( !f ) {
		return;
	}

	fprintf( f, "engine=idtech3\n" );
	fprintf( f, "reason=%s\n", reason ? reason : "unknown" );
	fprintf( f, "version=%s\n", com_version->string );
	fprintf( f, "map=%s\n", Cvar_VariableString( "mapname" ) );
	fprintf( f, "fs_game=%s\n", Cvar_VariableString( "fs_game" ) );
	fprintf( f, "time=%ld\n", (long)now );
	fclose( f );

	Com_Printf( "[crash] report written: %s\n", path );

	if ( com_crashReportURL && com_crashReportURL->string[0] ) {
		int uploadRc;

		Com_sprintf( line, sizeof( line ),
			"curl -sS -X POST -F 'file=@%s' '%s' >/dev/null 2>&1 &",
			path, com_crashReportURL->string );
		uploadRc = system( line );
		(void)uploadRc;
		Com_Printf( "[crash] upload queued to com_crashReportURL (opt-in)\n" );
	}
}

static void Com_Crash_SignalHandler( int sig )
{
	char msg[64];

	Com_sprintf( msg, sizeof( msg ), "signal %d", sig );
	Com_Crash_WriteReport( msg, "signal" );
	signal( sig, SIG_DFL );
	raise( sig );
}

void Com_Crash_Init( void )
{
	com_crashReportEnable = Cvar_Get( "com_crashReportEnable", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( com_crashReportEnable,
		"Write local crash_*.txt reports on fatal errors (no PII). Default off." );
	com_crashReportURL = Cvar_Get( "com_crashReportURL", "", CVAR_ARCHIVE );
	Cvar_SetDescription( com_crashReportURL,
		"Optional HTTPS endpoint for crash report upload (requires curl). Empty = local file only." );

	if ( com_crashReportEnable->integer && !com_crashInstalled ) {
		signal( SIGSEGV, Com_Crash_SignalHandler );
		signal( SIGABRT, Com_Crash_SignalHandler );
		com_crashInstalled = qtrue;
		Com_Printf( "[crash] com_crashReportEnable=1 (opt-in crash reports)\n" );
	}
}

void Com_Crash_OnFatal( const char *reason )
{
	Com_Crash_WriteReport( reason, "fatal" );
}

void Com_Crash_OnSignal( int sig )
{
	char msg[64];
	Com_sprintf( msg, sizeof( msg ), "signal %d", sig );
	Com_Crash_WriteReport( msg, "signal" );
}
