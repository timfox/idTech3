/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Record usercmds + high-level events for regression / playability tests.
See docs/BETA_AUTOMATED_TESTING.md.
===========================================================================
*/

#include "client.h"
#include "cl_beta_trace.h"
#include "cl_beta_petri.h"

#define BETA_TRACE_VERSION          1
#define BETA_TRACE_CMD_MAGIC        "# idtech3 betacmd v1"
#define BETA_TRACE_MAX_PATH         MAX_OSPATH
#define BETA_TRACE_LINE             256
#define BETA_TRACE_MAX_SUCCESS      8
#define BETA_TRACE_MAX_FAIL         8
#define BETA_TRACE_PATTERN_LEN      64

typedef enum {
	BETA_MODE_IDLE = 0,
	BETA_MODE_RECORD,
	BETA_MODE_REPLAY,
	BETA_MODE_TEST
} betaTraceMode_t;

static cvar_t *cl_betaTrace;
static cvar_t *cl_betaTraceLog;
static cvar_t *cl_betaTraceStudioMode;
static cvar_t *cl_betaTraceStudioBase;

static betaTraceMode_t beta_mode = BETA_MODE_IDLE;
static fileHandle_t beta_cmdHandle = FS_INVALID_HANDLE;
static fileHandle_t beta_evtHandle = FS_INVALID_HANDLE;
static char beta_baseName[BETA_TRACE_MAX_PATH];
static char beta_mapName[MAX_QPATH];
static int beta_recordStartTime;
static int beta_testStartTime;
static int beta_testMaxTimeMs;
static qboolean beta_testFinished;
static int beta_testResult; /* 0 unknown, 1 pass, -1 fail */
static qboolean beta_cmdReplayActive;
static int beta_replayFrameIndex;

static char beta_successPatterns[BETA_TRACE_MAX_SUCCESS][BETA_TRACE_PATTERN_LEN];
static int beta_successCount;
static char beta_failPatterns[BETA_TRACE_MAX_FAIL][BETA_TRACE_PATTERN_LEN];
static int beta_failCount;

static void CL_BetaTrace_LogStartup( void ) {
	if ( cl_betaTraceLog && cl_betaTraceLog->integer ) {
		Com_Printf( "Beta trace: cl_betaTrace=%d (beta_record / beta_play / beta_test)\n",
			cl_betaTrace ? cl_betaTrace->integer : 0 );
	}
}

static void CL_BetaTrace_PublishStudioCvars( void ) {
	if ( !cl_betaTraceStudioMode ) {
		return;
	}
	Cvar_Set( "cl_betaTraceStudioMode", va( "%d", (int)beta_mode ) );
	if ( cl_betaTraceStudioBase ) {
		Cvar_Set( "cl_betaTraceStudioBase", beta_baseName[0] ? beta_baseName : "" );
	}
}

static void CL_BetaTrace_CloseFiles( void ) {
	if ( beta_cmdHandle != FS_INVALID_HANDLE ) {
		FS_FCloseFile( beta_cmdHandle );
		beta_cmdHandle = FS_INVALID_HANDLE;
	}
	if ( beta_evtHandle != FS_INVALID_HANDLE ) {
		FS_FCloseFile( beta_evtHandle );
		beta_evtHandle = FS_INVALID_HANDLE;
	}
}

static void CL_BetaTrace_BuildPath( char *out, size_t outSize, const char *base, const char *ext ) {
	Com_sprintf( out, outSize, "beta_traces/%s.%s", base, ext );
}

static void CL_BetaTrace_WriteManifest( void ) {
	char path[BETA_TRACE_MAX_PATH];
	char line[BETA_TRACE_LINE];
	fileHandle_t f;
	int i;

	if ( beta_baseName[0] == '\0' ) {
		return;
	}

	CL_BetaTrace_BuildPath( path, sizeof( path ), beta_baseName, "betatest" );
	f = FS_FOpenFileWrite( path );
	if ( f == FS_INVALID_HANDLE ) {
		Com_Printf( S_COLOR_YELLOW "Warning: could not write %s\n", path );
		return;
	}

	Com_sprintf( line, sizeof( line ), "version=%d\n", BETA_TRACE_VERSION );
	FS_Write( line, (int)strlen( line ), f );
	Com_sprintf( line, sizeof( line ), "map=%s\n", beta_mapName[0] ? beta_mapName : "unknown" );
	FS_Write( line, (int)strlen( line ), f );
	Com_sprintf( line, sizeof( line ), "recorded_ms=%d\n",
		cls.realtime - beta_recordStartTime );
	FS_Write( line, (int)strlen( line ), f );
	for ( i = 0; i < beta_successCount; i++ ) {
		Com_sprintf( line, sizeof( line ), "success=%s\n", beta_successPatterns[i] );
		FS_Write( line, (int)strlen( line ), f );
	}
	for ( i = 0; i < beta_failCount; i++ ) {
		Com_sprintf( line, sizeof( line ), "fail=%s\n", beta_failPatterns[i] );
		FS_Write( line, (int)strlen( line ), f );
	}
	if ( beta_testMaxTimeMs > 0 ) {
		Com_sprintf( line, sizeof( line ), "max_time_ms=%d\n", beta_testMaxTimeMs );
		FS_Write( line, (int)strlen( line ), f );
	}
	FS_FCloseFile( f );
	Com_Printf( "Beta trace: wrote manifest %s\n", path );
}

static void CL_BetaTrace_ParseManifestLine( const char *line ) {
	char key[64];
	char value[BETA_TRACE_PATTERN_LEN];
	const char *semi;

	if ( sscanf( line, "%63[^=]=%63[^\n]", key, value ) != 2 ) {
		return;
	}

	if ( !Q_stricmp( key, "map" ) ) {
		Q_strncpyz( beta_mapName, value, sizeof( beta_mapName ) );
	} else if ( !Q_stricmp( key, "max_time_ms" ) ) {
		beta_testMaxTimeMs = atoi( value );
	} else if ( !Q_stricmp( key, "success" ) ) {
		if ( beta_successCount < BETA_TRACE_MAX_SUCCESS ) {
			const char *walk = value;

			while ( walk && *walk && beta_successCount < BETA_TRACE_MAX_SUCCESS ) {
				semi = strchr( walk, ';' );
				if ( semi ) {
					Q_strncpyz( beta_successPatterns[beta_successCount], walk,
						(int)( semi - walk ) + 1 );
					walk = semi + 1;
				} else {
					Q_strncpyz( beta_successPatterns[beta_successCount], walk,
						sizeof( beta_successPatterns[0] ) );
					walk = NULL;
				}
				beta_successCount++;
			}
		}
	} else if ( !Q_stricmp( key, "fail" ) ) {
		if ( beta_failCount < BETA_TRACE_MAX_FAIL ) {
			Q_strncpyz( beta_failPatterns[beta_failCount], value,
				sizeof( beta_failPatterns[0] ) );
			beta_failCount++;
		}
	} else if ( !Q_stricmp( key, "petrinet" ) ) {
		CL_BetaPetri_Load( value );
	} else if ( !Q_stricmp( key, "petri_goal" ) ) {
		CL_BetaPetri_SetGoalPlace( value );
	}
}

static qboolean CL_BetaTrace_LoadManifest( const char *base ) {
	char path[BETA_TRACE_MAX_PATH];
	fileHandle_t f;
	int len;
	char *buf;
	char *p;
	char *lineStart;
	char *lineEnd;
	char line[BETA_TRACE_LINE];

	CL_BetaTrace_BuildPath( path, sizeof( path ), base, "betatest" );
	len = FS_FOpenFileRead( path, &f, qtrue );
	if ( len < 0 || f == FS_INVALID_HANDLE ) {
		Com_Printf( S_COLOR_YELLOW "Warning: no manifest %s (optional)\n", path );
		return qfalse;
	}

	buf = (char *)Z_Malloc( len + 1 );
	FS_Read( buf, len, f );
	FS_FCloseFile( f );
	buf[len] = '\0';

	p = buf;
	while ( p && *p ) {
		lineStart = p;
		lineEnd = strchr( p, '\n' );
		if ( lineEnd ) {
			*lineEnd = '\0';
			p = lineEnd + 1;
		} else {
			p = NULL;
		}
		Q_strncpyz( line, lineStart, sizeof( line ) );
		if ( line[0] && line[0] != '#' ) {
			CL_BetaTrace_ParseManifestLine( line );
		}
	}

	Z_Free( buf );
	return qtrue;
}

static void CL_BetaTrace_WriteCmdLine( const usercmd_t *cmd ) {
	char line[BETA_TRACE_LINE];

	Com_sprintf( line, sizeof( line ), "%d %d %d %d %u %u %d %d %d\n",
		cmd->serverTime,
		cmd->angles[0], cmd->angles[1], cmd->angles[2],
		(unsigned)cmd->buttons,
		(unsigned)cmd->weapon,
		(int)cmd->forwardmove, (int)cmd->rightmove, (int)cmd->upmove );
	FS_Write( line, (int)strlen( line ), beta_cmdHandle );
}

static qboolean CL_BetaTrace_ReadCmdLine( usercmd_t *cmd ) {
	char line[BETA_TRACE_LINE];
	int n;
	int bytes;
	int i;
	char c;

	if ( beta_cmdHandle == FS_INVALID_HANDLE ) {
		return qfalse;
	}

	n = 0;
	while ( n < (int)sizeof( line ) - 1 ) {
		bytes = FS_Read( &c, 1, beta_cmdHandle );
		if ( bytes != 1 ) {
			return qfalse;
		}
		if ( c == '\n' ) {
			break;
		}
		if ( c == '\r' ) {
			continue;
		}
		line[n++] = c;
	}
	line[n] = '\0';

	if ( line[0] == '#' || line[0] == '\0' ) {
		return CL_BetaTrace_ReadCmdLine( cmd );
	}

	{
		unsigned int buttons;
		unsigned int weapon;
		int forward;
		int right;
		int up;

		if ( sscanf( line, "%d %d %d %d %u %u %d %d %d",
				&cmd->serverTime,
				&cmd->angles[0], &cmd->angles[1], &cmd->angles[2],
				&buttons, &weapon,
				&forward, &right, &up ) != 9 ) {
			return qfalse;
		}
		cmd->buttons = (int)buttons;
		cmd->weapon = (byte)weapon;
		cmd->forwardmove = (signed char)forward;
		cmd->rightmove = (signed char)right;
		cmd->upmove = (signed char)up;
	}

	/* Keep viewangles aligned with replayed look direction. */
	for ( i = 0; i < 3; i++ ) {
		cl.viewangles[i] = (float)SHORT2ANGLE( cmd->angles[i] );
	}

	beta_replayFrameIndex++;
	return qtrue;
}

void CL_BetaTrace_LogEvent( const char *type, const char *source, const char *target ) {
	char line[BETA_TRACE_LINE];
	int t;
	const char *src;
	const char *tgt;

	if ( !type || !type[0] ) {
		return;
	}

	src = source ? source : "";
	tgt = target ? target : "";
	t = cls.realtime;

	if ( beta_mode == BETA_MODE_RECORD && beta_evtHandle != FS_INVALID_HANDLE ) {
		Com_sprintf( line, sizeof( line ),
			"{\"t\":%d,\"type\":\"%s\",\"source\":\"%s\",\"target\":\"%s\"}\n",
			t, type, src, tgt );
		FS_Write( line, (int)strlen( line ), beta_evtHandle );
	}

	CL_BetaPetri_OnGameplayEvent( type, src, tgt );

	if ( beta_mode == BETA_MODE_TEST && !beta_testFinished ) {
		int i;

		if ( CL_BetaPetri_GoalReached() ) {
			beta_testFinished = qtrue;
			beta_testResult = 1;
			Com_Printf( S_COLOR_GREEN "Beta test PASS (Petri goal reached)\n" );
			return;
		}

		for ( i = 0; i < beta_failCount; i++ ) {
			if ( Q_stristr( type, beta_failPatterns[i] ) ||
				Q_stristr( src, beta_failPatterns[i] ) ||
				Q_stristr( tgt, beta_failPatterns[i] ) ) {
				beta_testFinished = qtrue;
				beta_testResult = -1;
				Com_Printf( S_COLOR_RED "Beta test FAIL (event '%s' matched fail pattern)\n", type );
				return;
			}
		}

		for ( i = 0; i < beta_successCount; i++ ) {
			if ( Q_stristr( type, beta_successPatterns[i] ) ||
				Q_stristr( src, beta_successPatterns[i] ) ||
				Q_stristr( tgt, beta_successPatterns[i] ) ) {
				beta_testFinished = qtrue;
				beta_testResult = 1;
				Com_Printf( S_COLOR_GREEN "Beta test PASS (event '%s')\n", type );
				return;
			}
		}
	}

	if ( cl_betaTraceLog && cl_betaTraceLog->integer > 1 ) {
		Com_Printf( "Beta event: %s src=%s tgt=%s\n", type, src, tgt );
	}
}

void CL_BetaTrace_OnMapLoaded( const char *mapname ) {
	if ( !mapname || !mapname[0] ) {
		return;
	}

	Q_strncpyz( beta_mapName, mapname, sizeof( beta_mapName ) );
	COM_StripExtension( beta_mapName, beta_mapName, sizeof( beta_mapName ) );

	if ( beta_mode == BETA_MODE_RECORD || beta_mode == BETA_MODE_TEST ) {
		CL_BetaTrace_LogEvent( "map_loaded", beta_mapName, "" );
	}
}

static void CL_BetaTrace_BeginRecord( const char *base ) {
	char path[BETA_TRACE_MAX_PATH];
	const char *hdr;

	if ( !cl_betaTrace || !cl_betaTrace->integer ) {
		Com_Printf( S_COLOR_YELLOW "Warning: cl_betaTrace is 0; set to 1 to record\n" );
	}

	CL_BetaTrace_CloseFiles();
	Q_strncpyz( beta_baseName, base, sizeof( beta_baseName ) );
	beta_mode = BETA_MODE_RECORD;
	beta_recordStartTime = cls.realtime;
	beta_mapName[0] = '\0';
	beta_successCount = 0;
	beta_failCount = 0;

	CL_BetaTrace_BuildPath( path, sizeof( path ), base, "betacmd" );
	beta_cmdHandle = FS_FOpenFileWrite( path );
	if ( beta_cmdHandle == FS_INVALID_HANDLE ) {
		Com_Printf( S_COLOR_RED "Error: could not open %s for write\n", path );
		beta_mode = BETA_MODE_IDLE;
		return;
	}
	hdr = BETA_TRACE_CMD_MAGIC "\n";
	FS_Write( hdr, (int)strlen( hdr ), beta_cmdHandle );

	CL_BetaTrace_BuildPath( path, sizeof( path ), base, "betaevt" );
	beta_evtHandle = FS_FOpenFileWrite( path );
	if ( beta_evtHandle == FS_INVALID_HANDLE ) {
		Com_Printf( S_COLOR_YELLOW "Warning: could not open %s for write\n", path );
	}

	Com_Printf( "Beta trace: recording to beta_traces/%s.*\n", base );
	CL_BetaTrace_PublishStudioCvars();
}

static qboolean CL_BetaTrace_BeginPlayback( const char *base, qboolean testMode ) {
	char path[BETA_TRACE_MAX_PATH];
	int len;

	CL_BetaTrace_CloseFiles();
	Q_strncpyz( beta_baseName, base, sizeof( beta_baseName ) );
	beta_successCount = 0;
	beta_failCount = 0;
	beta_testMaxTimeMs = 0;
	beta_testFinished = qfalse;
	beta_testResult = 0;
	beta_cmdReplayActive = qfalse;
	beta_replayFrameIndex = 0;
	beta_mapName[0] = '\0';

	if ( testMode ) {
		CL_BetaTrace_LoadManifest( base );
		beta_mode = BETA_MODE_TEST;
		beta_testStartTime = cls.realtime;
	} else {
		beta_mode = BETA_MODE_REPLAY;
	}

	CL_BetaTrace_BuildPath( path, sizeof( path ), base, "betacmd" );
	len = FS_FOpenFileRead( path, &beta_cmdHandle, qtrue );
	if ( len < 0 || beta_cmdHandle == FS_INVALID_HANDLE ) {
		Com_Printf( S_COLOR_RED "Error: could not open %s\n", path );
		beta_mode = BETA_MODE_IDLE;
		return qfalse;
	}

	Com_Printf( "Beta trace: %s from beta_traces/%s.betacmd (%d bytes)\n",
		testMode ? "testing" : "replaying", base, len );
	CL_BetaTrace_PublishStudioCvars();
	return qtrue;
}

static void CL_BetaTrace_Stop_f( void ) {
	if ( beta_mode == BETA_MODE_RECORD ) {
		CL_BetaTrace_WriteManifest();
	}
	if ( beta_testFinished ) {
		if ( beta_testResult > 0 ) {
			Com_Printf( S_COLOR_GREEN "Beta trace: test_result=pass\n" );
		} else if ( beta_testResult < 0 ) {
			Com_Printf( S_COLOR_RED "Beta trace: test_result=fail\n" );
		}
	}
	CL_BetaTrace_CloseFiles();
	beta_mode = BETA_MODE_IDLE;
	beta_cmdReplayActive = qfalse;
	beta_testFinished = qfalse;
	CL_BetaTrace_PublishStudioCvars();
	Com_Printf( "Beta trace: stopped.\n" );
}

static void CL_BetaTrace_Status_f( void ) {
	const char *modeName = "idle";

	switch ( beta_mode ) {
	case BETA_MODE_RECORD:
		modeName = "record";
		break;
	case BETA_MODE_REPLAY:
		modeName = "replay";
		break;
	case BETA_MODE_TEST:
		modeName = "test";
		break;
	default:
		break;
	}

	Com_Printf( "Beta trace status:\n" );
	Com_Printf( "  mode=%s cl_betaTrace=%d\n", modeName,
		cl_betaTrace ? cl_betaTrace->integer : 0 );
	if ( beta_baseName[0] ) {
		Com_Printf( "  basename=%s\n", beta_baseName );
	}
	if ( beta_mapName[0] ) {
		Com_Printf( "  map=%s\n", beta_mapName );
	}
	if ( beta_mode == BETA_MODE_REPLAY || beta_mode == BETA_MODE_TEST ) {
		Com_Printf( "  replay_frame=%d replay_active=%d\n",
			beta_replayFrameIndex, beta_cmdReplayActive );
	}
	if ( beta_mode == BETA_MODE_TEST ) {
		const char *resultName = "pending";

		if ( beta_testFinished ) {
			resultName = ( beta_testResult > 0 ) ? "pass" : "fail";
		}
		Com_Printf( "  test_finished=%d test_result=%s max_time_ms=%d\n",
			beta_testFinished, resultName, beta_testMaxTimeMs );
	}
}

static void CL_BetaTrace_Record_f( void ) {
	const char *base;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: beta_record <basename>\n" );
		return;
	}

	base = Cmd_Argv( 1 );
	CL_BetaTrace_BeginRecord( base );
}

static void CL_BetaTrace_Play_f( void ) {
	const char *base;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: beta_play <basename>\n" );
		return;
	}

	base = Cmd_Argv( 1 );
	CL_BetaTrace_BeginPlayback( base, qfalse );
}

static void CL_BetaTrace_Test_f( void ) {
	const char *base;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: beta_test <basename>\n" );
		return;
	}

	base = Cmd_Argv( 1 );
	if ( !CL_BetaTrace_BeginPlayback( base, qtrue ) ) {
		return;
	}

	if ( beta_testMaxTimeMs <= 0 ) {
		beta_testMaxTimeMs = 300000;
	}
}

static void CL_BetaTrace_Event_f( void ) {
	const char *type;
	const char *source;
	const char *target;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: beta_event <type> [source] [target]\n" );
		return;
	}

	type = Cmd_Argv( 1 );
	source = Cmd_Argc() > 2 ? Cmd_Argv( 2 ) : "";
	target = Cmd_Argc() > 3 ? Cmd_Argv( 3 ) : "";
	CL_BetaTrace_LogEvent( type, source, target );
}

static void CL_BetaTrace_MarkSuccess_f( void ) {
	const char *pat;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: beta_mark_success <pattern>\n" );
		return;
	}

	pat = Cmd_Argv( 1 );
	if ( beta_successCount < BETA_TRACE_MAX_SUCCESS ) {
		Q_strncpyz( beta_successPatterns[beta_successCount], pat,
			sizeof( beta_successPatterns[0] ) );
		beta_successCount++;
	}
}

static void CL_BetaTrace_MarkFail_f( void ) {
	const char *pat;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: beta_mark_fail <pattern>\n" );
		return;
	}

	pat = Cmd_Argv( 1 );
	if ( beta_failCount < BETA_TRACE_MAX_FAIL ) {
		Q_strncpyz( beta_failPatterns[beta_failCount], pat,
			sizeof( beta_failPatterns[0] ) );
		beta_failCount++;
	}
}

void CL_BetaTrace_OnUserCmd( usercmd_t *cmd ) {
	if ( beta_mode == BETA_MODE_RECORD ) {
		if ( beta_cmdHandle != FS_INVALID_HANDLE ) {
			CL_BetaTrace_WriteCmdLine( cmd );
		}
		return;
	}

	if ( beta_mode == BETA_MODE_REPLAY || beta_mode == BETA_MODE_TEST ) {
		usercmd_t replay;

		if ( CL_BetaTrace_ReadCmdLine( &replay ) ) {
			*cmd = replay;
			beta_cmdReplayActive = qtrue;
		} else {
			beta_cmdReplayActive = qfalse;
			CL_BetaTrace_LogEvent( "trace_eof", beta_baseName, "" );
			if ( beta_mode == BETA_MODE_TEST && !beta_testFinished ) {
				if ( beta_successCount == 0 ) {
					beta_testFinished = qtrue;
					beta_testResult = 1;
					Com_Printf( S_COLOR_GREEN "Beta test PASS (trace completed, no success patterns)\n" );
				} else {
					beta_testFinished = qtrue;
					beta_testResult = -1;
					Com_Printf( S_COLOR_RED "Beta test FAIL (trace ended before success event)\n" );
				}
			}
			CL_BetaTrace_Stop_f();
		}
	}
}

qboolean CL_BetaTrace_IsReplaying( void ) {
	return ( beta_mode == BETA_MODE_REPLAY || beta_mode == BETA_MODE_TEST ) &&
		beta_cmdReplayActive;
}

qboolean CL_BetaTrace_ShouldSuppressInput( void ) {
	return beta_mode == BETA_MODE_REPLAY || beta_mode == BETA_MODE_TEST;
}

int CL_BetaTrace_LastTestResult( void ) {
	if ( !beta_testFinished ) {
		return 0;
	}
	return beta_testResult;
}

void CL_BetaTrace_Frame( void ) {
	CL_BetaTrace_PublishStudioCvars();

	if ( beta_mode != BETA_MODE_TEST || beta_testFinished ) {
		return;
	}

	if ( beta_testMaxTimeMs > 0 &&
		cls.realtime - beta_testStartTime > beta_testMaxTimeMs ) {
		beta_testFinished = qtrue;
		beta_testResult = -1;
		Com_Printf( S_COLOR_RED "Beta test FAIL (max_time_ms exceeded)\n" );
		CL_BetaTrace_Stop_f();
	}
}

void CL_BetaTrace_Init( void ) {
	CL_BetaPetri_Init();

	cl_betaTrace = Cvar_Get( "cl_betaTrace", "1", CVAR_ARCHIVE );
	cl_betaTraceLog = Cvar_Get( "cl_betaTraceLog", "1", CVAR_ARCHIVE );
	cl_betaTraceStudioMode = Cvar_Get( "cl_betaTraceStudioMode", "0", CVAR_ROM );
	cl_betaTraceStudioBase = Cvar_Get( "cl_betaTraceStudioBase", "", CVAR_ROM );
	Cvar_SetDescription( cl_betaTraceStudioMode,
		"Read-only: beta trace mode for ImGui Studio (0=idle 1=record 2=replay 3=test)." );
	Cvar_SetDescription( cl_betaTraceStudioBase,
		"Read-only: active beta trace basename (Studio / Author panel)." );

	Cmd_AddCommand( "beta_record", CL_BetaTrace_Record_f );
	Cmd_AddCommand( "beta_stop", CL_BetaTrace_Stop_f );
	Cmd_AddCommand( "beta_play", CL_BetaTrace_Play_f );
	Cmd_AddCommand( "beta_test", CL_BetaTrace_Test_f );
	Cmd_AddCommand( "beta_event", CL_BetaTrace_Event_f );
	Cmd_AddCommand( "beta_mark_success", CL_BetaTrace_MarkSuccess_f );
	Cmd_AddCommand( "beta_mark_fail", CL_BetaTrace_MarkFail_f );
	Cmd_AddCommand( "beta_status", CL_BetaTrace_Status_f );

	CL_BetaTrace_LogStartup();
}

void CL_BetaTrace_Shutdown( void ) {
	CL_BetaTrace_Stop_f();
	CL_BetaPetri_Shutdown();
	Cmd_RemoveCommand( "beta_record" );
	Cmd_RemoveCommand( "beta_stop" );
	Cmd_RemoveCommand( "beta_play" );
	Cmd_RemoveCommand( "beta_test" );
	Cmd_RemoveCommand( "beta_event" );
	Cmd_RemoveCommand( "beta_mark_success" );
	Cmd_RemoveCommand( "beta_mark_fail" );
	Cmd_RemoveCommand( "beta_status" );
}
