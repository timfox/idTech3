/*
=============================================================================
Common Engine Functions

Basic functions used throughout the engine.
=============================================================================
*/

#include "qcommon.h"
#include <stdio.h>
#include <stdarg.h>

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

// Basic cvar stub
cvar_t *Cvar_Get( const char *var_name, const char *value, int flags ) {
    static cvar_t dummy_cvar;
    (void)flags;
    Q_strncpyz( dummy_cvar.name, var_name, sizeof( dummy_cvar.name ) );
    Q_strncpyz( dummy_cvar.string, value, sizeof( dummy_cvar.string ) );
    dummy_cvar.value = atof( value );
    dummy_cvar.integer = atoi( value );
    return &dummy_cvar;
}

// Basic filesystem stubs
qboolean FS_Initialized( void ) {
    return qtrue;
}

qboolean FS_StartupInProgress( void ) {
    return qfalse;
}

void FS_WriteConfiguration( void ) {
    // Stub
}

// Basic UI stubs
qboolean UI_GameCommand( void ) {
    // Stub - return false to indicate command not handled
    return qfalse;
}

qboolean CL_GameCommand( void ) {
    // Stub - return false to indicate command not handled
    return qfalse;
}

qboolean SV_GameCommand( void ) {
    // Stub - return false to indicate command not handled
    return qfalse;
}

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

// Basic command stubs
int Cmd_Argc( void ) {
    return 1;
}

const char *Cmd_Argv( int arg ) {
    static char dummy[] = "dummy";
    (void)arg;
    return dummy;
}

void Cmd_AddCommand( const char *cmd_name, xcommand_t function ) {
    (void)cmd_name;
    (void)function;
}

void Cmd_RemoveCommand( const char *cmd_name ) {
    (void)cmd_name;
}

void Cmd_ExecuteString( const char *text ) {
    (void)text;
}

void Cmd_Init( void ) {
    // Stub
}

void Cmd_Shutdown( void ) {
    // Stub
}

// Basic Com_Milliseconds
int Com_Milliseconds( void ) {
    return (int)(clock() * 1000 / CLOCKS_PER_SEC);
}

// Basic Com_Quit_f
void Com_Quit_f( void ) {
    exit( 0 );
}
