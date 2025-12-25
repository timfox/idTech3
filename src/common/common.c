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
// common.c -- misc functions used in client and server

#include "q_shared.h"
#include "qcommon.h"
#include "q_stability.h"
#include "q_memory_safety.h"
#include "q_error_recovery.h"
#include "q_input_validation.h"
#include "q_log.h"
#include "streaming_thread.h"
#include "net_threads.h"
#include "code_review.h"
#include "live_code_analysis.h"
#include "perf_test.h"
#include "memory_safety_test.h"
#include "thread_safety_test.h"
#include "code_quality.h"
#include "technical_debt.h"
#include "performance_benchmark.h"
#include "cross_platform_test.h"
#include "profiler.h"
#include "performance_counters.h"
#include "crash_handler.h"
#include "q_assert.h"
#include "q_watchdog.h"
#include "vm_hot_reload.h"
#include "event_system.h"
#include "q_memtrack.h"
#include "memory_stats.h"
#include "i18n.h"
#include "q_scalability.h"
#include "q_asset_loaders.h"
#include <locale.h>
#include <ctype.h>
#ifdef USE_CURL
#include "q_telemetry.h"
#endif
#include <setjmp.h>
#ifndef _WIN32
#include <netinet/in.h>
#include <sys/stat.h> // umask
#include <sys/time.h>
#else
#include <winsock.h>
#if defined(_DEBUG)
#include "../win32/win_local.h"
#endif
#endif

#include "../client/keys.h"

const int demo_protocols[] = { PROTOCOL_VERSION_66, PROTOCOL_VERSION_67, OLD_PROTOCOL_VERSION, NEW_PROTOCOL_VERSION, 0 };

#define USE_MULTI_SEGMENT // allocate additional zone segments on demand

#ifdef DEDICATED
#define MIN_COMHUNKMEGS		48
#define DEF_COMHUNKMEGS		56
#else
#define MIN_COMHUNKMEGS		64
#define DEF_COMHUNKMEGS		128
#endif

#ifdef USE_MULTI_SEGMENT
#define DEF_COMZONEMEGS		12
#else
#define DEF_COMZONEMEGS		25
#endif

static jmp_buf abortframe;	// an ERR_DROP occurred, exit the entire frame

int		CPU_Flags = 0;

static fileHandle_t logfile = FS_INVALID_HANDLE;
static fileHandle_t com_journalFile = FS_INVALID_HANDLE ; // events are written here
fileHandle_t com_journalDataFile = FS_INVALID_HANDLE; // config files are written here

cvar_t	*com_viewlog;
cvar_t	*com_speeds;
cvar_t	*com_developer;
cvar_t	*com_safemode;
cvar_t	*com_dedicated;
cvar_t	*com_timescale;
static cvar_t *com_fixedtime;
cvar_t	*com_journal;
cvar_t	*com_protocol;
qboolean com_protocolCompat;
#ifndef DEDICATED
cvar_t	*com_maxfps;
cvar_t	*com_maxfpsUnfocused;
cvar_t	*com_yieldCPU;
cvar_t	*com_timedemo;
#endif
#ifdef USE_AFFINITY_MASK
cvar_t	*com_affinityMask;
#endif
static cvar_t *com_logfile;		// 1 = buffer log, 2 = flush after each print
static cvar_t *com_showtrace;
cvar_t	*com_version;
static cvar_t *com_buildScript;	// for automated data building scripts

cvar_t	*com_jobThreads;

// Safety and monitoring cvars
cvar_t	*com_memoryStats;
cvar_t	*com_preciseTime;
cvar_t	*com_assertLevel;
cvar_t	*com_watchdogEnabled;
cvar_t	*com_crashHandlerEnabled;

// Advanced stability cvars
cvar_t	*com_crash_recovery;
cvar_t	*com_crash_minidump;
cvar_t	*com_crash_log_ringbuffer;
cvar_t	*com_crash_auto_restart;
cvar_t	*com_crash_telemetry;
cvar_t	*com_auto_save;
cvar_t	*com_error_recovery;
cvar_t	*com_fallback_renderer;
cvar_t	*com_safe_mode_detect;
cvar_t	*com_memory_guard;
cvar_t	*com_thread_safety;
cvar_t	*com_validation_level;

// Modern filesystem features
cvar_t	*fs_modern;
cvar_t	*fs_hotReloadEnabled;

// UI enhancement cvars
cvar_t	*ui_scale;
cvar_t	*ui_animationSpeed;
cvar_t	*ui_blur;
cvar_t	*ui_blurRadius;
cvar_t	*ui_mainMenuGlow;
cvar_t	*ui_mainMenuParticles;
cvar_t	*ui_mainMenuScanlines;
cvar_t	*cl_hideSystemCursor;
cvar_t	*cl_cursorSize;

// HUD enhancement cvars
cvar_t	*cg_hudScale;
cvar_t	*cg_hudBlur;
cvar_t	*cg_hudGlow;
cvar_t	*cg_crosshairScale;
cvar_t	*cg_crosshairGlow;
cvar_t	*cg_crosshairPulse;
cvar_t	*cg_screenDamage;
cvar_t	*cg_screenFlash;
cvar_t	*cg_screenBlur;

// Advanced performance monitoring cvars
cvar_t	*perf_monitor_enable;
cvar_t	*perf_gpu_profiler;
cvar_t	*perf_cpu_profiler;
cvar_t	*perf_memory_profiler;
cvar_t	*perf_frame_profiler;
cvar_t	*perf_cache_stats;
cvar_t	*perf_thread_stats;
cvar_t	*perf_network_stats;
cvar_t	*perf_log_interval;
cvar_t	*perf_csv_export;

#ifndef DEDICATED
static cvar_t	*com_introPlayed;
cvar_t	*com_skipIdLogo;

cvar_t	*cl_paused;
cvar_t	*cl_packetdelay;
cvar_t	*com_cl_running;
#endif

cvar_t	*sv_paused;
cvar_t  *sv_packetdelay;
cvar_t	*com_sv_running;

cvar_t	*com_cameraMode;
#if defined(_WIN32) && defined(_DEBUG)
cvar_t	*com_noErrorInterrupt;
#endif

// com_speeds times
int		time_game;
int		time_frontend;		// renderer frontend time
int		time_backend;		// renderer backend time

static int	lastTime;
int			com_frameTime;
static int	com_frameNumber;

qboolean	com_errorEntered = qfalse;
qboolean	com_fullyInitialized = qfalse;

static void Com_InitLocaleUTF8( void )
{
	const char *result;

	// Try user's locale first
	result = setlocale( LC_CTYPE, "" );
	if ( result && Q_stristr( result, "UTF-8" ) ) {
		return;
	}

	// Fallback to common UTF-8 locales
	result = setlocale( LC_CTYPE, "C.UTF-8" );
	if ( result && Q_stristr( result, "UTF-8" ) ) {
		return;
	}

	result = setlocale( LC_CTYPE, "en_US.UTF-8" );
	if ( result && Q_stristr( result, "UTF-8" ) ) {
		return;
	}

	Com_Printf( S_COLOR_YELLOW "Warning: Could not set UTF-8 locale. Text rendering may be limited.\n" );
}
// renderer window states
qboolean	gw_minimized = qfalse; // this will be always true for dedicated servers
#ifndef DEDICATED
qboolean	gw_active = qtrue;
#endif

static char com_errorMessage[ MAXPRINTMSG ];

static void Com_Shutdown( void );
static void Com_WriteConfig_f( void );
static void Com_NetThreads_f( void );
static void Com_StreamThreads_f( void );
static void Com_CodeReview_f( void );
static void Com_LiveCode_f( void );
static void Com_PerfTest_f( void );
static void Com_CrossPlatformTest_f( void );
static void Com_MemorySafetyTest_f( void );
static void Com_ThreadSafetyTest_f( void );
static void Com_CodeQuality_f( void );
static void Com_TechnicalDebt_f( void );
static void Com_PerformanceBenchmark_f( void );
void CIN_CloseAllVideos( void );

//============================================================================

static char	*rd_buffer;
static int	rd_buffersize;
static qboolean rd_flushing = qfalse;
static void	(*rd_flush)( const char *buffer );

void Com_BeginRedirect( char *buffer, int buffersize, void (*flush)(const char *) )
{
	if (!buffer || !buffersize || !flush)
		return;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;

	*rd_buffer = '\0';
}


void Com_EndRedirect( void )
{
	if ( rd_flush ) {
		rd_flushing = qtrue;
		rd_flush( rd_buffer );
		rd_flushing = qfalse;
	}

	rd_buffer = nullptr;
	rd_buffersize = 0;
	rd_flush = nullptr;
}

/*
=============
Com_LogFlush_f

Manually flush structured and legacy log outputs.
=============
*/
static void Com_LogFlush_f( void ) {
	if ( Q_Log_IsEnabled() ) {
		Q_Log_Flush();
	}

	if ( logfile != FS_INVALID_HANDLE ) {
		FS_ForceFlush( logfile );
	}
}


/*
=============
Com_Printf

Both client and server can use this, and it will output
to the appropriate place.

A raw string should NEVER be passed as fmt, because of "%f" type crashers.
=============
*/
void FORMAT_PRINTF(1, 2) QDECL Com_Printf( const char *fmt, ... ) {
	TracyCZoneCtx prof_printf;
	PROF_ZONE_BEGIN(prof_printf, "Com_Printf");
	static qboolean opening_qconsole = qfalse;
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	int			len;

	va_start( argptr, fmt );
	len = Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	// Add message to crash log ring buffer for diagnostics
	Crash_LogMessage( msg );

	if ( rd_buffer && !rd_flushing ) {
		if ( len + (int)strlen( rd_buffer ) > ( rd_buffersize - 1 ) ) {
			rd_flushing = qtrue;
			rd_flush( rd_buffer );
			rd_flushing = qfalse;
			*rd_buffer = '\0';
		}
		Q_strcat( rd_buffer, rd_buffersize, msg );
		// TTimo nooo .. that would defeat the purpose
		//rd_flush(rd_buffer);
		//*rd_buffer = '\0';
		return;
	}

	// If structured logging is enabled, use it (with deferred queue support)
	// Otherwise fall back to legacy behavior
	// Skip ALL file logging (structured and legacy) during filesystem startup/restart to prevent recursive errors
	// This is critical because FS_Restart clears fs_searchpaths temporarily
	// Structured logging now uses a deferred queue, so it's safe to call Q_Log_ComPrintf even during FS_Restart
	// CRITICAL: Check BOTH FS_StartupInProgress() AND FS_Initialized() to catch all cases
	// FS_StartupInProgress() may be false at the very start of FS_Startup before the flag is set
	// FS_Initialized() checks fs_searchpaths != NULL, which is NULL during FS_Startup
	qboolean fs_ready = FS_Initialized() && !FS_StartupInProgress();
	
	if (!fs_ready) {
		// During startup or when filesystem not initialized, only output to console/system, skip legacy file logging
		// Structured logging will queue messages for later (Q_Log handles the deferring)
		if (Q_Log_IsEnabled()) {
			Q_Log_ComPrintf("%s", msg);
		}
		// Skip legacy file logging during filesystem operations
		if (!(com_logfile && com_logfile->integer)) {
			return;
		}
	} else if (Q_Log_IsEnabled()) {
		Q_Log_ComPrintf("%s", msg);
		// Still do legacy file logging if enabled for compatibility
		if (!(com_logfile && com_logfile->integer)) {
			return;
		}
	}

#ifndef DEDICATED
	// echo to client console if we're not a dedicated server
	if ( !com_dedicated || !com_dedicated->integer ) {
		CL_ConsolePrint( msg );
	}
#endif

	// echo to dedicated console and early console
	Sys_Print( msg );

	// logfile
	if ( com_logfile && com_logfile->integer ) {
		// TTimo: only open the console.log if the filesystem is in an initialized state
		//   also, avoid recursing in the console.log opening (i.e. if fs_debug is on)
		// Check fs_startupInProgress to avoid calling FS_FOpenFileWrite/FS_FOpenFileAppend during startup
		if ( logfile == FS_INVALID_HANDLE && FS_Initialized() && !opening_qconsole && !FS_StartupInProgress() ) {
			const char *logName = "console.log";
			int mode;

			opening_qconsole = qtrue;

			mode = com_logfile->integer - 1;

			if ( mode & 2 )
				logfile = FS_FOpenFileAppend( logName );
			else
				logfile = FS_FOpenFileWrite( logName );

			if ( logfile != FS_INVALID_HANDLE ) {
				struct tm *newtime;
				time_t aclock;
				char timestr[32];

				time( &aclock );
				newtime = localtime( &aclock );
				strftime( timestr, sizeof( timestr ), "%a %b %d %X %Y", newtime );

				Com_Printf( "logfile opened on %s\n", timestr );

				if ( mode & 1 ) {
					// force it to not buffer so we get valid
					// data even if we are crashing
					FS_ForceFlush( logfile );
				}
			} else {
				Com_Printf( S_COLOR_YELLOW "Opening %s failed!\n", logName );
				Cvar_Set( "logfile", "0" );
			}

			opening_qconsole = qfalse;
		}
		// Only write if filesystem is initialized and not in startup
		if ( logfile != FS_INVALID_HANDLE && FS_Initialized() && !FS_StartupInProgress() ) {
			FS_Write( msg, len, logfile );
		}
	}

	// Add to crash handler ring buffer for crash diagnostics
	Crash_LogMessage(msg);

	PROF_ZONE_END(prof_printf);
}


/*
================
Com_DPrintf

A Com_Printf that only shows up if the "developer" cvar is set
================
*/
void FORMAT_PRINTF(1, 2) QDECL Com_DPrintf( const char *fmt, ... ) {
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if ( !com_developer || !com_developer->integer ) {
		return;			// don't confuse non-developers with techie stuff...
	}

	va_start( argptr,fmt );
	Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	Com_Printf( S_COLOR_CYAN "%s", msg );
}


/*
=============
Com_Error

Both client and server can use this, and it will
do the appropriate things.
=============
*/
static char *errorContext = NULL;

#include "../botlib/botlib.h"
botlib_export_t	*botlib_export;

void Com_SetErrorContext( const char *context ) {
	errorContext = (char *)context;
}

void Com_ClearErrorContext( void ) {
	errorContext = NULL;
}

static void Com_HandlePlatformErrorDebugging( errorParm_t code ) {
	(void)code;
#if defined(_WIN32) && defined(_DEBUG)
	if ( code != ERR_DISCONNECT && code != ERR_NEED_CD ) {
		if ( !com_noErrorInterrupt->integer ) {
			ShowWindow( g_wv.hWnd, SW_MINIMIZE );
			DebugBreak();
		}
	}
#endif
}

// Enhanced logging system
static const char *log_level_names[] = {
	"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"
};

static const char *log_category_names[] = {
	"GENERAL", "RENDERER", "FILESYSTEM", "NETWORK", "GAME", "SOUND", "PERFORMANCE"
};

void Com_Log(log_level_t level, log_category_t category, const char *fmt, ...) {
	va_list argptr;
	char msg[2048];
	static cvar_t *log_level = NULL;
	static cvar_t *log_categories = NULL;

	if (!log_level) {
		log_level = Cvar_Get("com_logLevel", "2", CVAR_ARCHIVE); // Default to WARNING
	}
	if (!log_categories) {
		log_categories = Cvar_Get("com_logCategories", "127", CVAR_ARCHIVE); // All categories enabled
	}

        if ((int)level < log_level->integer) {
		return; // Filter out messages below the current log level
	}

	if (!(log_categories->integer & (1 << category))) {
		return; // Category is disabled
	}

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Com_Printf("[%s] [%s] %s\n", log_level_names[level], log_category_names[category], msg);
}

#define MAX_TIMING_STACK 32
static struct {
	const char *operation;
	int start_time;
} timing_stack[MAX_TIMING_STACK];
static int timing_stack_depth = 0;

void Com_StartTiming(const char *operation) {
	if (timing_stack_depth < MAX_TIMING_STACK) {
		timing_stack[timing_stack_depth].operation = operation;
		timing_stack[timing_stack_depth].start_time = Sys_Milliseconds();
		timing_stack_depth++;
	}
}

void Com_EndTiming(const char *operation) {
	if (timing_stack_depth > 0) {
		timing_stack_depth--;
		int duration = Sys_Milliseconds() - timing_stack[timing_stack_depth].start_time;

		Com_Log(LOG_LEVEL_DEBUG, LOG_CATEGORY_PERFORMANCE,
			"Operation '%s' completed in %d ms", operation, duration);
	}
}

void Com_LogPerformance(const char *operation, int duration_ms) {
	Com_Log(LOG_LEVEL_INFO, LOG_CATEGORY_PERFORMANCE,
		"Performance: %s took %d ms", operation, duration_ms);
}

void NORETURN FORMAT_PRINTF(2, 3) QDECL Com_Error( errorParm_t code, const char *fmt, ... ) {
	va_list		argptr;
	static int	lastErrorTime;
	static int	errorCount;
	int			currentTime;
	char		debugMsg[1024];

	Com_HandlePlatformErrorDebugging( code );

	if ( com_errorEntered ) {
		// Prevent hard recursion: bail out hard to satisfy noreturn contract
		char recursionMsg[1024];
		va_start( argptr, fmt );
		Q_vsnprintf( recursionMsg, sizeof( recursionMsg ), fmt, argptr );
		va_end( argptr );
		Sys_Error( "Recursive Com_Error: %s", recursionMsg );
	}

	com_errorEntered = qtrue;

	Cvar_SetIntegerValue( "com_errorCode", code );

	// when we are running automated scripts, make sure we
	// know if anything failed
	if ( com_buildScript && com_buildScript->integer ) {
		code = ERR_FATAL;
	}

	// if we are getting a solid stream of ERR_DROP, do an ERR_FATAL
#define ERROR_RATE_LIMIT_MS 100
#define ERROR_RATE_LIMIT_COUNT 3

	currentTime = Sys_Milliseconds();
	if ( currentTime - lastErrorTime < ERROR_RATE_LIMIT_MS ) {
		if ( ++errorCount > ERROR_RATE_LIMIT_COUNT ) {
			code = ERR_FATAL;
		}
	} else {
		errorCount = 0;
	}
	lastErrorTime = currentTime;

	// capture incoming message for diagnostics before formatting with file:line
	va_start( argptr, fmt );
	Q_vsnprintf( debugMsg, sizeof( debugMsg ), fmt, argptr );
	va_end( argptr );

	if ( errorContext ) {
		Sys_Print( va( "Com_Error: context=\"%s\" code=%d msg=\"%s\"\n", errorContext, code, debugMsg ) );
	} else {
		Sys_Print( va( "Com_Error: code=%d msg=\"%s\"\n", code, debugMsg ) );
	}

	va_start( argptr, fmt );
#ifdef NDEBUG
	Q_vsnprintf( com_errorMessage, sizeof( com_errorMessage ), fmt, argptr );
#else
	// In debug builds, include file and line information for better debugging
	char temp[sizeof(com_errorMessage) - 256]; // Reserve space for file/line info
	Q_vsnprintf( temp, sizeof( temp ), fmt, argptr );
	// Use a safer approach to avoid truncation warnings
	int len = Q_snprintf( com_errorMessage, sizeof( com_errorMessage ), "%s [%s:%d]", temp, __FILE__, __LINE__ );
	if (len >= (int)sizeof(com_errorMessage)) {
		// Truncation occurred, ensure null termination
		com_errorMessage[sizeof(com_errorMessage) - 1] = '\0';
	}
#endif
	va_end( argptr );

	// If fatal mentions file-not-found (even with control codes), demote to drop
	if ( code == ERR_FATAL ) {
		if ( Q_stristr( com_errorMessage, "file not found" ) ) {
			code = ERR_DROP;
		}
	}

	// If a missing file triggers a fatal error, try to downgrade to a drop so we can continue
	if ( code == ERR_FATAL ) {
		if ( !Q_strnicmp( com_errorMessage, "file not found:", 15 ) ) {
			code = ERR_DROP;
		}
	}

	if ( code != ERR_DISCONNECT && code != ERR_NEED_CD ) {
		// we can't recover from ERR_FATAL so there is no recipients for com_errorMessage
		// also if ERR_FATAL was called from S_Malloc - CopyString for a long (2+ chars) text
		// will trigger recursive error without proper client/server shutdown
		if ( code != ERR_FATAL ) {
			Cvar_Set( "com_errorMessage", com_errorMessage );
		}
	}

	Cbuf_Init();

	if ( code == ERR_DISCONNECT || code == ERR_SERVERDISCONNECT ) {
		VM_Forced_Unload_Start();
		SV_Shutdown( "Server disconnected" );
		Com_EndRedirect();
#ifndef DEDICATED
		CL_Disconnect( qfalse );
		CL_FlushMemory();
#endif
		VM_Forced_Unload_Done();

		// make sure we can get at our local stuff
		FS_PureServerSetLoadedPaks( "", "" );
		com_errorEntered = qfalse;

		Q_longjmp( abortframe, 1 );
	} else if ( code == ERR_DROP ) {
		Com_Printf( "********************\nERROR: %s\n********************\n",
			com_errorMessage );
		VM_Forced_Unload_Start();
		SV_Shutdown( va( "Server crashed: %s",  com_errorMessage ) );
		Com_EndRedirect();
#ifndef DEDICATED
		CL_Disconnect( qfalse );
		CL_FlushMemory();
#endif
		VM_Forced_Unload_Done();

		FS_PureServerSetLoadedPaks( "", "" );
		com_errorEntered = qfalse;

		Q_longjmp( abortframe, 1 );
	} else if ( code == ERR_NEED_CD ) {
		SV_Shutdown( "Server didn't have CD" );
		Com_EndRedirect();
#ifndef DEDICATED
		if ( com_cl_running && com_cl_running->integer ) {
			CL_Disconnect( qfalse );
			VM_Forced_Unload_Start();
			CL_FlushMemory();
			VM_Forced_Unload_Done();
			CL_CDDialog();
		} else {
			Com_Printf( "Server didn't have CD\n" );
		}
#endif
		FS_PureServerSetLoadedPaks( "", "" );
		com_errorEntered = qfalse;

		Q_longjmp( abortframe, 1 );
	} else {
		VM_Forced_Unload_Start();
#ifndef DEDICATED
		CL_Shutdown( va( "Server fatal crashed: %s", com_errorMessage ), qtrue );
#endif
		SV_Shutdown( va( "Server fatal crashed: %s", com_errorMessage ) );
		Com_EndRedirect();
		VM_Forced_Unload_Done();
	}

	Com_Shutdown();

	Sys_Error( "%s", com_errorMessage );
}


/*
=============
Com_Quit_f

Both client and server can use this, and it will
do the appropriate things.
=============
*/
void Com_Quit_f( void ) {
	const char *p = Cmd_ArgsFrom( 1 );
	// don't try to shutdown if we are in a recursive error
	if ( !com_errorEntered ) {
		// Some VMs might execute "quit" command directly,
		// which would trigger an unload of active VM error.
		// Sys_Quit will kill this process anyways, so
		// a corrupt call stack makes no difference
		VM_Forced_Unload_Start();
		SV_Shutdown( p[0] ? p : "Server quit" );
#ifndef DEDICATED
		CL_Shutdown( p[0] ? p : "Client quit", qtrue );
#endif
		VM_Forced_Unload_Done();
		Com_Shutdown();
		FS_Shutdown( qtrue );
	}
	Sys_Quit();
}


/*
=============
Com_Help_f

Show help for commands. Usage: help [command]
If no command is specified, lists common commands.
=============
*/
void Com_Help_f( void ) {
	const char *cmdname;

	if ( Cmd_Argc() > 1 ) {
		// Show help for specific command
		cmdname = Cmd_Argv( 1 );
		Com_Printf( "\nHelp for: %s\n", cmdname );
		Com_Printf( "Use 'cmdlist %s' to see similar commands.\n", cmdname );
		Com_Printf( "Note: Some commands may be game-specific (game/cgame/ui modules).\n" );
		Com_Printf( "To see CVar information, use the CVar name directly (e.g., 'version').\n" );
	} else {
		// List common commands
		Com_Printf( "\n" );
		Com_Printf( "========================================\n" );
		Com_Printf( "Console Help\n" );
		Com_Printf( "========================================\n" );
		Com_Printf( "\nCommon commands:\n" );
		Com_Printf( "  help        - Show this help or help for a specific command\n" );
		Com_Printf( "  cmdlist     - List all available commands\n" );
		Com_Printf( "  clear       - Clear console output\n" );
		Com_Printf( "  quit/exit   - Exit the game\n" );
		Com_Printf( "  about       - Show engine information\n" );
		Com_Printf( "  buildinfo   - Show detailed build information\n" );
		Com_Printf( "  version     - Show version (also available as CVar)\n" );
		Com_Printf( "\nUsage:\n" );
		Com_Printf( "  help <command>  - Get help for a specific command\n" );
		Com_Printf( "  cmdlist        - List all commands\n" );
		Com_Printf( "  cmdlist <filter> - List commands matching filter\n" );
		Com_Printf( "\n========================================\n" );
		Com_Printf( "\n" );
	}
}


/*
=============
Com_About_f

Display engine version and information.
=============
*/
void Com_About_f( void ) {
	Com_Printf( "\n" );
	Com_Printf( "========================================\n" );
	Com_Printf( "%s\n", Q3_VERSION );
	Com_Printf( "========================================\n" );
	Com_Printf( "Platform: %s\n", PLATFORM_STRING );
	Com_Printf( "Build Date: %s\n", Q_BUILD_DATE );
	Com_Printf( "Build Time: %s\n", Q_BUILD_TIME );
	Com_Printf( "\n" );
	Com_Printf( "Based on Quake III Arena engine\n" );
	Com_Printf( "Enhanced with modern features\n" );
	Com_Printf( "\n" );
	Com_Printf( "Use 'buildinfo' for detailed build information.\n" );
	Com_Printf( "========================================\n" );
	Com_Printf( "\n" );
}


/*
=============
Com_BuildInfo_f

Display detailed build information including compiler and architecture.
=============
*/
void Com_BuildInfo_f( void ) {
	Com_Printf( "\n" );
	Com_Printf( "========================================\n" );
	Com_Printf( "Build Information\n" );
	Com_Printf( "========================================\n" );
	Com_Printf( "Version: %s\n", Q3_VERSION );
	Com_Printf( "Platform: %s\n", PLATFORM_STRING );
	Com_Printf( "Build Date: %s\n", Q_BUILD_DATE );
	Com_Printf( "Build Time: %s\n", Q_BUILD_TIME );
#ifdef __GNUC__
	Com_Printf( "Compiler: GCC %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__ );
#elif defined(_MSC_VER)
	Com_Printf( "Compiler: MSVC %d\n", _MSC_VER );
#elif defined(__clang__)
	Com_Printf( "Compiler: Clang %d.%d.%d\n", __clang_major__, __clang_minor__, __clang_patchlevel__ );
#else
	Com_Printf( "Compiler: Unknown\n" );
#endif
#ifdef NDEBUG
	Com_Printf( "Build Type: Release\n" );
#else
	Com_Printf( "Build Type: Debug\n" );
#endif
	Com_Printf( "Architecture: %s\n", ARCH_STRING );
#ifdef USE_SDL
	Com_Printf( "SDL: Enabled\n" );
#else
	Com_Printf( "SDL: Disabled\n" );
#endif
#ifdef USE_VULKAN
	Com_Printf( "Vulkan: Enabled\n" );
#endif
#ifdef USE_CURL
	Com_Printf( "CURL: Enabled\n" );
#endif
	Com_Printf( "========================================\n" );
	Com_Printf( "\n" );
}


/*
============================================================================

COMMAND LINE FUNCTIONS

+ characters separate the commandLine string into multiple console
command lines.

All of these are valid:

quake3 +set test blah +map test
quake3 set test blah+map test
quake3 set test blah + map test

============================================================================
*/

#define	MAX_CONSOLE_LINES	32
static int	com_numConsoleLines;
static char	*com_consoleLines[MAX_CONSOLE_LINES];

// master rcon password
char	rconPassword2[MAX_CVAR_VALUE_STRING];

/*
==================
Com_ParseCommandLine

Break it up into multiple console lines
==================
*/
static void Com_ParseCommandLine( char *commandLine ) {
	static int parsed = 0;
	int inq;

	if ( parsed )
		return;

	inq = 0;
	com_consoleLines[0] = commandLine;
	rconPassword2[0] = '\0';

	while ( *commandLine ) {
		if (*commandLine == '"') {
			inq = !inq;
		}
		// look for a + separating character
		// if commandLine came from a file, we might have real line separators
		if ( (*commandLine == '+' && !inq) || *commandLine == '\n'  || *commandLine == '\r' ) {
			if ( com_numConsoleLines == MAX_CONSOLE_LINES ) {
				break;
			}
			com_consoleLines[com_numConsoleLines] = commandLine + 1;
			com_numConsoleLines++;
			*commandLine = '\0';
		}
		commandLine++;
	}
	parsed = 1;
}

char cl_title[ MAX_CVAR_VALUE_STRING ] = CLIENT_WINDOW_TITLE;

/*
===================
Com_EarlyParseCmdLine

returns qtrue if both vid_xpos and vid_ypos was set
===================
*/
qboolean Com_EarlyParseCmdLine( char *commandLine, char *con_title, int title_size, int *vid_xpos, int *vid_ypos )
{
	int		flags = 0;
	int		i;

	*con_title = '\0';
	Com_ParseCommandLine( commandLine );

	for ( i = 0 ; i < com_numConsoleLines ; i++ ) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( !Q_stricmpn( Cmd_Argv(0), "set", 3 ) && !Q_stricmp( Cmd_Argv(1), "cl_title" ) ) {
			com_consoleLines[i][0] = '\0';
			Q_strncpyz( cl_title, Cmd_ArgsFrom( 2 ), sizeof(cl_title) );
			continue;
		}
		if ( !Q_stricmp( Cmd_Argv(0), "cl_title" ) ) {
			com_consoleLines[i][0] = '\0';
			Q_strncpyz( cl_title, Cmd_ArgsFrom( 1 ), sizeof(cl_title) );
			continue;
		}
		if ( !Q_stricmpn( Cmd_Argv(0), "set", 3 ) && !Q_stricmp( Cmd_Argv(1), "con_title" ) ) {
			com_consoleLines[i][0] = '\0';
			Q_strncpyz( con_title, Cmd_ArgsFrom( 2 ), title_size );
			continue;
		}
		if ( !Q_stricmp( Cmd_Argv(0), "con_title" ) ) {
			com_consoleLines[i][0] = '\0';
			Q_strncpyz( con_title, Cmd_ArgsFrom( 1 ), title_size );
			continue;
		}
		if ( !Q_stricmpn( Cmd_Argv(0), "set", 3 ) && !Q_stricmp( Cmd_Argv(1), "vid_xpos" ) ) {
			*vid_xpos = Q_SafeAtoi( Cmd_Argv( 2 ), 0, NULL );
			flags |= 1;
			continue;
		}
		if ( !Q_stricmp( Cmd_Argv(0), "vid_xpos" ) ) {
			*vid_xpos = Q_SafeAtoi( Cmd_Argv( 1 ), 0, NULL );
			flags |= 1;
			continue;
		}
		if ( !Q_stricmpn( Cmd_Argv(0), "set", 3 ) && !Q_stricmp( Cmd_Argv(1), "vid_ypos" ) ) {
			*vid_ypos = Q_SafeAtoi( Cmd_Argv( 2 ), 0, NULL );
			flags |= 2;
			continue;
		}
		if ( !Q_stricmp( Cmd_Argv(0), "vid_ypos" ) ) {
			*vid_ypos = Q_SafeAtoi( Cmd_Argv( 1 ), 0, NULL );
			flags |= 2;
			continue;
		}
		if ( !Q_stricmpn( Cmd_Argv(0), "set", 3 ) && !Q_stricmp( Cmd_Argv(1), "rconPassword2" ) ) {
			com_consoleLines[i][0] = '\0';
			Q_strncpyz( rconPassword2, Cmd_Argv( 2 ), sizeof( rconPassword2 ) );
			continue;
		}
	}

	return (flags == 3) ? qtrue : qfalse ;
}


/*
===================
Com_SafeMode

Check for "safe" on the command line, which will
skip loading of config.cfg
===================
*/
qboolean Com_SafeMode( void ) {
	int		i;

	for ( i = 0 ; i < com_numConsoleLines ; i++ ) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( !Q_stricmp( Cmd_Argv(0), "safe" )
			|| !Q_stricmp( Cmd_Argv(0), "cvar_restart" ) ) {
			com_consoleLines[i][0] = '\0';
			return qtrue;
		}
	}
	return qfalse;
}


/*
===============
Com_StartupVariable

Searches for command line parameters that are set commands.
If match is not NULL, only that cvar will be looked for.
That is necessary because cddir and basedir need to be set
before the filesystem is started, but all other sets should
be after execing the config and default.
===============
*/
void Com_StartupVariable( const char *match ) {
	int i;
	const char *name;

	for ( i = 0; i < com_numConsoleLines; i++ ) {
		Cmd_TokenizeString( com_consoleLines[i] );

		// Handle "set" commands
		if ( !Q_stricmp( Cmd_Argv( 0 ), "set" ) ) {
		name = Cmd_Argv( 1 );
		if ( !match || Q_stricmp( name, match ) == 0 ) {
			if ( Cvar_Flags( name ) == CVAR_NONEXISTENT )
				Cvar_Get( name, Cmd_ArgsFrom( 2 ), CVAR_USER_CREATED );
			else
				Cvar_Set2( name, Cmd_ArgsFrom( 2 ), qfalse );
			}
		}
		// Also handle direct cvar assignments like -fs_game mymod
		else if ( !match || !Q_stricmp( Cmd_Argv( 0 ), match ) ) {
			// Check if this is a known filesystem cvar that needs early processing
			if ( !Q_stricmp( Cmd_Argv( 0 ), "fs_game" ) && Cmd_Argc() >= 2 ) {
				if ( Cvar_Flags( "fs_game" ) == CVAR_NONEXISTENT )
					Cvar_Get( "fs_game", Cmd_Argv( 1 ), CVAR_INIT | CVAR_SYSTEMINFO );
				else
					Cvar_Set2( "fs_game", Cmd_Argv( 1 ), qfalse );
			}
		}
	}
}


/*
=================
Com_AddStartupCommands

Adds command line parameters as script statements
Commands are separated by + signs

Returns qtrue if any late commands were added, which
will keep the demoloop from immediately starting
=================
*/
static qboolean Com_AddStartupCommands( void ) {
	int		i;
	qboolean	added;

	added = qfalse;
	// quote every token, so args with semicolons can work
	for (i=0 ; i < com_numConsoleLines ; i++) {
		if ( !com_consoleLines[i] || !com_consoleLines[i][0] ) {
			continue;
		}

		// set commands already added with Com_StartupVariable
		if ( !Q_stricmpn( com_consoleLines[i], "set ", 4 ) ) {
			continue;
		}

		added = qtrue;
		Cbuf_AddText( com_consoleLines[i] );
		Cbuf_AddText( "\n" );
	}

	return added;
}


//============================================================================

void Info_Print( const char *s ) {
	char	key[BIG_INFO_KEY];
	char	value[BIG_INFO_VALUE];

	do {
		s = Info_NextPair( s, key, value );
		if ( key[0] == '\0' )
			break;

		if ( value[0] == '\0' )
			Q_strncpyz( value, "MISSING VALUE", sizeof( value ) );

		Com_Printf( "%-20s %s\n", key, value );

	} while ( *s != '\0' );
}


/*
============
Com_StringContains
============
*/
static const char *Com_StringContains( const char *str1, const char *str2, int len2 ) {
	int len, i, j;

	len = strlen(str1) - len2;
	for (i = 0; i <= len; i++, str1++) {
		for (j = 0; str2[j]; j++) {
			if (locase[(byte)str1[j]] != locase[(byte)str2[j]]) {
				break;
			}
		}
		if (!str2[j]) {
			return str1;
		}
	}
	return nullptr;
}


/*
============
Com_Filter
============
*/
int Com_Filter( const char *filter, const char *name )
{
	char buf[ MAX_TOKEN_CHARS ];
	const char *ptr;
	int i, found;

	while(*filter) {
		if (*filter == '*') {
			filter++;
			for (i = 0; *filter; i++) {
				if (*filter == '*' || *filter == '?')
					break;
				buf[i] = *filter;
				filter++;
			}
			buf[i] = '\0';
			if ( i ) {
				ptr = Com_StringContains( name, buf, i );
				if ( !ptr )
					return qfalse;
				name = ptr + i;
			}
		}
		else if (*filter == '?') {
			filter++;
			name++;
		}
		else if (*filter == '[' && *(filter+1) == '[') {
			filter++;
		}
		else if (*filter == '[') {
			filter++;
			found = qfalse;
			while(*filter && !found) {
				if (*filter == ']' && *(filter+1) != ']') break;
				if (*(filter+1) == '-' && *(filter+2) && (*(filter+2) != ']' || *(filter+3) == ']')) {
					if (locase[(byte)*name] >= locase[(byte)*filter] &&
						locase[(byte)*name] <= locase[(byte)*(filter+2)])
							found = qtrue;
					filter += 3;
				}
				else {
					if (locase[(byte)*filter] == locase[(byte)*name])
						found = qtrue;
					filter++;
				}
			}
			if (!found) return qfalse;
			while(*filter) {
				if (*filter == ']' && *(filter+1) != ']') break;
				filter++;
			}
			filter++;
			name++;
		}
		else {
			if (locase[(byte)*filter] != locase[(byte)*name])
				return qfalse;
			filter++;
			name++;
		}
	}
	return qtrue;
}


/*
============
Com_FilterExt
============
*/
qboolean Com_FilterExt( const char *filter, const char *name )
{
	char buf[ MAX_TOKEN_CHARS ];
	const char *ptr;
	int i;

	while ( *filter ) {
		if ( *filter == '*' ) {
			filter++;
			for ( i = 0; *filter != '\0' && i < (int)sizeof(buf)-1; i++ ) {
				if ( *filter == '*' || *filter == '?' )
					break;
				buf[i] = *filter++;
			}
			buf[ i ] = '\0';
			if ( i ) {
				ptr = Com_StringContains( name, buf, i );
				if ( !ptr )
					return qfalse;
				name = ptr + i;
			} else if ( *filter == '\0' ) {
				return qtrue;
			}
		}
		else if ( *filter == '?' ) {
			if ( *name == '\0' )
				return qfalse;
			filter++;
			name++;
		}
		else {
			if ( locase[(byte)*filter] != locase[(byte)*name] )
				return qfalse;
			filter++;
			name++;
		}
	}
	if ( *name ) {
		return qfalse;
	}
	return qtrue;
}


/*
============
Com_HasPatterns
============
*/
qboolean Com_HasPatterns( const char *str )
{
	int c;

	while ( (c = *str++) != '\0' )
	{
		if ( c == '*' || c == '?' )
		{
			return qtrue;
		}
	}

	return qfalse;
}


/*
============
Com_FilterPath
============
*/
int Com_FilterPath( const char *filter, const char *name )
{
	int i;
	char new_filter[MAX_QPATH];
	char new_name[MAX_QPATH];

	for (i = 0; i < MAX_QPATH-1 && filter[i]; i++) {
		if ( filter[i] == '\\' || filter[i] == ':' ) {
			new_filter[i] = '/';
		}
		else {
			new_filter[i] = filter[i];
		}
	}
	new_filter[i] = '\0';
	for (i = 0; i < MAX_QPATH-1 && name[i]; i++) {
		if ( name[i] == '\\' || name[i] == ':' ) {
			new_name[i] = '/';
		}
		else {
			new_name[i] = name[i];
		}
	}
	new_name[i] = '\0';
	return Com_Filter( new_filter, new_name );
}


/*
================
Com_RealTime
================
*/
int Com_RealTime(qtime_t *qtime) {
	time_t t;
	struct tm *tms;

	t = time(NULL);
	if (!qtime)
		return t;
	tms = localtime(&t);
	if (tms) {
		qtime->tm_sec = tms->tm_sec;
		qtime->tm_min = tms->tm_min;
		qtime->tm_hour = tms->tm_hour;
		qtime->tm_mday = tms->tm_mday;
		qtime->tm_mon = tms->tm_mon;
		qtime->tm_year = tms->tm_year;
		qtime->tm_wday = tms->tm_wday;
		qtime->tm_yday = tms->tm_yday;
		qtime->tm_isdst = tms->tm_isdst;
	}
	return t;
}


/*
================
Sys_Microseconds
================
*/
int64_t Sys_Microseconds( void )
{
#ifdef _WIN32
	static qboolean inited = qfalse;
	static LARGE_INTEGER base;
	static LARGE_INTEGER freq;
	LARGE_INTEGER curr;

	if ( !inited )
	{
		QueryPerformanceFrequency( &freq );
		QueryPerformanceCounter( &base );
		if ( !freq.QuadPart )
		{
			return (int64_t)Sys_Milliseconds() * 1000LL; // fallback
		}
		inited = qtrue;
		return 0;
	}

	QueryPerformanceCounter( &curr );

	return ((curr.QuadPart - base.QuadPart) * 1000000LL) / freq.QuadPart;
#else
	struct timeval curr;
	gettimeofday( &curr, NULL );

	return (int64_t)curr.tv_sec * 1000000LL + (int64_t)curr.tv_usec;
#endif
}


/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

The zone calls are pretty much only used for small strings and structures,
all big things are allocated on the hunk.
==============================================================================
*/

#define	ZONEID	0x1d4a11
#define MINFRAGMENT	64

#ifdef USE_MULTI_SEGMENT
#if 1 // forward lookup, faster allocation
#define DIRECTION next
// we may have up to 4 lists to group free blocks by size
//#define TINY_SIZE	32
#define SMALL_SIZE	64
#define MEDIUM_SIZE	128
#else // backward lookup, better free space consolidation
#define DIRECTION prev
#define TINY_SIZE	64
#define SMALL_SIZE	128
#define MEDIUM_SIZE	256
#endif
#endif

#define USE_STATIC_TAGS
#define USE_TRASH_TEST

#ifdef ZONE_DEBUG
typedef struct zonedebug_s {
	const char *label;
	const char *file;
	int line;
	int allocSize;
} zonedebug_t;
#endif

typedef struct memblock_s {
	struct memblock_s	*next, *prev;
	int			size;	// including the header and possibly tiny fragments
	memtag_t	tag;	// a tag of 0 is a free block
	int			id;		// should be ZONEID
#ifdef ZONE_DEBUG
	zonedebug_t d;
#endif
} memblock_t;

typedef struct freeblock_s {
	struct freeblock_s *prev;
	struct freeblock_s *next;
} freeblock_t;

typedef struct memzone_s {
	int		size;			// total bytes malloced, including header
	int		used;			// total bytes used
	memblock_t	blocklist;	// start / end cap for linked list
#ifdef USE_MULTI_SEGMENT
	memblock_t	dummy0;		// just to allocate some space before freelist
	freeblock_t	freelist_tiny;
	memblock_t	dummy1;
	freeblock_t	freelist_small;
	memblock_t	dummy2;
	freeblock_t	freelist_medium;
	memblock_t	dummy3;
	freeblock_t	freelist;
#else
	memblock_t	*rover;
#endif
} memzone_t;

static int minfragment = MINFRAGMENT; // may be adjusted at runtime

// main zone for all "dynamic" memory allocation
static memzone_t *mainzone;

// we also have a small zone for small allocations that would only
// fragment the main zone (think of cvar and cmd strings)
static memzone_t *smallzone;


#ifdef USE_MULTI_SEGMENT

static void InitFree( freeblock_t *fb )
{
	memblock_t *block = (memblock_t*)( (byte*)fb - sizeof( memblock_t ) );
	Com_Memset( block, 0, sizeof( *block ) );
}


static void RemoveFree( memblock_t *block )
{
	freeblock_t *fb = (freeblock_t*)( block + 1 );
	freeblock_t *prev;
	freeblock_t *next;

#ifdef ZONE_DEBUG
	if ( fb->next == NULL || fb->prev == NULL || fb->next == fb || fb->prev == fb ) {
		Com_Error( ERR_FATAL, "RemoveFree: bad pointers fb->next: %p, fb->prev: %p\n", fb->next, fb->prev );
	}
#endif

	prev = fb->prev;
	next = fb->next;

	prev->next = next;
	next->prev = prev;
}


static void InsertFree( memzone_t *zone, memblock_t *block )
{
	freeblock_t *fb = (freeblock_t*)( block + 1 );
	freeblock_t *prev, *next;
#ifdef TINY_SIZE
	if ( block->size <= TINY_SIZE )
		prev = &zone->freelist_tiny;
	else
#endif
#ifdef SMALL_SIZE
	if ( block->size <= SMALL_SIZE )
		prev = &zone->freelist_small;
	else
#endif
#ifdef MEDIUM_SIZE
	if ( block->size <= MEDIUM_SIZE )
		prev = &zone->freelist_medium;
	else
#endif
		prev = &zone->freelist;

	next = prev->next;

#ifdef ZONE_DEBUG
	if ( block->size < sizeof( *fb ) + sizeof( *block ) ) {
		Com_Error( ERR_FATAL, "InsertFree: bad block size: %i\n", block->size );
	}
#endif

	prev->next = fb;
	next->prev = fb;

	fb->prev = prev;
	fb->next = next;
}


/*
================
NewBlock

Allocates new free block within specified memory zone

Separator is needed to avoid additional runtime checks in Z_Free()
to prevent merging it with previous free block
================
*/
static freeblock_t *NewBlock( memzone_t *zone, int size )
{
	memblock_t *prev, *next;
	memblock_t *block, *sep;
	int alloc_size;

	// zone->prev is pointing on last block in the list
	prev = zone->blocklist.prev;
	next = prev->next;

	size = PAD( size, 1<<21 ); // round up to 2M blocks
	// allocate separator block before new free block
	alloc_size = size + sizeof( *sep );

	sep = (memblock_t *) calloc( alloc_size, 1 );
	if ( sep == NULL ) {
		Com_Error( ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes from the %s zone",
			size, zone == smallzone ? "small" : "main" );
		return nullptr;
	}
	block = sep+1;

	// link separator with prev
	prev->next = sep;
	sep->prev = prev;

	// link separator with block
	sep->next = block;
	block->prev = sep;

	// link block with next
	block->next = next;
	next->prev = block;

	sep->tag = TAG_GENERAL; // in-use block
	sep->id = -ZONEID;
	sep->size = 0;

	block->tag = TAG_FREE;
	block->id = ZONEID;
	block->size = size;

	// update zone statistics
	zone->size += alloc_size;
	zone->used += sizeof( *sep );

	InsertFree( zone, block );

	return (freeblock_t*)( block + 1 );
}


static memblock_t *SearchFree( memzone_t *zone, int size )
{
	const freeblock_t *fb;
	memblock_t *base;

#ifdef TINY_SIZE
	if ( size <= TINY_SIZE )
		fb = zone->freelist_tiny.DIRECTION;
	else
#endif
#ifdef SMALL_SIZE
	if ( size <= SMALL_SIZE )
		fb = zone->freelist_small.DIRECTION;
	else
#endif
#ifdef MEDIUM_SIZE
	if ( size <= MEDIUM_SIZE )
		fb = zone->freelist_medium.DIRECTION;
	else
#endif
		fb = zone->freelist.DIRECTION;

	for ( ;; ) {
		// not found, allocate new segment?
		if ( fb == &zone->freelist ) {
			fb = NewBlock( zone, size );
		} else {
#ifdef TINY_SIZE
			if ( fb == &zone->freelist_tiny ) {
				fb = zone->freelist_small.DIRECTION;
				continue;
			}
#endif
#ifdef SMALL_SIZE
			if ( fb == &zone->freelist_small ) {
				fb = zone->freelist_medium.DIRECTION;
				continue;
			}
#endif
#ifdef MEDIUM_SIZE
			if ( fb == &zone->freelist_medium ) {
				fb = zone->freelist.DIRECTION;
				continue;
			}
#endif
		}
		base = (memblock_t*)( (byte*) fb - sizeof( *base ) );
		fb = fb->DIRECTION;
		if ( base->size >= size ) {
			return base;
		}
	}
	return nullptr;
}
#endif // USE_MULTI_SEGMENT


/*
========================
Z_ClearZone
========================
*/
static void Z_ClearZone( memzone_t *zone, memzone_t *head, int size, int segnum ) {
	(void)head;    // Suppress unused parameter warning
	(void)segnum;  // Suppress unused parameter warning
	memblock_t	*block;
	int min_fragment;

#ifdef USE_MULTI_SEGMENT
	min_fragment = sizeof( memblock_t ) + sizeof( freeblock_t );
#else
	min_fragment = sizeof( memblock_t );
#endif

	if ( minfragment < min_fragment ) {
		// in debug mode size of memblock_t may exceed MINFRAGMENT
		minfragment = PAD( min_fragment, sizeof( intptr_t ) );
		Com_DPrintf( "zone.minfragment adjusted to %i bytes\n", minfragment );
	}

	// set the entire zone to one free block
	zone->blocklist.next = zone->blocklist.prev = block = (memblock_t *)( zone + 1 );
	zone->blocklist.tag = TAG_GENERAL; // in use block
	zone->blocklist.id = -ZONEID;
	zone->blocklist.size = 0;
#ifndef USE_MULTI_SEGMENT
	zone->rover = block;
#endif
	zone->size = size;
	zone->used = 0;

	block->prev = block->next = &zone->blocklist;
	block->tag = TAG_FREE;	// free block
	block->id = ZONEID;

	block->size = size - sizeof(memzone_t);

#ifdef USE_MULTI_SEGMENT
	InitFree( &zone->freelist );
	zone->freelist.next = zone->freelist.prev = &zone->freelist;

	InitFree( &zone->freelist_medium );
	zone->freelist_medium.next = zone->freelist_medium.prev = &zone->freelist_medium;

	InitFree( &zone->freelist_small );
	zone->freelist_small.next = zone->freelist_small.prev = &zone->freelist_small;

	InitFree( &zone->freelist_tiny );
	zone->freelist_tiny.next = zone->freelist_tiny.prev = &zone->freelist_tiny;

	InsertFree( zone, block );
#endif
}


/*
========================
Z_AvailableZoneMemory
========================
*/
static int Z_AvailableZoneMemory( const memzone_t *zone ) {
	(void)zone;  // Suppress unused parameter warning
#ifdef USE_MULTI_SEGMENT
	return (1024*1024*1024); // unlimited
#else
	return zone->size - zone->used;
#endif
}


/*
========================
Z_AvailableMemory
========================
*/
int Z_AvailableMemory( void ) {
	return Z_AvailableZoneMemory( mainzone );
}


static void MergeBlock( memblock_t *curr_free, const memblock_t *next )
{
	curr_free->size += next->size;
	curr_free->next = next->next;
	curr_free->next->prev = curr_free;
}


/*
========================
Z_Free
========================
*/
void Z_Free( void *ptr ) {
	memblock_t	*block, *other;
	memzone_t *zone;

	if (!ptr) {
		Com_Error( ERR_FATAL, "Z_Free: NULL pointer" );
	}

	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID) {
		Com_Error( ERR_FATAL, "Z_Free: freed a pointer without ZONEID" );
	}

	if (block->tag == TAG_FREE) {
		return;
	}

	// if static memory
#ifdef USE_STATIC_TAGS
	if (block->tag == TAG_STATIC) {
		return;
	}
#endif

	// check the memory trash tester
#ifdef USE_TRASH_TEST
	if ( *(int *)((byte *)block + block->size - 4 ) != ZONEID ) {
		Com_Error( ERR_FATAL, "Z_Free: memory block wrote past end" );
	}
#endif

	if ( block->tag == TAG_SMALL ) {
		zone = smallzone;
	} else {
		zone = mainzone;
	}

	zone->used -= block->size;

	// Track memory statistics (actual allocated size excluding header)
	memtag_t freed_tag = block->tag;
	MemStats_Free(freed_tag, block->size - sizeof(*block));

	// set the block to something that should cause problems
	// if it is referenced...
	Com_Memset( ptr, 0xaa, block->size - sizeof( *block ) );

	block->tag = TAG_FREE; // mark as free
	block->id = ZONEID;

	other = block->prev;
	if ( other->tag == TAG_FREE ) {
#ifdef USE_MULTI_SEGMENT
		RemoveFree( other );
#endif
		// merge with previous free block
		MergeBlock( other, block );
#ifndef USE_MULTI_SEGMENT
		if ( block == zone->rover ) {
			zone->rover = other;
		}
#endif
		block = other;
	}

#ifndef USE_MULTI_SEGMENT
	zone->rover = block;
#endif

	other = block->next;
	if ( other->tag == TAG_FREE ) {
#ifdef USE_MULTI_SEGMENT
		RemoveFree( other );
#endif
		// merge the next free block onto the end
		MergeBlock( block, other );
	}

#ifdef USE_MULTI_SEGMENT
	InsertFree( zone, block );
#endif
}


/*
================
Z_FreeTags
================
*/
int Z_FreeTags( memtag_t tag ) {
	int			count;
	memzone_t	*zone;
	memblock_t	*block, *freed;

	if ( tag == TAG_STATIC ) {
		Com_Error( ERR_FATAL, "Z_FreeTags( TAG_STATIC )" );
		return 0;
	} else if ( tag == TAG_SMALL ) {
		zone = smallzone;
	} else {
		zone = mainzone;
	}

	count = 0;
	for ( block = zone->blocklist.next ; ; ) {
		if ( block->tag == tag && block->id == ZONEID ) {
			if ( block->prev->tag == TAG_FREE )
				freed = block->prev;  // current block will be merged with previous
			else
				freed = block; // will leave in place
			Z_Free( (void*)( block + 1 ) );
			block = freed;
			count++;
		}
		if ( block->next == &zone->blocklist ) {
			break;	// all blocks have been hit
		}
		block = block->next;
	}

	return count;
}


/*
================
Z_TagMalloc
================
*/
#ifdef ZONE_DEBUG
void *Z_TagMallocDebug( int size, memtag_t tag, char *label, char *file, int line ) {
	TracyCZoneCtx prof_zmalloc;
	PROF_ZONE_BEGIN(prof_zmalloc, "Z_TagMallocDebug");
	int		allocSize;
#else
void *Z_TagMalloc( int size, memtag_t tag ) {
	TracyCZoneCtx prof_zmalloc;
	PROF_ZONE_BEGIN(prof_zmalloc, "Z_TagMalloc");
#endif
	int		extra;
#ifndef USE_MULTI_SEGMENT
	memblock_t	*start, *rover;
#endif
	memblock_t *base;
	memzone_t *zone;

	if ( tag == TAG_FREE ) {
		Com_Error( ERR_FATAL, "Z_TagMalloc: tried to use with TAG_FREE" );
	}

	if ( tag == TAG_SMALL ) {
		zone = smallzone;
	} else {
		zone = mainzone;
	}

#ifdef ZONE_DEBUG
	allocSize = size;
#endif

#ifdef USE_MULTI_SEGMENT
	if ( size < (int)sizeof( freeblock_t ) ) {
		size = (int)sizeof( freeblock_t );
	}
#endif

	//
	// scan through the block list looking for the first free block
	// of sufficient size
	//
	size += sizeof( *base );	// account for size of block header
#ifdef USE_TRASH_TEST
	size += 4;					// space for memory trash tester
#endif

	size = PAD(size, sizeof(intptr_t));		// align to 32/64 bit boundary

#ifdef USE_MULTI_SEGMENT
	base = SearchFree( zone, size );

	RemoveFree( base );
#else

	base = rover = zone->rover;
	start = base->prev;

	do {
		if ( rover == start ) {
			// scanned all the way around the list
#ifdef ZONE_DEBUG
			Z_LogHeap();
			Com_Error( ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes from the %s zone: %s, line: %d (%s)",
								size, zone == smallzone ? "small" : "main", file, line, label );
#else
			Com_Error( ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes from the %s zone",
								size, zone == smallzone ? "small" : "main" );
#endif
			return nullptr;
		}
		if ( rover->tag != TAG_FREE ) {
			base = rover = rover->next;
		} else {
			rover = rover->next;
		}
	} while (base->tag != TAG_FREE || base->size < size);
#endif

	//
	// found a block big enough
	//
	extra = base->size - size;
	if ( extra >= minfragment ) {
		memblock_t *fragment;
		// there will be a free fragment after the allocated block
		fragment = (memblock_t *)( (byte *)base + size );
		fragment->size = extra;
		fragment->tag = TAG_FREE; // free block
		fragment->id = ZONEID;
		fragment->prev = base;
		fragment->next = base->next;
		fragment->next->prev = fragment;
		base->next = fragment;
		base->size = size;
#ifdef USE_MULTI_SEGMENT
		InsertFree( zone, fragment );
#endif
	}

#ifndef USE_MULTI_SEGMENT
	zone->rover = base->next;	// next allocation will start looking here
#endif
	zone->used += base->size;

	base->tag = tag;			// no longer a free block
	base->id = ZONEID;

#ifdef ZONE_DEBUG
	base->d.label = label;
	base->d.file = file;
	base->d.line = line;
	base->d.allocSize = allocSize;
#endif

#ifdef USE_TRASH_TEST
	// marker for memory trash testing
	*(int *)((byte *)base + base->size - 4) = ZONEID;
#endif

	// Track memory statistics (actual allocated size excluding header)
	MemStats_Alloc(tag, base->size - sizeof(*base));

	PROF_ZONE_END(prof_zmalloc);
	return (void *) ( base + 1 );
}


/*
========================
Z_Malloc
========================
*/
#ifdef ZONE_DEBUG
void *Z_MallocDebug( int size, char *label, char *file, int line ) {
#else
void *Z_Malloc( int size ) {
#endif
	void	*buf;

  //Z_CheckHeap ();	// DEBUG

#ifdef ZONE_DEBUG
	buf = Z_TagMallocDebug( size, TAG_GENERAL, label, file, line );
#else
	buf = Z_TagMalloc( size, TAG_GENERAL );
#endif
	Com_Memset( buf, 0, size );

	return buf;
}


/*
========================
S_Malloc
========================
*/
#ifdef ZONE_DEBUG
void *S_MallocDebug( int size, char *label, char *file, int line ) {
	return Z_TagMallocDebug( size, TAG_SMALL, label, file, line );
}
#else
void *S_Malloc( int size ) {
	return Z_TagMalloc( size, TAG_SMALL );
}
#endif


/*
========================
Z_CheckHeap
========================
*/
static void Z_CheckHeap( void ) {
	const memblock_t *block;
	const memzone_t *zone;

	zone =  mainzone;
	for ( block = zone->blocklist.next ; ; ) {
		if ( block->next == &zone->blocklist ) {
			break;	// all blocks have been hit
		}
		if ( (byte *)block + block->size != (byte *)block->next) {
#ifdef USE_MULTI_SEGMENT
			const memblock_t *next = block->next;
			if ( next->size == 0 && next->id == -ZONEID && next->tag == TAG_GENERAL ) {
				block = next; // new zone segment
			} else
#endif
			Com_Error( ERR_FATAL, "Z_CheckHeap: block size does not touch the next block" );
		}
		if ( block->next->prev != block) {
			Com_Error( ERR_FATAL, "Z_CheckHeap: next block doesn't have proper back link" );
		}
		if ( block->tag == TAG_FREE && block->next->tag == TAG_FREE ) {
			Com_Error( ERR_FATAL, "Z_CheckHeap: two consecutive free blocks" );
		}
		block = block->next;
	}
}


/*
========================
Z_LogZoneHeap
========================
*/
static void Z_LogZoneHeap( memzone_t *zone, const char *name ) {
#ifdef ZONE_DEBUG
	char dump[32], *ptr;
	int  i, j;
#endif
	memblock_t	*block;
	char		buf[4096];
	int size, allocSize, numBlocks;
	int len;

	if ( logfile == FS_INVALID_HANDLE || !FS_Initialized() )
		return;

	size = numBlocks = 0;
#ifdef ZONE_DEBUG
	allocSize = 0;
#endif
	len = Com_sprintf( buf, sizeof(buf), "\r\n================\r\n%s log\r\n================\r\n", name );
	FS_Write( buf, len, logfile );
	for ( block = zone->blocklist.next ; ; ) {
		if ( block->tag != TAG_FREE ) {
#ifdef ZONE_DEBUG
			ptr = ((char *) block) + sizeof(memblock_t);
			j = 0;
			for (i = 0; i < 20 && i < block->d.allocSize; i++) {
				if (ptr[i] >= 32 && ptr[i] < 127) {
					dump[j++] = ptr[i];
				}
				else {
					dump[j++] = '_';
				}
			}
			dump[j] = '\0';
			len = Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s) [%s]\r\n", block->d.allocSize, block->d.file, block->d.line, block->d.label, dump);
			FS_Write( buf, len, logfile );
			allocSize += block->d.allocSize;
#endif
			size += block->size;
			numBlocks++;
		}
		if ( block->next == &zone->blocklist ) {
			break; // all blocks have been hit
		}
		block = block->next;
	}
#ifdef ZONE_DEBUG
	// subtract debug memory
	size -= numBlocks * sizeof(zonedebug_t);
#else
	allocSize = numBlocks * sizeof(memblock_t); // + 32 bit alignment
#endif
	len = Com_sprintf( buf, sizeof( buf ), "%d %s memory in %d blocks\r\n", size, name, numBlocks );
	FS_Write( buf, len, logfile );
	len = Com_sprintf( buf, sizeof( buf ), "%d %s memory overhead\r\n", size - allocSize, name );
	FS_Write( buf, len, logfile );
	FS_Flush( logfile );
}


/*
========================
Z_LogHeap
========================
*/
void Z_LogHeap( void ) {
	Z_LogZoneHeap( mainzone, "MAIN" );
	Z_LogZoneHeap( smallzone, "SMALL" );
}

#ifdef USE_STATIC_TAGS

// static mem blocks to reduce a lot of small zone overhead
typedef struct memstatic_s {
	memblock_t b;
	byte mem[2];
} memstatic_t;

#define MEM_STATIC(chr) { { NULL, NULL, PAD(sizeof(memstatic_t),4), TAG_STATIC, ZONEID }, {chr,'\0'} }

static const memstatic_t emptystring =
	MEM_STATIC( '\0' );

static const memstatic_t numberstring[] = {
	MEM_STATIC( '0' ),
	MEM_STATIC( '1' ),
	MEM_STATIC( '2' ),
	MEM_STATIC( '3' ),
	MEM_STATIC( '4' ),
	MEM_STATIC( '5' ),
	MEM_STATIC( '6' ),
	MEM_STATIC( '7' ),
	MEM_STATIC( '8' ),
	MEM_STATIC( '9' )
};
#endif // USE_STATIC_TAGS

/*
========================
CopyString

Allocates and copies a string. Returns a pointer to the copied string.

NOTE:	never write over the memory CopyString returns because
		memory from a memstatic_t might be returned

@param in Source string to copy
@return Pointer to newly allocated string copy
@note Uses S_Malloc for allocation
@note Returns static memory for empty strings and single-digit numbers (optimization)
========================
*/
char *CopyString( const char *in ) {
	char *out;
	if ( !in ) {
		Com_Printf( "CopyString: NULL input parameter, returning empty string\n" );
		in = ""; // Use empty string as fallback
	}
#ifdef USE_STATIC_TAGS
	if ( in[0] == '\0' ) {
		return ((char *)&emptystring) + sizeof(memblock_t);
	}
	else if ( in[0] >= '0' && in[0] <= '9' && in[1] == '\0' ) {
		return ((char *)&numberstring[in[0]-'0']) + sizeof(memblock_t);
	}
#endif
	out = S_Malloc( strlen( in ) + 1 );
	Q_strncpyz( out, in, strlen( in ) + 1 );
	return out;
}


/*
==============================================================================

Goals:
	reproducible without history effects -- no out of memory errors on weird map to map changes
	allow restarting of the client without fragmentation
	minimize total pages in use at run time
	minimize total pages needed during load time

  Single block of memory with stack allocators coming from both ends towards the middle.

  One side is designated the temporary memory allocator.

  Temporary memory can be allocated and freed in any order.

  A highwater mark is kept of the most in use at any time.

  When there is no temporary memory allocated, the permanent and temp sides
  can be switched, allowing the already touched temp memory to be used for
  permanent storage.

  Temp memory must never be allocated on two ends at once, or fragmentation
  could occur.

  If we have any in-use temp memory, additional temp allocations must come from
  that side.

  If not, we can choose to make either side the new temp side and push future
  permanent allocations to the other side.  Permanent allocations should be
  kept on the side that has the current greatest wasted highwater mark.

==============================================================================
*/


#define	HUNK_MAGIC	0x89537892
#define	HUNK_FREE_MAGIC	0x89537893

typedef struct {
	unsigned int magic;
	unsigned int size;
} hunkHeader_t;

typedef struct {
	int		mark;
	int		permanent;
	int		temp;
	int		tempHighwater;
} hunkUsed_t;

typedef struct hunkblock_s {
	int size;
	byte printed;
	struct hunkblock_s *next;
	const char *label;
	const char *file;
	int line;
} hunkblock_t;

static	hunkblock_t *hunkblocks;

static	hunkUsed_t	hunk_low, hunk_high;
static	hunkUsed_t	*hunk_permanent, *hunk_temp;

static	byte	*s_hunkData = NULL;
static	int		s_hunkTotal;

static const char *tagName[ TAG_COUNT ] = {
	"FREE",
	"GENERAL",
	"PACK",
	"SEARCH-PATH",
	"SEARCH-PACK",
	"SEARCH-DIR",
	"BOTLIB",
	"RENDERER",
	"CLIENTS",
	"SMALL",
	"STATIC"
};

typedef struct zone_stats_s {
	int	zoneSegments;
	int zoneBlocks;
	int	zoneBytes;
	int	botlibBytes;
	int	rendererBytes;
	int freeBytes;
	int freeBlocks;
	int freeSmallest;
	int freeLargest;
} zone_stats_t;


static void Zone_Stats( const char *name, const memzone_t *z, qboolean printDetails, zone_stats_t *stats )
{
	const memblock_t *block;
	const memzone_t *zone;
	zone_stats_t st;

	memset( &st, 0, sizeof( st ) );
	zone = z;
	st.zoneSegments = 1;
	st.freeSmallest = 0x7FFFFFFF;

	//if ( printDetails ) {
	//	Com_Printf( "---------- %s zone segment #%i ----------\n", name, zone->segnum );
	//}

	for ( block = zone->blocklist.next ; ; ) {
		if ( printDetails ) {
			int tag = block->tag;
			Com_Printf( "block:%p  size:%8i  tag: %s\n", (void *)block, block->size,
				(unsigned)tag < TAG_COUNT ? tagName[ tag ] : va( "%i", tag ) );
		}
		if ( block->tag != TAG_FREE ) {
			st.zoneBytes += block->size;
			st.zoneBlocks++;
			if ( block->tag == TAG_BOTLIB ) {
				st.botlibBytes += block->size;
			} else if ( block->tag == TAG_RENDERER ) {
				st.rendererBytes += block->size;
			}
		} else {
			st.freeBytes += block->size;
			st.freeBlocks++;
			if ( block->size > st.freeLargest )
				st.freeLargest = block->size;
			if ( block->size < st.freeSmallest )
				st.freeSmallest = block->size;
		}
		if ( block->next == &zone->blocklist ) {
			break; // all blocks have been hit
		}
		if ( (byte *)block + block->size != (byte *)block->next) {
#ifdef USE_MULTI_SEGMENT
			const memblock_t *next = block->next;
			if ( next->size == 0 && next->id == -ZONEID && next->tag == TAG_GENERAL ) {
				st.zoneSegments++;
				if ( printDetails ) {
					Com_Printf( "---------- %s zone segment #%i ----------\n", name, st.zoneSegments );
				}
				block = next->next;
				continue;
			} else
#endif
				Com_Printf( "ERROR: block size does not touch the next block\n" );
		}
		if ( block->next->prev != block) {
			Com_Printf( "ERROR: next block doesn't have proper back link\n" );
		}
		if ( block->tag == TAG_FREE && block->next->tag == TAG_FREE ) {
			Com_Printf( "ERROR: two consecutive free blocks\n" );
		}
		block = block->next;
	}

	// export stats
	if ( stats ) {
		memcpy( stats, &st, sizeof( *stats ) );
	}
}


/*
=================
Com_Meminfo_f
=================
*/
static void Com_Meminfo_f( void ) {
	zone_stats_t st;
	int		unused;

	Com_Printf( "%8i bytes total hunk\n", s_hunkTotal );
	Com_Printf( "\n" );
	Com_Printf( "%8i low mark\n", hunk_low.mark );
	Com_Printf( "%8i low permanent\n", hunk_low.permanent );
	if ( hunk_low.temp != hunk_low.permanent ) {
		Com_Printf( "%8i low temp\n", hunk_low.temp );
	}
	Com_Printf( "%8i low tempHighwater\n", hunk_low.tempHighwater );
	Com_Printf( "\n" );
	Com_Printf( "%8i high mark\n", hunk_high.mark );
	Com_Printf( "%8i high permanent\n", hunk_high.permanent );
	if ( hunk_high.temp != hunk_high.permanent ) {
		Com_Printf( "%8i high temp\n", hunk_high.temp );
	}
	Com_Printf( "%8i high tempHighwater\n", hunk_high.tempHighwater );
	Com_Printf( "\n" );
	Com_Printf( "%8i total hunk in use\n", hunk_low.permanent + hunk_high.permanent );
	unused = 0;
	if ( hunk_low.tempHighwater > hunk_low.permanent ) {
		unused += hunk_low.tempHighwater - hunk_low.permanent;
	}
	if ( hunk_high.tempHighwater > hunk_high.permanent ) {
		unused += hunk_high.tempHighwater - hunk_high.permanent;
	}
	Com_Printf( "%8i unused highwater\n", unused );
	Com_Printf( "\n" );

	Zone_Stats( "main", mainzone, !Q_stricmp( Cmd_Argv(1), "main" ) || !Q_stricmp( Cmd_Argv(1), "all" ), &st );
	Com_Printf( "%8i bytes total main zone\n\n", mainzone->size );
	Com_Printf( "%8i bytes in %i main zone blocks%s\n", st.zoneBytes, st.zoneBlocks,
		st.zoneSegments > 1 ? va( " and %i segments", st.zoneSegments ) : "" );
	Com_Printf( "        %8i bytes in botlib\n", st.botlibBytes );
	Com_Printf( "        %8i bytes in renderer\n", st.rendererBytes );
	Com_Printf( "        %8i bytes in other\n", st.zoneBytes - ( st.botlibBytes + st.rendererBytes ) );
	Com_Printf( "        %8i bytes in %i free blocks\n", st.freeBytes, st.freeBlocks );
	if ( st.freeBlocks > 1 ) {
		Com_Printf( "        (largest: %i bytes, smallest: %i bytes)\n\n", st.freeLargest, st.freeSmallest );
	}

	Zone_Stats( "small", smallzone, !Q_stricmp( Cmd_Argv(1), "small" ) || !Q_stricmp( Cmd_Argv(1), "all" ), &st );
	Com_Printf( "%8i bytes total small zone\n\n", smallzone->size );
	Com_Printf( "%8i bytes in %i small zone blocks%s\n", st.zoneBytes, st.zoneBlocks,
		st.zoneSegments > 1 ? va( " and %i segments", st.zoneSegments ) : "" );
	Com_Printf( "        %8i bytes in %i free blocks\n", st.freeBytes, st.freeBlocks );
	if ( st.freeBlocks > 1 ) {
		Com_Printf( "        (largest: %i bytes, smallest: %i bytes)\n\n", st.freeLargest, st.freeSmallest );
	}
}


/*
===============
Com_TouchMemory

Touch all known used data to make sure it is paged in
===============
*/
unsigned int Com_TouchMemory( void ) {
	const memblock_t *block;
	const memzone_t *zone;
	int		start, end;
	int		i, j;
	unsigned int sum;

	Z_CheckHeap();

	start = Sys_Milliseconds();

	sum = 0;

	j = hunk_low.permanent >> 2;
	for ( i = 0 ; i < j ; i+=64 ) {			// only need to touch each page
		sum += ((unsigned int *)s_hunkData)[i];
	}

	i = ( s_hunkTotal - hunk_high.permanent ) >> 2;
	j = hunk_high.permanent >> 2;
	for (  ; i < j ; i+=64 ) {			// only need to touch each page
		sum += ((unsigned int *)s_hunkData)[i];
	}

	zone = mainzone;
	for (block = zone->blocklist.next ; ; block = block->next) {
		if ( block->tag != TAG_FREE ) {
			j = block->size >> 2;
			for ( i = 0 ; i < j ; i+=64 ) {				// only need to touch each page
				sum += ((unsigned int *)block)[i];
			}
		}
		if ( block->next == &zone->blocklist ) {
			break; // all blocks have been hit
		}
	}

	end = Sys_Milliseconds();

	Com_Printf( "Com_TouchMemory: %i msec\n", end - start );

	return sum; // just to silent compiler warning
}


/*
=================
Com_InitSmallZoneMemory
=================
*/
static void Com_InitSmallZoneMemory( void ) {
	static byte s_buf[ 512 * 1024 ];
	int smallZoneSize;

	smallZoneSize = sizeof( s_buf );
	Com_Memset( s_buf, 0, smallZoneSize );
	smallzone = (memzone_t *)s_buf;
	Z_ClearZone( smallzone, smallzone, smallZoneSize, 1 );
}


/*
=================
Com_InitZoneMemory
=================
*/
static void Com_InitZoneMemory( void ) {
	int		mainZoneSize;
	cvar_t	*cv;

	// Please note: com_zoneMegs can only be set on the command line, and
	// not in config.cfg or Com_StartupVariable, as they haven't been
	// executed by this point. It's a chicken and egg problem. We need the
	// memory manager configured to handle those places where you would
	// configure the memory manager.

	// allocate the random block zone
	cv = Cvar_Get( "com_zoneMegs", XSTRING( DEF_COMZONEMEGS ), CVAR_LATCH | CVAR_ARCHIVE );
	Cvar_CheckRange( cv, "1", NULL, CV_INTEGER );
	Cvar_SetDescription( cv, "Initial amount of memory (RAM) allocated for the main block zone (in MB)." );

#ifndef USE_MULTI_SEGMENT
	if ( cv->integer < DEF_COMZONEMEGS )
		mainZoneSize = 1024 * 1024 * DEF_COMZONEMEGS;
	else
#endif
		mainZoneSize = cv->integer * 1024 * 1024;

	mainzone = calloc( mainZoneSize, 1 );
	if ( !mainzone ) {
		Com_Error( ERR_FATAL, "Zone data failed to allocate %i megs", mainZoneSize / (1024*1024) );
	}
	Z_ClearZone( mainzone, mainzone, mainZoneSize, 1 );
}


/*
=================
Hunk_Log
=================
*/
void Hunk_Log( void ) {
	hunkblock_t	*block;
	char		buf[4096];
	int size, numBlocks;

	if ( logfile == FS_INVALID_HANDLE || !FS_Initialized() )
		return;

	size = 0;
	numBlocks = 0;
	Com_sprintf(buf, sizeof(buf), "\r\n================\r\nHunk log\r\n================\r\n");
	FS_Write(buf, strlen(buf), logfile);
	for (block = hunkblocks ; block; block = block->next) {
#ifdef HUNK_DEBUG
		Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", block->size, block->file, block->line, block->label);
		FS_Write(buf, strlen(buf), logfile);
#endif
		size += block->size;
		numBlocks++;
	}
	Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
	FS_Write(buf, strlen(buf), logfile);
	Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
	FS_Write(buf, strlen(buf), logfile);
}


/*
=================
Hunk_SmallLog
=================
*/
#ifdef HUNK_DEBUG
void Hunk_SmallLog( void ) {
	hunkblock_t	*block, *block2;
	char		buf[4096];
	int size, locsize, numBlocks;

	if ( logfile == FS_INVALID_HANDLE || !FS_Initialized() )
		return;

	for (block = hunkblocks ; block; block = block->next) {
		block->printed = qfalse;
	}
	size = 0;
	numBlocks = 0;
	Com_sprintf(buf, sizeof(buf), "\r\n================\r\nHunk Small log\r\n================\r\n");
	FS_Write(buf, strlen(buf), logfile);
	for (block = hunkblocks; block; block = block->next) {
		if (block->printed) {
			continue;
		}
		locsize = block->size;
		for (block2 = block->next; block2; block2 = block2->next) {
			if (block->line != block2->line) {
				continue;
			}
			if (Q_stricmp(block->file, block2->file)) {
				continue;
			}
			size += block2->size;
			locsize += block2->size;
			block2->printed = qtrue;
		}
		Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", locsize, block->file, block->line, block->label);
		FS_Write(buf, strlen(buf), logfile);
		size += block->size;
		numBlocks++;
	}
	Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
	FS_Write(buf, strlen(buf), logfile);
	Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
	FS_Write(buf, strlen(buf), logfile);
}
#endif


/*
=================
Com_InitHunkMemory
=================
*/
static void Com_InitHunkMemory( void ) {
	cvar_t	*cv;

	// make sure the file system has allocated and "not" freed any temp blocks
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redundant routines in the file system utilizing different
	// memory systems
	int loadStack = FS_LoadStack();
	if ( loadStack < 0 ) {
		Com_Printf( "DEBUG: File system load stack is %d, should be >= 0\n", loadStack );
		Com_Error( ERR_FATAL, "Hunk initialization failed. File system load stack negative (%d)", loadStack );
	}
	// Note: Config files loaded during initialization remain loaded (loadStack may be > 0)
	// This is expected behavior as noted in the comment above

	// allocate the stack based hunk allocator
	cv = Cvar_Get( "com_hunkMegs", XSTRING( DEF_COMHUNKMEGS ), CVAR_LATCH | CVAR_ARCHIVE );
	Cvar_CheckRange( cv, XSTRING( MIN_COMHUNKMEGS ), NULL, CV_INTEGER );
	Cvar_SetDescription( cv, "The size of the hunk memory segment." );

	s_hunkTotal = cv->integer * 1024 * 1024;

	s_hunkData = calloc( s_hunkTotal + 63, 1 );
	if ( !s_hunkData ) {
		Com_Error( ERR_FATAL, "Hunk data failed to allocate %i megs", s_hunkTotal / (1024*1024) );
	}

	// cacheline align
	s_hunkData = PADP( s_hunkData, 64 );
	Hunk_Clear();

	Cmd_AddCommand( "meminfo", Com_Meminfo_f );
	Cmd_AddCommand( "netthreads", Com_NetThreads_f );
	Cmd_AddCommand( "streamthreads", Com_StreamThreads_f );
	Cmd_AddCommand( "codereview", Com_CodeReview_f );
	Cmd_AddCommand( "livecode", Com_LiveCode_f );
	Cmd_AddCommand( "perftest", Com_PerfTest_f );
	Cmd_AddCommand( "crosstest", Com_CrossPlatformTest_f );
	Cmd_AddCommand( "memtest", Com_MemorySafetyTest_f );
	Cmd_AddCommand( "threadtest", Com_ThreadSafetyTest_f );
	Cmd_AddCommand( "quality", Com_CodeQuality_f );
	Cmd_AddCommand( "debt", Com_TechnicalDebt_f );
	Cmd_AddCommand( "benchmark", Com_PerformanceBenchmark_f );

	// Initialize memory statistics tracking
	MemStats_Init();
	
#ifdef ZONE_DEBUG
	Cmd_AddCommand( "zonelog", Z_LogHeap );
#endif
#ifdef HUNK_DEBUG
	Cmd_AddCommand( "hunklog", Hunk_Log );
	Cmd_AddCommand( "hunksmalllog", Hunk_SmallLog );
#endif
}


/*
====================
Hunk_MemoryRemaining
====================
*/
int	Hunk_MemoryRemaining( void ) {
	int		low, high;

	low = hunk_low.permanent > hunk_low.temp ? hunk_low.permanent : hunk_low.temp;
	high = hunk_high.permanent > hunk_high.temp ? hunk_high.permanent : hunk_high.temp;

	return s_hunkTotal - ( low + high );
}


/*
===================
Hunk_SetMark

The server calls this after the level and game VM have been loaded
===================
*/
void Hunk_SetMark( void ) {
	hunk_low.mark = hunk_low.permanent;
	hunk_high.mark = hunk_high.permanent;
}


/*
=================
Hunk_ClearToMark

The client calls this before starting a vid_restart or snd_restart
=================
*/
void Hunk_ClearToMark( void ) {
	hunk_low.permanent = hunk_low.temp = hunk_low.mark;
	hunk_high.permanent = hunk_high.temp = hunk_high.mark;
}


/*
=================
Hunk_CheckMark
=================
*/
qboolean Hunk_CheckMark( void ) {
	if( hunk_low.mark || hunk_high.mark ) {
		return qtrue;
	}
	return qfalse;
}

void CL_ShutdownCGame( void );
void CL_ShutdownUI( void );
void SV_ShutdownGameProgs( void );

/*
=================
Hunk_Clear

The server calls this before shutting down or loading a new map
=================
*/
void Hunk_Clear( void ) {

#ifndef DEDICATED
	CL_ShutdownCGame();
	CL_ShutdownUI();
#endif
	SV_ShutdownGameProgs();
#ifndef DEDICATED
	CIN_CloseAllVideos();
#endif
	hunk_low.mark = 0;
	hunk_low.permanent = 0;
	hunk_low.temp = 0;
	hunk_low.tempHighwater = 0;

	hunk_high.mark = 0;
	hunk_high.permanent = 0;
	hunk_high.temp = 0;
	hunk_high.tempHighwater = 0;

	hunk_permanent = &hunk_low;
	hunk_temp = &hunk_high;

	Com_Printf( "Hunk_Clear: reset the hunk ok\n" );
	VM_Clear();
#ifdef HUNK_DEBUG
	hunkblocks = NULL;
#endif
}


static void Hunk_SwapBanks( void ) {
	hunkUsed_t	*swap;

	// can't swap banks if there is any temp already allocated
	if ( hunk_temp->temp != hunk_temp->permanent ) {
		return;
	}

	// if we have a larger highwater mark on this side, start making
	// our permanent allocations here and use the other side for temp
	if ( hunk_temp->tempHighwater - hunk_temp->permanent >
		hunk_permanent->tempHighwater - hunk_permanent->permanent ) {
		swap = hunk_temp;
		hunk_temp = hunk_permanent;
		hunk_permanent = swap;
	}
}


/*
=================
Hunk_Alloc

Allocate permanent (until the hunk is cleared) memory
=================
*/
#ifdef HUNK_DEBUG
void *Hunk_AllocDebug( int size, ha_pref preference, char *label, char *file, int line ) {
#else
void *Hunk_Alloc( int size, ha_pref preference ) {
#endif
	void	*buf;

	if ( s_hunkData == NULL)
	{
		Com_Error( ERR_FATAL, "Hunk_Alloc: Hunk memory system not initialized" );
	}

	// can't do preference if there is any temp allocated
	if (preference == h_dontcare || hunk_temp->temp != hunk_temp->permanent) {
		Hunk_SwapBanks();
	} else {
		if (preference == h_low && hunk_permanent != &hunk_low) {
			Hunk_SwapBanks();
		} else if (preference == h_high && hunk_permanent != &hunk_high) {
			Hunk_SwapBanks();
		}
	}

#ifdef HUNK_DEBUG
	size += sizeof(hunkblock_t);
#endif

	// round to cacheline
	size = PAD( size, 64 );

	if ( hunk_low.temp + hunk_high.temp + size > s_hunkTotal ) {
#ifdef HUNK_DEBUG
		Hunk_Log();
		Hunk_SmallLog();

		Com_Error(ERR_DROP, "Hunk_Alloc failed on %i: %s, line: %d (%s)", size, file, line, label);
#else
		Com_Error(ERR_DROP, "Hunk_Alloc failed on %i", size);
#endif
	}

	if ( hunk_permanent == &hunk_low ) {
		buf = (void *)(s_hunkData + hunk_permanent->permanent);
		hunk_permanent->permanent += size;
	} else {
		hunk_permanent->permanent += size;
		buf = (void *)(s_hunkData + s_hunkTotal - hunk_permanent->permanent );
	}

	hunk_permanent->temp = hunk_permanent->permanent;

	Com_Memset( buf, 0, size );

#ifdef HUNK_DEBUG
	{
		hunkblock_t *block;

		block = (hunkblock_t *) buf;
		block->size = size - sizeof(hunkblock_t);
		block->file = file;
		block->label = label;
		block->line = line;
		block->next = hunkblocks;
		hunkblocks = block;
		buf = ((byte *) buf) + sizeof(hunkblock_t);
	}
#endif
	return buf;
}


/*
=================
Hunk_AllocateTempMemory

This is used by the file loading system.
Multiple files can be loaded in temporary memory.
When the files-in-use count reaches zero, all temp memory will be deleted
=================
*/
void *Hunk_AllocateTempMemory( int size ) {
	void		*buf;
	hunkHeader_t	*hdr;

	// return a Z_Malloc'd block if the hunk has not been initialized
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redundant routines in the file system utilizing different
	// memory systems
	if ( s_hunkData == NULL )
	{
		return Z_Malloc(size);
	}

	Hunk_SwapBanks();

	size = PAD(size, sizeof(intptr_t)) + sizeof( hunkHeader_t );

	if ( hunk_temp->temp + hunk_permanent->permanent + size > s_hunkTotal ) {
		Com_Error( ERR_DROP, "Hunk_AllocateTempMemory: failed on %i", size );
	}

	if ( hunk_temp == &hunk_low ) {
		buf = (void *)(s_hunkData + hunk_temp->temp);
		hunk_temp->temp += size;
	} else {
		hunk_temp->temp += size;
		buf = (void *)(s_hunkData + s_hunkTotal - hunk_temp->temp );
	}

	if ( hunk_temp->temp > hunk_temp->tempHighwater ) {
		hunk_temp->tempHighwater = hunk_temp->temp;
	}

	hdr = (hunkHeader_t *)buf;
	buf = (void *)(hdr+1);

	hdr->magic = HUNK_MAGIC;
	hdr->size = size;

	// don't bother clearing, because we are going to load a file over it
	return buf;
}


/*
==================
Hunk_FreeTempMemory
==================
*/
void Hunk_FreeTempMemory( void *buf ) {
	hunkHeader_t	*hdr;

	// free with Z_Free if the hunk has not been initialized
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redundant routines in the file system utilizing different
	// memory systems
	if ( s_hunkData == NULL )
	{
		Z_Free(buf);
		return;
	}

	hdr = ( (hunkHeader_t *)buf ) - 1;
	if ( hdr->magic != HUNK_MAGIC ) {
		Com_Error( ERR_FATAL, "Hunk_FreeTempMemory: bad magic" );
	}

	hdr->magic = HUNK_FREE_MAGIC;

	// this only works if the files are freed in stack order,
	// otherwise the memory will stay around until Hunk_ClearTempMemory
	if ( hunk_temp == &hunk_low ) {
		if ( hdr == (void *)(s_hunkData + hunk_temp->temp - hdr->size ) ) {
			hunk_temp->temp -= hdr->size;
		} else {
			Com_Printf( "Hunk_FreeTempMemory: not the final block\n" );
		}
	} else {
		if ( hdr == (void *)(s_hunkData + s_hunkTotal - hunk_temp->temp ) ) {
			hunk_temp->temp -= hdr->size;
		} else {
			Com_Printf( "Hunk_FreeTempMemory: not the final block\n" );
		}
	}
}


/*
=================
Hunk_ClearTempMemory

The temp space is no longer needed.  If we have left more
touched but unused memory on this side, have future
permanent allocs use this side.
=================
*/
void Hunk_ClearTempMemory( void ) {
	if ( s_hunkData != NULL ) {
		hunk_temp->temp = hunk_temp->permanent;
	}
}

/*
===================================================================

EVENTS AND JOURNALING

In addition to these events, .cfg files are also copied to the
journaled file
===================================================================
*/

#define	MAX_PUSHED_EVENTS 256
static int com_pushedEventsHead = 0;
static int com_pushedEventsTail = 0;
static sysEvent_t com_pushedEvents[MAX_PUSHED_EVENTS];


/*
=================
Com_InitJournaling
=================
*/
static void Com_InitJournaling( void ) {
	if ( !com_journal->integer ) {
		return;
	}

	if ( com_journal->integer == 1 ) {
		Com_Printf( "Journaling events\n" );
		com_journalFile = FS_FOpenFileWrite( "journal.dat" );
		com_journalDataFile = FS_FOpenFileWrite( "journaldata.dat" );
	} else if ( com_journal->integer == 2 ) {
		Com_Printf( "Replaying journaled events\n" );
		FS_FOpenFileRead( "journal.dat", &com_journalFile, qtrue );
		FS_FOpenFileRead( "journaldata.dat", &com_journalDataFile, qtrue );
	}

	if ( com_journalFile == FS_INVALID_HANDLE || com_journalDataFile == FS_INVALID_HANDLE ) {
		Cvar_Set( "com_journal", "0" );
		if ( com_journalFile != FS_INVALID_HANDLE ) {
			FS_FCloseFile( com_journalFile );
			com_journalFile = FS_INVALID_HANDLE;
		}
		if ( com_journalDataFile != FS_INVALID_HANDLE ) {
			FS_FCloseFile( com_journalDataFile );
			com_journalDataFile = FS_INVALID_HANDLE;
		}
		Com_Printf( "Couldn't open journal files\n" );
	}
}


/*
========================================================================

EVENT LOOP

========================================================================
*/

#define MAX_QUED_EVENTS		128
#define MASK_QUED_EVENTS	( MAX_QUED_EVENTS - 1 )

static sysEvent_t			eventQue[ MAX_QUED_EVENTS ];
static sysEvent_t			*lastEvent = eventQue + MAX_QUED_EVENTS - 1;
static unsigned int			eventHead = 0;
static unsigned int			eventTail = 0;

static const char *Sys_EventName( sysEventType_t evType ) {

	static const char *evNames[ SE_MAX ] = {
		"SE_NONE",
		"SE_KEY",
		"SE_CHAR",
		"SE_MOUSE",
		"SE_JOYSTICK_AXIS",
		"SE_CONSOLE"
	};

	if ( (unsigned)evType >= ARRAY_LEN( evNames ) ) {
		return "SE_UNKNOWN";
	} else {
		return evNames[ evType ];
	}
}


/*
================
Sys_QueEvent

A time of 0 will get the current time
Ptr should either be null, or point to a block of data that can
be freed by the game later.
================
*/
void Sys_QueEvent( int evTime, sysEventType_t evType, int value, int value2, int ptrLength, void *ptr ) {
	sysEvent_t	*ev;

#if 0
	Com_Printf( "%-10s: evTime=%i, evTail=%i, evHead=%i\n",
		Sys_EventName( evType ), evTime, eventTail, eventHead );
#endif

	if ( evTime == 0 ) {
		evTime = Sys_Milliseconds();
	}

	// try to combine all sequential mouse moves in one event
	if ( evType == SE_MOUSE && lastEvent->evType == SE_MOUSE && eventHead != eventTail ) {
		lastEvent->evValue += value;
		lastEvent->evValue2 += value2;
		lastEvent->evTime = evTime;
		return;
	}

	ev = &eventQue[ eventHead & MASK_QUED_EVENTS ];

	if ( eventHead - eventTail >= MAX_QUED_EVENTS ) {
		Com_Printf( "%s(type=%s,keys=(%i,%i),time=%i): overflow\n", __func__, Sys_EventName( evType ), value, value2, evTime );
		// we are discarding an event, but don't leak memory
		if ( ev->evPtr ) {
			Z_Free( ev->evPtr );
		}
		eventTail++;
	}

	eventHead++;

	ev->evTime = evTime;
	ev->evType = evType;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;

	lastEvent = ev;
}


/*
================
Com_GetSystemEvent
================
*/
static sysEvent_t Com_GetSystemEvent( void )
{
	sysEvent_t  ev;
	const char	*s;
	int			evTime;

	// return if we have data
	if ( eventHead - eventTail > 0 )
		return eventQue[ ( eventTail++ ) & MASK_QUED_EVENTS ];

	Sys_SendKeyEvents();

	evTime = Sys_Milliseconds();

	// check for console commands
	s = Sys_ConsoleInput();
	if ( s )
	{
		char  *b;
		int   len;

		len = strlen( s ) + 1;
		b = Z_Malloc( len );
		Q_strncpyz( b, s, len );
		Sys_QueEvent( evTime, SE_CONSOLE, 0, 0, len, b );
	}

	// return if we have data
	if ( eventHead - eventTail > 0 )
		return eventQue[ ( eventTail++ ) & MASK_QUED_EVENTS ];

	// create an empty event to return
	memset( &ev, 0, sizeof( ev ) );
	ev.evTime = evTime;

	return ev;
}


/*
=================
Com_GetRealEvent
=================
*/
static sysEvent_t Com_GetRealEvent( void ) {

	// get or save an event from/to the journal file
	if ( com_journalFile != FS_INVALID_HANDLE ) {
		int			r;
		sysEvent_t	ev;

		if ( com_journal->integer == 2 ) {
			Sys_SendKeyEvents();
			r = FS_Read( &ev, sizeof(ev), com_journalFile );
			if ( r != sizeof(ev) ) {
				Com_Error( ERR_FATAL, "Error reading from journal file" );
			}
			if ( ev.evPtrLength ) {
				ev.evPtr = Z_Malloc( ev.evPtrLength );
				r = FS_Read( ev.evPtr, ev.evPtrLength, com_journalFile );
				if ( r != ev.evPtrLength ) {
					Com_Error( ERR_FATAL, "Error reading from journal file" );
				}
			}
		} else {
			ev = Com_GetSystemEvent();

			// write the journal value out if needed
			if ( com_journal->integer == 1 ) {
				r = FS_Write( &ev, sizeof(ev), com_journalFile );
				if ( r != sizeof(ev) ) {
					Com_Error( ERR_FATAL, "Error writing to journal file" );
				}
				if ( ev.evPtrLength ) {
					r = FS_Write( ev.evPtr, ev.evPtrLength, com_journalFile );
					if ( r != ev.evPtrLength ) {
						Com_Error( ERR_FATAL, "Error writing to journal file" );
					}
				}
			}
		}

		return ev;
	}

	return Com_GetSystemEvent();
}


/*
=================
Com_InitPushEvent
=================
*/
static void Com_InitPushEvent( void ) {
  // clear the static buffer array
  // this requires SE_NONE to be accepted as a valid but NOP event
  memset( com_pushedEvents, 0, sizeof(com_pushedEvents) );
  // reset counters while we are at it
  // beware: GetEvent might still return an SE_NONE from the buffer
  com_pushedEventsHead = 0;
  com_pushedEventsTail = 0;
}


/*
=================
Com_PushEvent
=================
*/
static void Com_PushEvent( const sysEvent_t *event ) {
	sysEvent_t		*ev;
	static int printedWarning = 0;

	ev = &com_pushedEvents[ com_pushedEventsHead & (MAX_PUSHED_EVENTS-1) ];

	if ( com_pushedEventsHead - com_pushedEventsTail >= MAX_PUSHED_EVENTS ) {

		// don't print the warning constantly, or it can give time for more...
		if ( !printedWarning ) {
			printedWarning = qtrue;
			Com_Printf( "WARNING: Com_PushEvent overflow\n" );
		}

		if ( ev->evPtr ) {
			Z_Free( ev->evPtr );
		}
		com_pushedEventsTail++;
	} else {
		printedWarning = qfalse;
	}

	*ev = *event;
	com_pushedEventsHead++;
}


/*
=================
Com_GetEvent
=================
*/
static sysEvent_t Com_GetEvent( void ) {
	if ( com_pushedEventsHead - com_pushedEventsTail > 0 ) {
		return com_pushedEvents[ (com_pushedEventsTail++) & (MAX_PUSHED_EVENTS-1) ];
	}
	return Com_GetRealEvent();
}


/*
=================
Com_RunAndTimeServerPacket
=================
*/
void Com_RunAndTimeServerPacket( const netadr_t *evFrom, msg_t *buf ) {
	int		t1, t2, msec;

	t1 = 0;

	if ( com_speeds->integer ) {
		t1 = Sys_Milliseconds ();
	}

	SV_PacketEvent( evFrom, buf );

	if ( com_speeds->integer ) {
		t2 = Sys_Milliseconds ();
		msec = t2 - t1;
		if ( com_speeds->integer == 3 ) {
			Com_Printf( "SV_PacketEvent time: %i\n", msec );
		}
	}
}


/*
=================
Com_EventLoop

Returns last event time
=================
*/
int Com_EventLoop( void ) {
	sysEvent_t	ev;

#ifndef DEDICATED
	byte		bufData[ MAX_MSGLEN_BUF ];
	msg_t		buf;

	MSG_Init( &buf, bufData, MAX_MSGLEN );
#endif // !DEDICATED

	while ( 1 ) {
		ev = Com_GetEvent();

		// if no more events are available
		if ( ev.evType == SE_NONE ) {
			// manually send packet events for the loopback channel
#ifndef DEDICATED
			netadr_t evFrom;
			while ( NET_GetLoopPacket( NS_CLIENT, &evFrom, &buf ) ) {
				CL_PacketEvent( &evFrom, &buf );
			}
			while ( NET_GetLoopPacket( NS_SERVER, &evFrom, &buf ) ) {
				// if the server just shut down, flush the events
				if ( com_sv_running->integer ) {
					Com_RunAndTimeServerPacket( &evFrom, &buf );
				}
			}
#endif // !DEDICATED
			return ev.evTime;
		}

		switch ( ev.evType ) {
#ifndef DEDICATED
		case SE_KEY:
			CL_KeyEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
		case SE_CHAR:
			CL_CharEvent( ev.evValue );
			break;
		case SE_MOUSE:
			CL_MouseEvent( ev.evValue, ev.evValue2 /*, ev.evTime*/ );
			break;
		case SE_JOYSTICK_AXIS:
			CL_JoystickEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
#endif // !DEDICATED
		case SE_CONSOLE:
			Cbuf_AddText( (char *)ev.evPtr );
			Cbuf_AddText( "\n" );
			break;
		default:
				Com_Error( ERR_FATAL, "Com_EventLoop: bad event type %i", ev.evType );
			break;
		}

		// free any block data
		if ( ev.evPtr ) {
			Z_Free( ev.evPtr );
			ev.evPtr = NULL;
		}
	}

	return 0;	// never reached
}


/*
================
Com_Milliseconds

Can be used for profiling, but will be journaled accurately
================
*/
int Com_Milliseconds( void ) {

	sysEvent_t	ev;

	// get events and push them until we get a null event with the current time
	do {
		ev = Com_GetRealEvent();
		if ( ev.evType != SE_NONE ) {
			Com_PushEvent( &ev );
		}
	} while ( ev.evType != SE_NONE );

	return ev.evTime;
}

//============================================================================

/*
=============
Com_Error_f

Just throw a fatal error to
test error shutdown procedures
=============
*/
static void Com_Error_f (void) {
	if ( Cmd_Argc() > 1 ) {
		Com_Error( ERR_DROP, "Testing drop error" );
	} else {
		Com_Error( ERR_FATAL, "Testing fatal error" );
	}
}


/*
=============
Com_Freeze_f

Just freeze in place for a given number of seconds to test
error recovery
=============
*/
static void Com_Freeze_f( void ) {
	int		s;
	int		start, now;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "freeze <seconds>\n" );
		return;
	}
	s = Q_SafeAtoi( Cmd_Argv(1), 0, NULL ) * 1000;

	start = Com_Milliseconds();

	while ( 1 ) {
		now = Com_Milliseconds();
		if ( now - start > s ) {
			break;
		}
	}
}


/*
=================
Com_Crash_f

A way to force a bus error for development reasons
=================
*/
static void Com_Crash_f( void ) {
	* ( volatile int * ) 0 = 0x12345678;
}


/*
==================
Com_ExecuteCfg

For controlling environment variables
==================
*/
static void Com_ExecuteCfg( void )
{
	Cbuf_ExecuteText(EXEC_NOW, "exec default.cfg\n");
	Cbuf_Execute(); // Always execute after exec to prevent text buffer overflowing

	if (!Com_SafeMode())
	{
		// skip the config.cfg and autoexec.cfg if "safe" is on the command line
		Cbuf_ExecuteText(EXEC_NOW, "exec " Q3CONFIG_CFG "\n");
		Cbuf_Execute();
		Cbuf_ExecuteText(EXEC_NOW, "exec autoexec.cfg\n");
		Cbuf_Execute();
	}
}


/*
==================
Com_GameRestart

Change to a new mod properly with cleaning up cvars before switching.
==================
*/
void Com_GameRestart( int checksumFeed, qboolean clientRestart )
{
	(void)clientRestart;  // Suppress unused parameter warning
	static qboolean com_gameRestarting = qfalse;

	// make sure no recursion can be triggered
	if ( !com_gameRestarting && com_fullyInitialized )
	{
		com_gameRestarting = qtrue;
#ifndef DEDICATED
		if ( clientRestart )
		{
			CL_Disconnect( qfalse );
			CL_ShutdownAll();
			CL_ClearMemory(); // Hunk_Clear(); // -EC-
		}
#endif

		// Kill server if we have one
		if ( com_sv_running->integer )
			SV_Shutdown( "Game directory changed" );

		// Reset console command history
		Con_ResetHistory();

		// Shutdown FS early so Cvar_Restart will not reset old game cvars
		FS_Shutdown( qtrue );

		// Clean out any user and VM created cvars
		Cvar_Restart( qtrue );

#ifndef DEDICATED
		// Reparse pure paks and update cvars before FS startup
		if ( CL_GameSwitch() )
			CL_SystemInfoChanged( qfalse );
#endif

		FS_Restart( checksumFeed );

		// Load new configuration
		Com_ExecuteCfg();

#ifndef DEDICATED
		if ( clientRestart )
			CL_StartHunkUsers();
#endif

		com_gameRestarting = qfalse;
	}
}


/*
==================
Com_GameRestart_f

Expose possibility to change current running mod to the user
==================
*/
static void Com_GameRestart_f( void )
{
	Cvar_Set( "fs_game", Cmd_Argv( 1 ) );

	Com_GameRestart( 0, qtrue );
}


// TTimo: centralizing the cl_cdkey stuff after I discovered a buffer overflow problem with the dedicated server version
//   not sure it's necessary to have different defaults for regular and dedicated, but I don't want to risk it
//   https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=470
#ifndef DEDICATED
char	cl_cdkey[34] = "                                ";
#else
char	cl_cdkey[34] = "123456789";
#endif

/*
=================
bool CL_CDKeyValidate
=================
*/
qboolean Com_CDKeyValidate( const char *key, const char *checksum ) {
	// CD key validation disabled - always return valid
	return qtrue;
#ifdef STANDALONE
	return qtrue;
#else
	char	ch;
	byte	sum;
	char	chs[10];
	int i, len;

	len = strlen(key);
	if( len != CDKEY_LEN ) {
		return qfalse;
	}

	if( checksum && strlen( checksum ) != CDCHKSUM_LEN ) {
		return qfalse;
	}

	sum = 0;
	// for loop gets rid of conditional assignment warning
	for (i = 0; i < len; i++) {
		ch = *key++;
		if (ch>='a' && ch<='z') {
			ch -= 32;
		}
		switch( ch ) {
		case '2':
		case '3':
		case '7':
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'G':
		case 'H':
		case 'J':
		case 'L':
		case 'P':
		case 'R':
		case 'S':
		case 'T':
		case 'W':
			sum += ch;
			continue;
		default:
			return qfalse;
		}
	}

	Com_sprintf(chs, sizeof(chs), "%02x", sum);

	if (checksum && !Q_stricmp(chs, checksum)) {
		return qtrue;
	}

	if (!checksum) {
		return qtrue;
	}

	return qfalse;
#endif
}


/*
=================
Com_ReadCDKey
=================
*/
void Com_ReadCDKey( const char *filename ) {
	fileHandle_t	f;
	char			buffer[33];
	char			fbuffer[MAX_OSPATH];

	Com_sprintf( fbuffer, sizeof( fbuffer ), "%s/key", filename );

	FS_SV_FOpenFileRead( fbuffer, &f );
	if ( f == FS_INVALID_HANDLE ) {
		Q_strncpyz( cl_cdkey, "                ", 17 );
		return;
	}

	Com_Memset( buffer, 0, sizeof( buffer ) );

	FS_Read( buffer, 16, f );
	FS_FCloseFile( f );

	if ( Com_CDKeyValidate(buffer, NULL) ) {
		Q_strncpyz( cl_cdkey, buffer, 17 );
	} else {
		Q_strncpyz( cl_cdkey, "                ", 17 );
	}
}


/*
=================
Com_AppendCDKey
=================
*/
void Com_AppendCDKey( const char *filename ) {
	fileHandle_t	f;
	char			buffer[33];
	char			fbuffer[MAX_OSPATH];

	Com_sprintf(fbuffer, sizeof(fbuffer), "%s/key", filename);

	FS_SV_FOpenFileRead( fbuffer, &f );
	if ( f == FS_INVALID_HANDLE ) {
		Q_strncpyz( &cl_cdkey[16], "                ", 17 );
		return;
	}

	Com_Memset( buffer, 0, sizeof(buffer) );

	FS_Read( buffer, 16, f );
	FS_FCloseFile( f );

	if ( Com_CDKeyValidate(buffer, NULL)) {
		Q_strcat( &cl_cdkey[16], sizeof(cl_cdkey) - 16, buffer );
	} else {
		Q_strncpyz( &cl_cdkey[16], "                ", 17 );
	}
}


#ifndef DEDICATED // bk001204
/*
=================
Com_WriteCDKey
=================
*/
static void Com_WriteCDKey( const char *filename, const char *ikey ) {
	fileHandle_t	f;
	char			fbuffer[MAX_OSPATH];
	char			key[17];
#ifndef _WIN32
	mode_t			savedumask;
#endif

	Com_sprintf( fbuffer, sizeof(fbuffer), "%s/key", filename );

	Q_strncpyz( key, ikey, 17 );

	if( !Com_CDKeyValidate(key, NULL) ) {
		return;
	}

#ifndef _WIN32
	savedumask = umask(0077);
#endif
	f = FS_SV_FOpenFileWrite( fbuffer );
	if ( f == FS_INVALID_HANDLE ) {
		Com_Printf( "Couldn't write key to %s.\n", fbuffer );
		goto out;
	}

	FS_Write( key, 16, f );

	FS_Printf( f, Q_NEWLINE "// generated by id Tech 3, do not modify" Q_NEWLINE );
	FS_Printf( f, "// Do not give this file to anyone." Q_NEWLINE );

	FS_FCloseFile( f );
out:
#ifndef _WIN32
	umask(savedumask);
#else
	;
#endif
}
#endif


/*
** --------------------------------------------------------------------------------
**
** PROCESSOR STUFF
**
** --------------------------------------------------------------------------------
*/

#ifdef USE_AFFINITY_MASK
static uint64_t eCoreMask;
static uint64_t pCoreMask;
static uint64_t affinityMask; // saved at startup
#endif

#if (idx64 || id386)

#if defined _MSC_VER
#include <intrin.h>
static void CPUID( int func, unsigned int *regs )
{
	__cpuid( (int*)regs, func );
}

#ifdef USE_AFFINITY_MASK
#if idx64
extern void CPUID_EX( int func, int param, unsigned int *regs );
#else
void CPUID_EX( int func, int param, unsigned int *regs )
{
	__asm {
		push edi
		mov eax, func
		mov ecx, param
		cpuid
		mov edi, regs
		mov [edi +0], eax
		mov [edi +4], ebx
		mov [edi +8], ecx
		mov [edi+12], edx
		pop edi
	}
}
#endif // !idx64
#endif // USE_AFFINITY_MASK

#else // clang/gcc/mingw

static void CPUID( int func, unsigned int *regs )
{
	__asm__ __volatile__( "cpuid" :
		"=a"(regs[0]),
		"=b"(regs[1]),
		"=c"(regs[2]),
		"=d"(regs[3]) :
		"a"(func) );
}

#ifdef USE_AFFINITY_MASK
static void CPUID_EX( int func, int param, unsigned int *regs )
{
	__asm__ __volatile__( "cpuid" :
		"=a"(regs[0]),
		"=b"(regs[1]),
		"=c"(regs[2]),
		"=d"(regs[3]) :
		"a"(func),
		"c"(param) );
}
#endif // USE_AFFINITY_MASK

#endif  // clang/gcc/mingw

static void Sys_GetProcessorId( char *vendor )
{
	uint32_t regs[4]; // EAX, EBX, ECX, EDX
	uint32_t cpuid_level_ex;
	char vendor_str[12 + 1]; // short CPU vendor string

	// setup initial features
#if idx64
	CPU_Flags |= CPU_SSE | CPU_SSE2 | CPU_FCOM;
#else
	CPU_Flags = 0;
#endif
	vendor[0] = '\0';

	CPUID( 0x80000000, regs );
	cpuid_level_ex = regs[0];

	// get CPUID level & short CPU vendor string
	CPUID( 0x0, regs );
	memcpy(vendor_str + 0, (char*)&regs[1], 4);
	memcpy(vendor_str + 4, (char*)&regs[3], 4);
	memcpy(vendor_str + 8, (char*)&regs[2], 4);
	vendor_str[12] = '\0';

	// get CPU feature bits
	CPUID( 0x1, regs );

	// bit 15 of EDX denotes CMOV/FCMOV/FCOMI existence
	if ( regs[3] & ( 1 << 15 ) )
		CPU_Flags |= CPU_FCOM;

	// bit 23 of EDX denotes MMX existence
	if ( regs[3] & ( 1 << 23 ) )
		CPU_Flags |= CPU_MMX;

	// bit 25 of EDX denotes SSE existence
	if ( regs[3] & ( 1 << 25 ) )
		CPU_Flags |= CPU_SSE;

	// bit 26 of EDX denotes SSE2 existence
	if ( regs[3] & ( 1 << 26 ) )
		CPU_Flags |= CPU_SSE2;

	// bit 0 of ECX denotes SSE3 existence
	//if ( regs[2] & ( 1 << 0 ) )
	//	CPU_Flags |= CPU_SSE3;

	// bit 19 of ECX denotes SSE41 existence
	if ( regs[ 2 ] & ( 1 << 19 ) )
		CPU_Flags |= CPU_SSE41;

	if ( vendor ) {
		if ( cpuid_level_ex >= 0x80000004 ) {
			// read CPU Brand string
			uint32_t i;
			for ( i = 0x80000002; i <= 0x80000004; i++) {
				CPUID( i, regs );
				memcpy( vendor+0, (char*)&regs[0], 4 );
				memcpy( vendor+4, (char*)&regs[1], 4 );
				memcpy( vendor+8, (char*)&regs[2], 4 );
				memcpy( vendor+12, (char*)&regs[3], 4 );
				vendor[16] = '\0';
				vendor += strlen( vendor );
			}
		} else {
			const int print_flags = CPU_Flags;
			vendor = Q_stradd( vendor, vendor_str );
			if (print_flags) {
				// print features
				Q_strcat(vendor, 128 - strlen(vendor), " w/");
				if (print_flags & CPU_FCOM)
					Q_strcat(vendor, 128 - strlen(vendor), " CMOV");
				if (print_flags & CPU_MMX)
					Q_strcat(vendor, 128 - strlen(vendor), " MMX");
				if (print_flags & CPU_SSE)
					Q_strcat(vendor, 128 - strlen(vendor), " SSE");
				if (print_flags & CPU_SSE2)
					Q_strcat(vendor, 128 - strlen(vendor), " SSE2");
				//if ( CPU_Flags & CPU_SSE3 )
				//	Q_strcat(vendor, 128 - strlen(vendor), " SSE3");
				if (print_flags & CPU_SSE41)
					Q_strcat(vendor, 128 - strlen(vendor), " SSE4.1");
			}
		}
	}
}


#ifdef USE_AFFINITY_MASK
static void DetectCPUCoresConfig( void )
{
	uint32_t regs[4];
	uint32_t i;

	// get highest function parameter and vendor id
	CPUID( 0x0, regs );
	if ( regs[1] != 0x756E6547 || regs[2] != 0x6C65746E || regs[3] != 0x49656E69 || regs[0] < 0x1A ) {
		// non-intel signature or too low cpuid level - unsupported
		eCoreMask = pCoreMask = affinityMask;
		return;
	}

	eCoreMask = 0;
	pCoreMask = 0;

	for ( i = 0; i < sizeof( affinityMask ) * 8; i++ ) {
		const uint64_t mask = 1ULL << i;
		if ( (mask & affinityMask) && Sys_SetAffinityMask( mask ) ) {
			CPUID_EX( 0x1A, 0x0, regs );
			switch ( (regs[0] >> 24) & 0xFF ) {
				case 0x20: eCoreMask |= mask; break;
				case 0x40: pCoreMask |= mask; break;
				default: // non-existing leaf
					eCoreMask = pCoreMask = 0;
					break;
			}
		}
	}

	// restore original affinity
	Sys_SetAffinityMask( affinityMask );

	if ( pCoreMask == 0 || eCoreMask == 0 ) {
		// if either mask is empty - assume non-hybrid configuration
		eCoreMask = pCoreMask = affinityMask;
	}
}
#endif // USE_AFFINITY_MASK

#else // non-x86

#ifndef __linux__

static void Sys_GetProcessorId( char *vendor )
{
	Com_sprintf( vendor, 100, "%s", ARCH_STRING );
}

#else // __linux__

#include <sys/auxv.h>

#if arm32
#include <asm/hwcap.h>
#endif

static void Sys_GetProcessorId( char *vendor )
{
#if arm32
	const char *platform;
	long hwcaps;
	CPU_Flags = 0;

	platform = (const char*)getauxval( AT_PLATFORM );

	if ( !platform || *platform == '\0' ) {
		platform = "(unknown)";
	}

	if ( platform[0] == 'v' || platform[0] == 'V' ) {
		if ( Q_SafeAtoi( platform + 1, 0, NULL ) >= 7 ) {
			CPU_Flags |= CPU_ARMv7;
		}
	}

	Com_sprintf( vendor, 100, "ARM %s", platform );
	hwcaps = getauxval( AT_HWCAP );
	if ( hwcaps & ( HWCAP_IDIVA | HWCAP_VFPv3 ) ) {
		strcat( vendor, " /w" );

		if ( hwcaps & HWCAP_IDIVA ) {
			CPU_Flags |= CPU_IDIVA;
			strcat( vendor, " IDIVA" );
		}

		if ( hwcaps & HWCAP_VFPv3 ) {
			CPU_Flags |= CPU_VFPv3;
			strcat( vendor, " VFPv3" );
		}

		if ( ( CPU_Flags & ( CPU_ARMv7 | CPU_VFPv3 ) ) == ( CPU_ARMv7 | CPU_VFPv3 ) ) {
			strcat( vendor, " QVM-bytecode" );
		}
	}
#else // !arm32
	CPU_Flags = 0;
#if arm64
	Com_sprintf( vendor, 100, "%s", ARCH_STRING );
#else
	Com_sprintf( vendor, 128, "%s %s", ARCH_STRING, (const char*)getauxval( AT_PLATFORM ) );
#endif
#endif // !arm32
}

#endif // __linux__

#endif // non-x86

/*
================
Sys_SnapVector
================
*/
#ifdef _MSC_VER
#if idx64
void Sys_SnapVector( float *vector )
{
	__m128 vf0, vf1, vf2;
	__m128i vi;
	DWORD mxcsr;

	mxcsr = _mm_getcsr();
	vf0 = _mm_setr_ps( vector[0], vector[1], vector[2], 0.0f );

	_mm_setcsr( mxcsr & ~0x6000 ); // enforce rounding mode to "round to nearest"

	vi = _mm_cvtps_epi32( vf0 );
	vf0 = _mm_cvtepi32_ps( vi );

	vf1 = _mm_shuffle_ps(vf0, vf0, _MM_SHUFFLE(1,1,1,1));
	vf2 = _mm_shuffle_ps(vf0, vf0, _MM_SHUFFLE(2,2,2,2));

	_mm_setcsr( mxcsr ); // restore rounding mode

	_mm_store_ss( &vector[0], vf0 );
	_mm_store_ss( &vector[1], vf1 );
	_mm_store_ss( &vector[2], vf2 );
}
#endif // idx64

#if id386
void Sys_SnapVector( float *vector )
{
	static const DWORD cw037F = 0x037F;
	DWORD cwCurr;
__asm {
	fnstcw word ptr [cwCurr]
	mov ecx, vector
	fldcw word ptr [cw037F]

	fld   dword ptr[ecx+8]
	fistp dword ptr[ecx+8]
	fild  dword ptr[ecx+8]
	fstp  dword ptr[ecx+8]

	fld   dword ptr[ecx+4]
	fistp dword ptr[ecx+4]
	fild  dword ptr[ecx+4]
	fstp  dword ptr[ecx+4]

	fld   dword ptr[ecx+0]
	fistp dword ptr[ecx+0]
	fild  dword ptr[ecx+0]
	fstp  dword ptr[ecx+0]

	fldcw word ptr cwCurr
	}; // __asm
}
#endif // id386

#if arm64
void Sys_SnapVector( float *vector )
{
	vector[0] = rint( vector[0] );
	vector[1] = rint( vector[1] );
	vector[2] = rint( vector[2] );
}
#endif

#else // clang/gcc/mingw

#if id386

#define QROUNDX87(src) \
	"flds " src "\n" \
	"fistpl " src "\n" \
	"fildl " src "\n" \
	"fstps " src "\n"

void Sys_SnapVector( float *vector )
{
	static const unsigned short cw037F = 0x037F;
	unsigned short cwCurr;

	__asm__ volatile
	(
		"fnstcw %1\n" \
		"fldcw %2\n" \
		QROUNDX87("0(%0)")
		QROUNDX87("4(%0)")
		QROUNDX87("8(%0)")
		"fldcw %1\n" \
		:
		: "r" (vector), "m"(cwCurr), "m"(cw037F)
		: "memory", "st"
	);
}

#else // idx64, non-x86

void Sys_SnapVector( float *vector )
{
	vector[0] = rint( vector[0] );
	vector[1] = rint( vector[1] );
	vector[2] = rint( vector[2] );
}

#endif

#endif // clang/gcc/mingw

#ifdef USE_AFFINITY_MASK

static int hex_code( const int code ) {
	if ( code >= '0' && code <= '9' ) {
		return code - '0';
	}
	if ( code >= 'A' && code <= 'F' ) {
		return code - 'A' + 10;
	}
	if ( code >= 'a' && code <= 'f' ) {
		return code - 'a' + 10;
	}
	return -1;
}


static const char *parseAffinityMask( const char *str, uint64_t *outv, int level ) {
	uint64_t v, mask = 0;

	while ( *str != '\0' ) {
		if ( *str == 'A' || *str == 'a' ) {
			mask = affinityMask;
			++str;
			continue;
		}
		else if ( *str == 'P' || *str == 'p' ) {
			mask = pCoreMask;
			++str;
			continue;
		}
		else if ( *str == 'E' || *str == 'e' ) {
			mask = eCoreMask;
			++str;
			continue;
		}
		else if ( *str == '0' && (str[1] == 'x' || str[1] == 'X') ) {
			int hex, hv = hex_code( str[2] );
			if ( hv < 0 ) {
				++str;
				continue;
			}
			v = (uint64_t)hv;
			str += 3; // 0xH
			while ( (hex = hex_code( *str )) >= 0 ) {
				v = v * 16 + hex;
				str++;
			}
			mask = v;
			continue;
		}
		else if ( *str >= '0' && *str <= '9' ) {
			mask = *str++ - '0';
			while ( *str >= '0' && *str <= '9' ) {
				mask = mask * 10 + *str - '0';
				++str;
			}
			continue;
		}

		if ( level == 0 ) {
			while ( *str == '+' || *str == '-' ) {
				str = parseAffinityMask( str + 1, &v, level + 1 );
				switch ( *str ) {
					case '+': mask |= v; break;
					case '-': mask &= ~v; break;
					default: str = ""; break;
				}
			}
			if ( *str != '\0' ) {
				++str; // skip unknown characters
			}
		} else {
			break;
		}
	}

	*outv = mask;
	return str;
}


// parse and set affinity mask
static void Com_SetAffinityMask( const char *str )
{
	uint64_t mask = 0;

	parseAffinityMask( str, &mask, 0 );

	if ( ( mask & affinityMask ) == 0 ) {
		mask = affinityMask; // reset to default
	}

	if ( mask != 0 ) {
		Sys_SetAffinityMask( mask );
	}
}
#endif // USE_AFFINITY_MASK


/*
=================
Com_Init
=================
*/
// Shared BotLib support for client and server
static void Com_BotImport_Print( int type, const char *fmt, ... ) {
	va_list argptr;
	char msg[2048];
	(void)type;
	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);
	Com_Printf("%s", msg);
}

static void *Com_BotImport_GetMemory(int size) {
	return Z_TagMalloc( size, TAG_BOTLIB );
}

static void Com_BotImport_FreeMemory(void *ptr) {
	Z_Free(ptr);
}

static void *Com_BotImport_HunkAlloc( int size ) {
	return Hunk_Alloc( size, h_high );
}

void Com_InitBotLib(void) {
	botlib_import_t	botlib_import;

	if (botlib_export) return;

	Com_Memset(&botlib_import, 0, sizeof(botlib_import));
	botlib_import.Print = Com_BotImport_Print;
	botlib_import.GetMemory = Com_BotImport_GetMemory;
	botlib_import.FreeMemory = Com_BotImport_FreeMemory;
	botlib_import.AvailableMemory = Z_AvailableMemory;
	botlib_import.HunkAlloc = Com_BotImport_HunkAlloc;
	botlib_import.FS_FOpenFile = FS_FOpenFileByMode;
	botlib_import.FS_Read = FS_Read;
	botlib_import.FS_Write = FS_Write;
	botlib_import.FS_FCloseFile = FS_FCloseFile;
	botlib_import.FS_Seek = FS_Seek;
	botlib_import.Sys_Milliseconds = Sys_Milliseconds;

	botlib_export = (botlib_export_t *)GetBotLibAPI( BOTLIB_API_VERSION, &botlib_import );
}

void Com_Init( char *commandLine ) {
    // Initialize hardening systems in order of dependency
	const char *s;
	int	qport;

	// get the initial time base
	Sys_Milliseconds();

	// Initialize crash handler early (must be before other subsystems)
	Crash_Init();

	Com_Printf( "%s %s %s\n", SVN_VERSION, PLATFORM_STRING, Q_BUILD_DATE );

	// Ensure libc locale uses UTF-8 for case folding, wide char ops, etc.
	Com_InitLocaleUTF8();

	if ( Q_setjmp( abortframe ) ) {
		Sys_Error ("Error during initialization");
	}

	// bk001129 - do this before anything else decides to push events
	Com_InitPushEvent();

	Com_InitSmallZoneMemory();

	// Initialize platform abstraction layer
	Platform_Init();

	Cvar_Init();

	// Initialize performance counters
	// Perf_Init(); // Moved after memory system initialization

	// Initialize VM hot reload system
	// VM_HotReloadInit(); // Moved after memory system initialization

	// Initialize event system
	Event_Init();

	// Initialize assert system
	Assert_Init();

	// Initialize watchdog system
	Watchdog_Init();

	// Initialize scalability system
	// Scalability_Init(); // Moved after memory system initialization

	// Initialize asset loader system
	// Asset_LoadersInit(); // Moved after memory system initialization

	// Check for safe mode boot
	if (Crash_ShouldBootSafeMode()) {
		Com_Printf(S_COLOR_YELLOW "Engine detected previous crash, booting in safe mode.\n");
		Cvar_Set("vid_renderer", "-1"); // Use default renderer
		Cvar_Set("r_fullscreen", "0");
		Cvar_Set("r_mode", "3");     // 640x480
		Cvar_Set("com_safemode", "1"); // Indicate safe mode active
	}
	Crash_ClearSafeModeFlag(); // Clear the flag after handling

#if defined(_WIN32) && defined(_DEBUG)
	com_noErrorInterrupt = Cvar_Get( "com_noErrorInterrupt", "0", 0 );
#endif

#ifdef DEFAULT_GAME
	Cvar_Set( "fs_game", DEFAULT_GAME );
#endif

	// prepare enough of the subsystems to handle
	// cvar and command buffer management
	Com_ParseCommandLine( commandLine );

//	Swap_Init ();
	Cbuf_Init();

	// override anything from the config files with command line args
	Com_StartupVariable( NULL );

	Com_InitZoneMemory();

	// Initialize hardening systems after memory system is ready
	MemorySafety_Init();
	ErrorRecovery_Init();
	InputValidation_Init();
	// Stability_Init(); // Temporarily disabled for debugging

	// Initialize scalability system
	Scalability_Init();

	// Initialize asset loader system
	Asset_LoadersInit();

	// Initialize performance counters
	Perf_Init();

	// Initialize VM hot reload system
	VM_HotReloadInit();

	Cmd_Init();

	// get the developer cvar set as early as possible
	Com_StartupVariable( "developer" );
com_developer = Cvar_Get( "developer", "0", CVAR_TEMP );
Cvar_CheckRange( com_developer, NULL, NULL, CV_INTEGER );

Com_StartupVariable( "safemode" );
com_safemode = Cvar_Get( "com_safemode", "0", CVAR_ARCHIVE );
Cvar_CheckRange( com_safemode, "0", "1", CV_INTEGER );

	Com_StartupVariable( "vm_rtChecks" );
	vm_rtChecks = Cvar_Get( "vm_rtChecks", "15", CVAR_INIT | CVAR_PROTECTED );
	Cvar_CheckRange( vm_rtChecks, "0", "15", CV_INTEGER );
	Cvar_SetDescription( vm_rtChecks,
		"Runtime checks in compiled vm code, bitmask:\n 1 - program stack overflow\n" \
		" 2 - opcode stack overflow\n 4 - jump target range\n 8 - data read/write range" );

	Com_StartupVariable( "journal" );
	com_journal = Cvar_Get( "journal", "0", CVAR_INIT | CVAR_PROTECTED );
	Cvar_CheckRange( com_journal, "0", "2", CV_INTEGER );
	Cvar_SetDescription( com_journal, "When enabled, writes events and its data to 'journal.dat' and 'journaldata.dat'.");

	Com_StartupVariable( "sv_master1" );
	Com_StartupVariable( "sv_master2" );
	Com_StartupVariable( "sv_master3" );
	Cvar_Get( "sv_master1", MASTER_SERVER_NAME, CVAR_INIT );
	Cvar_Get( "sv_master2", "master.ioquake3.org", CVAR_INIT );
	Cvar_Get( "sv_master3", "master.maverickservers.com", CVAR_INIT );

	com_protocol = Cvar_Get( "protocol", XSTRING( DEFAULT_PROTOCOL_VERSION ), 0 );
	Cvar_SetDescription( com_protocol, "Specify network protocol version number, use -compat suffix for OpenArena compatibility.");
	if ( Q_stristr( com_protocol->string, "-compat" ) > com_protocol->string ) {
		// strip -compat suffix
		Cvar_Set2( "protocol", va( "%i", com_protocol->integer ), qtrue );
		// enforce legacy stream encoding but with new challenge format
		com_protocolCompat = qtrue;
	} else {
		com_protocolCompat = qfalse;
	}

	Cvar_CheckRange( com_protocol, "0", NULL, CV_INTEGER );
	com_protocol->flags &= ~CVAR_USER_CREATED;
	com_protocol->flags |= CVAR_SERVERINFO | CVAR_ROM;

	// done early so bind command exists
	Com_InitKeyCommands();

	FS_InitFilesystem();

	// Initialize structured logging system
	Q_Log_Init();
	
	// Initialize memory tracking system
	Q_MemTrack_Init();
	
#ifdef USE_CURL
	// Initialize telemetry system
	Telemetry_Init();
#endif

	com_logfile = Cvar_Get( "logfile", "0", CVAR_TEMP );
	Cvar_CheckRange( com_logfile, "0", "4", CV_INTEGER );
	Cvar_SetDescription( com_logfile, "System console logging:\n"
		" 0 - disabled\n"
		" 1 - overwrite mode, buffered\n"
		" 2 - overwrite mode, synced\n"
		" 3 - append mode, buffered\n"
		" 4 - append mode, synced\n" );

	Com_InitJournaling();

	Com_ExecuteCfg();

	// override anything from the config files with command line args
	Com_StartupVariable( NULL );

	// get dedicated here for proper hunk megs initialization
#ifdef DEDICATED
	com_dedicated = Cvar_Get( "dedicated", "1", CVAR_INIT );
	Cvar_CheckRange( com_dedicated, "1", "2", CV_INTEGER );
#else
	com_dedicated = Cvar_Get( "dedicated", "0", CVAR_LATCH );
	Cvar_CheckRange( com_dedicated, "0", "2", CV_INTEGER );
#endif
	Cvar_SetDescription( com_dedicated, "Enables dedicated server mode.\n 0: Listen server\n 1: Unlisted dedicated server \n 2: Listed dedicated server" );
	// allocate the stack based hunk allocator
	Com_InitHunkMemory();

	// if any archived cvars are modified after this, we will trigger a writing
	// of the config file
	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

	//
	// init commands and vars
	//
#ifndef DEDICATED
	com_maxfps = Cvar_Get( "com_maxfps", "125", 0 ); // try to force that in some light way
	Cvar_CheckRange( com_maxfps, "0", "1000", CV_INTEGER );
	Cvar_SetDescription( com_maxfps, "Sets maximum frames per second." );
	com_maxfpsUnfocused = Cvar_Get( "com_maxfpsUnfocused", "60", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( com_maxfpsUnfocused, "0", "1000", CV_INTEGER );
	Cvar_SetDescription( com_maxfpsUnfocused, "Sets maximum frames per second in unfocused game window." );
	com_yieldCPU = Cvar_Get( "com_yieldCPU", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( com_yieldCPU, "0", "16", CV_INTEGER );
	Cvar_SetDescription( com_yieldCPU, "Attempt to sleep specified amount of time between rendered frames when game is active, this will greatly reduce CPU load. Use 0 only if you're experiencing some lag." );
#endif

#ifdef USE_AFFINITY_MASK
	com_affinityMask = Cvar_Get( "com_affinityMask", "", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( com_affinityMask, "Bind game process to bitmask-specified CPU core(s), special characters:\n A or a - all default cores\n P or p - performance cores\n E or e - efficiency cores\n 0x<value> - use hexadecimal notation\n + or - can be used to add or exclude particular cores" );
	com_affinityMask->modified = qfalse;
#endif

	// com_blood = Cvar_Get( "com_blood", "1", CVAR_ARCHIVE_ND );

	com_timescale = Cvar_Get( "timescale", "1", CVAR_CHEAT | CVAR_SYSTEMINFO );
	Cvar_CheckRange( com_timescale, "0", NULL, CV_FLOAT );
	Cvar_SetDescription( com_timescale, "System timing factor:\n < 1: Slows the game down\n = 1: Regular speed\n > 1: Speeds the game up" );
	com_fixedtime = Cvar_Get( "fixedtime", "0", CVAR_CHEAT );
	Cvar_SetDescription( com_fixedtime, "Toggle the rendering of every frame the game will wait until each frame is completely rendered before sending the next frame." );
	com_showtrace = Cvar_Get( "com_showtrace", "0", CVAR_CHEAT );
	Cvar_SetDescription( com_showtrace, "Debugging tool that prints out trace information." );
	com_viewlog = Cvar_Get( "viewlog", "0", 0 );
	Cvar_SetDescription( com_viewlog, "Toggle the display of the startup console window over the game screen." );
	com_speeds = Cvar_Get( "com_speeds", "0", 0 );
	Cvar_SetDescription( com_speeds, "Prints speed information per frame to the console. Used for debugging." );
	com_cameraMode = Cvar_Get( "com_cameraMode", "0", CVAR_CHEAT );

#ifndef DEDICATED
	com_timedemo = Cvar_Get( "timedemo", "0", 0 );
	Cvar_CheckRange( com_timedemo, "0", "1", CV_INTEGER );
	Cvar_SetDescription( com_timedemo, "When set to '1' times a demo and returns frames per second like a benchmark." );
	cl_paused = Cvar_Get( "cl_paused", "0", CVAR_ROM );
	Cvar_SetDescription( cl_paused, "Read-only CVAR to toggle functionality of paused games (the variable holds the status of the paused flag on the client side)." );
	cl_packetdelay = Cvar_Get( "cl_packetdelay", "0", CVAR_CHEAT );
	Cvar_SetDescription( cl_packetdelay, "Artificially set the client's latency. Simulates packet delay, which can lead to packet loss." );
	com_cl_running = Cvar_Get( "cl_running", "0", CVAR_ROM | CVAR_NOTABCOMPLETE );
	Cvar_SetDescription( com_cl_running, "Can be used to check the status of the client game." );
#endif

	sv_paused = Cvar_Get( "sv_paused", "0", CVAR_ROM );
	sv_packetdelay = Cvar_Get( "sv_packetdelay", "0", CVAR_CHEAT );
	Cvar_SetDescription( sv_packetdelay, "Simulates packet delay, which can lead to packet loss. Server side." );
	com_sv_running = Cvar_Get( "sv_running", "0", CVAR_ROM | CVAR_NOTABCOMPLETE );
	Cvar_SetDescription( com_sv_running, "Communicates to game modules if there is a server currently running." );

	com_buildScript = Cvar_Get( "com_buildScript", "0", 0 );
	Cvar_SetDescription( com_buildScript, "Loads all game assets, regardless whether they are required or not." );

	Cvar_Get( "com_errorMessage", "", CVAR_ROM | CVAR_NORESTART );

#ifndef DEDICATED
	com_introPlayed = Cvar_Get( "com_introplayed", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( com_introPlayed, "Skips the introduction cinematic." );
	com_skipIdLogo  = Cvar_Get( "com_skipIdLogo", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( com_skipIdLogo, "Skip playing Id Software logo cinematic at startup." );
#endif

	com_jobThreads = Cvar_Get( "com_jobThreads", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( com_jobThreads, "Number of worker threads for job system (0 = auto-detect CPU cores)" );

	com_memoryStats = Cvar_Get( "com_memoryStats", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( com_memoryStats, "Enable memory usage statistics and tracking" );

	com_preciseTime = Cvar_Get( "com_preciseTime", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_preciseTime, "Use high-precision timing (may impact performance slightly)" );

	com_assertLevel = Cvar_Get( "com_assertLevel", "2", CVAR_ARCHIVE );
	Cvar_SetDescription( com_assertLevel, "Assertion level: 0=off, 1=hard, 2=once, 3=debug" );

	com_watchdogEnabled = Cvar_Get( "com_watchdogEnabled", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_watchdogEnabled, "Enable watchdog thread monitoring for deadlocks" );

	com_crashHandlerEnabled = Cvar_Get( "com_crashHandlerEnabled", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_crashHandlerEnabled, "Enable crash handler with minidump generation" );

	// Advanced stability cvars
	com_crash_recovery = Cvar_Get( "com_crash_recovery", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_crash_recovery, "Enable automatic crash recovery and restart" );

	com_crash_minidump = Cvar_Get( "com_crash_minidump", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_crash_minidump, "Generate minidump files for crash analysis" );

	com_crash_log_ringbuffer = Cvar_Get( "com_crash_log_ringbuffer", "4096", CVAR_ARCHIVE );
	Cvar_SetDescription( com_crash_log_ringbuffer, "Size of log ring buffer preserved on crash (KB)" );

	com_crash_auto_restart = Cvar_Get( "com_crash_auto_restart", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( com_crash_auto_restart, "Automatically restart engine after fatal crash" );

	com_crash_telemetry = Cvar_Get( "com_crash_telemetry", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( com_crash_telemetry, "Send anonymous crash telemetry (user consent required)" );

	com_auto_save = Cvar_Get( "com_auto_save", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_auto_save, "Enable automatic saving of game state" );

	com_error_recovery = Cvar_Get( "com_error_recovery", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_error_recovery, "Enable error recovery and graceful degradation" );

	com_fallback_renderer = Cvar_Get( "com_fallback_renderer", "opengl", CVAR_ARCHIVE );
	Cvar_SetDescription( com_fallback_renderer, "Fallback renderer when primary renderer fails" );

	com_safe_mode_detect = Cvar_Get( "com_safe_mode_detect", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_safe_mode_detect, "Automatically detect and enter safe mode on errors" );

	com_memory_guard = Cvar_Get( "com_memory_guard", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_memory_guard, "Enable memory corruption detection and guards" );

	com_thread_safety = Cvar_Get( "com_thread_safety", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_thread_safety, "Enable thread safety checks and validation" );

	com_validation_level = Cvar_Get( "com_validation_level", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( com_validation_level, "Data validation level (0=none, 1=basic, 2=full)" );

	fs_modern = Cvar_Get( "fs_modern", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( fs_modern, "Enable modern filesystem features and optimizations" );

	fs_hotReloadEnabled = Cvar_Get( "fs_hotReloadEnabled", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( fs_hotReloadEnabled, "Enable filesystem hot reloading for development" );

	// UI enhancement cvars
	ui_scale = Cvar_Get( "ui_scale", "1.0", CVAR_ARCHIVE );
	Cvar_SetDescription( ui_scale, "UI scale factor (0.5-2.0)" );

	ui_animationSpeed = Cvar_Get( "ui_animationSpeed", "1.0", CVAR_ARCHIVE );
	Cvar_SetDescription( ui_animationSpeed, "UI animation speed multiplier (0.1-5.0)" );

	ui_blur = Cvar_Get( "ui_blur", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( ui_blur, "Enable UI background blur effect" );

	ui_blurRadius = Cvar_Get( "ui_blurRadius", "2", CVAR_ARCHIVE );
	Cvar_SetDescription( ui_blurRadius, "UI blur radius (1-10)" );

	ui_mainMenuGlow = Cvar_Get( "ui_mainMenuGlow", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( ui_mainMenuGlow, "Enable main menu glow effects" );

	ui_mainMenuParticles = Cvar_Get( "ui_mainMenuParticles", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( ui_mainMenuParticles, "Enable main menu particle effects" );

	ui_mainMenuScanlines = Cvar_Get( "ui_mainMenuScanlines", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( ui_mainMenuScanlines, "Enable retro scanline effects on main menu" );

	cl_hideSystemCursor = Cvar_Get( "ui_hideSystemCursor", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_hideSystemCursor, "Hide system cursor when UI is active (0=off, 1=on)" );

	cl_cursorSize = Cvar_Get( "ui_cursorSize", "32", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_cursorSize, "Size of the UI cursor (e.g., 32, 48, 64)" );

// HUD enhancement cvars
cg_hudScale = Cvar_Get( "cg_hudScale", "1.0", CVAR_ARCHIVE );
Cvar_SetDescription( cg_hudScale, "HUD scale factor (0.5-2.0)" );

cg_hudBlur = Cvar_Get( "cg_hudBlur", "0", CVAR_ARCHIVE );
Cvar_SetDescription( cg_hudBlur, "Enable HUD background blur effect" );

cg_hudGlow = Cvar_Get( "cg_hudGlow", "1", CVAR_ARCHIVE );
Cvar_SetDescription( cg_hudGlow, "Enable HUD glow effects" );

// Crosshair enhancements
cg_crosshairScale = Cvar_Get( "cg_crosshairScale", "1.0", CVAR_ARCHIVE );
Cvar_SetDescription( cg_crosshairScale, "Crosshair scale factor (0.5-3.0)" );

cg_crosshairGlow = Cvar_Get( "cg_crosshairGlow", "0", CVAR_ARCHIVE );
Cvar_SetDescription( cg_crosshairGlow, "Enable crosshair glow effect" );

cg_crosshairPulse = Cvar_Get( "cg_crosshairPulse", "0", CVAR_ARCHIVE );
Cvar_SetDescription( cg_crosshairPulse, "Enable crosshair pulse animation" );

// Screen effects
cg_screenDamage = Cvar_Get( "cg_screenDamage", "1", CVAR_ARCHIVE );
Cvar_SetDescription( cg_screenDamage, "Enable screen damage effects" );

cg_screenFlash = Cvar_Get( "cg_screenFlash", "1", CVAR_ARCHIVE );
Cvar_SetDescription( cg_screenFlash, "Enable screen flash effects" );

	cg_screenBlur = Cvar_Get( "cg_screenBlur", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( cg_screenBlur, "Enable screen blur effects (motion/concussion)" );

	// Advanced performance monitoring cvars
	perf_monitor_enable = Cvar_Get( "perf_monitor_enable", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( perf_monitor_enable, "Enable advanced performance monitoring system" );

	perf_gpu_profiler = Cvar_Get( "perf_gpu_profiler", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( perf_gpu_profiler, "Enable GPU performance profiling (Vulkan)" );

	perf_cpu_profiler = Cvar_Get( "perf_cpu_profiler", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( perf_cpu_profiler, "Enable CPU performance profiling" );

	perf_memory_profiler = Cvar_Get( "perf_memory_profiler", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( perf_memory_profiler, "Enable memory allocation profiling" );

	perf_frame_profiler = Cvar_Get( "perf_frame_profiler", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( perf_frame_profiler, "Enable per-frame performance profiling" );

	perf_cache_stats = Cvar_Get( "perf_cache_stats", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( perf_cache_stats, "Enable CPU cache performance statistics" );

	perf_thread_stats = Cvar_Get( "perf_thread_stats", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( perf_thread_stats, "Enable thread utilization statistics" );

	perf_network_stats = Cvar_Get( "perf_network_stats", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( perf_network_stats, "Enable network performance statistics" );

	perf_log_interval = Cvar_Get( "perf_log_interval", "1000", CVAR_ARCHIVE );
	Cvar_SetDescription( perf_log_interval, "Performance logging interval in milliseconds" );

	perf_csv_export = Cvar_Get( "perf_csv_export", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( perf_csv_export, "Export performance data to CSV files" );
	Cvar_SetDescription( com_jobThreads, "Number of worker threads for job system (0 = auto-detect, uses CPU count - 1)" );

	if ( com_dedicated->integer ) {
		if ( !com_viewlog->integer ) {
			Cvar_Set( "viewlog", "1" );
		}
		gw_minimized = qtrue;
	} else {
		gw_minimized = qfalse;
	}

	if ( com_developer->integer ) {
		Cmd_AddCommand( "error", Com_Error_f );
		Cmd_AddCommand( "crash", Com_Crash_f );
		Cmd_AddCommand( "freeze", Com_Freeze_f );
	}

	Cmd_AddCommand( "log_flush", Com_LogFlush_f );
	Cmd_AddCommand( "quit", Com_Quit_f );
	Cmd_AddCommand( "exit", Com_Quit_f ); // Alias for quit
	Cmd_AddCommand( "help", Com_Help_f );
	Cmd_AddCommand( "about", Com_About_f );
	Cmd_AddCommand( "buildinfo", Com_BuildInfo_f );
	Cmd_AddCommand( "perfinfo", Perf_DisplayInfo_f );

	// Register VM hot reload commands
	VM_HotReloadRegisterCommands();

	// Register event system commands
	Cmd_AddCommand( "event_stats", Event_PrintStats );
	Cmd_AddCommand( "changeVectors", MSG_ReportChangeVectors_f );
	Cmd_AddCommand( "writeconfig", Com_WriteConfig_f );
	Cmd_SetCommandCompletionFunc( "writeconfig", Cmd_CompleteWriteCfgName );
	Cmd_AddCommand( "game_restart", Com_GameRestart_f );

	s = va( "%s %s %s", Q3_VERSION, PLATFORM_STRING, Q_BUILD_DATE );
	com_version = Cvar_Get( "version", s, CVAR_PROTECTED | CVAR_ROM | CVAR_SERVERINFO );
	Cvar_SetDescription( com_version, "Read-only CVAR to see the version of the game." );

	// this cvar is the single entry point of the entire extension system
	Cvar_Get( "//trap_GetValue", va( "%i", COM_TRAP_GETVALUE ), CVAR_PROTECTED | CVAR_ROM | CVAR_NOTABCOMPLETE );

	Sys_Init();

	// CPU detection
	Cvar_Get( "sys_cpustring", "detect", CVAR_PROTECTED | CVAR_ROM | CVAR_NORESTART );
	if ( !Q_stricmp( Cvar_VariableString( "sys_cpustring" ), "detect" ) ) {
		char vendor[128];
		Com_Printf( "...detecting CPU, found " );
		Sys_GetProcessorId( vendor );
		Cvar_Set( "sys_cpustring", vendor );
	}
	Com_Printf( "%s\n", Cvar_VariableString( "sys_cpustring" ) );

#ifdef USE_AFFINITY_MASK
	// get initial process affinity - we will respect it when setting custom affinity masks
	eCoreMask = pCoreMask = affinityMask = Sys_GetAffinityMask();
#if (idx64 || id386)
	DetectCPUCoresConfig();
#endif
	if ( com_affinityMask->string[0] != '\0' ) {
		Com_SetAffinityMask( com_affinityMask->string );
		com_affinityMask->modified = qfalse;
	}
#endif

	// Pick a random port value
	Com_RandomBytes( (byte*)&qport, sizeof( qport ) );
	Netchan_Init( qport & 0xffff );

	// Initialize network thread system
	if (!NetThread_Init()) {
		Com_Printf("Failed to initialize network thread system\n");
	}

	// Initialize streaming thread system
	if (!StreamThread_Init()) {
		Com_Printf("Failed to initialize streaming thread system\n");
	}

	// Initialize code review system
	if (!CodeReview_Init()) {
		Com_Printf("Failed to initialize code review system\n");
	}

	// Initialize live code analysis system
	if (!LiveCodeAnalysis_Init()) {
		Com_Printf("Failed to initialize live code analysis system\n");
	}

	// Initialize performance test system
	if (!PerfTest_Init()) {
		Com_Printf("Failed to initialize performance test system\n");
	}

	// Initialize cross-platform test system
	if (!CrossPlatformTest_Init()) {
		Com_Printf("Failed to initialize cross-platform test system\n");
	}

	// Initialize memory safety test system
	if (!MemorySafetyTest_Init()) {
		Com_Printf("Failed to initialize memory safety test system\n");
	}

	// Initialize thread safety test system
	if (!ThreadSafetyTest_Init()) {
		Com_Printf("Failed to initialize thread safety test system\n");
	}

	// Initialize code quality analysis system
	if (!CodeQuality_Init()) {
		Com_Printf("Failed to initialize code quality analysis system\n");
	}

	// Initialize technical debt tracking system
	if (!TechnicalDebt_Init()) {
		Com_Printf("Failed to initialize technical debt tracking system\n");
	}

	// Initialize performance benchmarking system
	if (!Benchmark_Init()) {
		Com_Printf("Failed to initialize performance benchmarking system\n");
	}

	VM_Init();
	SV_Init();

	com_dedicated->modified = qfalse;

#ifndef DEDICATED
	if ( !com_dedicated->integer ) {
		CL_Init();
		// Sys_ShowConsole( com_viewlog->integer, qfalse ); // moved down
	}
#endif

	// add + commands from command line
	if ( !Com_AddStartupCommands() ) {
		// if the user didn't give any commands, run default action
		if ( !com_dedicated->integer ) {
#ifndef DEDICATED
			if ( !com_skipIdLogo || !com_skipIdLogo->integer )
				Cbuf_AddText( "cinematic idlogo.RoQ\n" );
			if( !com_introPlayed->integer ) {
				Cvar_Set( com_introPlayed->name, "1" );
				Cvar_Set( "nextmap", "cinematic intro.RoQ" );
			}
#endif
		}
	}

#ifndef DEDICATED
	CL_StartHunkUsers();
#endif

	// set com_frameTime so that if a map is started on the
	// command line it will still be able to count on com_frameTime
	// being random enough for a serverid
	// lastTime = com_frameTime = Com_Milliseconds();
	Com_FrameInit();

	if ( !com_errorEntered )
		Sys_ShowConsole( com_viewlog->integer, qfalse );

#ifndef DEDICATED
	// make sure single player is off by default
	Cvar_Set( "ui_singlePlayerActive", "0" );
#endif

	com_fullyInitialized = qtrue;

	Com_Printf( "--- Common Initialization Complete ---\n" );

	Com_InitBotLib();

#ifdef USE_CJSON
	JSON_Init();
#endif

#ifdef USE_ZSTD
	ZSTD_Init();
#endif

#ifdef USE_ENET
	NET_ENet_Init();
#endif

#ifdef USE_SQLITE
	SQLite_Init();
#endif

#ifdef USE_OPENSSL
	OpenSSL_Init();
#endif

#ifdef USE_LUA
	Lua_Init();
#endif

#ifdef USE_AIML
	AIML_Init();
#endif

	// Initialize job system for multi-threading
#ifdef USE_JOBSYSTEM
	{
		int num_threads = com_jobThreads ? com_jobThreads->integer : 0;
		if (JobSystem_Init(num_threads)) {
			Com_Printf("Job system initialized\n");
		} else {
			Com_Printf("Warning: Job system initialization failed\n");
		}
	}
#endif

	I18n_Init();

#ifdef USE_FREETYPE
	FreeType_Init();
#endif

	NET_Init();

	Com_Printf( "Working directory: %s\n", Sys_Pwd() );
}


//==================================================================

static void Com_WriteConfigToFile( const char *filename ) {
	fileHandle_t	f;

	f = FS_FOpenFileWrite( filename );
	if ( f == FS_INVALID_HANDLE ) {
		if ( !FS_ResetReadOnlyAttribute( filename ) || ( f = FS_FOpenFileWrite( filename ) ) == FS_INVALID_HANDLE ) {
			Com_Printf( "Couldn't write %s.\n", filename );
			return;
		}
	}

	FS_Printf( f, "// generated by quake, do not modify" Q_NEWLINE );
#ifndef DEDICATED
	Key_WriteBindings( f );
#endif
	Cvar_WriteVariables( f );
	FS_FCloseFile( f );
}


/*
===============
Com_WriteConfiguration

Writes key bindings and archived cvars to config file if modified
===============
*/
void Com_WriteConfiguration( void ) {
#ifndef DEDICATED
	const char *basegame;
	const char *gamedir;
#endif
	// if we are quitting without fully initializing, make sure
	// we don't write out anything
	if ( !com_fullyInitialized ) {
		return;
	}

	if ( !(cvar_modifiedFlags & CVAR_ARCHIVE ) ) {
		return;
	}
	cvar_modifiedFlags &= ~CVAR_ARCHIVE;

	Com_WriteConfigToFile( Q3CONFIG_CFG );

#ifndef DEDICATED
	gamedir = FS_GetCurrentGameDir();
	basegame = FS_GetBaseGameDir();
	if ( UI_usesUniqueCDKey() && gamedir[0] && Q_stricmp( basegame, gamedir ) ) {
		Com_WriteCDKey( gamedir, &cl_cdkey[16] );
	} else {
		Com_WriteCDKey( basegame, cl_cdkey );
	}
#endif
}


/*
===============
Com_WriteConfig_f

Write the config file to a specific name
===============
*/
static void Com_WriteConfig_f( void ) {
	char	filename[MAX_QPATH];
	const char *ext;

	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "Usage: writeconfig <filename>\n" );
		return;
	}

	Q_strncpyz( filename, Cmd_Argv(1), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".cfg" );

	if ( !FS_AllowedExtension( filename, qfalse, &ext ) ) {
		Com_Printf( "%s: Invalid filename extension: '%s'.\n", __func__, ext );
		return;
	}

	Com_Printf( "Writing %s.\n", filename );
	Com_WriteConfigToFile( filename );
}


/*
================
Com_ModifyMsec
================
*/
static int Com_ModifyMsec( int msec ) {
	int		clampTime;

	//
	// modify time for debugging values
	//
	if ( com_fixedtime->integer ) {
		msec = com_fixedtime->integer;
	} else if ( com_timescale->value ) {
		msec *= com_timescale->value;
	} else if (com_cameraMode->integer) {
		msec *= com_timescale->value;
	}

	// don't let it scale below 1 msec
	if ( msec < 1 && com_timescale->value) {
		msec = 1;
	}

	if ( com_dedicated->integer ) {
		// dedicated servers don't want to clamp for a much longer
		// period, because it would mess up all the client's views
		// of time.
		if (com_sv_running->integer && msec > 500)
			Com_Printf( "Hitch warning: %i msec frame time\n", msec );

		clampTime = 5000;
	} else
	if ( !com_sv_running->integer ) {
		// clients of remote servers do not want to clamp time, because
		// it would skew their view of the server's time temporarily
		clampTime = 5000;
	} else {
		// for local single player gaming
		// we may want to clamp the time to prevent players from
		// flying off edges when something hitches.
		clampTime = 200;
	}

	if ( msec > clampTime ) {
		msec = clampTime;
	}

	return msec;
}


/*
=================
Com_TimeVal
=================
*/
static int Com_TimeVal( int minMsec )
{
	int timeVal;

	timeVal = Com_Milliseconds() - com_frameTime;

	if ( timeVal >= minMsec )
		timeVal = 0;
	else
		timeVal = minMsec - timeVal;

	return timeVal;
}

/*
=================
Com_FrameInit
=================
*/
void Com_FrameInit( void )
{
	lastTime = com_frameTime = Com_Milliseconds();
}

/*
=================
Com_Frame
=================
*/
void Com_Frame( qboolean noDelay ) {

#ifndef DEDICATED
	static int bias = 0;
#endif
	int	msec, realMsec, minMsec;
	int	sleepMsec;
	int	timeVal;
	int	timeValSV;

	int	timeBeforeFirstEvents;
	int	timeBeforeServer;
	int	timeBeforeEvents;
	int	timeBeforeClient;
	int	timeAfter;
	TracyCZoneCtx prof_frame;

	if ( Q_setjmp( abortframe ) ) {
		return;			// an ERR_DROP was thrown
	}

	PROF_FRAME_MARK();
	PROF_ZONE_BEGIN(prof_frame, "Com_Frame");

	// Publish frame start event
	{
		event_t *evt = Event_Create(EVENT_TYPE_ENGINE_FRAME_START, EVENT_CATEGORY_ENGINE, 0);
		if (evt) {
			Event_Publish(evt);
		}
	}

	minMsec = 0; // silent compiler warning

	// bk001204 - init to zero.
	//  also:  might be clobbered by `longjmp' or `vfork'
	timeBeforeFirstEvents = 0;
	timeBeforeServer = 0;
	timeBeforeEvents = 0;
	timeBeforeClient = 0;
	timeAfter = 0;

	// write config file if anything changed
#ifndef DELAY_WRITECONFIG
	Com_WriteConfiguration();
#endif

	// if "viewlog" has been modified, show or hide the log console
	if ( com_viewlog->modified ) {
		if ( !com_dedicated->integer ) {
			Sys_ShowConsole( com_viewlog->integer, qfalse );
		}
		com_viewlog->modified = qfalse;
	}

#ifdef USE_AFFINITY_MASK
	if ( com_affinityMask->modified ) {
		Com_SetAffinityMask( com_affinityMask->string );
		com_affinityMask->modified = qfalse;
	}
#endif

	//
	// main event loop
	//
	TracyCZoneCtx prof_events;
	PROF_ZONE_BEGIN(prof_events, "Event Processing");
	if ( com_speeds->integer ) {
		timeBeforeFirstEvents = Sys_Milliseconds();
	}

	// we may want to spin here if things are going too fast
	if ( com_dedicated->integer ) {
		minMsec = SV_FrameMsec();
#ifndef DEDICATED
		bias = 0;
#endif
	} else {
#ifndef DEDICATED
		if ( noDelay ) {
			minMsec = 0;
			bias = 0;
		} else {
			if ( !gw_active && com_maxfpsUnfocused->integer > 0 )
				minMsec = 1000 / com_maxfpsUnfocused->integer;
			else
			if ( com_maxfps->integer > 0 )
				minMsec = 1000 / com_maxfps->integer;
			else
				minMsec = 1;

			timeVal = com_frameTime - lastTime;
			bias += timeVal - minMsec;

			if ( bias > minMsec )
				bias = minMsec;

			// Adjust minMsec if previous frame took too long to render so
			// that framerate is stable at the requested value.
			minMsec -= bias;
		}
#endif
	}

	// waiting for incoming packets
	if ( noDelay == qfalse )
	do {
		if ( com_sv_running->integer ) {
			timeValSV = SV_SendQueuedPackets();
			timeVal = Com_TimeVal( minMsec );
			if ( timeValSV < timeVal )
				timeVal = timeValSV;
		} else {
			timeVal = Com_TimeVal( minMsec );
		}
		sleepMsec = timeVal;
#ifndef DEDICATED
		if ( !gw_minimized && timeVal > com_yieldCPU->integer )
			sleepMsec = com_yieldCPU->integer;
		if ( timeVal > sleepMsec )
			Com_EventLoop();
#endif
		NET_Sleep( sleepMsec * 1000 - 500 );
	} while( Com_TimeVal( minMsec ) );

	lastTime = com_frameTime;
	com_frameTime = Com_EventLoop();
	realMsec = com_frameTime - lastTime;

	Cbuf_Execute();

	// mess with msec if needed
	msec = Com_ModifyMsec( realMsec );
	PROF_ZONE_END(prof_events);

	//
	// server side
	//
	TracyCZoneCtx prof_server;
	PROF_ZONE_BEGIN(prof_server, "Server Frame");
	if ( com_speeds->integer ) {
		timeBeforeServer = Sys_Milliseconds();
	}

	SV_Frame( msec );
	PROF_ZONE_END(prof_server);

#ifdef USE_LUA
	// Update Lua event bus (process queued events)
	Lua_Events_Update();
	
	// Update coroutine scheduler
	{
		float deltaTime = msec / 1000.0f;
		Lua_Coroutine_Update(deltaTime);
		
	// Update encounter system
	Lua_Encounter_Update();
#endif

	// Update stability framework
	Stability_Frame();

#ifdef USE_CURL
	// Update telemetry system
	Telemetry_Update();
#endif

	// if "dedicated" has been modified, start up
	// or shut down the client system.
	// Do this after the server may have started,
	// but before the client tries to auto-connect
	if ( com_dedicated->modified ) {
		// get the latched value
		Cvar_Get( "dedicated", "0", 0 );
		com_dedicated->modified = qfalse;
		if ( !com_dedicated->integer ) {
			SV_Shutdown( "dedicated set to 0" );
			SV_RemoveDedicatedCommands();
#ifndef DEDICATED
			CL_Init();
#endif
			Sys_ShowConsole( com_viewlog->integer, qfalse );
#ifndef DEDICATED
			gw_minimized = qfalse;
			CL_StartHunkUsers();
#endif
		} else {
#ifndef DEDICATED
			CL_Shutdown( "", qfalse );
			CL_ClearMemory();
#endif
			Sys_ShowConsole( 1, qtrue );
			SV_AddDedicatedCommands();
			gw_minimized = qtrue;
		}
	}

#ifdef DEDICATED
	if ( com_speeds->integer ) {
		timeAfter = Sys_Milliseconds ();
		timeBeforeEvents = timeAfter;
		timeBeforeClient = timeAfter;
	}
#else
	//
	// client system
	//
	if ( !com_dedicated->integer ) {
		//
		// run event loop a second time to get server to client packets
		// without a frame of latency
		//
		if ( com_speeds->integer ) {
			timeBeforeEvents = Sys_Milliseconds();
		}
		Com_EventLoop();

		Cbuf_Execute();

		//
		// client side
		//
		TracyCZoneCtx prof_client;
		PROF_ZONE_BEGIN(prof_client, "Client Frame");
		if ( com_speeds->integer ) {
			timeBeforeClient = Sys_Milliseconds();
		}

		CL_Frame( msec, realMsec );

		if ( com_speeds->integer ) {
			timeAfter = Sys_Milliseconds();
		}
		PROF_ZONE_END(prof_client);
	}
#endif

	NET_FlushPacketQueue( 0 );

	// Update job system
#ifdef USE_JOBSYSTEM
	JobSystem_Update();
#endif

	Cbuf_Wait();

	//
	// report timing information
	//
	if ( com_speeds->integer ) {
		int			all, sv, ev, cl;

		all = timeAfter - timeBeforeServer;
		sv = timeBeforeEvents - timeBeforeServer;
		ev = timeBeforeServer - timeBeforeFirstEvents + timeBeforeClient - timeBeforeEvents;
		cl = timeAfter - timeBeforeClient;
		sv -= time_game;
		cl -= time_frontend + time_backend;

		Com_Printf ("frame:%i all:%3i sv:%3i ev:%3i cl:%3i gm:%3i rf:%3i bk:%3i\n",
					 com_frameNumber, all, sv, ev, cl, time_game, time_frontend, time_backend );
	}

	//
	// trace optimization tracking
	//
	if ( com_showtrace->integer ) {

		extern	int c_traces, c_brush_traces, c_patch_traces;
		extern	int	c_pointcontents;

		Com_Printf ("%4i traces  (%ib %ip) %4i points\n", c_traces,
			c_brush_traces, c_patch_traces, c_pointcontents);
		c_traces = 0;
		c_brush_traces = 0;
		c_patch_traces = 0;
		c_pointcontents = 0;
	}

	com_frameNumber++;

	// Update performance counters
	Perf_Frame(msec);

	// Check for VM hot reload
	VM_CheckHotReload();

	// Publish frame end event
	{
		event_t *evt = Event_Create(EVENT_TYPE_ENGINE_FRAME_END, EVENT_CATEGORY_ENGINE, 0);
		if (evt) {
			Event_Publish(evt);
		}
	}

	// Process all event phases (immediate, deferred, scheduled)
	Event_ProcessAll();

	PROF_ZONE_END(prof_frame);
}


/*
=================
Com_Shutdown
=================
*/
static void Com_Shutdown( void ) {
	// Shutdown memory tracking system
	Q_MemTrack_Shutdown();
	
	// Shutdown structured logging system
	Q_Log_Shutdown();
	
#ifdef USE_CURL
	// Shutdown telemetry system
	Telemetry_Shutdown();
#endif
	
	if ( logfile != FS_INVALID_HANDLE ) {
		FS_FCloseFile( logfile );
		logfile = FS_INVALID_HANDLE;
	}

	if ( com_journalFile != FS_INVALID_HANDLE ) {
		FS_FCloseFile( com_journalFile );
		com_journalFile = FS_INVALID_HANDLE;
	}

	if ( com_journalDataFile != FS_INVALID_HANDLE ) {
		FS_FCloseFile( com_journalDataFile );
		com_journalDataFile = FS_INVALID_HANDLE;
	}

#ifdef USE_LUA
	Lua_Shutdown();
#endif

#ifdef USE_AIML
	AIML_Shutdown();
#endif

	// Shutdown job system
#ifdef USE_JOBSYSTEM
	JobSystem_Shutdown();
#endif

	I18n_Shutdown();

#ifdef USE_OPENSSL
	OpenSSL_Shutdown();
#endif

#ifdef USE_FREETYPE
	FreeType_Shutdown();
#endif

	// Shutdown network thread system
	NetThread_Shutdown();

	// Shutdown streaming thread system
	StreamThread_Shutdown();

	// Shutdown code review system
	CodeReview_Shutdown();

	// Shutdown live code analysis system
	LiveCodeAnalysis_Shutdown();

	// Shutdown performance test system
	PerfTest_Shutdown();

	// Shutdown cross-platform test system
	CrossPlatformTest_Shutdown();

	// Shutdown memory safety test system
	MemorySafetyTest_Shutdown();

	// Shutdown thread safety test system
	ThreadSafetyTest_Shutdown();

	// Shutdown code quality analysis system
	CodeQuality_Shutdown();

	// Shutdown technical debt tracking system
	TechnicalDebt_Shutdown();

	// Shutdown performance benchmarking system
	Benchmark_Shutdown();
}

//------------------------------------------------------------------------


/*
===========================================
command line completion
===========================================
*/

/*
==================
Field_Clear
==================
*/
void Field_Clear( field_t *edit ) {
	memset( edit->buffer, 0, sizeof( edit->buffer ) );
	edit->cursor = 0;
	edit->scroll = 0;
}

static const char *completionString;
static char shortestMatch[MAX_TOKEN_CHARS];
static int	matchCount;
// field we are working on, passed to Field_AutoComplete(&g_consoleCommand for instance)
static field_t *completionField;

/*
===============
FindMatches
===============
*/
static void FindMatches( const char *s ) {
	int		i, n;

	if ( Q_stricmpn( s, completionString, strlen( completionString ) ) ) {
		return;
	}
	matchCount++;
	if ( matchCount == 1 ) {
		Q_strncpyz( shortestMatch, s, sizeof( shortestMatch ) );
		return;
	}

	n = (int)strlen(s);
	// cut shortestMatch to the amount common with s
	for ( i = 0 ; shortestMatch[i] ; i++ ) {
		if ( i >= n ) {
			shortestMatch[i] = '\0';
			break;
		}

		if ( tolower(shortestMatch[i]) != tolower(s[i]) ) {
			shortestMatch[i] = '\0';
		}
	}
}


/*
===============
PrintMatches
===============
*/
static void PrintMatches( const char *s ) {
	if ( !Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) ) {
		Com_Printf( "    %s\n", s );
	}
}


/*
===============
PrintCvarMatches
===============
*/
static void PrintCvarMatches( const char *s ) {
	char value[ TRUNCATE_LENGTH ];

	if ( !Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) ) {
		Com_TruncateLongString( value, Cvar_VariableString( s ) );
		Com_Printf( "    %s = \"%s\"\n", s, value );
	}
}


/*
===============
Field_FindFirstSeparator
===============
*/
static const char *Field_FindFirstSeparator( const char *s )
{
	char c;
	while ( (c = *s) != '\0' ) {
		if ( c == ';' )
			return s;
		s++;
	}
	return nullptr;
}


/*
===============
Field_AddSpace
===============
*/
static void Field_AddSpace( void )
{
	size_t len = strlen( completionField->buffer );
	if ( len && len < sizeof( completionField->buffer ) - 1 && completionField->buffer[ len - 1 ] != ' ' )
	{
		memcpy( completionField->buffer + len, " ", 2 );
		completionField->cursor = (int)(len + 1);
	}
}


/*
===============
Field_Complete
===============
*/
static qboolean Field_Complete( void )
{
	int completionOffset;

	if( matchCount == 0 )
		return qtrue;

	completionOffset = strlen( completionField->buffer ) - strlen( completionString );

	Q_strncpyz( &completionField->buffer[ completionOffset ], shortestMatch,
		sizeof( completionField->buffer ) - completionOffset );

	completionField->cursor = strlen( completionField->buffer );

	if( matchCount == 1 )
	{
		Field_AddSpace();
		return qtrue;
	}

	Com_Printf( "]%s\n", completionField->buffer );

	return qfalse;
}


/*
===============
Field_CompleteKeyname
===============
*/
void Field_CompleteKeyname( void )
{
	matchCount = 0;
	shortestMatch[ 0 ] = '\0';

	Key_KeynameCompletion( FindMatches );

	if ( !Field_Complete() )
		Key_KeynameCompletion( PrintMatches );
}


/*
===============
Field_CompleteKeyBind
===============
*/
void Field_CompleteKeyBind( int key )
{
	const char *value;
	int vlen;
	int blen;

	value = Key_GetBinding( key );
	if ( value == NULL || *value == '\0' )
		return;

	blen = (int)strlen( completionField->buffer );
	vlen = (int)strlen( value );

	if ( Field_FindFirstSeparator( (char*)value ) )
	{
		value = va( "\"%s\"", value );
		vlen += 2;
	}

	if ( (size_t)( vlen + blen ) > sizeof( completionField->buffer ) - 1 )
	{
		//vlen = sizeof( completionField->buffer ) - 1 - blen;
		return;
	}

	memcpy( completionField->buffer + blen, value, vlen + 1 );
	completionField->cursor = blen + vlen;

	Field_AddSpace();
}


static void Field_CompleteCvarValue( const char *value, const char *current )
{
	int vlen;
	int blen;

	if ( *value == '\0' )
		return;

	blen = (int)strlen( completionField->buffer );
	vlen = (int)strlen( value );

	if ( *current != '\0' )
	{
#if 0
		int clen = (int) strlen( current );
		if ( strncmp( value, current, clen ) == 0 ) // current value is a substring of new value
		{
			value += clen;
			vlen -= clen;
		}
		else // modification, nothing to complete
#endif
		{
			return;
		}
	}

	if ( Field_FindFirstSeparator( (char*)value ) )
	{
		value = va( "\"%s\"", value );
		vlen += 2;
	}

	if ( (size_t)( vlen + blen ) > sizeof( completionField->buffer ) - 1 )
	{
		//vlen = sizeof( completionField->buffer ) - 1 - blen;
		return;
	}

	if ( blen > 1 )
	{
		if ( completionField->buffer[ blen-1 ] == '"' && completionField->buffer[ blen-2 ] == ' ' )
		{
			completionField->buffer[ blen-- ] = '\0'; // strip starting quote
		}
	}

	memcpy( completionField->buffer + blen, value, vlen + 1 );
	completionField->cursor = vlen + blen;

	Field_AddSpace();
}


/*
===============
Field_CompleteFilename
===============
*/
void Field_CompleteFilename( const char *dir, const char *ext, qboolean stripExt, int flags )
{
	matchCount = 0;
	shortestMatch[ 0 ] = '\0';

	FS_FilenameCompletion( dir, ext, stripExt, FindMatches, flags );

	if ( !Field_Complete() )
		FS_FilenameCompletion( dir, ext, stripExt, PrintMatches, flags );
}


/*
===============
Field_CompleteCommand
===============
*/
void Field_CompleteCommand( const char *cmd, qboolean doCommands, qboolean doCvars )
{
	int	completionArgument;

	// Skip leading whitespace and quotes
	cmd = Com_SkipCharset( cmd, " \"" );

	Cmd_TokenizeStringIgnoreQuotes( cmd );
	completionArgument = Cmd_Argc();

	// If there is trailing whitespace on the cmd
	if( *( cmd + strlen( cmd ) - 1 ) == ' ' )
	{
		completionString = "";
		completionArgument++;
	}
	else
		completionString = Cmd_Argv( completionArgument - 1 );

#ifndef DEDICATED
	// Unconditionally add a '\' to the start of the buffer
	if ( completionField->buffer[ 0 ] && completionField->buffer[ 0 ] != '\\' )
	{
		if( completionField->buffer[ 0 ] != '/' )
		{
			// Buffer is full, refuse to complete
			if ( strlen( completionField->buffer ) + 1 >= sizeof( completionField->buffer ) )
				return;

			memmove( &completionField->buffer[ 1 ],
				&completionField->buffer[ 0 ],
				strlen( completionField->buffer ) + 1 );
			completionField->cursor++;
		}

		completionField->buffer[ 0 ] = '\\';
	}
#endif

	if ( completionArgument > 1 )
	{
		const char *baseCmd = Cmd_Argv( 0 );
		const char *p;

#ifndef DEDICATED
			// This should always be true
			if ( baseCmd[ 0 ] == '\\' || baseCmd[ 0 ] == '/' )
				baseCmd++;
#endif

		if( ( p = Field_FindFirstSeparator( cmd ) ) != NULL )
		{
 			Field_CompleteCommand( p + 1, qtrue, qtrue ); // Compound command
		}
		else
		{
			qboolean argumentCompleted = Cmd_CompleteArgument( baseCmd, cmd, completionArgument );
			if ( ( matchCount == 1 || argumentCompleted ) && doCvars )
			{
				if ( cmd[0] == '/' || cmd[0] == '\\' )
					cmd++;
				Cmd_TokenizeString( cmd );
				Field_CompleteCvarValue( Cvar_VariableString( Cmd_Argv( 0 ) ), Cmd_Argv( 1 ) );
			}
		}
	}
	else
	{
		if ( completionString[0] == '\\' || completionString[0] == '/' )
			completionString++;

		matchCount = 0;
		shortestMatch[ 0 ] = '\0';

		if ( completionString[0] == '\0' ) {
			return;
		}

		if ( doCommands )
			Cmd_CommandCompletion( FindMatches );

		if ( doCvars )
			Cvar_CommandCompletion( FindMatches );

		if ( !Field_Complete() )
		{
			// run through again, printing matches
			if ( doCommands )
				Cmd_CommandCompletion( PrintMatches );

			if ( doCvars )
				Cvar_CommandCompletion( PrintCvarMatches );
		}
	}
}


/*
===============
Field_AutoComplete

Perform Tab expansion
===============
*/
void Field_AutoComplete( field_t *field )
{
	completionField = field;

	Field_CompleteCommand( completionField->buffer, qtrue, qtrue );
}


/*
==================
Com_RandomBytes

fills string array with len random bytes, preferably from the OS randomizer
==================
*/
void Com_RandomBytes( byte *string, int len )
{
	int i;

	if ( Sys_RandomBytes( string, len ) )
		return;

	Com_Printf( S_COLOR_YELLOW "Com_RandomBytes: using weak randomization\n" );
	srand( time( NULL ) );
	for( i = 0; i < len; i++ )
		string[i] = (unsigned char)( rand() % 256 );
}


#if 0
static qboolean strgtr(const char *s0, const char *s1) {
	int l0, l1, i;

	l0 = strlen( s0 );
	l1 = strlen( s1 );

	if ( l1 < l0 ) {
		l0 = l1;
	}

	for( i = 0; i < l0; i++ ) {
		if ( s1[i] > s0[i] ) {
			return qtrue;
		}
		if ( s1[i] < s0[i] ) {
			return qfalse;
		}
	}
	return qfalse;
}
#endif


/*
==================
Com_SortList
==================
*/
void Com_SortList( char **list, int n )
{
	const char *m;
	char *temp;
	int i, j;
	i = 0;
	j = n;
	m = list[ n >> 1 ];
	do
	{
		while ( strcmp( list[i], m ) < 0 ) i++;
		while ( strcmp( list[j], m ) > 0 ) j--;
		if ( i <= j )
		{
			temp = list[i];
			list[i] = list[j];
			list[j] = temp;
			i++;
			j--;
		}
	}
	while ( i <= j );
	if ( j > 0 ) Com_SortList( list, j );
	if ( n > i ) Com_SortList( list+i, n-i );
}


/*
==================
Com_SortFileList
==================
*/
#if 0
void Com_SortFileList( char **list, int nfiles, int fastSort )
{
	if ( nfiles > 1 && fastSort )
	{
		Com_SortList( list, nfiles-1 );
	}
	else // defrag mod demo UI can't handle _properly_ sorted directories
	{
		int i, flag;
		do {
			flag = 0;
			for( i = 1; i < nfiles; i++ ) {
				if ( strgtr( list[i-1], list[i] ) ) {
					char *temp = list[i];
					list[i] = list[i-1];
					list[i-1] = temp;
					flag = 1;
				}
			}
		} while( flag );
	}
}
#endif

/*
===============================================================================

SECURITY FUNCTIONS

===============================================================================
*/

/*
============
Q_ValidateFilePath

Validates a file path to prevent directory traversal attacks.
Returns qtrue if the path is safe, qfalse if it contains traversal attempts.
============
*/
qboolean Q_ValidateFilePath(const char *path) {
	const char *p;
	qboolean hasDotDot = qfalse;

	if (!path || !*path) {
		return qfalse;
	}

	// Check length to prevent extremely long paths
	if (strlen(path) >= MAX_QPATH) {
		return qfalse;
	}

	// Check for directory traversal patterns and other security issues
	for (p = path; *p; p++) {
		// Check for "../" or "..\" patterns (directory traversal)
		if (p[0] == '.' && p[1] == '.' &&
		    (p[2] == '/' || p[2] == '\\' || p[2] == '\0')) {
			hasDotDot = qtrue;
			continue; // Allow but track - we'll decide based on context
		}

		// Check for absolute Unix paths
		if (p == path && *p == '/') {
			// Reject absolute system paths, but allow game-relative paths
			if (strlen(path) > 1 && path[1] != '/') {
				return qfalse; // Absolute path
			}
		}

		// Check for Windows absolute paths (C:\, D:\, etc.)
		if (p == path && ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) &&
		    p[1] == ':' && (p[2] == '/' || p[2] == '\\' || p[2] == '\0')) {
			return qfalse;
		}

		// Check for UNC paths (\\server\share)
		if (p == path && p[0] == '\\' && p[1] == '\\') {
			return qfalse;
		}

		// Check for control characters (security risk)
		if (*p < 32 && *p != '\t' && *p != '\n' && *p != '\r') {
			return qfalse;
		}

		// Check for potentially dangerous characters
		// Note: we allow '*' at the very beginning for BSP submodels (*1, *2, etc.)
		if (strchr("*?\"<>|", *p)) {
			if (!(*p == '*' && p == path && isdigit(p[1]))) {
				return qfalse;
			}
		}
	}

	// Allow .. in specific safe contexts (like shader paths), but generally block
	if (hasDotDot) {
		// Only allow .. in paths that are clearly game-controlled (like shaders/)
		if (!Q_stristr(path, "scripts/") &&
		    !Q_stristr(path, "models/") &&
		    !Q_stristr(path, "sound/") &&
		    !Q_stristr(path, "textures/")) {
			return qfalse;
		}
	}

	return qtrue;
}

/*
============
Q_SanitizeFilePath

Sanitizes a file path by removing dangerous elements and ensuring it's safe.
Returns a sanitized version of the path, or NULL if the path is too dangerous.
============
*/
const char *Q_SanitizeFilePath(const char *path, char *output, int outputSize) {
	const char *src;
	char *dst;
	int len = 0;

	if (!path || !output || outputSize <= 0) {
		return NULL;
	}

	// Validate the path first
	if (!Q_ValidateFilePath(path)) {
		return NULL;
	}

	src = path;
	dst = output;

	// Copy path while sanitizing
	while (*src && len < outputSize - 1) {
		// Skip dangerous characters
		if (*src == '<' || *src == '>' || *src == '|' || *src == '"' ||
		    *src == '*' || *src == '?') {
			src++;
			continue;
		}

		// Convert backslashes to forward slashes for consistency
		if (*src == '\\') {
			*dst++ = '/';
			len++;
		} else {
			*dst++ = *src++;
			len++;
		}
	}

	*dst = '\0';

	// Final validation
	if (!Q_ValidateFilePath(output)) {
		return NULL;
	}

	return output;
}

/*
=================
Com_NetThreads_f
=================
*/
static void Com_NetThreads_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: netthreads <command> [args]\n" );
		Com_Printf( "Commands:\n" );
		Com_Printf( "  status          - Show current thread status\n" );
		Com_Printf( "  enable <type>   - Enable dedicated network thread\n" );
		Com_Printf( "  disable <type>  - Disable dedicated network thread\n" );
		Com_Printf( "  stats [type]    - Show thread statistics\n" );
		Com_Printf( "  flush           - Flush all queued messages\n" );
		Com_Printf( "\nThread types: send, recv, reliable, fragment, encrypt, compress\n" );
		return;
	}

	const char* cmd = Cmd_Argv(1);

	if ( Q_stricmp( cmd, "status" ) == 0 ) {
		Com_Printf( "=== Network Threads Status ===\n" );
		Com_Printf( "Network Threading: %s\n", net_thread_system.enabled ? "Enabled" : "Disabled" );

		const char* threadNames[NET_THREAD_MAX] = {
			"Send", "Receive", "Reliable", "Fragment", "Encrypt", "Compress"
		};

		for ( int i = 0; i < NET_THREAD_MAX; i++ ) {
			Com_Printf( "  %s Thread: %s\n", threadNames[i],
				NetThread_IsThreadEnabled( (net_thread_type_t)i ) ? "Enabled" : "Disabled" );
		}

		uint64_t sent, received, sentBytes, recvBytes, dropped;
		NetThread_GetGlobalStats(&sent, &received, &sentBytes, &recvBytes, &dropped);
		Com_Printf( "\nGlobal Statistics:\n" );
		Com_Printf( "  Messages Sent: %llu\n", (unsigned long long)sent );
		Com_Printf( "  Messages Received: %llu\n", (unsigned long long)received );
		Com_Printf( "  Bytes Sent: %llu\n", (unsigned long long)sentBytes );
		Com_Printf( "  Bytes Received: %llu\n", (unsigned long long)recvBytes );
		Com_Printf( "  Dropped Messages: %llu\n", (unsigned long long)dropped );
	}
	else if ( Q_stricmp( cmd, "enable" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: netthreads enable <type>\n" );
			return;
		}

		const char* typeStr = Cmd_Argv(2);
		net_thread_type_t threadType = NET_THREAD_MAX;

		if ( Q_stricmp( typeStr, "send" ) == 0 ) threadType = NET_THREAD_SEND;
		else if ( Q_stricmp( typeStr, "recv" ) == 0 ) threadType = NET_THREAD_RECV;
		else if ( Q_stricmp( typeStr, "reliable" ) == 0 ) threadType = NET_THREAD_RELIABLE;
		else if ( Q_stricmp( typeStr, "fragment" ) == 0 ) threadType = NET_THREAD_FRAGMENT;
		else if ( Q_stricmp( typeStr, "encrypt" ) == 0 ) threadType = NET_THREAD_ENCRYPT;
		else if ( Q_stricmp( typeStr, "compress" ) == 0 ) threadType = NET_THREAD_COMPRESS;

		if ( threadType == NET_THREAD_MAX ) {
			Com_Printf( "Invalid thread type: %s\n", typeStr );
			return;
		}

		if ( NetThread_EnableThread( threadType ) ) {
			Com_Printf( "Enabled %s network thread\n", typeStr );
		} else {
			Com_Printf( "Failed to enable %s network thread\n", typeStr );
		}
	}
	else if ( Q_stricmp( cmd, "disable" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: netthreads disable <type>\n" );
			return;
		}

		const char* typeStr = Cmd_Argv(2);
		net_thread_type_t threadType = NET_THREAD_MAX;

		if ( Q_stricmp( typeStr, "send" ) == 0 ) threadType = NET_THREAD_SEND;
		else if ( Q_stricmp( typeStr, "recv" ) == 0 ) threadType = NET_THREAD_RECV;
		else if ( Q_stricmp( typeStr, "reliable" ) == 0 ) threadType = NET_THREAD_RELIABLE;
		else if ( Q_stricmp( typeStr, "fragment" ) == 0 ) threadType = NET_THREAD_FRAGMENT;
		else if ( Q_stricmp( typeStr, "encrypt" ) == 0 ) threadType = NET_THREAD_ENCRYPT;
		else if ( Q_stricmp( typeStr, "compress" ) == 0 ) threadType = NET_THREAD_COMPRESS;

		if ( threadType == NET_THREAD_MAX ) {
			Com_Printf( "Invalid thread type: %s\n", typeStr );
			return;
		}

		NetThread_DisableThread( threadType );
		Com_Printf( "Disabled %s network thread\n", typeStr );
	}
	else if ( Q_stricmp( cmd, "stats" ) == 0 ) {
		if ( Cmd_Argc() >= 3 ) {
			const char* typeStr = Cmd_Argv(2);
			net_thread_type_t threadType = NET_THREAD_MAX;

			if ( Q_stricmp( typeStr, "send" ) == 0 ) threadType = NET_THREAD_SEND;
			else if ( Q_stricmp( typeStr, "recv" ) == 0 ) threadType = NET_THREAD_RECV;
			else if ( Q_stricmp( typeStr, "reliable" ) == 0 ) threadType = NET_THREAD_RELIABLE;
			else if ( Q_stricmp( typeStr, "fragment" ) == 0 ) threadType = NET_THREAD_FRAGMENT;
			else if ( Q_stricmp( typeStr, "encrypt" ) == 0 ) threadType = NET_THREAD_ENCRYPT;
			else if ( Q_stricmp( typeStr, "compress" ) == 0 ) threadType = NET_THREAD_COMPRESS;

			if ( threadType != NET_THREAD_MAX ) {
				uint64_t processedItems = 0;
				float avgTimeMs = 0.0f;
				uint64_t totalTimeNs = 0;

				NetThread_GetStats( threadType, &processedItems, &avgTimeMs, &totalTimeNs );
				Com_Printf( "%s Thread Stats:\n", typeStr );
				Com_Printf( "  Processed Items: %llu\n", (unsigned long long)processedItems );
				Com_Printf( "  Average Time: %.2f ms\n", avgTimeMs );
				Com_Printf( "  Total Time: %.2f ms\n", totalTimeNs / 1000000.0 );
			} else {
				Com_Printf( "Invalid thread type: %s\n", typeStr );
			}
		} else {
			Com_Printf( "=== All Network Thread Statistics ===\n" );
			const char* threadNames[NET_THREAD_MAX] = {
				"Send", "Receive", "Reliable", "Fragment", "Encrypt", "Compress"
			};

			for ( int i = 0; i < NET_THREAD_MAX; i++ ) {
				if ( NetThread_IsThreadEnabled( (net_thread_type_t)i ) ) {
					uint64_t processedItems = 0;
					float avgTimeMs = 0.0f;
					uint64_t totalTimeNs = 0;

					NetThread_GetStats( (net_thread_type_t)i, &processedItems, &avgTimeMs, &totalTimeNs );
					Com_Printf( "%s: %llu items, %.2f ms avg\n", threadNames[i],
						(unsigned long long)processedItems, avgTimeMs );
				}
			}
		}
	}
	else if ( Q_stricmp( cmd, "flush" ) == 0 ) {
		NetThread_FlushQueues();
		Com_Printf( "Flushed all network queues\n" );
	}
	else {
		Com_Printf( "Unknown command: %s\n", cmd );
		Com_Printf( "Use 'netthreads' with no arguments for help\n" );
	}
}

/*
=================
Com_StreamThreads_f
=================
*/
static void Com_StreamThreads_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: streamthreads <command> [args]\n" );
		Com_Printf( "Commands:\n" );
		Com_Printf( "  status          - Show current thread status\n" );
		Com_Printf( "  enable <type>   - Enable dedicated streaming thread\n" );
		Com_Printf( "  disable <type>  - Disable dedicated streaming thread\n" );
		Com_Printf( "  stats [type]    - Show thread statistics\n" );
		Com_Printf( "  cache <on|off>  - Enable/disable asset caching\n" );
		Com_Printf( "  flush [pri]     - Flush queues (pri = min priority 0-4)\n" );
		Com_Printf( "\nThread types: general, texture, model, sound, shader\n" );
		Com_Printf( "Priorities: 0=critical, 1=high, 2=normal, 3=low, 4=idle\n" );
		return;
	}

	const char* cmd = Cmd_Argv(1);

	if ( Q_stricmp( cmd, "status" ) == 0 ) {
		Com_Printf( "=== Streaming Threads Status ===\n" );
		Com_Printf( "Streaming Threading: %s\n", stream_thread_system.enabled ? "Enabled" : "Disabled" );
		Com_Printf( "Asset Caching: %s (%u/%u)\n", stream_thread_system.cache_enabled ? "Enabled" : "Disabled",
			stream_thread_system.current_cache_size, stream_thread_system.max_cache_size);

		const char* threadNames[STREAM_THREAD_MAX] = {
			"Stream_General", "Stream_Texture", "Stream_Model", "Stream_Sound", "Stream_Shader"
		};

		for ( int i = 0; i < STREAM_THREAD_MAX; i++ ) {
			Com_Printf( "  %s Thread: %s\n", threadNames[i],
				StreamThread_IsThreadEnabled( (stream_thread_type_t)i ) ? "Enabled" : "Disabled" );
		}

		uint64_t submitted, completed, failed, bytesLoaded, cacheHits, cacheMisses;
		StreamThread_GetGlobalStats(&submitted, &completed, &failed, &bytesLoaded, &cacheHits, &cacheMisses);
		Com_Printf( "\nGlobal Statistics:\n" );
		Com_Printf( "  Requests Submitted: %llu\n", (unsigned long long)submitted );
		Com_Printf( "  Requests Completed: %llu\n", (unsigned long long)completed );
		Com_Printf( "  Requests Failed: %llu\n", (unsigned long long)failed );
		Com_Printf( "  Bytes Loaded: %llu\n", (unsigned long long)bytesLoaded );
		Com_Printf( "  Cache Hits: %llu\n", (unsigned long long)cacheHits );
		Com_Printf( "  Cache Misses: %llu\n", (unsigned long long)cacheMisses );
	}
	else if ( Q_stricmp( cmd, "enable" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: streamthreads enable <type>\n" );
			return;
		}

		const char* typeStr = Cmd_Argv(2);
		stream_thread_type_t threadType = STREAM_THREAD_MAX;

		if ( Q_stricmp( typeStr, "general" ) == 0 ) threadType = STREAM_THREAD_GENERAL;
		else if ( Q_stricmp( typeStr, "texture" ) == 0 ) threadType = STREAM_THREAD_TEXTURE;
		else if ( Q_stricmp( typeStr, "model" ) == 0 ) threadType = STREAM_THREAD_MODEL;
		else if ( Q_stricmp( typeStr, "sound" ) == 0 ) threadType = STREAM_THREAD_SOUND;
		else if ( Q_stricmp( typeStr, "shader" ) == 0 ) threadType = STREAM_THREAD_SHADER;

		if ( threadType == STREAM_THREAD_MAX ) {
			Com_Printf( "Invalid thread type: %s\n", typeStr );
			return;
		}

		if ( StreamThread_EnableThread( threadType ) ) {
			Com_Printf( "Enabled %s streaming thread\n", typeStr );
		} else {
			Com_Printf( "Failed to enable %s streaming thread\n", typeStr );
		}
	}
	else if ( Q_stricmp( cmd, "disable" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: streamthreads disable <type>\n" );
			return;
		}

		const char* typeStr = Cmd_Argv(2);
		stream_thread_type_t threadType = STREAM_THREAD_MAX;

		if ( Q_stricmp( typeStr, "general" ) == 0 ) threadType = STREAM_THREAD_GENERAL;
		else if ( Q_stricmp( typeStr, "texture" ) == 0 ) threadType = STREAM_THREAD_TEXTURE;
		else if ( Q_stricmp( typeStr, "model" ) == 0 ) threadType = STREAM_THREAD_MODEL;
		else if ( Q_stricmp( typeStr, "sound" ) == 0 ) threadType = STREAM_THREAD_SOUND;
		else if ( Q_stricmp( typeStr, "shader" ) == 0 ) threadType = STREAM_THREAD_SHADER;

		if ( threadType == STREAM_THREAD_MAX ) {
			Com_Printf( "Invalid thread type: %s\n", typeStr );
			return;
		}

		StreamThread_DisableThread( threadType );
		Com_Printf( "Disabled %s streaming thread\n", typeStr );
	}
	else if ( Q_stricmp( cmd, "stats" ) == 0 ) {
		if ( Cmd_Argc() >= 3 ) {
			const char* typeStr = Cmd_Argv(2);
			stream_thread_type_t threadType = STREAM_THREAD_MAX;

			if ( Q_stricmp( typeStr, "general" ) == 0 ) threadType = STREAM_THREAD_GENERAL;
			else if ( Q_stricmp( typeStr, "texture" ) == 0 ) threadType = STREAM_THREAD_TEXTURE;
			else if ( Q_stricmp( typeStr, "model" ) == 0 ) threadType = STREAM_THREAD_MODEL;
			else if ( Q_stricmp( typeStr, "sound" ) == 0 ) threadType = STREAM_THREAD_SOUND;
			else if ( Q_stricmp( typeStr, "shader" ) == 0 ) threadType = STREAM_THREAD_SHADER;

			if ( threadType != STREAM_THREAD_MAX ) {
				uint64_t processedItems = 0;
				float avgTimeMs = 0.0f;
				uint64_t totalTimeNs = 0;

				StreamThread_GetStats( threadType, &processedItems, &avgTimeMs, &totalTimeNs );
				Com_Printf( "%s Thread Stats:\n", typeStr );
				Com_Printf( "  Processed Items: %llu\n", (unsigned long long)processedItems );
				Com_Printf( "  Average Time: %.2f ms\n", avgTimeMs );
				Com_Printf( "  Total Time: %.2f ms\n", totalTimeNs / 1000000.0 );
			} else {
				Com_Printf( "Invalid thread type: %s\n", typeStr );
			}
		} else {
			Com_Printf( "=== All Streaming Thread Statistics ===\n" );
			const char* threadNames[STREAM_THREAD_MAX] = {
				"Stream_General", "Stream_Texture", "Stream_Model", "Stream_Sound", "Stream_Shader"
			};

			for ( int i = 0; i < STREAM_THREAD_MAX; i++ ) {
				if ( StreamThread_IsThreadEnabled( (stream_thread_type_t)i ) ) {
					uint64_t processedItems = 0;
					float avgTimeMs = 0.0f;
					uint64_t totalTimeNs = 0;

					StreamThread_GetStats( (stream_thread_type_t)i, &processedItems, &avgTimeMs, &totalTimeNs );
					Com_Printf( "%s: %llu items, %.2f ms avg\n", threadNames[i],
						(unsigned long long)processedItems, avgTimeMs );
				}
			}
		}
	}
	else if ( Q_stricmp( cmd, "cache" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: streamthreads cache <on|off>\n" );
			return;
		}

		const char* cacheStr = Cmd_Argv(2);
		if ( Q_stricmp( cacheStr, "on" ) == 0 ) {
			StreamThread_EnableCache( qtrue );
			Com_Printf( "Asset caching enabled\n" );
		} else if ( Q_stricmp( cacheStr, "off" ) == 0 ) {
			StreamThread_EnableCache( qfalse );
			Com_Printf( "Asset caching disabled\n" );
		} else {
			Com_Printf( "Invalid cache setting: %s (use 'on' or 'off')\n", cacheStr );
		}
	}
	else if ( Q_stricmp( cmd, "flush" ) == 0 ) {
		asset_priority_t minPriority = ASSET_PRIORITY_NORMAL;
		if ( Cmd_Argc() >= 3 ) {
			minPriority = atoi( Cmd_Argv(2) );
			if ( minPriority >= ASSET_PRIORITY_MAX ) {
				minPriority = ASSET_PRIORITY_NORMAL;
			}
		}

		StreamThread_FlushQueues( minPriority );
		Com_Printf( "Flushed streaming queues (min priority: %d)\n", minPriority );
	}
	else {
		Com_Printf( "Unknown command: %s\n", cmd );
		Com_Printf( "Use 'streamthreads' with no arguments for help\n" );
	}
}

/*
=================
Com_CodeReview_f
=================
*/
static void Com_CodeReview_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: codereview <command> [args]\n" );
		Com_Printf( "Commands:\n" );
		Com_Printf( "  analyze <file>     - Analyze a specific source file\n" );
		Com_Printf( "  summary            - Show analysis summary\n" );
		Com_Printf( "  list [severity]    - List findings (filter by severity: info/warn/error/crit)\n" );
		Com_Printf( "  clear              - Clear all findings\n" );
		Com_Printf( "  config <setting> <value> - Configure analysis settings\n" );
		Com_Printf( "  help               - Show detailed help\n" );
		Com_Printf( "\nCurrent status: %s\n", code_review_system.initialized ? "Initialized" : "Not initialized");
		return;
	}

	const char* cmd = Cmd_Argv(1);

	if ( Q_stricmp( cmd, "analyze" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: codereview analyze <filename>\n" );
			return;
		}

		const char* filename = Cmd_Argv(2);
		if ( CodeReview_AnalyzeFile( filename ) ) {
			Com_Printf( "Analysis completed for %s\n", filename );
			CodeReview_PrintSummary();
		} else {
			Com_Printf( "Failed to analyze %s\n", filename );
		}
	}
	else if ( Q_stricmp( cmd, "summary" ) == 0 ) {
		CodeReview_PrintSummary();

		uint32_t num_findings = CodeReview_GetNumFindings();
		Com_Printf( "\nDetailed breakdown:\n" );

		const char* severity_names[REVIEW_SEVERITY_MAX] = {
			"Info", "Warning", "Error", "Critical"
		};
		const char* category_names[REVIEW_CATEGORY_MAX] = {
			"Style", "Best Practice", "Performance", "Security",
			"Maintainability", "Bugs", "Memory", "Threading"
		};

		for ( int i = 0; i < REVIEW_SEVERITY_MAX; i++ ) {
			if ( code_review_system.stats.findings_by_severity[i] > 0 ) {
				Com_Printf( "  %s: %u\n", severity_names[i],
					code_review_system.stats.findings_by_severity[i] );
			}
		}

		Com_Printf( "\nBy category:\n" );
		for ( int i = 0; i < REVIEW_CATEGORY_MAX; i++ ) {
			if ( code_review_system.stats.findings_by_category[i] > 0 ) {
				Com_Printf( "  %s: %u\n", category_names[i],
					code_review_system.stats.findings_by_category[i] );
			}
		}
	}
	else if ( Q_stricmp( cmd, "list" ) == 0 ) {
		review_severity_t min_severity = REVIEW_SEVERITY_INFO;
		if ( Cmd_Argc() >= 3 ) {
			const char* severity_str = Cmd_Argv(2);
			if ( Q_stricmp( severity_str, "warn" ) == 0 ) min_severity = REVIEW_SEVERITY_WARNING;
			else if ( Q_stricmp( severity_str, "error" ) == 0 ) min_severity = REVIEW_SEVERITY_ERROR;
			else if ( Q_stricmp( severity_str, "crit" ) == 0 ) min_severity = REVIEW_SEVERITY_CRITICAL;
		}

		uint32_t num_findings = CodeReview_GetNumFindings();
		uint32_t shown = 0;

		const char* severity_names[REVIEW_SEVERITY_MAX] = {
			"INFO", "WARN", "ERROR", "CRIT"
		};
		const char* category_names[REVIEW_CATEGORY_MAX] = {
			"Style", "BestPrac", "Perf", "Security",
			"Maint", "Bugs", "Memory", "Thread"
		};

		for ( uint32_t i = 0; i < num_findings && shown < 50; i++ ) {
			const code_review_finding_t* finding = CodeReview_GetFinding(i);
			if (!finding || finding->severity < min_severity) continue;

			Com_Printf( "[%s] %s:%d:%d [%s] %s\n",
				severity_names[finding->severity],
				finding->file, finding->line, finding->column,
				category_names[finding->category],
				finding->message );

			if ( finding->suggestion[0] ) {
				Com_Printf( "  Suggestion: %s\n", finding->suggestion );
			}

			shown++;
		}

		if ( shown == 50 ) {
			Com_Printf( "... and %u more findings (showing first 50)\n",
				num_findings - shown );
		} else if ( shown == 0 ) {
			Com_Printf( "No findings found\n" );
		}
	}
	else if ( Q_stricmp( cmd, "clear" ) == 0 ) {
		CodeReview_ClearFindings();
		Com_Printf( "Cleared all code review findings\n" );
	}
	else if ( Q_stricmp( cmd, "config" ) == 0 ) {
		if ( Cmd_Argc() < 4 ) {
			Com_Printf( "Usage: codereview config <setting> <value>\n" );
			Com_Printf( "Settings:\n" );
			Com_Printf( "  min_severity <info|warn|error|crit> - Minimum severity to report\n" );
			Com_Printf( "  max_line_length <length> - Maximum line length\n" );
			Com_Printf( "  category_<name> <0|1> - Enable/disable category\n" );
			return;
		}

		const char* setting = Cmd_Argv(2);
		const char* value = Cmd_Argv(3);

		if ( Q_stricmp( setting, "min_severity" ) == 0 ) {
			if ( Q_stricmp( value, "info" ) == 0 ) CodeReview_FilterBySeverity( REVIEW_SEVERITY_INFO );
			else if ( Q_stricmp( value, "warn" ) == 0 ) CodeReview_FilterBySeverity( REVIEW_SEVERITY_WARNING );
			else if ( Q_stricmp( value, "error" ) == 0 ) CodeReview_FilterBySeverity( REVIEW_SEVERITY_ERROR );
			else if ( Q_stricmp( value, "crit" ) == 0 ) CodeReview_FilterBySeverity( REVIEW_SEVERITY_CRITICAL );
			else Com_Printf( "Invalid severity: %s\n", value );
		}
		else if ( Q_stricmp( setting, "max_line_length" ) == 0 ) {
			int length = atoi( value );
			if ( length > 0 ) {
				code_review_system.config.max_line_length = length;
				Com_Printf( "Max line length set to %d\n", length );
			} else {
				Com_Printf( "Invalid line length: %s\n", value );
			}
		}
		else if ( strstr( setting, "category_" ) == setting ) {
			const char* category_name = setting + 9; // Skip "category_"
			review_category_t category = REVIEW_CATEGORY_MAX;

			if ( Q_stricmp( category_name, "style" ) == 0 ) category = REVIEW_CATEGORY_STYLE;
			else if ( Q_stricmp( category_name, "best_practice" ) == 0 ) category = REVIEW_CATEGORY_BEST_PRACTICE;
			else if ( Q_stricmp( category_name, "performance" ) == 0 ) category = REVIEW_CATEGORY_PERFORMANCE;
			else if ( Q_stricmp( category_name, "security" ) == 0 ) category = REVIEW_CATEGORY_SECURITY;
			else if ( Q_stricmp( category_name, "maintainability" ) == 0 ) category = REVIEW_CATEGORY_MAINTAINABILITY;
			else if ( Q_stricmp( category_name, "bugs" ) == 0 ) category = REVIEW_CATEGORY_BUGS;
			else if ( Q_stricmp( category_name, "memory" ) == 0 ) category = REVIEW_CATEGORY_MEMORY;
			else if ( Q_stricmp( category_name, "threading" ) == 0 ) category = REVIEW_CATEGORY_THREADING;

			if ( category != REVIEW_CATEGORY_MAX ) {
				qboolean enable = atoi( value ) != 0;
				CodeReview_FilterByCategory( category, enable );
				Com_Printf( "%s category %s\n", category_name, enable ? "enabled" : "disabled" );
			} else {
				Com_Printf( "Unknown category: %s\n", category_name );
			}
		}
		else {
			Com_Printf( "Unknown setting: %s\n", setting );
		}
	}
	else if ( Q_stricmp( cmd, "help" ) == 0 ) {
		Com_Printf( "Automated Code Review System Help\n" );
		Com_Printf( "=================================\n" );
		Com_Printf( "\nThis system analyzes C/C++ source code for:\n" );
		Com_Printf( "- Code style violations\n" );
		Com_Printf( "- Best practice violations\n" );
		Com_Printf( "- Performance issues\n" );
		Com_Printf( "- Security vulnerabilities\n" );
		Com_Printf( "- Potential bugs\n" );
		Com_Printf( "- Memory management issues\n" );
		Com_Printf( "- Threading problems\n" );
		Com_Printf( "\nCategories can be enabled/disabled individually.\n" );
		Com_Printf( "Findings are categorized by severity: Info, Warning, Error, Critical.\n" );
		Com_Printf( "\nExample usage:\n" );
		Com_Printf( "  codereview analyze src/common/common.c\n" );
		Com_Printf( "  codereview config min_severity warn\n" );
		Com_Printf( "  codereview config category_security 1\n" );
		Com_Printf( "  codereview list error\n" );
	}
	else {
		Com_Printf( "Unknown command: %s\n", cmd );
		Com_Printf( "Use 'codereview' with no arguments for help\n" );
	}
}

/*
=================
Com_LiveCode_f
=================
*/
static void Com_LiveCode_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: livecode <command> [args]\n" );
		Com_Printf( "Commands:\n" );
		Com_Printf( "  status            - Show current status\n" );
		Com_Printf( "  mode <off|background|realtime|incremental> - Set analysis mode\n" );
		Com_Printf( "  protocol <none|lsp|vscode|clion|vim|emacs> - Set IDE protocol\n" );
		Com_Printf( "  open <file>       - Open file for live analysis\n" );
		Com_Printf( "  close <file>      - Close file for live analysis\n" );
		Com_Printf( "  analyze <file>    - Analyze file now\n" );
		Com_Printf( "  findings <file>   - Show findings for file\n" );
		Com_Printf( "  ack <file> <line> - Acknowledge finding at line\n" );
		Com_Printf( "  stats             - Show performance statistics\n" );
		Com_Printf( "\nCurrent status: %s, Mode: %s, Protocol: %s\n",
			live_code_analysis_system.initialized ? "Initialized" : "Not initialized",
			LiveCodeAnalysis_GetModeName(LiveCodeAnalysis_GetMode()),
			LiveCodeAnalysis_GetProtocolName(LiveCodeAnalysis_GetIDEProtocol()));
		return;
	}

	const char* cmd = Cmd_Argv(1);

	if ( Q_stricmp( cmd, "status" ) == 0 ) {
		Com_Printf( "=== Live Code Analysis Status ===\n" );
		Com_Printf( "Initialized: %s\n", live_code_analysis_system.initialized ? "Yes" : "No" );
		Com_Printf( "Mode: %s\n", LiveCodeAnalysis_GetModeName(live_code_analysis_system.config.mode) );
		Com_Printf( "IDE Protocol: %s\n", LiveCodeAnalysis_GetProtocolName(live_code_analysis_system.config.ide.protocol) );
		Com_Printf( "Active Sessions: %u/%u\n", live_code_analysis_system.num_sessions, live_code_analysis_system.max_sessions );
		Com_Printf( "Analysis on Type: %s\n", live_code_analysis_system.config.analyze_on_type ? "Yes" : "No" );
		Com_Printf( "Analysis on Save: %s\n", live_code_analysis_system.config.analyze_on_save ? "Yes" : "No" );
		Com_Printf( "Incremental Analysis: %s\n", live_code_analysis_system.config.use_incremental ? "Yes" : "No" );

		uint64_t analyses, findings, time;
		LiveCodeAnalysis_GetStats(&analyses, &findings, &time);
		Com_Printf( "\nStatistics:\n" );
		Com_Printf( "Total Analyses: %llu\n", (unsigned long long)analyses );
		Com_Printf( "Total Findings: %llu\n", (unsigned long long)findings );
		Com_Printf( "Total Analysis Time: %.2f seconds\n", time / 1000.0f );
		if (analyses > 0) {
			Com_Printf( "Average Analysis Time: %.2f ms\n", (float)time / (float)analyses );
		}
	}
	else if ( Q_stricmp( cmd, "mode" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: livecode mode <off|background|realtime|incremental>\n" );
			return;
		}

		const char* mode_str = Cmd_Argv(2);
		live_analysis_mode_t mode = LIVE_MODE_OFF;

		if ( Q_stricmp( mode_str, "off" ) == 0 ) mode = LIVE_MODE_OFF;
		else if ( Q_stricmp( mode_str, "background" ) == 0 ) mode = LIVE_MODE_BACKGROUND;
		else if ( Q_stricmp( mode_str, "realtime" ) == 0 ) mode = LIVE_MODE_REALTIME;
		else if ( Q_stricmp( mode_str, "incremental" ) == 0 ) mode = LIVE_MODE_INCREMENTAL;
		else {
			Com_Printf( "Invalid mode: %s\n", mode_str );
			return;
		}

		if ( LiveCodeAnalysis_SetMode( mode ) ) {
			Com_Printf( "Live code analysis mode set to: %s\n", LiveCodeAnalysis_GetModeName(mode) );
		} else {
			Com_Printf( "Failed to set mode\n" );
		}
	}
	else if ( Q_stricmp( cmd, "protocol" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: livecode protocol <none|lsp|vscode|clion|vim|emacs>\n" );
			return;
		}

		const char* proto_str = Cmd_Argv(2);
		ide_protocol_t protocol = IDE_PROTOCOL_NONE;

		if ( Q_stricmp( proto_str, "none" ) == 0 ) protocol = IDE_PROTOCOL_NONE;
		else if ( Q_stricmp( proto_str, "lsp" ) == 0 ) protocol = IDE_PROTOCOL_LSP;
		else if ( Q_stricmp( proto_str, "vscode" ) == 0 ) protocol = IDE_PROTOCOL_VS_CODE;
		else if ( Q_stricmp( proto_str, "clion" ) == 0 ) protocol = IDE_PROTOCOL_CLION;
		else if ( Q_stricmp( proto_str, "vim" ) == 0 ) protocol = IDE_PROTOCOL_VIM;
		else if ( Q_stricmp( proto_str, "emacs" ) == 0 ) protocol = IDE_PROTOCOL_EMACS;
		else {
			Com_Printf( "Invalid protocol: %s\n", proto_str );
			return;
		}

		if ( LiveCodeAnalysis_SetIDEProtocol( protocol ) ) {
			Com_Printf( "IDE protocol set to: %s\n", LiveCodeAnalysis_GetProtocolName(protocol) );
		} else {
			Com_Printf( "Failed to set protocol\n" );
		}
	}
	else if ( Q_stricmp( cmd, "open" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: livecode open <filename>\n" );
			return;
		}

		const char* filename = Cmd_Argv(2);
		live_session_t* session = LiveCodeAnalysis_OpenFile(filename);
		if ( session ) {
			Com_Printf( "Opened file for live analysis: %s\n", filename );
		} else {
			Com_Printf( "Failed to open file: %s\n", filename );
		}
	}
	else if ( Q_stricmp( cmd, "close" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: livecode close <filename>\n" );
			return;
		}

		const char* filename = Cmd_Argv(2);
		if ( LiveCodeAnalysis_CloseFile(filename) ) {
			Com_Printf( "Closed file for live analysis: %s\n", filename );
		} else {
			Com_Printf( "Failed to close file or file not found: %s\n", filename );
		}
	}
	else if ( Q_stricmp( cmd, "analyze" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: livecode analyze <filename>\n" );
			return;
		}

		const char* filename = Cmd_Argv(2);
		if ( LiveCodeAnalysis_AnalyzeNow(filename) ) {
			live_session_t* session = LiveCodeAnalysis_GetSession(filename);
			if ( session ) {
				Com_Printf( "Analysis completed for %s: %u findings\n", filename, session->num_findings );
			}
		} else {
			Com_Printf( "Failed to analyze file: %s\n", filename );
		}
	}
	else if ( Q_stricmp( cmd, "findings" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: livecode findings <filename>\n" );
			return;
		}

		const char* filename = Cmd_Argv(2);
		live_finding_t* findings;
		uint32_t count = LiveCodeAnalysis_GetFindings(filename, &findings);

		if ( count == 0 ) {
			Com_Printf( "No findings for file: %s\n", filename );
			return;
		}

		Com_Printf( "Findings for %s:\n", filename );
		for ( uint32_t i = 0; i < count && i < 20; i++ ) { // Show first 20
			const live_finding_t* finding = &findings[i];
			if ( finding->suppressed ) continue;

			Com_Printf( "  [%s] Line %d: %s\n",
				finding->acknowledged ? "ACK" : LiveCodeAnalysis_GetSeverityName(finding->base.severity),
				finding->base.line, finding->base.message );

			if ( finding->base.suggestion[0] ) {
				Com_Printf( "    Suggestion: %s\n", finding->base.suggestion );
			}
		}

		if ( count > 20 ) {
			Com_Printf( "  ... and %u more findings\n", count - 20 );
		}
	}
	else if ( Q_stricmp( cmd, "ack" ) == 0 ) {
		if ( Cmd_Argc() < 4 ) {
			Com_Printf( "Usage: livecode ack <filename> <line>\n" );
			return;
		}

		const char* filename = Cmd_Argv(2);
		int line = atoi( Cmd_Argv(3) );

		if ( LiveCodeAnalysis_AcknowledgeFinding( filename, line, 0 ) ) {
			Com_Printf( "Acknowledged finding at %s:%d\n", filename, line );
		} else {
			Com_Printf( "No finding found at %s:%d\n", filename, line );
		}
	}
	else if ( Q_stricmp( cmd, "stats" ) == 0 ) {
		uint64_t analyses, findings, time;
		LiveCodeAnalysis_GetStats(&analyses, &findings, &time);

		Com_Printf( "=== Live Code Analysis Statistics ===\n" );
		Com_Printf( "Total Analyses: %llu\n", (unsigned long long)analyses );
		Com_Printf( "Total Findings: %llu\n", (unsigned long long)findings );
		Com_Printf( "Total Analysis Time: %llu ms\n", (unsigned long long)time );

		if ( analyses > 0 ) {
			Com_Printf( "Average Analysis Time: %.2f ms\n", (float)time / (float)analyses );
			Com_Printf( "Findings per Analysis: %.2f\n", (float)findings / (float)analyses );
		}

		Com_Printf( "\nPer-Session Statistics:\n" );
		for ( uint32_t i = 0; i < live_code_analysis_system.num_sessions; i++ ) {
			live_session_t* session = &live_code_analysis_system.sessions[i];
			if ( session->is_open ) {
				Com_Printf( "  %s: %u findings, v%u\n",
					session->filename, session->num_findings, session->version );
			}
		}
	}
	else {
		Com_Printf( "Unknown command: %s\n", cmd );
		Com_Printf( "Use 'livecode' with no arguments for help\n" );
	}
}

/*
=================
Com_PerfTest_f
=================
*/
static void Com_PerfTest_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: perftest <command> [args]\n" );
		Com_Printf( "Commands:\n" );
		Com_Printf( "  status             - Show system status\n" );
		Com_Printf( "  run <name> [duration] [warmup] - Run a single performance test\n" );
		Com_Printf( "  suite create <name> <desc>    - Create a test suite\n" );
		Com_Printf( "  suite add <suite> <test> <dur> <warmup> - Add test to suite\n" );
		Com_Printf( "  suite run <suite>             - Run a test suite\n" );
		Com_Printf( "  baseline set <test>           - Set baseline from last result\n" );
		Com_Printf( "  baseline list                 - List all baselines\n" );
		Com_Printf( "  report <file>                 - Generate performance report\n" );
		Com_Printf( "  ci export <dir>               - Export results for CI\n" );
		Com_Printf( "  stats                         - Show performance statistics\n" );
		Com_Printf( "\nSystem Status: %s\n",
			perf_test_system.initialized ? "Initialized" : "Not initialized");
		return;
	}

	const char* cmd = Cmd_Argv(1);

	if ( Q_stricmp( cmd, "status" ) == 0 ) {
		Com_Printf( "=== Performance Test System Status ===\n" );
		Com_Printf( "Initialized: %s\n", perf_test_system.initialized ? "Yes" : "No" );
		Com_Printf( "Test Running: %s\n", PerfTest_IsTestRunning() ? "Yes" : "No" );
		Com_Printf( "Baselines: %u/%u\n", perf_test_system.num_baselines, perf_test_system.max_baselines );
		Com_Printf( "CI System: %s\n", perf_test_system.ci_config.ci_system[0] ?
			perf_test_system.ci_config.ci_system : "None" );
		Com_Printf( "Output Directory: %s\n", perf_test_system.ci_config.output_directory );
		Com_Printf( "Report Format: %s\n", perf_test_system.ci_config.report_format );

		uint64_t total_tests = perf_test_system.total_tests_run;
		uint64_t regressions = perf_test_system.total_regressions_detected;
		uint64_t improvements = perf_test_system.total_improvements_detected;
		uint64_t test_time = perf_test_system.total_test_time_ms;

		Com_Printf( "\nStatistics:\n" );
		Com_Printf( "Total Tests Run: %llu\n", (unsigned long long)total_tests );
		Com_Printf( "Regressions Detected: %llu\n", (unsigned long long)regressions );
		Com_Printf( "Improvements Detected: %llu\n", (unsigned long long)improvements );
		Com_Printf( "Total Test Time: %.2f seconds\n", test_time / 1000.0f );

		if ( total_tests > 0 ) {
			Com_Printf( "Average Test Time: %.2f seconds\n", (test_time / 1000.0f) / total_tests );
		}
	}
	else if ( Q_stricmp( cmd, "run" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: perftest run <name> [duration_seconds] [warmup_seconds]\n" );
			return;
		}

		const char* test_name = Cmd_Argv(2);
		int duration = Cmd_Argc() >= 4 ? atoi( Cmd_Argv(3) ) : 30;
		int warmup = Cmd_Argc() >= 5 ? atoi( Cmd_Argv(4) ) : 5;

		if ( duration <= 0 || duration > 300 ) {
			Com_Printf( "Invalid duration: %d (must be 1-300 seconds)\n", duration );
			return;
		}

		if ( warmup < 0 || warmup > 60 ) {
			Com_Printf( "Invalid warmup: %d (must be 0-60 seconds)\n", warmup );
			return;
		}

		perf_test_config_t config;
		memset( &config, 0, sizeof( config ) );
		Q_strncpyz( config.name, test_name, sizeof( config.name ) );
		Com_sprintf( config.description, sizeof( config.description ),
			"Performance test: %s", test_name );
		config.duration_seconds = duration;
		config.warmup_seconds = warmup;
		config.sample_interval_ms = 100;
		config.save_screenshots = qfalse;
		config.record_video = qfalse;

		perf_test_result_t result;
		if ( PerfTest_RunTest( &config, &result ) ) {
			Com_Printf( "Test completed: %s\n", PerfTest_GetResultString( result.result ) );
			Com_Printf( "FPS: %.1f avg (%.1f min - %.1f max)\n",
				result.avg_fps, result.min_fps, result.max_fps );
			Com_Printf( "Frame Time: %.2f ms avg, %.2f ms max\n",
				result.avg_frame_time, result.max_frame_time );
			Com_Printf( "CPU Usage: %.1f%% avg, %.1f%% peak\n",
				result.avg_cpu_usage, result.peak_cpu_usage );
			Com_Printf( "Memory Usage: %.1f MB avg, %.1f MB peak\n",
				result.avg_memory_usage, result.peak_memory_usage );

			if ( result.regression_detected ) {
				Com_Printf( "REGRESSION DETECTED: %s\n", result.regression_reason );
			}
		} else {
			Com_Printf( "Failed to run performance test\n" );
		}
	}
	else if ( Q_stricmp( cmd, "suite" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: perftest suite <create|add|run> ...\n" );
			return;
		}

		const char* subcmd = Cmd_Argv(2);

		if ( Q_stricmp( subcmd, "create" ) == 0 ) {
			if ( Cmd_Argc() < 5 ) {
				Com_Printf( "Usage: perftest suite create <name> <description>\n" );
				return;
			}

			const char* suite_name = Cmd_Argv(3);
			const char* description = Cmd_Argv(4);

			perf_test_suite_t* suite = PerfTest_CreateSuite( suite_name, description );
			if ( suite ) {
				Com_Printf( "Created performance test suite: %s\n", suite_name );
				perf_test_system.current_suite = suite;
			} else {
				Com_Printf( "Failed to create test suite\n" );
			}
		}
		else if ( Q_stricmp( subcmd, "add" ) == 0 ) {
			if ( Cmd_Argc() < 7 ) {
				Com_Printf( "Usage: perftest suite add <suite_name> <test_name> <duration> <warmup>\n" );
				return;
			}

			const char* suite_name = Cmd_Argv(3);
			const char* test_name = Cmd_Argv(4);
			int duration = atoi( Cmd_Argv(5) );
			int warmup = atoi( Cmd_Argv(6) );

			if ( !perf_test_system.current_suite ||
				 strcmp( perf_test_system.current_suite->suite_name, suite_name ) != 0 ) {
				Com_Printf( "Suite '%s' not found or not current\n", suite_name );
				return;
			}

			perf_test_config_t config;
			memset( &config, 0, sizeof( config ) );
			Q_strncpyz( config.name, test_name, sizeof( config.name ) );
			config.duration_seconds = duration;
			config.warmup_seconds = warmup;
			config.sample_interval_ms = 100;

			if ( PerfTest_AddTestToSuite( perf_test_system.current_suite, &config ) ) {
				Com_Printf( "Added test '%s' to suite '%s'\n", test_name, suite_name );
			} else {
				Com_Printf( "Failed to add test to suite\n" );
			}
		}
		else if ( Q_stricmp( subcmd, "run" ) == 0 ) {
			if ( Cmd_Argc() < 4 ) {
				Com_Printf( "Usage: perftest suite run <suite_name>\n" );
				return;
			}

			const char* suite_name = Cmd_Argv(3);

			if ( !perf_test_system.current_suite ||
				 strcmp( perf_test_system.current_suite->suite_name, suite_name ) != 0 ) {
				Com_Printf( "Suite '%s' not found\n", suite_name );
				return;
			}

			if ( PerfTest_RunSuite( perf_test_system.current_suite ) ) {
				Com_Printf( "Suite '%s' completed successfully\n", suite_name );
			} else {
				Com_Printf( "Suite '%s' completed with failures\n", suite_name );
			}
		}
		else {
			Com_Printf( "Unknown suite command: %s\n", subcmd );
		}
	}
	else if ( Q_stricmp( cmd, "baseline" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: perftest baseline <set|list> ...\n" );
			return;
		}

		const char* subcmd = Cmd_Argv(2);

		if ( Q_stricmp( subcmd, "set" ) == 0 ) {
			if ( Cmd_Argc() < 4 ) {
				Com_Printf( "Usage: perftest baseline set <test_name>\n" );
				return;
			}

			const char* test_name = Cmd_Argv(3);

			// For now, create a dummy result to set baseline
			perf_test_result_t dummy_result;
			memset( &dummy_result, 0, sizeof( dummy_result ) );
			dummy_result.avg_fps = 60.0;
			dummy_result.min_fps = 55.0;
			dummy_result.avg_frame_time = 16.67;
			dummy_result.max_frame_time = 20.0;
			dummy_result.avg_cpu_usage = 45.0;
			dummy_result.avg_memory_usage = 512.0;

			if ( PerfTest_SetBaseline( test_name, &dummy_result ) ) {
				Com_Printf( "Set baseline for test: %s\n", test_name );
			} else {
				Com_Printf( "Failed to set baseline for test: %s\n", test_name );
			}
		}
		else if ( Q_stricmp( subcmd, "list" ) == 0 ) {
			Com_Printf( "=== Performance Baselines ===\n" );
			for ( uint32_t i = 0; i < perf_test_system.num_baselines; i++ ) {
				const perf_baseline_t* baseline = &perf_test_system.baselines[i];
				Com_Printf( "%s:\n", baseline->test_name );
				Com_Printf( "  FPS: %.1f avg, %.1f min\n",
					baseline->baseline_fps_avg, baseline->baseline_fps_min );
				Com_Printf( "  Frame Time: %.2f ms avg, %.2f ms max\n",
					baseline->baseline_frame_time_avg, baseline->baseline_frame_time_max );
				Com_Printf( "  Threshold: %.1f%%\n", baseline->regression_threshold_percent );
			}
		}
		else {
			Com_Printf( "Unknown baseline command: %s\n", subcmd );
		}
	}
	else if ( Q_stricmp( cmd, "report" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: perftest report <output_file>\n" );
			return;
		}

		const char* output_file = Cmd_Argv(2);

		uint32_t count;
		perf_test_result_t* results = PerfTest_GetSuiteResults( NULL, &count );

		if ( PerfTest_GenerateReport( results, count, output_file, "JSON" ) ) {
			Com_Printf( "Generated performance report: %s\n", output_file );
		} else {
			Com_Printf( "Failed to generate performance report\n" );
		}
	}
	else if ( Q_stricmp( cmd, "ci" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: perftest ci <export> <directory>\n" );
			return;
		}

		const char* subcmd = Cmd_Argv(2);

		if ( Q_stricmp( subcmd, "export" ) == 0 ) {
			if ( Cmd_Argc() < 4 ) {
				Com_Printf( "Usage: perftest ci export <directory>\n" );
				return;
			}

			const char* output_dir = Cmd_Argv(3);

			uint32_t count;
			perf_test_result_t* results = PerfTest_GetSuiteResults( NULL, &count );

			if ( PerfTest_ExportForCI( results, count, output_dir ) ) {
				Com_Printf( "Exported performance results for CI: %s\n", output_dir );
			} else {
				Com_Printf( "Failed to export results for CI\n" );
			}
		}
		else {
			Com_Printf( "Unknown CI command: %s\n", subcmd );
		}
	}
	else if ( Q_stricmp( cmd, "stats" ) == 0 ) {
		uint64_t total_tests = perf_test_system.total_tests_run;
		uint64_t regressions = perf_test_system.total_regressions_detected;
		uint64_t improvements = perf_test_system.total_improvements_detected;
		uint64_t test_time = perf_test_system.total_test_time_ms;

		Com_Printf( "=== Performance Test Statistics ===\n" );
		Com_Printf( "Total Tests Executed: %llu\n", (unsigned long long)total_tests );
		Com_Printf( "Performance Regressions: %llu\n", (unsigned long long)regressions );
		Com_Printf( "Performance Improvements: %llu\n", (unsigned long long)improvements );
		Com_Printf( "Total Execution Time: %.2f seconds\n", test_time / 1000.0f );

		if ( total_tests > 0 ) {
			Com_Printf( "Success Rate: %.1f%%\n",
				(float)(total_tests - regressions) / total_tests * 100.0f );
			Com_Printf( "Average Test Duration: %.2f seconds\n",
				(test_time / 1000.0f) / total_tests );
		}

		if ( regressions > 0 ) {
			Com_Printf( "Regression Rate: %.1f%%\n",
				(float)regressions / total_tests * 100.0f );
		}
	}
	else {
		Com_Printf( "Unknown command: %s\n", cmd );
		Com_Printf( "Use 'perftest' with no arguments for help\n" );
	}
}

/*
=================
Com_CrossPlatformTest_f
=================
*/
static void Com_CrossPlatformTest_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: crosstest <command> [args]\n" );
		Com_Printf( "Commands:\n" );
		Com_Printf( "  status             - Show system status and platform info\n" );
		Com_Printf( "  detect             - Detect and display platform information\n" );
		Com_Printf( "  run <test_name>    - Run a specific cross-platform test\n" );
		Com_Printf( "  suite create <name> <desc> - Create a test suite\n" );
		Com_Printf( "  suite add <suite> <test>   - Add test to suite\n" );
		Com_Printf( "  suite run <suite>          - Run a test suite\n" );
		Com_Printf( "  generate <type>            - Generate tests (platform/arch/compiler)\n" );
		Com_Printf( "  results                    - Show test results\n" );
		Com_Printf( "  export <dir>               - Export results for CI\n" );
		Com_Printf( "  validate                   - Validate platform compatibility\n" );
		Com_Printf( "\nSystem Status: %s\n",
			cross_platform_test_system.initialized ? "Initialized" : "Not initialized");
		return;
	}

	const char* cmd = Cmd_Argv(1);

	if ( Q_stricmp( cmd, "status" ) == 0 ) {
		Com_Printf( "=== Cross-Platform Test System Status ===\n" );
		Com_Printf( "Initialized: %s\n", cross_platform_test_system.initialized ? "Yes" : "No" );

		if ( cross_platform_test_system.initialized ) {
			platform_info_t* platform = &cross_platform_test_system.current_platform;
			Com_Printf( "Platform: %s %s\n", platform->name, platform->version );
			Com_Printf( "Architecture: %s (%d-bit)\n",
				platform->arch.name, platform->arch.bits );
			Com_Printf( "Compiler: %s %s\n",
				platform->compiler.name, platform->compiler.version );

			platform_capabilities_t* caps = &platform->capabilities;
			Com_Printf( "Capabilities:\n" );
			Com_Printf( "  Graphics: Vulkan=%s OpenGL=%s Metal=%s DirectX=%s\n",
				caps->has_vulkan ? "Yes" : "No",
				caps->has_opengl ? "Yes" : "No",
				caps->has_metal ? "Yes" : "No",
				caps->has_directx ? "Yes" : "No" );
			Com_Printf( "  Audio: %s\n", caps->has_audio ? "Yes" : "No" );
			Com_Printf( "  Networking: %s\n", caps->has_networking ? "Yes" : "No" );
			Com_Printf( "  Memory: %.1f GB total, %.1f GB available\n",
				caps->total_memory / (1024.0 * 1024.0 * 1024.0),
				caps->available_memory / (1024.0 * 1024.0 * 1024.0) );
		}

		Com_Printf( "\nStatistics:\n" );
		Com_Printf( "Total Tests Run: %u\n", cross_platform_test_system.total_tests_run );
		Com_Printf( "Passed: %u\n", cross_platform_test_system.total_passed );
		Com_Printf( "Failed: %u\n", cross_platform_test_system.total_failed );
		Com_Printf( "Skipped: %u\n", cross_platform_test_system.total_skipped );
		Com_Printf( "Timeouts: %u\n", cross_platform_test_system.total_timeouts );
		Com_Printf( "Crashes: %u\n", cross_platform_test_system.total_crashes );
	}
	else if ( Q_stricmp( cmd, "detect" ) == 0 ) {
		platform_info_t info;
		if ( CrossPlatformTest_DetectPlatform( &info ) ) {
			Com_Printf( "=== Platform Detection Results ===\n" );
			Com_Printf( "Platform: %s\n", info.name );
			Com_Printf( "Architecture: %s (%d-bit, %s endian)\n",
				info.arch.name, info.arch.bits,
				info.arch.little_endian ? "little" : "big" );
			Com_Printf( "Compiler: %s %s\n", info.compiler.name, info.compiler.version );

			Com_Printf( "\nCompiler Features:\n" );
			Com_Printf( "  C11 Support: %s\n", info.compiler.supports_c11 ? "Yes" : "No" );
			Com_Printf( "  C23 Support: %s\n", info.compiler.supports_c23 ? "Yes" : "No" );
			Com_Printf( "  C++11 Support: %s\n", info.compiler.supports_cpp11 ? "Yes" : "No" );
			Com_Printf( "  C++23 Support: %s\n", info.compiler.supports_cpp23 ? "Yes" : "No" );

			Com_Printf( "\nArchitecture Features:\n" );
			Com_Printf( "  SIMD Support: %s\n", info.arch.supports_simd ? "Yes" : "No" );
			Com_Printf( "  64-bit Atomic Support: %s\n", info.arch.supports_atomic64 ? "Yes" : "No" );

			Com_Printf( "\nPlatform Capabilities:\n" );
			Com_Printf( "  Threads: %s (%d max)\n",
				info.capabilities.has_threads ? "Yes" : "No",
				info.capabilities.max_threads );
			Com_Printf( "  Unicode: %s\n", info.capabilities.has_unicode ? "Yes" : "No" );
			Com_Printf( "  Large Files: %s\n", info.capabilities.has_large_files ? "Yes" : "No" );
		} else {
			Com_Printf( "Failed to detect platform information\n" );
		}
	}
	else if ( Q_stricmp( cmd, "run" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: crosstest run <test_name>\n" );
			Com_Printf( "Available tests:\n" );
			Com_Printf( "  basic_functionality\n" );
			Com_Printf( "  memory_management\n" );
			Com_Printf( "  threading\n" );
			Com_Printf( "  file_system\n" );
			Com_Printf( "  network_basic\n" );
			Com_Printf( "  graphics_api\n" );
			Com_Printf( "  audio_api\n" );
			Com_Printf( "  large_file_support\n" );
			Com_Printf( "  unicode_support\n" );
			Com_Printf( "  time_and_date\n" );
			Com_Printf( "  math_precision\n" );
			return;
		}

		const char* test_name = Cmd_Argv(2);

		cross_platform_test_config_t config;
		memset( &config, 0, sizeof( config ) );
		Q_strncpyz( config.test_name, test_name, sizeof( config.test_name ) );
		Com_sprintf( config.description, sizeof( config.description ),
			"Cross-platform test: %s", test_name );
		config.timeout_seconds = 30;

		cross_platform_test_result_t result;
		if ( CrossPlatformTest_RunTest( &config, &result ) ) {
			Com_Printf( "Test completed: %s\n", CrossPlatformTest_GetResultString( result.result ) );
			Com_Printf( "Duration: %.2f seconds\n", result.duration_ms / 1000.0f );

			if ( result.error_message[0] ) {
				Com_Printf( "Error: %s\n", result.error_message );
			}
		} else {
			Com_Printf( "Failed to run cross-platform test\n" );
		}
	}
	else if ( Q_stricmp( cmd, "suite" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: crosstest suite <create|add|run> ...\n" );
			return;
		}

		const char* subcmd = Cmd_Argv(2);

		if ( Q_stricmp( subcmd, "create" ) == 0 ) {
			if ( Cmd_Argc() < 5 ) {
				Com_Printf( "Usage: crosstest suite create <name> <description>\n" );
				return;
			}

			const char* suite_name = Cmd_Argv(3);
			const char* description = Cmd_Argv(4);

			cross_platform_test_suite_t* suite = CrossPlatformTest_CreateSuite( suite_name, description );
			if ( suite ) {
				Com_Printf( "Created cross-platform test suite: %s\n", suite_name );
				cross_platform_test_system.current_suite = suite;
			} else {
				Com_Printf( "Failed to create test suite\n" );
			}
		}
		else if ( Q_stricmp( subcmd, "add" ) == 0 ) {
			if ( Cmd_Argc() < 5 ) {
				Com_Printf( "Usage: crosstest suite add <suite_name> <test_name>\n" );
				return;
			}

			const char* suite_name = Cmd_Argv(3);
			const char* test_name = Cmd_Argv(4);

			if ( !cross_platform_test_system.current_suite ||
				 strcmp( cross_platform_test_system.current_suite->suite_name, suite_name ) != 0 ) {
				Com_Printf( "Suite '%s' not found or not current\n", suite_name );
				return;
			}

			cross_platform_test_config_t config;
			memset( &config, 0, sizeof( config ) );
			Q_strncpyz( config.test_name, test_name, sizeof( config.test_name ) );
			config.timeout_seconds = 30;

			if ( CrossPlatformTest_AddTestToSuite( cross_platform_test_system.current_suite, &config ) ) {
				Com_Printf( "Added test '%s' to suite '%s'\n", test_name, suite_name );
			} else {
				Com_Printf( "Failed to add test to suite\n" );
			}
		}
		else if ( Q_stricmp( subcmd, "run" ) == 0 ) {
			if ( Cmd_Argc() < 4 ) {
				Com_Printf( "Usage: crosstest suite run <suite_name>\n" );
				return;
			}

			const char* suite_name = Cmd_Argv(3);

			if ( !cross_platform_test_system.current_suite ||
				 strcmp( cross_platform_test_system.current_suite->suite_name, suite_name ) != 0 ) {
				Com_Printf( "Suite '%s' not found\n", suite_name );
				return;
			}

			if ( CrossPlatformTest_RunSuite( cross_platform_test_system.current_suite ) ) {
				Com_Printf( "Suite '%s' completed successfully\n", suite_name );
			} else {
				Com_Printf( "Suite '%s' completed with failures\n", suite_name );
			}
		}
		else {
			Com_Printf( "Unknown suite command: %s\n", subcmd );
		}
	}
	else if ( Q_stricmp( cmd, "generate" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: crosstest generate <platform|arch|compiler>\n" );
			return;
		}

		const char* type = Cmd_Argv(2);
		const char* suite_name = "generated_suite";

		cross_platform_test_suite_t* suite = CrossPlatformTest_CreateSuite(
			suite_name, "Auto-generated cross-platform test suite" );

		if ( !suite ) {
			Com_Printf( "Failed to create test suite\n" );
			return;
		}

		qboolean success = qfalse;

		if ( Q_stricmp( type, "platform" ) == 0 ) {
			success = CrossPlatformTest_GeneratePlatformTests( suite );
			Com_Printf( "Generated platform-specific tests\n" );
		} else if ( Q_stricmp( type, "arch" ) == 0 ) {
			success = CrossPlatformTest_GenerateArchitectureTests( suite );
			Com_Printf( "Generated architecture-specific tests\n" );
		} else if ( Q_stricmp( type, "compiler" ) == 0 ) {
			success = CrossPlatformTest_GenerateCompilerTests( suite );
			Com_Printf( "Generated compiler-specific tests\n" );
		} else {
			Com_Printf( "Unknown generation type: %s\n", type );
			return;
		}

		if ( success ) {
			cross_platform_test_system.current_suite = suite;
			Com_Printf( "Test suite '%s' created with %u tests\n",
				suite_name, suite->num_tests );
		} else {
			Com_Printf( "Failed to generate tests\n" );
		}
	}
	else if ( Q_stricmp( cmd, "results" ) == 0 ) {
		cross_platform_test_result_t* results;
		uint32_t count = CrossPlatformTest_GetResults( &results );

		if ( count == 0 ) {
			Com_Printf( "No test results available\n" );
			return;
		}

		Com_Printf( "=== Cross-Platform Test Results ===\n" );
		for ( uint32_t i = 0; i < count && i < 20; i++ ) { // Show first 20
			const cross_platform_test_result_t* result = &results[i];
			Com_Printf( "%s: %s (%.2fs)",
				result->test_name,
				CrossPlatformTest_GetResultString( result->result ),
				result->duration_ms / 1000.0f );

			if ( result->error_message[0] ) {
				Com_Printf( " - %s", result->error_message );
			}
			Com_Printf( "\n" );
		}

		if ( count > 20 ) {
			Com_Printf( "... and %u more results\n", count - 20 );
		}
	}
	else if ( Q_stricmp( cmd, "export" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: crosstest export <directory>\n" );
			return;
		}

		const char* export_dir = Cmd_Argv(2);

		if ( CrossPlatformTest_ExportForCI( export_dir ) ) {
			Com_Printf( "Exported cross-platform test results to: %s\n", export_dir );
		} else {
			Com_Printf( "Failed to export test results\n" );
		}
	}
	else if ( Q_stricmp( cmd, "validate" ) == 0 ) {
		Com_Printf( "=== Platform Compatibility Validation ===\n" );

		qboolean requirements_met = CrossPlatformTest_CheckMinimumRequirements();
		Com_Printf( "Minimum Requirements: %s\n", requirements_met ? "MET" : "NOT MET" );

		qboolean platform_valid = CrossPlatformTest_ValidatePlatformCompatibility();
		Com_Printf( "Platform Compatibility: %s\n", platform_valid ? "VALID" : "INVALID" );

		qboolean caps_ok = CrossPlatformTest_TestPlatformCapabilities();
		Com_Printf( "Platform Capabilities: %s\n", caps_ok ? "OK" : "ISSUES DETECTED" );

		qboolean arch_ok = CrossPlatformTest_TestArchitectureFeatures();
		Com_Printf( "Architecture Features: %s\n", arch_ok ? "OK" : "ISSUES DETECTED" );

		qboolean compiler_ok = CrossPlatformTest_TestCompilerFeatures();
		Com_Printf( "Compiler Features: %s\n", compiler_ok ? "OK" : "ISSUES DETECTED" );

		qboolean overall = requirements_met && platform_valid && caps_ok && arch_ok && compiler_ok;
		Com_Printf( "\nOverall Compatibility: %s\n", overall ? "COMPATIBLE" : "NOT COMPATIBLE" );
	}
	else {
		Com_Printf( "Unknown command: %s\n", cmd );
		Com_Printf( "Use 'crosstest' with no arguments for help\n" );
	}
}

/*
=================
Com_MemorySafetyTest_f
=================
*/
static void Com_MemorySafetyTest_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: memtest <command> [args]\n" );
		Com_Printf( "Commands:\n" );
		Com_Printf( "  status             - Show system status and sanitizer info\n" );
		Com_Printf( "  enable <asan|ubsan|lsan> - Enable specific sanitizer\n" );
		Com_Printf( "  disable <asan|ubsan|lsan> - Disable specific sanitizer\n" );
		Com_Printf( "  strict <on|off>   - Set strict mode (warnings as errors)\n" );
		Com_Printf( "  run <test_name>    - Run a specific memory safety test\n" );
		Com_Printf( "  suite create <name> <desc> - Create a test suite\n" );
		Com_Printf( "  suite add <suite> <test>   - Add test to suite\n" );
		Com_Printf( "  suite run <suite>          - Run a test suite\n" );
		Com_Printf( "  generate <fuzz|boundary|concurrency> - Generate test suites\n" );
		Com_Printf( "  results                    - Show test results\n" );
		Com_Printf( "  export <dir>               - Export results for CI\n" );
		Com_Printf( "  report <file>              - Generate detailed report\n" );
		Com_Printf( "\nSystem Status: %s\n",
			memory_safety_test_system.initialized ? "Initialized" : "Not initialized");
		return;
	}

	const char* cmd = Cmd_Argv(1);

	if ( Q_stricmp( cmd, "status" ) == 0 ) {
		Com_Printf( "=== Memory Safety Test System Status ===\n" );
		Com_Printf( "Initialized: %s\n", memory_safety_test_system.initialized ? "Yes" : "No" );

		if ( memory_safety_test_system.initialized ) {
			Com_Printf( "ASan: %s (%s)\n",
				memory_safety_test_system.asan_available ? "Available" : "Not Available",
				memory_safety_test_system.asan_version);
			Com_Printf( "UBSan: %s (%s)\n",
				memory_safety_test_system.ubsan_available ? "Available" : "Not Available",
				memory_safety_test_system.ubsan_version);
			Com_Printf( "LSan: %s (%s)\n",
				memory_safety_test_system.lsan_available ? "Available" : "Not Available",
				memory_safety_test_system.lsan_version);
		}

		Com_Printf( "\nStatistics:\n" );
		Com_Printf( "Total Tests Run: %u\n", memory_safety_test_system.total_tests_run );
		Com_Printf( "Passed: %u\n", memory_safety_test_system.total_passed );
		Com_Printf( "ASan Errors: %u\n", memory_safety_test_system.total_asan_errors );
		Com_Printf( "UBSan Errors: %u\n", memory_safety_test_system.total_ubsan_errors );
		Com_Printf( "Leaks Detected: %u\n", memory_safety_test_system.total_leaks_detected );
		Com_Printf( "Crashes: %u\n", memory_safety_test_system.total_crashes );
	}
	else if ( Q_stricmp( cmd, "enable" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: memtest enable <asan|ubsan|lsan>\n" );
			return;
		}

		const char* sanitizer = Cmd_Argv(2);
		qboolean success = qfalse;

		if ( Q_stricmp( sanitizer, "asan" ) == 0 ) {
			success = MemorySafetyTest_EnableASan();
			Com_Printf( "ASan %s\n", success ? "enabled" : "failed to enable" );
		} else if ( Q_stricmp( sanitizer, "ubsan" ) == 0 ) {
			success = MemorySafetyTest_EnableUBSan();
			Com_Printf( "UBSan %s\n", success ? "enabled" : "failed to enable" );
		} else if ( Q_stricmp( sanitizer, "lsan" ) == 0 ) {
			success = MemorySafetyTest_EnableLSan();
			Com_Printf( "LSan %s\n", success ? "enabled" : "failed to enable" );
		} else {
			Com_Printf( "Unknown sanitizer: %s\n", sanitizer );
		}
	}
	else if ( Q_stricmp( cmd, "disable" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: memtest disable <asan|ubsan|lsan>\n" );
			return;
		}

		const char* sanitizer = Cmd_Argv(2);
		// Note: Disabling sanitizers at runtime is not always possible
		Com_Printf( "Sanitizer disabling not supported at runtime: %s\n", sanitizer );
		Com_Printf( "Sanitizers must be disabled at compile time.\n" );
	}
	else if ( Q_stricmp( cmd, "strict" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: memtest strict <on|off>\n" );
			return;
		}

		const char* mode = Cmd_Argv(2);
		qboolean strict = qfalse;

		if ( Q_stricmp( mode, "on" ) == 0 ) {
			strict = qtrue;
		} else if ( Q_stricmp( mode, "off" ) == 0 ) {
			strict = qfalse;
		} else {
			Com_Printf( "Invalid mode: %s (use 'on' or 'off')\n", mode );
			return;
		}

		if ( MemorySafetyTest_SetStrictMode( strict ) ) {
			Com_Printf( "Strict mode %s\n", strict ? "enabled" : "disabled" );
		} else {
			Com_Printf( "Failed to set strict mode\n" );
		}
	}
	else if ( Q_stricmp( cmd, "run" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: memtest run <test_name>\n" );
			Com_Printf( "Available tests:\n" );
			Com_Printf( "  buffer_overflow\n" );
			Com_Printf( "  use_after_free\n" );
			Com_Printf( "  double_free\n" );
			Com_Printf( "  memory_leak\n" );
			Com_Printf( "  integer_overflow\n" );
			Com_Printf( "  division_by_zero\n" );
			Com_Printf( "  null_pointer_deref\n" );
			Com_Printf( "  uninitialized_variable\n" );
			Com_Printf( "  type_confusion\n" );
			Com_Printf( "  stack_overflow\n" );
			Com_Printf( "  file_operations\n" );
			Com_Printf( "  network_operations\n" );
			Com_Printf( "  thread_operations\n" );
			Com_Printf( "  memory_management\n" );
			Com_Printf( "  string_operations\n" );
			Com_Printf( "  data_structures\n" );
			return;
		}

		const char* test_name = Cmd_Argv(2);

		memory_safety_test_config_t config;
		memset( &config, 0, sizeof( config ) );
		Q_strncpyz( config.test_name, test_name, sizeof( config.test_name ) );
		Q_strncpyz( config.description, "Manual memory safety test", sizeof( config.description ) );
		config.enable_asan = memory_safety_test_system.asan_available;
		config.enable_ubsan = memory_safety_test_system.ubsan_available;
		config.enable_lsan = memory_safety_test_system.lsan_available;
		config.timeout_seconds = 30;

		memory_safety_test_result_t result;
		if ( MemorySafetyTest_RunTest( &config, &result ) ) {
			Com_Printf( "Test completed: %s\n", MemorySafetyTest_GetResultString( result.result ) );
			Com_Printf( "Duration: %.2f seconds\n", result.duration_ms / 1000.0f );

			if ( result.error_count > 0 ) {
				Com_Printf( "Detected %u error(s):\n", result.error_count );
				for ( uint32_t i = 0; i < result.error_count && i < 5; i++ ) {
					const sanitizer_error_t* error = &result.errors[i];
					Com_Printf( "  %s: %s\n", error->error_type, error->description );
				}
			}

			Com_Printf( "Memory Usage: Peak %.1f MB, %llu allocations, %llu frees\n",
				result.peak_heap_usage / (1024.0 * 1024.0),
				(unsigned long long)result.total_allocations,
				(unsigned long long)result.total_frees );
		} else {
			Com_Printf( "Failed to run memory safety test\n" );
		}
	}
	else if ( Q_stricmp( cmd, "suite" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: memtest suite <create|add|run> ...\n" );
			return;
		}

		const char* subcmd = Cmd_Argv(2);

		if ( Q_stricmp( subcmd, "create" ) == 0 ) {
			if ( Cmd_Argc() < 5 ) {
				Com_Printf( "Usage: memtest suite create <name> <description>\n" );
				return;
			}

			const char* suite_name = Cmd_Argv(3);
			const char* description = Cmd_Argv(4);

			memory_safety_test_suite_t* suite = MemorySafetyTest_CreateSuite( suite_name, description );
			if ( suite ) {
				Com_Printf( "Created memory safety test suite: %s\n", suite_name );
				memory_safety_test_system.current_suite = suite;
			} else {
				Com_Printf( "Failed to create test suite\n" );
			}
		}
		else if ( Q_stricmp( subcmd, "add" ) == 0 ) {
			if ( Cmd_Argc() < 5 ) {
				Com_Printf( "Usage: memtest suite add <suite_name> <test_name>\n" );
				return;
			}

			const char* suite_name = Cmd_Argv(3);
			const char* test_name = Cmd_Argv(4);

			if ( !memory_safety_test_system.current_suite ||
				 strcmp( memory_safety_test_system.current_suite->suite_name, suite_name ) != 0 ) {
				Com_Printf( "Suite '%s' not found or not current\n", suite_name );
				return;
			}

			memory_safety_test_config_t config;
			memset( &config, 0, sizeof( config ) );
			Q_strncpyz( config.test_name, test_name, sizeof( config.test_name ) );
			config.enable_asan = memory_safety_test_system.asan_available;
			config.enable_ubsan = memory_safety_test_system.ubsan_available;
			config.enable_lsan = memory_safety_test_system.lsan_available;
			config.timeout_seconds = 30;

			if ( MemorySafetyTest_AddTestToSuite( memory_safety_test_system.current_suite, &config ) ) {
				Com_Printf( "Added test '%s' to suite '%s'\n", test_name, suite_name );
			} else {
				Com_Printf( "Failed to add test to suite\n" );
			}
		}
		else if ( Q_stricmp( subcmd, "run" ) == 0 ) {
			if ( Cmd_Argc() < 4 ) {
				Com_Printf( "Usage: memtest suite run <suite_name>\n" );
				return;
			}

			const char* suite_name = Cmd_Argv(3);

			if ( !memory_safety_test_system.current_suite ||
				 strcmp( memory_safety_test_system.current_suite->suite_name, suite_name ) != 0 ) {
				Com_Printf( "Suite '%s' not found\n", suite_name );
				return;
			}

			if ( MemorySafetyTest_RunSuite( memory_safety_test_system.current_suite ) ) {
				Com_Printf( "Suite '%s' completed successfully\n", suite_name );
			} else {
				Com_Printf( "Suite '%s' completed with failures\n", suite_name );
			}
		}
		else {
			Com_Printf( "Unknown suite command: %s\n", subcmd );
		}
	}
	else if ( Q_stricmp( cmd, "generate" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: memtest generate <fuzz|boundary|concurrency>\n" );
			return;
		}

		const char* type = Cmd_Argv(2);
		const char* suite_name = "generated_memtest_suite";

		memory_safety_test_suite_t* suite = MemorySafetyTest_CreateSuite(
			suite_name, "Auto-generated memory safety test suite" );

		if ( !suite ) {
			Com_Printf( "Failed to create test suite\n" );
			return;
		}

		qboolean success = qfalse;

		if ( Q_stricmp( type, "fuzz" ) == 0 ) {
			success = MemorySafetyTest_GenerateFuzzTests( suite );
			Com_Printf( "Generated fuzz tests\n" );
		} else if ( Q_stricmp( type, "boundary" ) == 0 ) {
			success = MemorySafetyTest_GenerateBoundaryTests( suite );
			Com_Printf( "Generated boundary tests\n" );
		} else if ( Q_stricmp( type, "concurrency" ) == 0 ) {
			success = MemorySafetyTest_GenerateConcurrencyTests( suite );
			Com_Printf( "Generated concurrency tests\n" );
		} else {
			Com_Printf( "Unknown generation type: %s\n", type );
			return;
		}

		if ( success ) {
			memory_safety_test_system.current_suite = suite;
			Com_Printf( "Test suite '%s' created with %u tests\n",
				suite_name, suite->num_tests );
		} else {
			Com_Printf( "Failed to generate tests\n" );
		}
	}
	else if ( Q_stricmp( cmd, "results" ) == 0 ) {
		memory_safety_test_result_t* results;
		uint32_t count = MemorySafetyTest_GetResults( &results );

		if ( count == 0 ) {
			Com_Printf( "No test results available\n" );
			return;
		}

		Com_Printf( "=== Memory Safety Test Results ===\n" );
		for ( uint32_t i = 0; i < count && i < 20; i++ ) { // Show first 20
			const memory_safety_test_result_t* result = &results[i];
			Com_Printf( "%-20s: %s (%.2fs) - %u errors\n",
				result->test_name,
				MemorySafetyTest_GetResultString( result->result ),
				result->duration_ms / 1000.0f,
				result->error_count );
		}

		if ( count > 20 ) {
			Com_Printf( "... and %u more results\n", count - 20 );
		}
	}
	else if ( Q_stricmp( cmd, "export" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: memtest export <directory>\n" );
			return;
		}

		const char* export_dir = Cmd_Argv(2);

		if ( MemorySafetyTest_ExportForCI( export_dir ) ) {
			Com_Printf( "Exported memory safety test results to: %s\n", export_dir );
		} else {
			Com_Printf( "Failed to export test results\n" );
		}
	}
	else if ( Q_stricmp( cmd, "report" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: memtest report <output_file>\n" );
			return;
		}

		const char* output_file = Cmd_Argv(2);

		if ( MemorySafetyTest_GenerateReport( output_file, "JSON" ) ) {
			Com_Printf( "Generated memory safety report: %s\n", output_file );
		} else {
			Com_Printf( "Failed to generate report\n" );
		}
	}
	else {
		Com_Printf( "Unknown command: %s\n", cmd );
		Com_Printf( "Use 'memtest' with no arguments for help\n" );
	}
}

/*
=================
Com_ThreadSafetyTest_f
=================
*/
static void Com_ThreadSafetyTest_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: threadtest <command> [args]\n" );
		Com_Printf( "Commands:\n" );
		Com_Printf( "  status             - Show system status and TSan info\n" );
		Com_Printf( "  enable <tsan|deadlock|race> - Enable specific detection\n" );
		Com_Printf( "  disable <tsan|deadlock|race> - Disable specific detection\n" );
		Com_Printf( "  strict <on|off>   - Set strict mode (warnings as errors)\n" );
		Com_Printf( "  run <test_name>    - Run a specific thread safety test\n" );
		Com_Printf( "  suite create <name> <desc> - Create a test suite\n" );
		Com_Printf( "  suite add <suite> <test>   - Add test to suite\n" );
		Com_Printf( "  suite run <suite>          - Run a test suite\n" );
		Com_Printf( "  generate <race|deadlock|stress> - Generate test suites\n" );
		Com_Printf( "  results                    - Show test results\n" );
		Com_Printf( "  export <dir>               - Export results for CI\n" );
		Com_Printf( "  report <file>              - Generate detailed report\n" );
		Com_Printf( "\nSystem Status: %s\n",
			thread_safety_test_system.initialized ? "Initialized" : "Not initialized");
		return;
	}

	const char* cmd = Cmd_Argv(1);

	if ( Q_stricmp( cmd, "status" ) == 0 ) {
		Com_Printf( "=== Thread Safety Test System Status ===\n" );
		Com_Printf( "Initialized: %s\n", thread_safety_test_system.initialized ? "Yes" : "No" );

		if ( thread_safety_test_system.initialized ) {
			Com_Printf( "TSan: %s (%s)\n",
				thread_safety_test_system.tsan_available ? "Available" : "Not Available",
				thread_safety_test_system.tsan_version);
			Com_Printf( "Deadlock Detection: %s\n",
				thread_safety_test_system.supports_deadlock_detection ? "Supported" : "Not Supported");
			Com_Printf( "Race Detection: %s\n",
				thread_safety_test_system.tsan_available ? "Available" : "Not Available");
			Com_Printf( "Max Threads: %d\n", thread_safety_test_system.max_supported_threads);
		}

		Com_Printf( "\nStatistics:\n" );
		Com_Printf( "Total Tests Run: %u\n", thread_safety_test_system.total_tests_run );
		Com_Printf( "Race Conditions: %u\n", thread_safety_test_system.total_races_detected );
		Com_Printf( "Deadlocks: %u\n", thread_safety_test_system.total_deadlocks_detected );
		Com_Printf( "Atomic Violations: %u\n", thread_safety_test_system.total_atomic_violations );
		Com_Printf( "Crashes: %u\n", thread_safety_test_system.total_crashes );
	}
	else if ( Q_stricmp( cmd, "enable" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: threadtest enable <tsan|deadlock|race>\n" );
			return;
		}

		const char* detector = Cmd_Argv(2);
		qboolean success = qfalse;

		if ( Q_stricmp( detector, "tsan" ) == 0 ) {
			success = ThreadSafetyTest_EnableTSan();
			Com_Printf( "TSan %s\n", success ? "enabled" : "failed to enable" );
		} else if ( Q_stricmp( detector, "deadlock" ) == 0 ) {
			success = ThreadSafetyTest_EnableDeadlockDetection();
			Com_Printf( "Deadlock detection %s\n", success ? "enabled" : "failed to enable" );
		} else if ( Q_stricmp( detector, "race" ) == 0 ) {
			success = ThreadSafetyTest_EnableRaceDetection();
			Com_Printf( "Race detection %s\n", success ? "enabled" : "failed to enable" );
		} else {
			Com_Printf( "Unknown detector: %s\n", detector );
		}
	}
	else if ( Q_stricmp( cmd, "disable" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: threadtest disable <tsan|deadlock|race>\n" );
			return;
		}

		const char* detector = Cmd_Argv(2);
		// Note: Disabling detectors at runtime is not always possible
		Com_Printf( "Detector disabling not supported at runtime: %s\n", detector );
		Com_Printf( "Detectors must be disabled at compile time.\n" );
	}
	else if ( Q_stricmp( cmd, "strict" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: threadtest strict <on|off>\n" );
			return;
		}

		const char* mode = Cmd_Argv(2);
		qboolean strict = qfalse;

		if ( Q_stricmp( mode, "on" ) == 0 ) {
			strict = qtrue;
		} else if ( Q_stricmp( mode, "off" ) == 0 ) {
			strict = qfalse;
		} else {
			Com_Printf( "Invalid mode: %s (use 'on' or 'off')\n", mode );
			return;
		}

		if ( ThreadSafetyTest_SetStrictMode( strict ) ) {
			Com_Printf( "Strict mode %s\n", strict ? "enabled" : "disabled" );
		} else {
			Com_Printf( "Failed to set strict mode\n" );
		}
	}
	else if ( Q_stricmp( cmd, "run" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: threadtest run <test_name>\n" );
			Com_Printf( "Available tests:\n" );
			Com_Printf( "  data_race_basic\n" );
			Com_Printf( "  data_race_atomic\n" );
			Com_Printf( "  deadlock_mutex\n" );
			Com_Printf( "  deadlock_rwlock\n" );
			Com_Printf( "  lock_order_violation\n" );
			Com_Printf( "  use_after_free_concurrent\n" );
			Com_Printf( "  double_lock\n" );
			Com_Printf( "  unlock_unlocked_mutex\n" );
			Com_Printf( "  condition_variable_race\n" );
			Com_Printf( "  semaphore_race\n" );
			Com_Printf( "  memory_allocator\n" );
			Com_Printf( "  shared_data_structures\n" );
			Com_Printf( "  thread_pool\n" );
			Com_Printf( "  high_contention\n" );
			return;
		}

		const char* test_name = Cmd_Argv(2);

		thread_safety_test_config_t config;
		memset( &config, 0, sizeof( config ) );
		Q_strncpyz( config.test_name, test_name, sizeof( config.test_name ) );
		Q_strncpyz( config.description, "Manual thread safety test", sizeof( config.description ) );
		config.enable_tsan = thread_safety_test_system.tsan_available;
		config.enable_deadlock_detection = thread_safety_test_system.supports_deadlock_detection;
		config.enable_race_detection = thread_safety_test_system.tsan_available;
		config.num_threads = 4;
		config.iterations = 1000;
		config.timeout_seconds = 30;

		thread_safety_test_result_t result;
		if ( ThreadSafetyTest_RunTest( &config, &result ) ) {
			Com_Printf( "Test completed: %s\n", ThreadSafetyTest_GetResultString( result.result ) );
			Com_Printf( "Duration: %.2f seconds\n", result.duration_ms / 1000.0f );

			if ( result.error_count > 0 ) {
				Com_Printf( "Detected %u error(s):\n", result.error_count );
				for ( uint32_t i = 0; i < result.error_count && i < 5; i++ ) {
					const tsan_error_t* error = &result.errors[i];
					Com_Printf( "  %s: %s\n", ThreadSafetyTest_GetTSanErrorString( error->error_type ), error->description );
				}
			}

			Com_Printf( "Threads Created: %d\n", result.num_threads_created );
			Com_Printf( "Iterations: %d\n", result.test_iterations_completed );
		} else {
			Com_Printf( "Failed to run thread safety test\n" );
		}
	}
	else if ( Q_stricmp( cmd, "suite" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: threadtest suite <create|add|run> ...\n" );
			return;
		}

		const char* subcmd = Cmd_Argv(2);

		if ( Q_stricmp( subcmd, "create" ) == 0 ) {
			if ( Cmd_Argc() < 5 ) {
				Com_Printf( "Usage: threadtest suite create <name> <description>\n" );
				return;
			}

			const char* suite_name = Cmd_Argv(3);
			const char* description = Cmd_Argv(4);

			thread_safety_test_suite_t* suite = ThreadSafetyTest_CreateSuite( suite_name, description );
			if ( suite ) {
				Com_Printf( "Created thread safety test suite: %s\n", suite_name );
				thread_safety_test_system.current_suite = suite;
			} else {
				Com_Printf( "Failed to create test suite\n" );
			}
		}
		else if ( Q_stricmp( subcmd, "add" ) == 0 ) {
			if ( Cmd_Argc() < 5 ) {
				Com_Printf( "Usage: threadtest suite add <suite_name> <test_name>\n" );
				return;
			}

			const char* suite_name = Cmd_Argv(3);
			const char* test_name = Cmd_Argv(4);

			if ( !thread_safety_test_system.current_suite ||
				 strcmp( thread_safety_test_system.current_suite->suite_name, suite_name ) != 0 ) {
				Com_Printf( "Suite '%s' not found or not current\n", suite_name );
				return;
			}

			thread_safety_test_config_t config;
			memset( &config, 0, sizeof( config ) );
			Q_strncpyz( config.test_name, test_name, sizeof( config.test_name ) );
			config.enable_tsan = thread_safety_test_system.tsan_available;
			config.enable_deadlock_detection = thread_safety_test_system.supports_deadlock_detection;
			config.enable_race_detection = thread_safety_test_system.tsan_available;
			config.num_threads = 4;
			config.iterations = 1000;
			config.timeout_seconds = 30;

			if ( ThreadSafetyTest_AddTestToSuite( thread_safety_test_system.current_suite, &config ) ) {
				Com_Printf( "Added test '%s' to suite '%s'\n", test_name, suite_name );
			} else {
				Com_Printf( "Failed to add test to suite\n" );
			}
		}
		else if ( Q_stricmp( subcmd, "run" ) == 0 ) {
			if ( Cmd_Argc() < 4 ) {
				Com_Printf( "Usage: threadtest suite run <suite_name>\n" );
				return;
			}

			const char* suite_name = Cmd_Argv(3);

			if ( !thread_safety_test_system.current_suite ||
				 strcmp( thread_safety_test_system.current_suite->suite_name, suite_name ) != 0 ) {
				Com_Printf( "Suite '%s' not found\n", suite_name );
				return;
			}

			if ( ThreadSafetyTest_RunSuite( thread_safety_test_system.current_suite ) ) {
				Com_Printf( "Suite '%s' completed successfully\n", suite_name );
			} else {
				Com_Printf( "Suite '%s' completed with failures\n", suite_name );
			}
		}
		else {
			Com_Printf( "Unknown suite command: %s\n", subcmd );
		}
	}
	else if ( Q_stricmp( cmd, "generate" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: threadtest generate <race|deadlock|stress>\n" );
			return;
		}

		const char* type = Cmd_Argv(2);
		const char* suite_name = "generated_threadtest_suite";

		thread_safety_test_suite_t* suite = ThreadSafetyTest_CreateSuite(
			suite_name, "Auto-generated thread safety test suite" );

		if ( !suite ) {
			Com_Printf( "Failed to create test suite\n" );
			return;
		}

		qboolean success = qfalse;

		if ( Q_stricmp( type, "race" ) == 0 ) {
			success = ThreadSafetyTest_GenerateRaceConditionTests( suite );
			Com_Printf( "Generated race condition tests\n" );
		} else if ( Q_stricmp( type, "deadlock" ) == 0 ) {
			success = ThreadSafetyTest_GenerateDeadlockTests( suite );
			Com_Printf( "Generated deadlock tests\n" );
		} else if ( Q_stricmp( type, "stress" ) == 0 ) {
			success = ThreadSafetyTest_GenerateStressTests( suite );
			Com_Printf( "Generated stress tests\n" );
		} else {
			Com_Printf( "Unknown generation type: %s\n", type );
			return;
		}

		if ( success ) {
			thread_safety_test_system.current_suite = suite;
			Com_Printf( "Test suite '%s' created with %u tests\n",
				suite_name, suite->num_tests );
		} else {
			Com_Printf( "Failed to generate tests\n" );
		}
	}
	else if ( Q_stricmp( cmd, "results" ) == 0 ) {
		thread_safety_test_result_t* results;
		uint32_t count = ThreadSafetyTest_GetResults( &results );

		if ( count == 0 ) {
			Com_Printf( "No test results available\n" );
			return;
		}

		Com_Printf( "=== Thread Safety Test Results ===\n" );
		for ( uint32_t i = 0; i < count && i < 20; i++ ) { // Show first 20
			const thread_safety_test_result_t* result = &results[i];
			Com_Printf( "%-25s: %s (%.2fs) - %u errors, %d threads\n",
				result->test_name,
				ThreadSafetyTest_GetResultString( result->result ),
				result->duration_ms / 1000.0f,
				result->error_count,
				result->num_threads_created );
		}

		if ( count > 20 ) {
			Com_Printf( "... and %u more results\n", count - 20 );
		}
	}
	else if ( Q_stricmp( cmd, "export" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: threadtest export <directory>\n" );
			return;
		}

		const char* export_dir = Cmd_Argv(2);

		if ( ThreadSafetyTest_ExportForCI( export_dir ) ) {
			Com_Printf( "Exported thread safety test results to: %s\n", export_dir );
		} else {
			Com_Printf( "Failed to export test results\n" );
		}
	}
	else if ( Q_stricmp( cmd, "report" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: threadtest report <output_file>\n" );
			return;
		}

		const char* output_file = Cmd_Argv(2);

		if ( ThreadSafetyTest_GenerateReport( output_file, "JSON" ) ) {
			Com_Printf( "Generated thread safety report: %s\n", output_file );
		} else {
			Com_Printf( "Failed to generate report\n" );
		}
	}
	else {
		Com_Printf( "Unknown command: %s\n", cmd );
		Com_Printf( "Use 'threadtest' with no arguments for help\n" );
	}
}

/*
=================
Com_CodeQuality_f
=================
*/
static void Com_CodeQuality_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: quality <command> [args]\n" );
		Com_Printf( "Commands:\n" );
		Com_Printf( "  status             - Show system status and gate info\n" );
		Com_Printf( "  analyze            - Run full code quality analysis\n" );
		Com_Printf( "  incremental <files> - Run incremental analysis on changed files\n" );
		Com_Printf( "  gates              - List all quality gates\n" );
		Com_Printf( "  gate add <name> <metric> <min> <max> - Add custom gate\n" );
		Com_Printf( "  gate remove <name> - Remove gate\n" );
		Com_Printf( "  gate enable <name> - Enable gate\n" );
		Com_Printf( "  gate disable <name> - Disable gate\n" );
		Com_Printf( "  gate threshold <name> <min> <max> - Set gate thresholds\n" );
		Com_Printf( "  coverage           - Show coverage analysis results\n" );
		Com_Printf( "  complexity         - Show complexity analysis results\n" );
		Com_Printf( "  results            - Show latest analysis results\n" );
		Com_Printf( "  export <dir>       - Export results for CI\n" );
		Com_Printf( "  report <file> <format> - Generate detailed report\n" );
		Com_Printf( "  config load <file> - Load quality configuration\n" );
		Com_Printf( "  config save <file> - Save current configuration\n" );
		Com_Printf( "  strict <on|off>    - Set strict mode\n" );
		Com_Printf( "  ci-check           - Check if CI gates pass\n" );
		Com_Printf( "\nSystem Status: %s\n",
			code_quality_system.initialized ? "Initialized" : "Not initialized");
		return;
	}

	const char* cmd = Cmd_Argv(1);

	if ( Q_stricmp( cmd, "status" ) == 0 ) {
		Com_Printf( "=== Code Quality Analysis System Status ===\n" );
		Com_Printf( "Initialized: %s\n", code_quality_system.initialized ? "Yes" : "No" );
		Com_Printf( "Strict Mode: %s\n", code_quality_system.strict_mode ? "On" : "Off" );

		if ( code_quality_system.initialized ) {
			Com_Printf( "Quality Gates: %u\n", code_quality_system.gate_count );
			Com_Printf( "Min Coverage: %.1f%%\n", code_quality_system.min_coverage_percentage );
			Com_Printf( "Max Complexity: %d\n", code_quality_system.max_cyclomatic_complexity );
			Com_Printf( "Min Maintainability: %.1f\n", code_quality_system.min_maintainability_index );
			Com_Printf( "Max Duplication: %.1f%%\n", code_quality_system.max_duplication_percentage );

			Com_Printf( "\nStatistics:\n" );
			Com_Printf( "Total Analyses Run: %u\n", code_quality_system.total_analyses_run );
			Com_Printf( "Gates Passed: %u\n", code_quality_system.total_gates_passed );
			Com_Printf( "Gates Failed: %u\n", code_quality_system.total_gates_failed );
		}
	}
	else if ( Q_stricmp( cmd, "analyze" ) == 0 ) {
		code_quality_analysis_t analysis;
		memset( &analysis, 0, sizeof( analysis ) );

		if ( CodeQuality_RunAnalysis( &analysis ) ) {
			Com_Printf( "Code quality analysis completed: %s\n", CodeQuality_GetResultString( analysis.result ) );
			Com_Printf( "Duration: %.2f seconds\n", analysis.duration_ms / 1000.0f );

			Com_Printf( "\nCoverage Metrics:\n" );
			Com_Printf( "  Overall Coverage: %.1f%%\n", analysis.overall_coverage );
			Com_Printf( "  Files Analyzed: %d\n", analysis.total_files );
			Com_Printf( "  Functions Covered: %d/%d\n", analysis.covered_functions, analysis.total_functions );

			Com_Printf( "\nComplexity Metrics:\n" );
			Com_Printf( "  Average Complexity: %.1f\n", analysis.average_complexity );
			Com_Printf( "  Maximum Complexity: %d\n", analysis.max_complexity );
			Com_Printf( "  Functions Above Threshold: %d\n", analysis.functions_above_threshold );

			Com_Printf( "\nQuality Scores:\n" );
			Com_Printf( "  Maintainability Index: %.1f\n", analysis.maintainability_index );
			Com_Printf( "  Code Duplication: %.1f%%\n", analysis.duplication_percentage );
			Com_Printf( "  Style Score: %.1f\n", analysis.style_score );
			Com_Printf( "  Security Score: %.1f\n", analysis.security_score );

			if ( analysis.failed_gate_count > 0 ) {
				Com_Printf( "\nFailed Gates (%u):\n", analysis.failed_gate_count );
				for ( uint32_t i = 0; i < analysis.failed_gate_count && i < 5; i++ ) {
					const quality_gate_config_t* gate = &analysis.failed_gates[i];
					Com_Printf( "  %s: %s (%s)\n",
						gate->gate_name,
						gate->description,
						gate->blocking ? "BLOCKING" : "WARNING" );
				}
			} else {
				Com_Printf( "\nAll quality gates passed!\n" );
			}

			// Clean up analysis data
			if ( analysis.coverage_data ) free( analysis.coverage_data );
			if ( analysis.complexity_data ) free( analysis.complexity_data );
			if ( analysis.failed_gates ) free( analysis.failed_gates );
		} else {
			Com_Printf( "Failed to run code quality analysis\n" );
		}
	}
	else if ( Q_stricmp( cmd, "incremental" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: quality incremental <file1> [file2] ...\n" );
			return;
		}

		// Collect changed files
		const char* changed_files[32];
		uint32_t file_count = 0;

		for ( int i = 2; i < Cmd_Argc() && file_count < 32; i++ ) {
			changed_files[file_count++] = Cmd_Argv(i);
		}

		code_quality_analysis_t analysis;
		memset( &analysis, 0, sizeof( analysis ) );
		Q_strncpyz( analysis.analysis_name, "Incremental Code Quality Analysis", sizeof( analysis.analysis_name ) );

		if ( CodeQuality_RunIncrementalAnalysis( changed_files, file_count, &analysis ) ) {
			Com_Printf( "Incremental analysis completed: %s\n", CodeQuality_GetResultString( analysis.result ) );
			Com_Printf( "Files analyzed: %u\n", file_count );

			// Clean up
			if ( analysis.coverage_data ) free( analysis.coverage_data );
			if ( analysis.complexity_data ) free( analysis.complexity_data );
			if ( analysis.failed_gates ) free( analysis.failed_gates );
		} else {
			Com_Printf( "Failed to run incremental analysis\n" );
		}
	}
	else if ( Q_stricmp( cmd, "gates" ) == 0 ) {
		Com_Printf( "=== Quality Gates ===\n" );

		for ( uint32_t i = 0; i < code_quality_system.gate_count; i++ ) {
			const quality_gate_config_t* gate = &code_quality_system.gates[i];
			Com_Printf( "%-20s: %s\n", gate->gate_name, gate->description );
			Com_Printf( "  Metric: %s\n", CodeQuality_GetMetricString( gate->metric_type ) );
			Com_Printf( "  Thresholds: Min=%.1f, Max=%.1f\n",
				gate->minimum_threshold, gate->maximum_threshold );
			Com_Printf( "  Status: %s (%s)\n",
				gate->enabled ? "Enabled" : "Disabled",
				gate->blocking ? "Blocking" : "Warning" );
			Com_Printf( "\n" );
		}

		Com_Printf( "Total Gates: %u\n", code_quality_system.gate_count );
	}
	else if ( Q_stricmp( cmd, "gate" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: quality gate <add|remove|enable|disable|threshold> ...\n" );
			return;
		}

		const char* subcmd = Cmd_Argv(2);

		if ( Q_stricmp( subcmd, "add" ) == 0 ) {
			if ( Cmd_Argc() < 7 ) {
				Com_Printf( "Usage: quality gate add <name> <metric> <min> <max>\n" );
				Com_Printf( "Metrics: coverage, complexity, maintainability, duplication, style, security\n" );
				return;
			}

			const char* gate_name = Cmd_Argv(3);
			const char* metric_str = Cmd_Argv(4);
			float min_threshold = atof( Cmd_Argv(5) );
			float max_threshold = atof( Cmd_Argv(6) );

			quality_metric_type_t metric_type;
			if ( Q_stricmp( metric_str, "coverage" ) == 0 ) metric_type = QUALITY_METRIC_COVERAGE;
			else if ( Q_stricmp( metric_str, "complexity" ) == 0 ) metric_type = QUALITY_METRIC_COMPLEXITY;
			else if ( Q_stricmp( metric_str, "maintainability" ) == 0 ) metric_type = QUALITY_METRIC_MAINTAINABILITY;
			else if ( Q_stricmp( metric_str, "duplication" ) == 0 ) metric_type = QUALITY_METRIC_DUPLICATION;
			else if ( Q_stricmp( metric_str, "style" ) == 0 ) metric_type = QUALITY_METRIC_STYLE;
			else if ( Q_stricmp( metric_str, "security" ) == 0 ) metric_type = QUALITY_METRIC_SECURITY;
			else {
				Com_Printf( "Unknown metric: %s\n", metric_str );
				return;
			}

			quality_gate_config_t gate;
			memset( &gate, 0, sizeof( gate ) );
			Q_strncpyz( gate.gate_name, gate_name, sizeof( gate.gate_name ) );
			Q_strncpyz( gate.description, "Custom quality gate", sizeof( gate.description ) );
			gate.metric_type = metric_type;
			gate.minimum_threshold = min_threshold;
			gate.maximum_threshold = max_threshold;
			gate.enabled = qtrue;
			gate.blocking = qtrue;
			gate.priority = 5;

			if ( CodeQuality_AddGate( &gate ) ) {
				Com_Printf( "Added quality gate: %s\n", gate_name );
			} else {
				Com_Printf( "Failed to add quality gate\n" );
			}
		}
		else if ( Q_stricmp( subcmd, "remove" ) == 0 ) {
			if ( Cmd_Argc() < 4 ) {
				Com_Printf( "Usage: quality gate remove <name>\n" );
				return;
			}

			const char* gate_name = Cmd_Argv(3);

			if ( CodeQuality_RemoveGate( gate_name ) ) {
				Com_Printf( "Removed quality gate: %s\n", gate_name );
			} else {
				Com_Printf( "Failed to remove quality gate: %s\n", gate_name );
			}
		}
		else if ( Q_stricmp( subcmd, "enable" ) == 0 ) {
			if ( Cmd_Argc() < 4 ) {
				Com_Printf( "Usage: quality gate enable <name>\n" );
				return;
			}

			const char* gate_name = Cmd_Argv(3);

			if ( CodeQuality_EnableGate( gate_name ) ) {
				Com_Printf( "Enabled quality gate: %s\n", gate_name );
			} else {
				Com_Printf( "Failed to enable quality gate: %s\n", gate_name );
			}
		}
		else if ( Q_stricmp( subcmd, "disable" ) == 0 ) {
			if ( Cmd_Argc() < 4 ) {
				Com_Printf( "Usage: quality gate disable <name>\n" );
				return;
			}

			const char* gate_name = Cmd_Argv(3);

			if ( CodeQuality_DisableGate( gate_name ) ) {
				Com_Printf( "Disabled quality gate: %s\n", gate_name );
			} else {
				Com_Printf( "Failed to disable quality gate: %s\n", gate_name );
			}
		}
		else if ( Q_stricmp( subcmd, "threshold" ) == 0 ) {
			if ( Cmd_Argc() < 6 ) {
				Com_Printf( "Usage: quality gate threshold <name> <min> <max>\n" );
				return;
			}

			const char* gate_name = Cmd_Argv(3);
			float min_threshold = atof( Cmd_Argv(4) );
			float max_threshold = atof( Cmd_Argv(5) );

			if ( CodeQuality_SetGateThreshold( gate_name, min_threshold, max_threshold ) ) {
				Com_Printf( "Set thresholds for gate '%s': min=%.1f, max=%.1f\n",
					gate_name, min_threshold, max_threshold );
			} else {
				Com_Printf( "Failed to set gate thresholds\n" );
			}
		}
		else {
			Com_Printf( "Unknown gate command: %s\n", subcmd );
		}
	}
	else if ( Q_stricmp( cmd, "coverage" ) == 0 ) {
		Com_Printf( "Coverage analysis not yet implemented (requires test execution)\n" );
		Com_Printf( "Use 'quality analyze' to run full analysis with coverage metrics\n" );
	}
	else if ( Q_stricmp( cmd, "complexity" ) == 0 ) {
		Com_Printf( "Complexity analysis not yet implemented\n" );
		Com_Printf( "Use 'quality analyze' to run full analysis with complexity metrics\n" );
	}
	else if ( Q_stricmp( cmd, "results" ) == 0 ) {
		Com_Printf( "Latest analysis results not available\n" );
		Com_Printf( "Run 'quality analyze' to generate results\n" );
	}
	else if ( Q_stricmp( cmd, "export" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: quality export <directory>\n" );
			return;
		}

		const char* export_dir = Cmd_Argv(2);

		// Run analysis first to get results
		code_quality_analysis_t analysis;
		memset( &analysis, 0, sizeof( analysis ) );

		if ( CodeQuality_RunAnalysis( &analysis ) ) {
			if ( CodeQuality_ExportForCI( &analysis, export_dir ) ) {
				Com_Printf( "Exported code quality results to: %s\n", export_dir );
			} else {
				Com_Printf( "Failed to export results\n" );
			}

			// Clean up
			if ( analysis.coverage_data ) free( analysis.coverage_data );
			if ( analysis.complexity_data ) free( analysis.complexity_data );
			if ( analysis.failed_gates ) free( analysis.failed_gates );
		} else {
			Com_Printf( "Failed to run analysis for export\n" );
		}
	}
	else if ( Q_stricmp( cmd, "report" ) == 0 ) {
		if ( Cmd_Argc() < 4 ) {
			Com_Printf( "Usage: quality report <output_file> <format>\n" );
			Com_Printf( "Formats: json, xml, html, text\n" );
			return;
		}

		const char* output_file = Cmd_Argv(2);
		const char* format = Cmd_Argv(3);

		// Run analysis first to get results
		code_quality_analysis_t analysis;
		memset( &analysis, 0, sizeof( analysis ) );

		if ( CodeQuality_RunAnalysis( &analysis ) ) {
			if ( CodeQuality_GenerateReport( &analysis, output_file, format ) ) {
				Com_Printf( "Generated code quality report: %s (%s)\n", output_file, format );
			} else {
				Com_Printf( "Failed to generate report\n" );
			}

			// Clean up
			if ( analysis.coverage_data ) free( analysis.coverage_data );
			if ( analysis.complexity_data ) free( analysis.complexity_data );
			if ( analysis.failed_gates ) free( analysis.failed_gates );
		} else {
			Com_Printf( "Failed to run analysis for report generation\n" );
		}
	}
	else if ( Q_stricmp( cmd, "config" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: quality config <load|save> <file>\n" );
			return;
		}

		const char* subcmd = Cmd_Argv(2);

		if ( Q_stricmp( subcmd, "load" ) == 0 ) {
			if ( Cmd_Argc() < 4 ) {
				Com_Printf( "Usage: quality config load <file>\n" );
				return;
			}

			const char* config_file = Cmd_Argv(3);

			if ( CodeQuality_LoadConfig( config_file ) ) {
				Com_Printf( "Loaded quality configuration from: %s\n", config_file );
			} else {
				Com_Printf( "Failed to load configuration\n" );
			}
		}
		else if ( Q_stricmp( subcmd, "save" ) == 0 ) {
			if ( Cmd_Argc() < 4 ) {
				Com_Printf( "Usage: quality config save <file>\n" );
				return;
			}

			const char* config_file = Cmd_Argv(3);

			if ( CodeQuality_SaveConfig( config_file ) ) {
				Com_Printf( "Saved quality configuration to: %s\n", config_file );
			} else {
				Com_Printf( "Failed to save configuration\n" );
			}
		}
		else {
			Com_Printf( "Unknown config command: %s\n", subcmd );
		}
	}
	else if ( Q_stricmp( cmd, "strict" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: quality strict <on|off>\n" );
			return;
		}

		const char* mode = Cmd_Argv(2);
		qboolean strict = qfalse;

		if ( Q_stricmp( mode, "on" ) == 0 ) {
			strict = qtrue;
		} else if ( Q_stricmp( mode, "off" ) == 0 ) {
			strict = qfalse;
		} else {
			Com_Printf( "Invalid mode: %s (use 'on' or 'off')\n", mode );
			return;
		}

		if ( CodeQuality_SetStrictMode( strict ) ) {
			Com_Printf( "Strict mode %s\n", strict ? "enabled" : "disabled" );
		} else {
			Com_Printf( "Failed to set strict mode\n" );
		}
	}
	else if ( Q_stricmp( cmd, "ci-check" ) == 0 ) {
		// Run analysis and check CI gates
		code_quality_analysis_t analysis;
		memset( &analysis, 0, sizeof( analysis ) );

		if ( CodeQuality_RunAnalysis( &analysis ) ) {
			qboolean ci_pass = CodeQuality_CheckCIGates( &analysis );

			Com_Printf( "CI Gate Check: %s\n", ci_pass ? "PASS" : "FAIL" );

			if ( analysis.failed_gate_count > 0 ) {
				Com_Printf( "Blocking Issues:\n" );
				for ( uint32_t i = 0; i < analysis.failed_gate_count; i++ ) {
					const quality_gate_config_t* gate = &analysis.failed_gates[i];
					if ( gate->blocking ) {
						Com_Printf( "  ❌ %s: %s\n", gate->gate_name, gate->description );
					}
				}
			} else {
				Com_Printf( "✅ All blocking quality gates passed\n" );
			}

			// Clean up
			if ( analysis.coverage_data ) free( analysis.coverage_data );
			if ( analysis.complexity_data ) free( analysis.complexity_data );
			if ( analysis.failed_gates ) free( analysis.failed_gates );
		} else {
			Com_Printf( "CI Gate Check: FAIL (analysis error)\n" );
		}
	}
	else {
		Com_Printf( "Unknown command: %s\n", cmd );
		Com_Printf( "Use 'quality' with no arguments for help\n" );
	}
}

/*
=================
Com_TechnicalDebt_f
=================
*/
static void Com_TechnicalDebt_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: debt <command> [args]\n" );
		Com_Printf( "Commands:\n" );
		Com_Printf( "  status             - Show debt tracking status and metrics\n" );
		Com_Printf( "  list               - List all debt items\n" );
		Com_Printf( "  add <id> <title> <category> <severity> - Add new debt item\n" );
		Com_Printf( "  resolve <id> [notes] - Mark debt item as resolved\n" );
		Com_Printf( "  assign <id> <assignee> - Assign debt item to person\n" );
		Com_Printf( "  update <id> <field> <value> - Update debt item field\n" );
		Com_Printf( "  remove <id>       - Remove debt item\n" );
		Com_Printf( "  metrics            - Show current debt metrics\n" );
		Com_Printf( "  trends [days]      - Show debt trends over time\n" );
		Com_Printf( "  predict <months>   - Predict future debt levels\n" );
		Com_Printf( "  alerts             - Check for debt alerts\n" );
		Com_Printf( "  analyze            - Run automated debt analysis\n" );
		Com_Printf( "  history            - Show debt history\n" );
		Com_Printf( "  export <dir>       - Export debt data for CI\n" );
		Com_Printf( "  report <file> <format> - Generate debt report\n" );
		Com_Printf( "  config load <file> - Load debt configuration\n" );
		Com_Printf( "  config save <file> - Save current configuration\n" );
		Com_Printf( "  auto <on|off>      - Enable/disable auto tracking\n" );
		Com_Printf( "  alerts <on|off>    - Enable/disable alerts\n" );
		Com_Printf( "  thresholds <critical> <high> - Set alert thresholds\n" );
		Com_Printf( "\nSystem Status: %s\n",
			technical_debt_system.initialized ? "Initialized" : "Not initialized");
		return;
	}

	const char* cmd = Cmd_Argv(1);

	if ( Q_stricmp( cmd, "status" ) == 0 ) {
		Com_Printf( "=== Technical Debt Tracking Status ===\n" );
		Com_Printf( "Initialized: %s\n", technical_debt_system.initialized ? "Yes" : "No" );
		Com_Printf( "Auto Tracking: %s\n", technical_debt_system.auto_tracking_enabled ? "Enabled" : "Disabled" );
		Com_Printf( "Alerts: %s\n", technical_debt_system.alerts_enabled ? "Enabled" : "Disabled" );

		if ( technical_debt_system.initialized ) {
			Com_Printf( "Debt Items: %u\n", technical_debt_system.num_debt_items );
			Com_Printf( "History Points: %u\n", technical_debt_system.num_history_points );
			Com_Printf( "Alert Thresholds: Critical=%d, High=%d\n",
				technical_debt_system.alert_threshold_critical,
				technical_debt_system.alert_threshold_high );
			Com_Printf( "Retention Policy: %d days\n", technical_debt_system.history_retention_days );

			// Show current metrics summary
			const debt_metrics_t* metrics = &technical_debt_system.current_metrics;
			char health_status[32];
			TechnicalDebt_GetDebtHealthStatus(metrics, health_status, sizeof(health_status));

			Com_Printf( "\nCurrent Metrics:\n" );
			Com_Printf( "  Health Status: %s\n", health_status );
			Com_Printf( "  Total Debt Score: %.1f\n", metrics->total_debt_score );
			Com_Printf( "  Unresolved Items: %d\n", metrics->unresolved_items );
			Com_Printf( "  Critical Items: %d\n", metrics->critical_items );
			Com_Printf( "  Debt Velocity: %.2f/day\n", metrics->debt_velocity );
		}
	}
	else if ( Q_stricmp( cmd, "list" ) == 0 ) {
		debt_item_t* items;
		uint32_t count = TechnicalDebt_GetItems( &items );

		Com_Printf( "=== Technical Debt Items (%u total) ===\n", count );

		for ( uint32_t i = 0; i < count && i < 50; i++ ) { // Show first 50
			const debt_item_t* item = &items[i];
			Com_Printf( "%-15s: %s (%s/%s)\n",
				item->item_id,
				item->title,
				TechnicalDebt_GetCategoryString( item->category ),
				TechnicalDebt_GetSeverityString( item->severity ) );

			if ( item->resolved ) {
				Com_Printf( "  Status: RESOLVED\n" );
			} else {
				Com_Printf( "  Status: ACTIVE (Priority: %d)\n", item->priority_score );
				if ( item->assigned_to[0] ) {
					Com_Printf( "  Assigned to: %s\n", item->assigned_to );
				}
				if ( item->estimated_effort > 0 ) {
					Com_Printf( "  Estimated effort: %.1f hours\n", item->estimated_effort );
				}
			}
			Com_Printf( "\n" );
		}

		if ( count > 50 ) {
			Com_Printf( "... and %u more items\n", count - 50 );
		}
	}
	else if ( Q_stricmp( cmd, "add" ) == 0 ) {
		if ( Cmd_Argc() < 6 ) {
			Com_Printf( "Usage: debt add <id> <title> <category> <severity>\n" );
			Com_Printf( "Categories: quality, complexity, coverage, maintainability, duplication, security, performance, architecture, documentation\n" );
			Com_Printf( "Severities: low, medium, high, critical\n" );
			return;
		}

		const char* item_id = Cmd_Argv(2);
		const char* title = Cmd_Argv(3);
		const char* category_str = Cmd_Argv(4);
		const char* severity_str = Cmd_Argv(5);

		debt_category_t category;
		if ( Q_stricmp( category_str, "quality" ) == 0 ) category = DEBT_CATEGORY_CODE_QUALITY;
		else if ( Q_stricmp( category_str, "complexity" ) == 0 ) category = DEBT_CATEGORY_COMPLEXITY;
		else if ( Q_stricmp( category_str, "coverage" ) == 0 ) category = DEBT_CATEGORY_COVERAGE;
		else if ( Q_stricmp( category_str, "maintainability" ) == 0 ) category = DEBT_CATEGORY_MAINTAINABILITY;
		else if ( Q_stricmp( category_str, "duplication" ) == 0 ) category = DEBT_CATEGORY_DUPLICATION;
		else if ( Q_stricmp( category_str, "security" ) == 0 ) category = DEBT_CATEGORY_SECURITY;
		else if ( Q_stricmp( category_str, "performance" ) == 0 ) category = DEBT_CATEGORY_PERFORMANCE;
		else if ( Q_stricmp( category_str, "architecture" ) == 0 ) category = DEBT_CATEGORY_ARCHITECTURE;
		else if ( Q_stricmp( category_str, "documentation" ) == 0 ) category = DEBT_CATEGORY_DOCUMENTATION;
		else {
			Com_Printf( "Unknown category: %s\n", category_str );
			return;
		}

		debt_severity_t severity;
		if ( Q_stricmp( severity_str, "low" ) == 0 ) severity = DEBT_SEVERITY_LOW;
		else if ( Q_stricmp( severity_str, "medium" ) == 0 ) severity = DEBT_SEVERITY_MEDIUM;
		else if ( Q_stricmp( severity_str, "high" ) == 0 ) severity = DEBT_SEVERITY_HIGH;
		else if ( Q_stricmp( severity_str, "critical" ) == 0 ) severity = DEBT_SEVERITY_CRITICAL;
		else {
			Com_Printf( "Unknown severity: %s\n", severity_str );
			return;
		}

		debt_item_t item;
		memset( &item, 0, sizeof( item ) );
		Q_strncpyz( item.item_id, item_id, sizeof( item.item_id ) );
		Q_strncpyz( item.title, title, sizeof( item.title ) );
		Q_strncpyz( item.description, "Added via console command", sizeof( item.description ) );
		item.category = category;
		item.severity = severity;

		if ( TechnicalDebt_AddItem( &item ) ) {
			Com_Printf( "Added technical debt item: %s\n", item_id );
		} else {
			Com_Printf( "Failed to add technical debt item\n" );
		}
	}
	else if ( Q_stricmp( cmd, "resolve" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: debt resolve <id> [resolution_notes]\n" );
			return;
		}

		const char* item_id = Cmd_Argv(2);
		const char* notes = (Cmd_Argc() >= 4) ? Cmd_Argv(3) : NULL;

		if ( TechnicalDebt_ResolveItem( item_id, notes ) ) {
			Com_Printf( "Resolved technical debt item: %s\n", item_id );
		} else {
			Com_Printf( "Failed to resolve technical debt item: %s\n", item_id );
		}
	}
	else if ( Q_stricmp( cmd, "assign" ) == 0 ) {
		if ( Cmd_Argc() < 4 ) {
			Com_Printf( "Usage: debt assign <id> <assignee>\n" );
			return;
		}

		const char* item_id = Cmd_Argv(2);
		const char* assignee = Cmd_Argv(3);

		if ( TechnicalDebt_AssignItem( item_id, assignee ) ) {
			Com_Printf( "Assigned technical debt item '%s' to %s\n", item_id, assignee );
		} else {
			Com_Printf( "Failed to assign technical debt item\n" );
		}
	}
	else if ( Q_stricmp( cmd, "update" ) == 0 ) {
		if ( Cmd_Argc() < 5 ) {
			Com_Printf( "Usage: debt update <id> <field> <value>\n" );
			Com_Printf( "Fields: title, description, severity, effort, tags\n" );
			return;
		}

		const char* item_id = Cmd_Argv(2);
		const char* field = Cmd_Argv(3);
		const char* value = Cmd_Argv(4);

		debt_item_t updates;
		memset( &updates, 0, sizeof( updates ) );

		if ( Q_stricmp( field, "title" ) == 0 ) {
			Q_strncpyz( updates.title, value, sizeof( updates.title ) );
		} else if ( Q_stricmp( field, "description" ) == 0 ) {
			Q_strncpyz( updates.description, value, sizeof( updates.description ) );
		} else if ( Q_stricmp( field, "severity" ) == 0 ) {
			if ( Q_stricmp( value, "low" ) == 0 ) updates.severity = DEBT_SEVERITY_LOW;
			else if ( Q_stricmp( value, "medium" ) == 0 ) updates.severity = DEBT_SEVERITY_MEDIUM;
			else if ( Q_stricmp( value, "high" ) == 0 ) updates.severity = DEBT_SEVERITY_HIGH;
			else if ( Q_stricmp( value, "critical" ) == 0 ) updates.severity = DEBT_SEVERITY_CRITICAL;
			else {
				Com_Printf( "Unknown severity: %s\n", value );
				return;
			}
		} else if ( Q_stricmp( field, "effort" ) == 0 ) {
			updates.estimated_effort = atof( value );
		} else if ( Q_stricmp( field, "tags" ) == 0 ) {
			Q_strncpyz( updates.tags, value, sizeof( updates.tags ) );
		} else {
			Com_Printf( "Unknown field: %s\n", field );
			return;
		}

		if ( TechnicalDebt_UpdateItem( item_id, &updates ) ) {
			Com_Printf( "Updated technical debt item '%s'\n", item_id );
		} else {
			Com_Printf( "Failed to update technical debt item\n" );
		}
	}
	else if ( Q_stricmp( cmd, "remove" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: debt remove <id>\n" );
			return;
		}

		const char* item_id = Cmd_Argv(2);

		if ( TechnicalDebt_RemoveItem( item_id ) ) {
			Com_Printf( "Removed technical debt item: %s\n", item_id );
		} else {
			Com_Printf( "Failed to remove technical debt item: %s\n", item_id );
		}
	}
	else if ( Q_stricmp( cmd, "metrics" ) == 0 ) {
		debt_metrics_t metrics;
		if ( TechnicalDebt_CalculateMetrics( &metrics ) ) {
			Com_Printf( "=== Technical Debt Metrics ===\n" );
			Com_Printf( "Overall Debt Score: %.1f\n", metrics.total_debt_score );
			Com_Printf( "Total Debt Items: %d (%d unresolved)\n",
				metrics.total_debt_items, metrics.unresolved_items );
			Com_Printf( "Critical Items: %d\n", metrics.critical_items );
			Com_Printf( "High Priority Items: %d\n", metrics.high_priority_items );

			Com_Printf( "\nEffort Tracking:\n" );
			Com_Printf( "Estimated Effort: %.1f hours\n", metrics.total_estimated_effort );
			Com_Printf( "Actual Effort: %.1f hours\n", metrics.total_actual_effort );
			Com_Printf( "Effort Efficiency: %.1f%%\n", metrics.effort_efficiency * 100 );

			Com_Printf( "\nTrends:\n" );
			Com_Printf( "Debt Velocity: %.2f points/day\n", metrics.debt_velocity );
			Com_Printf( "Paydown Rate: %.2f items/day\n", metrics.debt_paydown_rate );

			Com_Printf( "\nQuality Integration:\n" );
			Com_Printf( "Code Coverage: %.1f%%\n", metrics.code_coverage );
			Com_Printf( "Avg Complexity: %.1f\n", metrics.avg_complexity );
			Com_Printf( "Maintainability: %.1f\n", metrics.maintainability_index );
			Com_Printf( "Duplication: %.1f%%\n", metrics.duplication_percentage );
		} else {
			Com_Printf( "Failed to calculate debt metrics\n" );
		}
	}
	else if ( Q_stricmp( cmd, "trends" ) == 0 ) {
		int days = 30; // Default 30 days
		if ( Cmd_Argc() >= 3 ) {
			days = atoi( Cmd_Argv(2) );
		}

		debt_metrics_t trends;
		if ( TechnicalDebt_AnalyzeTrends( &trends, days ) ) {
			Com_Printf( "=== Debt Trends (last %d days) ===\n", days );
			Com_Printf( "Debt Velocity: %.2f points/day\n", trends.debt_velocity );
			Com_Printf( "Paydown Rate: %.2f items/day\n", trends.debt_paydown_rate );

			if ( trends.debt_velocity > 0 ) {
				Com_Printf( "⚠️  Debt is accumulating at %.1f points per day\n", trends.debt_velocity );
			} else if ( trends.debt_velocity < -1 ) {
				Com_Printf( "✅ Debt is being reduced at %.1f points per day\n", -trends.debt_velocity );
			} else {
				Com_Printf( "📊 Debt level is stable\n" );
			}
		} else {
			Com_Printf( "Insufficient historical data for trend analysis\n" );
		}
	}
	else if ( Q_stricmp( cmd, "predict" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: debt predict <months>\n" );
			return;
		}

		int months = atoi( Cmd_Argv(2) );
		float projected_score;

		if ( TechnicalDebt_PredictFutureDebt( &projected_score, months ) ) {
			Com_Printf( "Projected debt score in %d months: %.1f\n", months, projected_score );
			Com_Printf( "Current debt score: %.1f\n", technical_debt_system.current_metrics.total_debt_score );

			float change = projected_score - technical_debt_system.current_metrics.total_debt_score;
			if ( change > 10 ) {
				Com_Printf( "⚠️  Significant debt increase projected\n" );
			} else if ( change < -10 ) {
				Com_Printf( "✅ Debt reduction projected\n" );
			} else {
				Com_Printf( "📊 Debt level projected to remain stable\n" );
			}
		} else {
			Com_Printf( "Failed to predict future debt levels\n" );
		}
	}
	else if ( Q_stricmp( cmd, "alerts" ) == 0 ) {
		char alert_message[2048];
		if ( TechnicalDebt_CheckAlerts( alert_message, sizeof( alert_message ) ) ) {
			Com_Printf( "Technical Debt Alerts:\n%s\n", alert_message );
		} else {
			Com_Printf( "No technical debt alerts at this time\n" );
		}
	}
	else if ( Q_stricmp( cmd, "analyze" ) == 0 ) {
		if ( TechnicalDebt_RunAutomatedAnalysis() ) {
			Com_Printf( "Automated technical debt analysis completed\n" );

			// Show updated metrics
			const debt_metrics_t* metrics = &technical_debt_system.current_metrics;
			Com_Printf( "Current debt score: %.1f (%d unresolved items)\n",
				metrics->total_debt_score, metrics->unresolved_items );
		} else {
			Com_Printf( "Failed to run automated debt analysis\n" );
		}
	}
	else if ( Q_stricmp( cmd, "history" ) == 0 ) {
		debt_history_point_t* history;
		uint32_t count = TechnicalDebt_GetHistory( &history );

		Com_Printf( "=== Debt History (%u points) ===\n", count );

		for ( uint32_t i = 0; i < count && i < 20; i++ ) { // Show last 20
			const debt_history_point_t* point = &history[count - 1 - i]; // Most recent first
			uint64_t age_hours = (Sys_Milliseconds() - point->timestamp) / (1000 * 60 * 60);

			Com_Printf( "%.1f hours ago: Score=%.1f, Items=%d, Critical=%d\n",
				age_hours / 1.0f,
				point->metrics.total_debt_score,
				point->metrics.unresolved_items,
				point->metrics.critical_items );
		}
	}
	else if ( Q_stricmp( cmd, "export" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: debt export <directory>\n" );
			return;
		}

		const char* export_dir = Cmd_Argv(2);

		if ( TechnicalDebt_ExportForCI( export_dir ) ) {
			Com_Printf( "Exported technical debt data to: %s\n", export_dir );
		} else {
			Com_Printf( "Failed to export debt data\n" );
		}
	}
	else if ( Q_stricmp( cmd, "report" ) == 0 ) {
		if ( Cmd_Argc() < 4 ) {
			Com_Printf( "Usage: debt report <output_file> <format>\n" );
			Com_Printf( "Formats: json, html, csv, text\n" );
			return;
		}

		const char* output_file = Cmd_Argv(2);
		const char* format = Cmd_Argv(3);

		if ( TechnicalDebt_GenerateReport( output_file, format ) ) {
			Com_Printf( "Generated technical debt report: %s (%s)\n", output_file, format );
		} else {
			Com_Printf( "Failed to generate debt report\n" );
		}
	}
	else if ( Q_stricmp( cmd, "config" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: debt config <load|save> <file>\n" );
			return;
		}

		const char* subcmd = Cmd_Argv(2);

		if ( Q_stricmp( subcmd, "load" ) == 0 ) {
			if ( Cmd_Argc() < 4 ) {
				Com_Printf( "Usage: debt config load <file>\n" );
				return;
			}

			const char* config_file = Cmd_Argv(3);

			if ( TechnicalDebt_LoadConfig( config_file ) ) {
				Com_Printf( "Loaded technical debt configuration from: %s\n", config_file );
			} else {
				Com_Printf( "Failed to load debt configuration\n" );
			}
		}
		else if ( Q_stricmp( subcmd, "save" ) == 0 ) {
			if ( Cmd_Argc() < 4 ) {
				Com_Printf( "Usage: debt config save <file>\n" );
				return;
			}

			const char* config_file = Cmd_Argv(3);

			if ( TechnicalDebt_SaveConfig( config_file ) ) {
				Com_Printf( "Saved technical debt configuration to: %s\n", config_file );
			} else {
				Com_Printf( "Failed to save debt configuration\n" );
			}
		}
		else {
			Com_Printf( "Unknown config command: %s\n", subcmd );
		}
	}
	else if ( Q_stricmp( cmd, "auto" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: debt auto <on|off>\n" );
			return;
		}

		const char* mode = Cmd_Argv(2);
		qboolean enable = qfalse;

		if ( Q_stricmp( mode, "on" ) == 0 ) {
			enable = qtrue;
		} else if ( Q_stricmp( mode, "off" ) == 0 ) {
			enable = qfalse;
		} else {
			Com_Printf( "Invalid mode: %s (use 'on' or 'off')\n", mode );
			return;
		}

		if ( TechnicalDebt_EnableAutoTracking( enable ) ) {
			Com_Printf( "Auto tracking %s\n", enable ? "enabled" : "disabled" );
		} else {
			Com_Printf( "Failed to set auto tracking\n" );
		}
	}
	else if ( Q_stricmp( cmd, "alerts" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: debt alerts <on|off>\n" );
			return;
		}

		const char* mode = Cmd_Argv(2);
		// Note: This conflicts with the alerts checking command above
		// This sets alert enable/disable, the other checks alerts
		Com_Printf( "Use 'debt alerts' without arguments to check alerts\n" );
		Com_Printf( "Alerts are currently %s\n",
			technical_debt_system.alerts_enabled ? "enabled" : "disabled" );
	}
	else if ( Q_stricmp( cmd, "thresholds" ) == 0 ) {
		if ( Cmd_Argc() < 4 ) {
			Com_Printf( "Usage: debt thresholds <critical> <high>\n" );
			return;
		}

		int critical = atoi( Cmd_Argv(2) );
		int high = atoi( Cmd_Argv(3) );

		if ( TechnicalDebt_SetAlertThresholds( critical, high ) ) {
			Com_Printf( "Set alert thresholds: critical=%d, high=%d\n", critical, high );
		} else {
			Com_Printf( "Failed to set alert thresholds\n" );
		}
	}
	else {
		Com_Printf( "Unknown command: %s\n", cmd );
		Com_Printf( "Use 'debt' with no arguments for help\n" );
	}
}

/*
=================
Com_PerformanceBenchmark_f
=================
*/
static void Com_PerformanceBenchmark_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: benchmark <command> [args]\n" );
		Com_Printf( "Commands:\n" );
		Com_Printf( "  status              - Show benchmarking system status\n" );
		Com_Printf( "  create <suite_name> - Create a new benchmark suite\n" );
		Com_Printf( "  list                - List all benchmark suites\n" );
		Com_Printf( "  add-rendering <suite> <map> <quality> - Add rendering benchmark\n" );
		Com_Printf( "  add-memory <suite> <alloc_size> <count> - Add memory benchmark\n" );
		Com_Printf( "  add-io <suite> <file> <size_mb> - Add I/O benchmark\n" );
		Com_Printf( "  run <suite_name>    - Run a benchmark suite\n" );
		Com_Printf( "  results             - Show recent benchmark results\n" );
		Com_Printf( "  compare <baseline> <current> - Compare benchmark results\n" );
		Com_Printf( "  report <format>     - Generate benchmark report\n" );
		Com_Printf( "  export <dir>        - Export results for CI\n" );
		Com_Printf( "  baseline update <id> - Update baseline for benchmark\n" );
		Com_Printf( "  baseline reset <id> - Reset baseline for benchmark\n" );
		Com_Printf( "  hardware            - Show detected hardware configuration\n" );
		Com_Printf( "  cancel              - Cancel currently running benchmark\n" );
		Com_Printf( "\nSystem Status: %s\n",
			benchmark_system.initialized ? "Initialized" : "Not initialized");
		return;
	}

	const char* cmd = Cmd_Argv(1);

	if ( Q_stricmp( cmd, "status" ) == 0 ) {
		Com_Printf( "=== Performance Benchmarking Status ===\n" );
		Com_Printf( "Initialized: %s\n", benchmark_system.initialized ? "Yes" : "No" );

		if ( benchmark_system.initialized ) {
			Com_Printf( "Suites: %u/%u\n", benchmark_system.suite_count, benchmark_system.max_suites );
			Com_Printf( "Results: %u/%u\n", benchmark_system.result_count, benchmark_system.max_results );
			Com_Printf( "Auto Baseline Update: %s\n", benchmark_system.auto_baseline_update ? "Enabled" : "Disabled" );
			Com_Printf( "Regression Alerts: %s\n", benchmark_system.enable_regression_alerts ? "Enabled" : "Disabled" );
			Com_Printf( "Hardware Profiling: %s\n", benchmark_system.enable_hardware_profiling ? "Enabled" : "Disabled" );
			Com_Printf( "Currently Running: %s\n", benchmark_system.currently_running ? benchmark_system.current_benchmark : "None" );

			if ( benchmark_system.cpu_cores > 0 ) {
				Com_Printf( "\nHardware Configuration:\n" );
				Com_Printf( "  CPU: %s (%d cores)\n", benchmark_system.cpu_model, benchmark_system.cpu_cores );
				Com_Printf( "  RAM: %d MB\n", benchmark_system.ram_mb );
				Com_Printf( "  GPU: %s (%d MB VRAM)\n", benchmark_system.gpu_model, benchmark_system.vram_mb );
			}
		}
	}
	else if ( Q_stricmp( cmd, "create" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: benchmark create <suite_name>\n" );
			return;
		}

		const char* suite_name = Cmd_Argv(2);
		benchmark_suite_t* suite = Benchmark_CreateSuite( suite_name, va("Benchmark suite: %s", suite_name) );

		if ( suite ) {
			Com_Printf( "Created benchmark suite: %s\n", suite_name );
		} else {
			Com_Printf( "Failed to create benchmark suite\n" );
		}
	}
	else if ( Q_stricmp( cmd, "list" ) == 0 ) {
		Com_Printf( "=== Benchmark Suites ===\n" );

		for ( uint32_t i = 0; i < benchmark_system.suite_count; i++ ) {
			const benchmark_suite_t* suite = &benchmark_system.suites[i];
			Com_Printf( "%s: %s (%u benchmarks)\n",
				suite->suite_name, suite->description, suite->benchmark_count );
		}

		if ( benchmark_system.suite_count == 0 ) {
			Com_Printf( "No benchmark suites defined\n" );
		}
	}
	else if ( Q_stricmp( cmd, "add-rendering" ) == 0 ) {
		if ( Cmd_Argc() < 5 ) {
			Com_Printf( "Usage: benchmark add-rendering <suite> <map> <quality>\n" );
			Com_Printf( "Quality presets: 0=Potato, 1=Low, 2=Medium, 3=High, 4=Ultra\n" );
			return;
		}

		const char* suite_name = Cmd_Argv(2);
		const char* map_name = Cmd_Argv(3);
		int quality = atoi( Cmd_Argv(4) );

		// Find suite
		benchmark_suite_t* suite = NULL;
		for ( uint32_t i = 0; i < benchmark_system.suite_count; i++ ) {
			if ( Q_stricmp( benchmark_system.suites[i].suite_name, suite_name ) == 0 ) {
				suite = &benchmark_system.suites[i];
				break;
			}
		}

		if ( !suite ) {
			Com_Printf( "Benchmark suite not found: %s\n", suite_name );
			return;
		}

		if ( Benchmark_AddRenderingBenchmark( suite, map_name, quality ) ) {
			Com_Printf( "Added rendering benchmark to suite %s\n", suite_name );
		} else {
			Com_Printf( "Failed to add rendering benchmark\n" );
		}
	}
	else if ( Q_stricmp( cmd, "add-memory" ) == 0 ) {
		if ( Cmd_Argc() < 5 ) {
			Com_Printf( "Usage: benchmark add-memory <suite> <alloc_size> <count>\n" );
			return;
		}

		const char* suite_name = Cmd_Argv(2);
		int alloc_size = atoi( Cmd_Argv(3) );
		int count = atoi( Cmd_Argv(4) );

		// Find suite
		benchmark_suite_t* suite = NULL;
		for ( uint32_t i = 0; i < benchmark_system.suite_count; i++ ) {
			if ( Q_stricmp( benchmark_system.suites[i].suite_name, suite_name ) == 0 ) {
				suite = &benchmark_system.suites[i];
				break;
			}
		}

		if ( !suite ) {
			Com_Printf( "Benchmark suite not found: %s\n", suite_name );
			return;
		}

		if ( Benchmark_AddMemoryBenchmark( suite, alloc_size, count ) ) {
			Com_Printf( "Added memory benchmark to suite %s\n", suite_name );
		} else {
			Com_Printf( "Failed to add memory benchmark\n" );
		}
	}
	else if ( Q_stricmp( cmd, "add-io" ) == 0 ) {
		if ( Cmd_Argc() < 5 ) {
			Com_Printf( "Usage: benchmark add-io <suite> <file> <size_mb>\n" );
			return;
		}

		const char* suite_name = Cmd_Argv(2);
		const char* test_file = Cmd_Argv(3);
		int size_mb = atoi( Cmd_Argv(4) );

		// Find suite
		benchmark_suite_t* suite = NULL;
		for ( uint32_t i = 0; i < benchmark_system.suite_count; i++ ) {
			if ( Q_stricmp( benchmark_system.suites[i].suite_name, suite_name ) == 0 ) {
				suite = &benchmark_system.suites[i];
				break;
			}
		}

		if ( !suite ) {
			Com_Printf( "Benchmark suite not found: %s\n", suite_name );
			return;
		}

		if ( Benchmark_AddIOBenchmark( suite, test_file, size_mb ) ) {
			Com_Printf( "Added I/O benchmark to suite %s\n", suite_name );
		} else {
			Com_Printf( "Failed to add I/O benchmark\n" );
		}
	}
	else if ( Q_stricmp( cmd, "run" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: benchmark run <suite_name>\n" );
			return;
		}

		const char* suite_name = Cmd_Argv(2);

		// Find suite
		benchmark_suite_t* suite = NULL;
		for ( uint32_t i = 0; i < benchmark_system.suite_count; i++ ) {
			if ( Q_stricmp( benchmark_system.suites[i].suite_name, suite_name ) == 0 ) {
				suite = &benchmark_system.suites[i];
				break;
			}
		}

		if ( !suite ) {
			Com_Printf( "Benchmark suite not found: %s\n", suite_name );
			return;
		}

		Com_Printf( "Starting benchmark suite: %s\n", suite_name );

		if ( Benchmark_RunSuite( suite ) ) {
			Com_Printf( "Benchmark suite completed successfully\n" );
		} else {
			Com_Printf( "Benchmark suite failed or had regressions\n" );
		}
	}
	else if ( Q_stricmp( cmd, "results" ) == 0 ) {
		benchmark_result_t* results;
		uint32_t count = Benchmark_GetResults( &results );

		Com_Printf( "=== Recent Benchmark Results (%u total) ===\n", count );

		for ( uint32_t i = 0; i < count && i < 20; i++ ) { // Show last 20
			const benchmark_result_t* result = &results[count - 1 - i]; // Most recent first
			uint64_t age_minutes = (Sys_Milliseconds() - result->start_time) / (1000 * 60);

			Com_Printf( "%.1f minutes ago: %s (%s) - %.2fs",
				age_minutes / 1.0f,
				result->benchmark_id,
				Benchmark_GetResultString( result->result ),
				result->duration_ms / 1000.0f );

			if ( result->overall_score > 0 ) {
				Com_Printf( " - Score: %.1f", result->overall_score );
			}

			if ( result->has_regression ) {
				Com_Printf( " ⚠️ REGRESSION" );
			}

			Com_Printf( "\n" );
		}
	}
	else if ( Q_stricmp( cmd, "compare" ) == 0 ) {
		if ( Cmd_Argc() < 4 ) {
			Com_Printf( "Usage: benchmark compare <baseline_run_id> <current_run_id>\n" );
			return;
		}

		const char* baseline_id = Cmd_Argv(2);
		const char* current_id = Cmd_Argv(3);

		const benchmark_result_t* baseline = Benchmark_GetResultById( baseline_id );
		const benchmark_result_t* current = Benchmark_GetResultById( current_id );

		if ( !baseline || !current ) {
			Com_Printf( "Benchmark result not found\n" );
			return;
		}

		char regression_report[2048];
		if ( Benchmark_DetectRegressions( current, baseline, regression_report, sizeof( regression_report ) ) ) {
			Com_Printf( "Performance Regressions Detected:\n%s\n", regression_report );
		} else {
			Com_Printf( "No significant performance regressions detected\n" );
		}
	}
	else if ( Q_stricmp( cmd, "report" ) == 0 ) {
		const char* format = "text";
		if ( Cmd_Argc() >= 3 ) {
			format = Cmd_Argv(2);
		}

		benchmark_result_t* results;
		uint32_t count = Benchmark_GetResults( &results );

		if ( count > 0 ) {
			char report_file[256];
			Com_sprintf( report_file, sizeof( report_file ), "benchmark_report.%s", format );

			if ( Benchmark_GenerateReport( results, count, report_file, format ) ) {
				Com_Printf( "Generated benchmark report: %s\n", report_file );
			} else {
				Com_Printf( "Failed to generate benchmark report\n" );
			}
		} else {
			Com_Printf( "No benchmark results available\n" );
		}
	}
	else if ( Q_stricmp( cmd, "export" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: benchmark export <directory>\n" );
			return;
		}

		const char* export_dir = Cmd_Argv(2);

		benchmark_result_t* results;
		uint32_t count = Benchmark_GetResults( &results );

		if ( Benchmark_ExportForCI( results, count, export_dir ) ) {
			Com_Printf( "Exported benchmark results to: %s\n", export_dir );
		} else {
			Com_Printf( "Failed to export benchmark results\n" );
		}
	}
	else if ( Q_stricmp( cmd, "baseline" ) == 0 ) {
		if ( Cmd_Argc() < 3 ) {
			Com_Printf( "Usage: benchmark baseline <update|reset> <benchmark_id>\n" );
			return;
		}

		const char* subcmd = Cmd_Argv(2);
		const char* benchmark_id = Cmd_Argv(3);

		if ( Q_stricmp( subcmd, "update" ) == 0 ) {
			// Find most recent result for this benchmark
			benchmark_result_t* results;
			uint32_t count = Benchmark_GetResults( &results );

			const benchmark_result_t* latest_result = NULL;
			for ( uint32_t i = 0; i < count; i++ ) {
				if ( Q_stricmp( results[i].benchmark_id, benchmark_id ) == 0 ) {
					if ( !latest_result || results[i].start_time > latest_result->start_time ) {
						latest_result = &results[i];
					}
				}
			}

			if ( latest_result ) {
				if ( Benchmark_UpdateBaseline( benchmark_id, latest_result ) ) {
					Com_Printf( "Updated baseline for benchmark: %s\n", benchmark_id );
				} else {
					Com_Printf( "Failed to update baseline\n" );
				}
			} else {
				Com_Printf( "No results found for benchmark: %s\n", benchmark_id );
			}
		}
		else if ( Q_stricmp( subcmd, "reset" ) == 0 ) {
			if ( Benchmark_ResetBaseline( benchmark_id ) ) {
				Com_Printf( "Reset baseline for benchmark: %s\n", benchmark_id );
			} else {
				Com_Printf( "Failed to reset baseline\n" );
			}
		}
		else {
			Com_Printf( "Unknown baseline command: %s\n", subcmd );
		}
	}
	else if ( Q_stricmp( cmd, "hardware" ) == 0 ) {
		char hardware_info[512];
		if ( Benchmark_ProfileHardware( hardware_info, sizeof( hardware_info ) ) ) {
			Com_Printf( "Hardware Configuration:\n%s\n", hardware_info );
		} else {
			Com_Printf( "Failed to profile hardware\n" );
		}
	}
	else if ( Q_stricmp( cmd, "cancel" ) == 0 ) {
		if ( Benchmark_CancelCurrentBenchmark() ) {
			Com_Printf( "Cancelled currently running benchmark\n" );
		} else {
			Com_Printf( "No benchmark currently running\n" );
		}
	}
	else {
		Com_Printf( "Unknown command: %s\n", cmd );
		Com_Printf( "Use 'benchmark' with no arguments for help\n" );
	}
}
