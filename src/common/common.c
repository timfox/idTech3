/*
=============================================================================
Common Engine Functions

Basic functions used throughout the engine.
=============================================================================
*/

#include "qcommon.h"
#include "net_threads.h"
#include "performance_counters.h"
#include "crash_handler.h"
#include "../renderers/renderercommon/tr_public.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

// Global variables expected by the engine
char cl_title[ MAX_CVAR_VALUE_STRING ] = "idtech3";
refimport_t ri = {0};
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

// SDL/Input state stubs
qboolean gw_active = qtrue;
qboolean gw_minimized = qfalse;

/*
==================
Com_Printf
==================
*/
void QDECL Com_Printf( const char *fmt, ... ) {
    va_list argptr;
    va_start( argptr, fmt );
    vprintf( fmt, argptr );
    va_end( argptr );
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
void Com_Init( char *commandLine ) {
    Com_Printf( "----- Com_Init -----\n" );

    // Initialize core systems
    Cvar_Init();
    Cmd_Init();

    // Register engine-wide cvars
    com_developer = Cvar_Get( "developer", "0", CVAR_ARCHIVE );
    com_dedicated = Cvar_Get( "dedicated", "0", CVAR_ROM );
    com_timescale = Cvar_Get( "timescale", "1", CVAR_CHEAT );
    com_timedemo = Cvar_Get( "timedemo", "0", 0 );
    com_sv_running = Cvar_Get( "sv_running", "0", CVAR_ROM );
    com_cl_running = Cvar_Get( "cl_running", "0", CVAR_ROM );
    com_speeds = Cvar_Get( "com_speeds", "0", 0 );
    com_assertLevel = Cvar_Get( "com_assertLevel", "1", CVAR_ARCHIVE );
    
    cl_paused = Cvar_Get( "cl_paused", "0", CVAR_ROM );
    sv_paused = Cvar_Get( "sv_paused", "0", CVAR_ROM );

    // Disable resource cache to debug buffer overflow
    Cvar_Set( "fs_resourceCache", "0" );

    // Initialize filesystem
    FS_InitFilesystem();

    if ( commandLine ) {
        Com_Printf( "Command line: %s\n", commandLine );
    }

    com_frameTime = Sys_Milliseconds();
    com_fullyInitialized = qtrue;

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
    if ( com_cl_running && com_cl_running->integer ) {
        CL_Frame( msec, realMsec );
    }

    perfCounters.frameCount++;
}

// Memory management stubs
void *Z_Malloc( int size ) { return calloc( 1, size ); }
void Z_Free( void *ptr ) { if (ptr) free( ptr ); }
void *Z_TagMalloc( int size, memtag_t tag ) { (void)tag; return malloc( size ); }
void *Hunk_AllocateTempMemory( int size ) { return calloc( 1, size + 16 ); }
void Hunk_FreeTempMemory( void *ptr ) { if (ptr) free( ptr ); }
int Hunk_MemoryRemaining( void ) { return 1024 * 1024; }
void *Hunk_Alloc( int size, ha_pref pref ) { (void)pref; return malloc(size); }
void Hunk_Clear( void ) { }
void Hunk_ClearToMark( void ) { }
void Hunk_ClearTempMemory( void ) { }

// Timing
int Com_Milliseconds( void ) {
    return Sys_Milliseconds();
}

// CD Key
void Com_ReadCDKey( const char *filename ) { (void)filename; }
void Com_AppendCDKey( const char *filename ) { (void)filename; }

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

int Com_EventLoop( void ) { return 0; }
void Com_Quit_f( void ) { exit( 0 ); }
char *CopyString( const char *in ) {
    if (!in) return NULL;
    char *out = malloc(strlen(in) + 1);
    if (out) strcpy(out, in);
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
    (void)commandLine; (void)con_title; (void)title_size; (void)vid_xpos; (void)vid_ypos;
    return qfalse;
}

void Com_WriteConfiguration( void ) { }
void Com_StartupVariable( const char *match ) { (void)match; }
void S_Spatialize( void ) { }
void Field_AutoComplete( field_t *edit ) { (void)edit; }
void Field_CompleteFilename( const char *dir, const char *ext, qboolean stripExt, int flags ) { (void)dir; (void)ext; (void)stripExt; (void)flags; }
void Field_CompleteCommand( const char *cmd, qboolean doCommands, qboolean doCvars ) { (void)cmd; (void)doCommands; (void)doCvars; }
void Field_CompleteKeyname( void ) { }
void Field_CompleteKeyBind( int key ) { (void)key; }
void Info_Print( const char *s ) { printf("%s", s); }
void *S_Malloc( int size ) { return malloc(size); }
int Z_FreeTags( memtag_t tag ) { (void)tag; return 0; }
int Com_Filter( const char *filter, const char *name ) { (void)filter; (void)name; return 0; }
qboolean Com_HasPatterns( const char *str ) { (void)str; return qfalse; }
int Com_FilterPath( const char *filter, const char *name ) { (void)filter; (void)name; return 1; }
qboolean Com_FilterExt( const char *filter, const char *name ) { (void)filter; (void)name; return qtrue; }
void Com_SortList( char** list, int n ) { (void)list; (void)n; }
unsigned int Com_TouchMemory( void ) { return 0; }
void Com_RandomBytes( byte *buffer, int len ) { for (int i = 0; i < len; i++) buffer[i] = rand() % 256; }
void Perf_DisplayInfo_f( void ) { }
void FS_MountTable_IsActive( void ) { }
void FS_WritePolicy_GetMount( void ) { }
void FS_Sandbox_ValidateOperation( void ) { }
qboolean Q_ValidateFilePath( const char *path ) { (void)path; return qtrue; }
void FS_Mount_FindFile( void ) { }
void FS_MountTable_Shutdown( void ) { }
void FS_MountTable_Init( void ) { }
void FS_MigrateLegacySearchPaths( void ) { }
void FS_Mount_RegisterCommands( void ) { }
qboolean NetThread_IsThreadEnabled(net_thread_type_t threadType) { (void)threadType; return qfalse; }
qboolean NetThread_QueueSendMessage(const netadr_t* address, const msg_t* message, int flags) { (void)address; (void)message; (void)flags; return qfalse; }

void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr ) { (void)time; (void)type; (void)value; (void)value2; (void)ptrLength; (void)ptr; }
