/*
=============================================================================
Common Engine Functions

Basic functions used throughout the engine.
=============================================================================
*/

#include "qcommon.h"
#include "net_threads.h"
#include "performance_counters.h"
#include "../renderers/renderercommon/tr_public.h"
#include <stdio.h>
#include <stdarg.h>

// Forward declarations for stub functions to suppress missing prototype warnings
void FS_WriteConfiguration( void );
void Cmd_Shutdown( void );
void S_Spatialize( void );
// BotDrawDebugPolygons is implemented in server code
void FS_MountTable_IsActive( void );
void FS_WritePolicy_GetMount( void );
void FS_Sandbox_ValidateOperation( void );
void FS_Mount_FindFile( void );
void FS_MountTable_Shutdown( void );
void FS_MountTable_Init( void );
void FS_MigrateLegacySearchPaths( void );
void FS_Mount_RegisterCommands( void );
void Perf_DisplayInfo_f( void );

// Basic Com_Printf implementation - can be overridden by renderers
#ifndef DEDICATED
void QDECL Com_Printf( const char *fmt, ... ) {
    va_list argptr;
    va_start( argptr, fmt );
    vprintf( fmt, argptr );
    va_end( argptr );
}
#endif

// Basic Com_Error implementation
void QDECL Com_Error( errorParm_t level, const char *fmt, ... ) {
    va_list argptr;
    va_start( argptr, fmt );
    vfprintf( stderr, fmt, argptr );
    va_end( argptr );
    exit( level );  // Basic implementation - real implementation should handle error levels
}

// Basic Com_Frame implementation - stub for minimal engine
void Com_Frame( qboolean noDelay ) {
    // Basic frame processing stub
    // In a real engine, this would handle client/server frame updates
    (void)noDelay;
}

// Com_FrameInit stub
void Com_FrameInit( void ) {
    // Initialize frame processing
}

// Basic memory management stubs - should be overridden
void *Z_Malloc( int size ) {
    return calloc( 1, size ); // Always zeroed as per header comment
}

void Z_Free( void *ptr ) {
    free( ptr );
}

void *Z_TagMalloc( int size, memtag_t tag ) {
    (void)tag;
    return malloc( size );
}

void *Hunk_AllocateTempMemory( int size ) {
    return malloc( size );
}

void Hunk_FreeTempMemory( void *ptr ) {
    free( ptr );
}

int Hunk_MemoryRemaining( void ) {
    return 1024 * 1024;  // Dummy value
}



void FS_WriteConfiguration( void ) __attribute__((unused));

// Basic UI stubs


// Basic field stubs
void Field_Clear( field_t *field ) {
    if ( field ) {
        field->cursor = 0;
        field->scroll = 0;
        memset( field->buffer, 0, sizeof( field->buffer ) );
    }
}

void Field_CompleteFilename( const char *dir, const char *ext, qboolean stripExt, int flags ) {
    // Stub - unused parameters
    (void)dir;
    (void)ext;
    (void)stripExt;
    (void)flags;
}

void Field_CompleteCommand( const char *cmd, qboolean doCommands, qboolean doCvars ) {
    // Stub - unused parameters
    (void)cmd;
    (void)doCommands;
    (void)doCvars;
}

void Field_CompleteKeyname( void ) {
    // Stub
}

void Field_CompleteKeyBind( int key ) {
    // Stub - unused parameter
    (void)key;
}


// Basic Com_Milliseconds
int Com_Milliseconds( void ) {
    return (int)(clock() * 1000 / CLOCKS_PER_SEC);
}

// Basic Com_Quit_f
void Com_Quit_f( void ) {
    exit( 0 );
}

// Additional stubs for monolithic build
qboolean Com_HasPatterns( const char *str ) {
    Q_UNUSED(str);
    return qfalse;
}

int Com_FilterPath( const char *filter, const char *name ) {
    Q_UNUSED(filter);
    Q_UNUSED(name);
    return 1;
}

qboolean Com_FilterExt( const char *filter, const char *name ) {
    Q_UNUSED(filter);
    Q_UNUSED(name);
    return qtrue;
}

void Com_SortList( char** list, int n ) {
    Q_UNUSED(list);
    Q_UNUSED(n);
    // Stub implementation - no sorting
}

// Developer printf - only prints if developer cvar is set
void Com_DPrintf( const char *fmt, ... ) {
    // Stub implementation - always print for monolithic build
    va_list argptr;
    va_start( argptr, fmt );
    vprintf( fmt, argptr );
    va_end( argptr );
}

// System error - terminates the program
void Sys_Error( const char *error, ... ) {
    va_list argptr;
    va_start( argptr, error );
    vfprintf( stderr, error, argptr );
    va_end( argptr );
    exit( 1 );
}

// Global variables expected by the engine
char cl_title[ MAX_CVAR_VALUE_STRING ] = "idtech3";

// Renderer interface stub for monolithic build
refimport_t ri = {0};

// Sound spatialization stub
void S_Spatialize( void ) {
    // Stub implementation
}

// Additional global variable stubs for monolithic build
int CPU_Flags = 0;
int Com_Filter( const char *filter, const char *name ) {
    Q_UNUSED(filter); Q_UNUSED(name);
    return 0;
}
char *CopyString( const char *in ) {
    if (!in) return NULL;
    size_t len = strlen(in) + 1;
    char *out = (char*)malloc(len);
    if (out) strcpy(out, in);
    return out;
}
void *Hunk_Alloc( int size, ha_pref pref ) {
    Q_UNUSED(pref);
    return malloc(size);
}
void Hunk_Clear( void ) { /* Stub */ }
void Hunk_ClearToMark( void ) { /* Stub */ }
void Hunk_ClearTempMemory( void ) { /* Stub */ }
void Info_Print( const char *s ) { printf("%s", s); }
void *S_Malloc( int size ) { return malloc(size); }
int Z_FreeTags( memtag_t tag ) { Q_UNUSED(tag); return 0; /* Stub */ }
void *botlib_export = NULL;
char cl_cdkey[34] = "000000000000000000000000000000000";
cvar_t *cl_packetdelay = NULL;
cvar_t *cl_paused = NULL;
cvar_t *com_assertLevel = NULL;
cvar_t *com_cl_running = NULL;
cvar_t *com_developer = NULL;
cvar_t *com_dedicated = NULL;
qboolean com_errorEntered = qfalse;
int com_frameTime = 0;
qboolean com_fullyInitialized = qfalse;
cvar_t *com_journal = NULL;
fileHandle_t com_journalDataFile = 0;
cvar_t *com_protocol = NULL;
qboolean com_protocolCompat = qfalse;
cvar_t *com_speeds = NULL;
cvar_t *com_sv_running = NULL;
cvar_t *com_timedemo = NULL;
cvar_t *com_timescale = NULL;
const int demo_protocols[] = {0};
cvar_t *sv_paused = NULL;
cvar_t *sv_packetdelay = NULL;
int time_backend = 0;
int time_frontend = 0;

// Global performance counters instance
performanceCounters_t perfCounters = {0};

// Additional function stubs

void Com_Init( char *commandLine ) {
    Q_UNUSED(commandLine);
    /* Stub - initialize common systems */
}
void Com_WriteConfiguration( void ) { /* Stub */ }
void Com_ReadCDKey( const char *filename ) {
    Q_UNUSED(filename);
}
void Com_AppendCDKey( const char *filename ) {
    Q_UNUSED(filename);
}
void Com_StartupVariable( const char *match ) {
    Q_UNUSED(match);
}
void Com_GameRestart( int checksumFeed, qboolean clientRestart ) {
    Q_UNUSED(checksumFeed); Q_UNUSED(clientRestart);
}
qboolean Com_SafeMode( void ) { return qfalse; }
void Com_RunAndTimeServerPacket( const netadr_t *evFrom, msg_t *buf ) {
    Q_UNUSED(evFrom); Q_UNUSED(buf);
}
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
unsigned int Com_TouchMemory( void ) { return 0; /* Stub */ }
void Com_RandomBytes( byte *buffer, int len ) {
    for (int i = 0; i < len; i++) buffer[i] = rand() % 256;
}
void Field_AutoComplete( field_t *edit ) {
    Q_UNUSED(edit);
}
void Perf_DisplayInfo_f( void ) { /* Stub */ }
void FS_MountTable_IsActive( void ) { /* Stub */ }
void FS_WritePolicy_GetMount( void ) { /* Stub */ }
void FS_Sandbox_ValidateOperation( void ) { /* Stub */ }
qboolean Q_ValidateFilePath( const char *path ) {
    Q_UNUSED(path);
    return qtrue;
}
void FS_Mount_FindFile( void ) { /* Stub */ }
void FS_MountTable_Shutdown( void ) { /* Stub */ }
void FS_MountTable_Init( void ) { /* Stub */ }
void FS_MigrateLegacySearchPaths( void ) { /* Stub */ }
void FS_Mount_RegisterCommands( void ) { /* Stub */ }
qboolean NetThread_IsThreadEnabled(net_thread_type_t threadType) {
    Q_UNUSED(threadType);
    return qfalse;
}
qboolean NetThread_QueueSendMessage(const netadr_t* address, const msg_t* message, int flags) {
    Q_UNUSED(address); Q_UNUSED(message); Q_UNUSED(flags);
    return qfalse;
}

