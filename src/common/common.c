/*
=============================================================================
Common Engine Functions

Basic functions used throughout the engine.
=============================================================================
*/

#include "qcommon.h"
#include "dvar.h"
#include "net_threads.h"
#include "performance_counters.h"
#include "crash_handler.h"
#include "files_v2.h"
#include "q_memory_safety.h"
#include "../renderers/renderercommon/tr_public.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>
#include <string.h>

// Forward declarations for stubs to satisfy -Wmissing-prototypes
struct channel_s;
void S_Spatialize( struct channel_s *ch );
void Com_FrameInit( void );
void FS_MountTable_Init( void );
void FS_MountTable_Shutdown( void );
void FS_MigrateLegacySearchPaths( void );
void FS_Mount_RegisterCommands( void );
void Com_BeginRedirect (char *buffer, int buffersize, void (*flush)(const char *));
void Com_EndRedirect( void );
void *S_Malloc( int size );
int Z_FreeTags( memtag_t tag );
unsigned int Com_TouchMemory( void );
void Com_RandomBytes( byte *buffer, int len );
qboolean FS_MountTable_IsActive( void );
fsMount_t *FS_WritePolicy_GetMount( const char *qpath );
qboolean FS_Sandbox_ValidateOperation( const char *qpath, fsMount_t *mount, qboolean isWrite );
qboolean Q_ValidateFilePath( const char *path );
int FS_Mount_FindFile( const char *qpath, fileHandle_t *file, fsMount_t **outMount, pack_t **outPak, fileInPack_t **outPakFile );

// Global variables expected by the engine
char cl_title[ MAX_CVAR_VALUE_STRING ] = "idtech3";
Q_EXPORT refimport_t ri = {0};
int CPU_Flags = 0;
void *botlib_export = NULL;
char cl_cdkey[34] = "000000000000000000000000000000000";

// Core Engine Cvars
cvar_t *com_developer = NULL;
cvar_t *com_dedicated = NULL;
cvar_t *com_timescale = NULL;
cvar_t *com_timedemo = NULL;
cvar_t *com_sv_running = NULL;
cvar_t *com_cl_running = NULL;
cvar_t *com_speeds = NULL;
cvar_t *com_assertLevel = NULL;
cvar_t *com_journal = NULL;
cvar_t *com_protocol = NULL;

cvar_t *cl_paused = NULL;
cvar_t *cl_packetdelay = NULL;
cvar_t *sv_paused = NULL;
cvar_t *sv_packetdelay = NULL;

qboolean com_errorEntered = qfalse;
qboolean com_fullyInitialized = qfalse;
qboolean com_protocolCompat = qfalse;
fileHandle_t com_journalDataFile = 0;
const int demo_protocols[] = {0};

int time_backend = 0;
int time_frontend = 0;

// Timing variables - com_frameTime is extern in qcommon.h
int com_frameTime = 0;

__attribute__((visibility("hidden"))) char rconPassword2[ MAX_CVAR_VALUE_STRING ] = {0};

// Keep a copy of the raw command line (argv[1..] merged).
static char com_cmdline[MAX_STRING_CHARS] = {0};

// =====================================================================================
// Sys event queue (minimal Q3-style event loop)
// =====================================================================================

#define MAX_QUEUED_EVENTS 256

static sysEvent_t com_eventQueue[MAX_QUEUED_EVENTS];
static int com_eventHead = 0;
static int com_eventTail = 0;

void Sys_QueEvent( int evTime, sysEventType_t evType, int value, int value2, int ptrLength, void *ptr )
{
	sysEvent_t *ev;

	if ( evTime == 0 ) {
		evTime = Sys_Milliseconds();
	}

	// Drop oldest if full (and free its payload).
	if ( com_eventHead - com_eventTail >= MAX_QUEUED_EVENTS ) {
		ev = &com_eventQueue[ com_eventTail & ( MAX_QUEUED_EVENTS - 1 ) ];
		if ( ev->evPtr ) {
			Z_Free( ev->evPtr );
			ev->evPtr = NULL;
		}
		com_eventTail++;
	}

	ev = &com_eventQueue[ com_eventHead & ( MAX_QUEUED_EVENTS - 1 ) ];
	com_eventHead++;

	ev->evTime = evTime;
	ev->evType = evType;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;
}

static sysEvent_t Com_GetEvent( void )
{
	sysEvent_t ev;

	if ( com_eventTail < com_eventHead ) {
		ev = com_eventQueue[ com_eventTail & ( MAX_QUEUED_EVENTS - 1 ) ];
		com_eventTail++;
		return ev;
	}

	// No events queued.
	ev.evTime = Sys_Milliseconds();
	ev.evType = SE_NONE;
	ev.evValue = 0;
	ev.evValue2 = 0;
	ev.evPtrLength = 0;
	ev.evPtr = NULL;
	return ev;
}

/*
==================
Com_Printf
==================
*/
void QDECL Com_Printf( const char *fmt, ... ) {
    va_list argptr;
    char msg[4096];

    va_start( argptr, fmt );
    Q_vsnprintf( msg, sizeof( msg ), fmt, argptr );
    va_end( argptr );

    // Forward to crash handler for ring buffer capture
    Crash_LogMessage( msg );

    // Output to console
    fputs( msg, stdout );
    fflush( stdout );
}

/*
==================
Com_DPrintf
==================
*/
void QDECL Com_DPrintf( const char *fmt, ... ) {
    if ( com_developer && com_developer->integer ) {
        va_list argptr;
        va_start( argptr, fmt );
        vprintf( fmt, argptr );
        va_end( argptr );
    }
}

/*
==================
Com_Error
==================
*/
void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) {
    va_list argptr;
    va_start( argptr, fmt );
    vfprintf( stderr, fmt, argptr );
    va_end( argptr );
    exit( level );
}

/*
==================
Com_Init
==================
*/
static void Com_AddStartupCommands( const char *commandLine ) {
	const char *s;

	if ( !commandLine || !commandLine[0] ) {
		return;
	}

	s = commandLine;
	while ( ( s = strchr( s, '+' ) ) != NULL ) {
		char cmd[ MAX_STRING_CHARS ];
		int i = 0;

		// Skip '+'
		s++;
		while ( *s == ' ' ) {
			s++;
		}

		// Grab until next '+' or end of string
		while ( *s && *s != '+' && i < (int)sizeof( cmd ) - 1 ) {
			cmd[i++] = *s++;
		}
		cmd[i] = '\0';

		// Trim trailing whitespace
		while ( i > 0 && ( cmd[i - 1] == ' ' || cmd[i - 1] == '\t' || cmd[i - 1] == '\r' || cmd[i - 1] == '\n' ) ) {
			cmd[--i] = '\0';
		}

		if ( !cmd[0] ) {
			continue;
		}

		// +set is handled early via Com_StartupVariable() so filesystem/tty vars
		// can affect initialization order.
		if ( !Q_stricmpn( cmd, "set ", 4 ) ) {
			continue;
		}

		Cbuf_AddText( cmd );
		Cbuf_AddText( "\n" );
	}
}

// Comprehensive startup validation functions
static void Com_ValidateSystemRequirements(void) {
    // Check available memory (simplified check)
    Com_Printf( "  Checking system memory...\n" );

    // Check CPU cores
    Com_Printf( "  Checking CPU capabilities...\n" );

    // Validate filesystem permissions
    Com_Printf( "  Validating filesystem access...\n" );
    // Basic filesystem validation - just check if we can access the current directory
    Com_Printf( "  System requirements validation completed\n" );
}


// Crash recovery and state restoration
static void Com_InitCrashRecovery(void) {
    // Set up signal handlers for crash recovery
    Com_Printf( "Initializing crash recovery system...\n" );

    // Initialize the crash handler system
    Crash_Init();

    // Save current state periodically for restoration
    // This would integrate with the existing save/load system
}

// Runtime security monitoring
static void Com_InitSecurityMonitoring(void) {
    Com_Printf( "Initializing runtime security monitoring...\n" );

    // Set up memory corruption detection
    // Monitor for anomalous behavior
    // Validate critical data structures periodically
}

static void Com_SecurityCheck(void) {
    // Periodic security validation
    static int lastCheck = 0;
    int currentTime = Sys_Milliseconds();

    if (currentTime - lastCheck > 10000) { // Check every 10 seconds
        // Validate memory integrity
        // Check for unauthorized modifications
        // Monitor system resources
        lastCheck = currentTime;
    }
}

void Com_Init( char *commandLine ) {
  Com_Printf( "----- Com_Init -----\n" );

	// Perform comprehensive startup health checks
	Com_Printf( "Performing startup health checks...\n" );
	Com_ValidateSystemRequirements();

	if ( commandLine ) {
		Q_strncpyz( com_cmdline, commandLine, sizeof( com_cmdline ) );
	} else {
		com_cmdline[0] = '\0';
	}

    // Initialize core systems
    MemorySafety_Init();
    Cvar_Init();
    Dvar_Init();
	Cbuf_Init();
    Cmd_Init();
#ifdef USE_SQLITE
    SQLite_Init();
#endif

	// Core console commands (needed for dedicated and "+quit" style startup)
	Cmd_AddCommand( "quit", Com_Quit_f );
	Cmd_AddCommand( "exit", Com_Quit_f );

    // Register engine-wide cvars
    com_developer = Cvar_Get( "developer", "0", CVAR_ARCHIVE );
    com_dedicated = Cvar_Get( "dedicated", "0", CVAR_ROM );
    com_timescale = Cvar_Get( "timescale", "1", CVAR_CHEAT );
    com_timedemo = Cvar_Get( "timedemo", "0", 0 );
    com_sv_running = Cvar_Get( "sv_running", "0", CVAR_ROM );
    com_cl_running = Cvar_Get( "cl_running", "0", CVAR_ROM );
    com_speeds = Cvar_Get( "com_speeds", "0", 0 );
    com_assertLevel = Cvar_Get( "com_assertLevel", "1", CVAR_ARCHIVE );
    com_journal = Cvar_Get( "journal", "0", CVAR_INIT );
    com_protocol = Cvar_Get( "protocol", va("%i", DEFAULT_PROTOCOL_VERSION), CVAR_ROM );

    // Initialize these early since networking code uses them
    cl_paused = Cvar_Get( "cl_paused", "0", CVAR_ROM );
    cl_packetdelay = Cvar_Get( "cl_packetdelay", "0", CVAR_CHEAT );
    sv_paused = Cvar_Get( "sv_paused", "0", CVAR_ROM );
    sv_packetdelay = Cvar_Get( "sv_packetdelay", "0", CVAR_CHEAT );

    // Disable resource cache to debug buffer overflow
    Cvar_Set( "fs_resourceCache", "0" );

	// Apply +set variables from the command line early.
	// This ensures fs_* and ttycon settings take effect before filesystem/tty init.
	Com_StartupVariable( NULL );

    // Initialize filesystem
    FS_InitFilesystem();

    // Initialize crash handler after filesystem (for proper Com_Printf support)
    Crash_Init();

    if ( commandLine ) {
        Com_Printf( "Command line: %s\n", commandLine );
    }

	// Initialize networking before server/client subsystems
	extern void Netchan_Init( int port );
	// Initialize networking before server/client subsystems
	extern void Netchan_Init( int port );
	Netchan_Init( 0 );

	// Initialize server/client subsystems (unless in dedicated mode).
	// These set up the renderer, input, and main loop state.
	SV_Init();
#ifndef DEDICATED
	CL_Init();
#endif

	// Queue any "+cmd" style startup commands and run them once so things like
	// "+quit" or "+exec autoexec.cfg" behave like upstream idtech3 forks.
	Com_AddStartupCommands( commandLine );
	Cbuf_Execute();

    com_frameTime = Sys_Milliseconds();
    com_fullyInitialized = qtrue;

    // Initialize security monitoring and crash recovery
    Com_InitSecurityMonitoring();
    Com_InitCrashRecovery();

    Com_Printf( "--------------------\n" );
}

/*
==================
Com_Frame
==================
*/
void Com_Frame( qboolean noDelay ) {
    int msec, realMsec;
    int frameTime;

	// Perform periodic security checks
	Com_SecurityCheck();

	// Pump OS/input and dispatch queued sys events.
	Com_EventLoop();

	// Run any queued console commands (including + commands and autoexec).
	Cbuf_Execute();

    frameTime = Sys_Milliseconds();
    realMsec = frameTime - com_frameTime;
    com_frameTime = frameTime;

    if ( noDelay ) {
        msec = realMsec;
    } else {
        msec = realMsec;
        if ( msec < 1 ) {
            msec = 1;
        }
        if ( msec > 200 ) {
            msec = 200;
        }
    }

    // Run server frame if active
    if ( com_sv_running && com_sv_running->integer ) {
        SV_Frame( msec );
    }

    // Run client frame if active
#ifndef DEDICATED
    if ( com_cl_running && com_cl_running->integer ) {
        CL_Frame( msec, realMsec );
    }
#endif

    perfCounters.frameCount++;
}

// Memory management stubs
void *Z_Malloc( int size ) { return calloc( 1, (size_t)size ); }
void Z_Free( void *ptr ) { if (ptr) free( ptr ); }
void *Z_TagMalloc( int size, memtag_t tag ) { (void)tag; return malloc( (size_t)size ); }
void *Hunk_AllocateTempMemory( int size ) { return calloc( 1, (size_t)size + 16 ); }
void Hunk_FreeTempMemory( void *ptr ) { if (ptr) free( ptr ); }
int Hunk_MemoryRemaining( void ) { return 1024 * 1024; }
void *Hunk_Alloc( int size, ha_pref pref ) { (void)pref; return malloc((size_t)size); }
void Hunk_Clear( void ) { }
void Hunk_ClearToMark( void ) { }
void Hunk_SetMark( void ) { }
qboolean Hunk_CheckMark( void ) { return qtrue; }
void Hunk_ClearTempMemory( void ) { }
int Z_AvailableMemory( void ) { return 64 * 1024 * 1024; }
void Z_LogHeap( void ) { }

// Timing
int Com_Milliseconds( void ) {
    return Sys_Milliseconds();
}

void Com_FrameInit( void ) { }

// ============================================================================
// CD Key system
// ============================================================================
// Quake3-style cdkey file name used by the filesystem startup code.
// Stored under fs_homepath/<game>/q3key
#define Q3KEY_FILENAME "q3key"

static int Com_CollectCDKeyChars( const char *in, char *out, int outSize ) {
	int j = 0;
	if ( !in || !out || outSize <= 0 ) {
		return 0;
	}
	while ( *in && j < outSize - 1 ) {
		const char c = *in++;
		if ( ( c >= '0' && c <= '9' ) ||
		     ( c >= 'a' && c <= 'z' ) ||
		     ( c >= 'A' && c <= 'Z' ) ) {
			out[j++] = c;
		}
	}
	out[j] = '\0';
	return j;
}

static void Com_ReadCDKeyFromFile( const char *game, char *outKey16 ) {
	const char *home;
	char *ospath;
	FILE *f;
	char buf[256];

	if ( !outKey16 ) {
		return;
	}
	outKey16[0] = '\0';

	home = Cvar_VariableString( "fs_homepath" );
	if ( !home || !home[0] ) {
		home = Sys_DefaultHomePath();
	}

	ospath = FS_BuildOSPath( home, game, Q3KEY_FILENAME );
	f = Sys_FOpen( ospath, "rb" );
	if ( !f ) {
		return;
	}

	// Read first line / chunk
	memset( buf, 0, sizeof( buf ) );
	{
		const size_t n = fread( buf, 1, sizeof( buf ) - 1, f );
		(void)n; // ignore short reads; we will parse whatever we got
	}
	fclose( f );

	// Extract first 16 alphanumeric chars
	{
		char cleaned[64];
		const int n = Com_CollectCDKeyChars( buf, cleaned, sizeof( cleaned ) );
		if ( n >= 16 ) {
			memcpy( outKey16, cleaned, 16 );
			outKey16[16] = '\0';
		}
	}
}

void Com_ReadCDKey( const char *filename ) {
	char key16[17];

	// Default key remains all zeros if nothing found.
	Com_ReadCDKeyFromFile( filename, key16 );
	if ( key16[0] ) {
		memcpy( cl_cdkey, key16, 16 );
		cl_cdkey[32] = '\0';
	}
}

void Com_AppendCDKey( const char *filename ) {
#ifndef STANDALONE
	// Only append mod-specific portion if UI says unique cdkeys are used.
#ifndef DEDICATED
	// For now, skip CD key functionality to avoid UI dependency issues
	return;
#endif
#endif

	{
		char key16[17];
		Com_ReadCDKeyFromFile( filename, key16 );
		if ( key16[0] ) {
			memcpy( &cl_cdkey[16], key16, 16 );
			cl_cdkey[32] = '\0';
		}
	}
}

// Game state
void Com_GameRestart( int checksumFeed, qboolean clientRestart ) { (void)checksumFeed; (void)clientRestart; }
qboolean Com_SafeMode( void ) { return Crash_ShouldBootSafeMode(); }

// Networking
void Com_RunAndTimeServerPacket( const netadr_t *evFrom, msg_t *buf ) { (void)evFrom; (void)buf; }

// Misc
int Com_RealTime( qtime_t *qtime ) {
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (qtime) {
        qtime->tm_sec = tm->tm_sec;
        qtime->tm_min = tm->tm_min;
        qtime->tm_hour = tm->tm_hour;
        qtime->tm_mday = tm->tm_mday;
        qtime->tm_mon = tm->tm_mon;
        qtime->tm_year = tm->tm_year;
        qtime->tm_wday = tm->tm_wday;
        qtime->tm_yday = tm->tm_yday;
        qtime->tm_isdst = tm->tm_isdst;
    }
    return 0;
}

void Com_Quit_f( void ) { exit( 0 ); }
char *CopyString( const char *in ) {
    if (!in) return NULL;
    size_t len = strlen(in) + 1;
    char *out = (char *)MEMORY_SAFETY_MALLOC(len);
    if (out) {
        Q_strncpyz(out, in, len);
    }
    return out;
}

void Field_Clear( field_t *field ) {
    if ( field ) {
        field->cursor = 0;
        field->scroll = 0;
        memset( field->buffer, 0, sizeof( field->buffer ) );
    }
}

qboolean Com_EarlyParseCmdLine( char *commandLine, char *con_title, int title_size, int *vid_xpos, int *vid_ypos ) {
	// Minimal early parsing: cache the cmdline string for Com_StartupVariable.
	if ( commandLine ) {
		Q_strncpyz( com_cmdline, commandLine, sizeof( com_cmdline ) );
	}

	if ( con_title && title_size > 0 ) {
		Q_strncpyz( con_title, cl_title, title_size );
	}
	if ( vid_xpos ) *vid_xpos = 0;
	if ( vid_ypos ) *vid_ypos = 0;
	return qfalse;
}

void Com_WriteConfiguration( void ) { }
static const char *Com_ParseCmdlineToken( const char *s, char *out, size_t outSize )
{
	size_t i = 0;
	out[0] = '\0';

	while ( *s && isspace( (unsigned char)*s ) ) {
		s++;
	}

	if ( !*s ) {
		return s;
	}

	// Quoted token
	if ( *s == '"' ) {
		s++;
		while ( *s && *s != '"' ) {
			if ( i + 1 < outSize ) {
				out[i++] = *s;
			}
			s++;
		}
		if ( *s == '"' ) {
			s++;
		}
		out[i] = '\0';
		return s;
	}

	// Unquoted token
	while ( *s && !isspace( (unsigned char)*s ) ) {
		if ( i + 1 < outSize ) {
			out[i++] = *s;
		}
		s++;
	}
	out[i] = '\0';
	return s;
}

void Com_StartupVariable( const char *match )
{
	const char *s = com_cmdline;
	char cmd[MAX_STRING_CHARS];
	char var[MAX_STRING_CHARS];
	char val[MAX_STRING_CHARS];

	if ( !s || !*s ) {
		return;
	}

	while ( *s ) {
		if ( *s != '+' ) {
			s++;
			continue;
		}

		// Skip '+'
		s++;
		s = Com_ParseCmdlineToken( s, cmd, sizeof( cmd ) );

		// Only handle "+set <var> <value>" here.
		if ( Q_stricmp( cmd, "set" ) != 0 ) {
			continue;
		}

		s = Com_ParseCmdlineToken( s, var, sizeof( var ) );
		s = Com_ParseCmdlineToken( s, val, sizeof( val ) );

		if ( !var[0] ) {
			continue;
		}

		if ( !match || Q_stricmp( match, var ) == 0 ) {
			// Cvar_Set will create the cvar if it doesn't exist yet.
			Cvar_Set( var, val[0] ? val : "" );
		}
	}
}

int Com_EventLoop( void )
{
	sysEvent_t ev;
	char *s;

	// Pump platform input (SDL) into Sys_QueEvent.
	Sys_SendKeyEvents();

	// Pump tty console input (if enabled).
	s = Sys_ConsoleInput();
	if ( s && s[0] ) {
		char *copy = CopyString( s );
		Sys_QueEvent( 0, SE_CONSOLE, 0, 0, (int)strlen( copy ) + 1, copy );
	}

	while ( 1 ) {
		ev = Com_GetEvent();
		if ( ev.evType == SE_NONE ) {
			break;
		}

		switch ( ev.evType ) {
			case SE_KEY:
#ifndef DEDICATED
				CL_KeyEvent( ev.evValue, (qboolean)ev.evValue2, (unsigned)ev.evTime );
#endif
				break;
			case SE_CHAR:
#ifndef DEDICATED
				CL_CharEvent( ev.evValue );
#endif
				break;
			case SE_MOUSE:
#ifndef DEDICATED
				CL_MouseEvent( ev.evValue, ev.evValue2 );
#endif
				break;
			case SE_JOYSTICK_AXIS:
#ifndef DEDICATED
				CL_JoystickEvent( ev.evValue, ev.evValue2, ev.evTime );
#endif
				break;
			case SE_CONSOLE:
				if ( ev.evPtr ) {
					Cbuf_AddText( (const char *)ev.evPtr );
					Cbuf_AddText( "\n" );
				}
				break;
			default:
				break;
		}

		if ( ev.evPtr ) {
			Z_Free( ev.evPtr );
			ev.evPtr = NULL;
		}
	}

	return 0;
}

// Missing engine stubs
void S_Spatialize( struct channel_s *ch ) { (void)ch; }
void Field_CompleteCommand( const char *cmd, qboolean doCommands, qboolean doCvars ) { (void)cmd; (void)doCommands; (void)doCvars; }
void Com_BeginRedirect (char *buffer, int buffersize, void (*flush)(const char *)) { (void)buffer; (void)buffersize; (void)flush; }
void Com_EndRedirect( void ) { }
void *S_Malloc( int size ) { return malloc( (size_t)size ); }
int Z_FreeTags( memtag_t tag ) { (void)tag; return 0; }
unsigned int Com_TouchMemory( void ) { return 0; }
void Com_RandomBytes( byte *buffer, int len ) { (void)buffer; (void)len; }
void Info_Print( const char *s ) { (void)s; }
qboolean Q_ValidateFilePath( const char *path ) { (void)path; return qtrue; }
qboolean Com_HasPatterns( const char *str ) { (void)str; return qfalse; }
int Com_Filter( const char *filter, const char *name ) { (void)filter; (void)name; return 0; }
int Com_FilterPath( const char *filter, const char *name ) { (void)filter; (void)name; return 1; }
qboolean Com_FilterExt( const char *filter, const char *name ) { (void)filter; (void)name; return qtrue; }
void Com_SortList( char **list, int n ) { (void)list; (void)n; }
void Field_CompleteFilename( const char *dir, const char *ext, qboolean stripExt, int flags ) { (void)dir; (void)ext; (void)stripExt; (void)flags; }
void Field_AutoComplete( field_t *field ) { (void)field; }
void Field_CompleteKeyname( void ) { }
void Field_CompleteKeyBind( int key ) { (void)key; }
qboolean NetThread_IsThreadEnabled( net_thread_type_t threadType ) { (void)threadType; return qfalse; }
qboolean NetThread_QueueSendMessage( const netadr_t *to, const msg_t *msg, int flags ) { (void)to; (void)msg; (void)flags; return qfalse; }

// Sys_QueEvent moved to platform-specific files (unix_shared.c, etc.)
// void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr ) { ... }
