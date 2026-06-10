/*
 * Stubs for unit_sv_auth (cvars, commands, minimal server globals).
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "server/sv_auth.h"

static cvar_t s_cvars[32];
static char s_cvarNames[32][64];
static char s_cvarStrings[32][128];
static int s_cvarCount;

cvar_t *sv_maxclients;

void QDECL Com_DPrintf( const char *fmt, ... )
{
	(void)fmt;
}

cvar_t *Cvar_Get( const char *name, const char *value, int flags )
{
	cvar_t *cv;
	int i;
	(void)flags;

	for ( i = 0; i < s_cvarCount; i++ ) {
		if ( !Q_stricmp( s_cvarNames[i], name ) ) {
			return &s_cvars[i];
		}
	}
	if ( s_cvarCount >= (int)( sizeof( s_cvars ) / sizeof( s_cvars[0] ) ) ) {
		return &s_cvars[0];
	}
	cv = &s_cvars[s_cvarCount];
	Q_strncpyz( s_cvarNames[s_cvarCount], name, sizeof( s_cvarNames[0] ) );
	Q_strncpyz( s_cvarStrings[s_cvarCount], value, sizeof( s_cvarStrings[0] ) );
	cv->name = s_cvarNames[s_cvarCount];
	cv->string = s_cvarStrings[s_cvarCount];
	cv->modified = qtrue;
	cv->integer = atoi( value );
	cv->value = atof( value );
	s_cvarCount++;
	return cv;
}

void Cvar_Set( const char *name, const char *value )
{
	cvar_t *cv = Cvar_Get( name, value, 0 );
	Q_strncpyz( s_cvarStrings[cv - s_cvars], value, sizeof( s_cvarStrings[0] ) );
	cv->string = s_cvarStrings[cv - s_cvars];
	cv->modified = qtrue;
	cv->integer = atoi( value );
	cv->value = atof( value );
}

void Cvar_CheckRange( cvar_t *cv, const char *minVal, const char *maxVal, cvarValidator_t type )
{
	(void)cv;
	(void)minVal;
	(void)maxVal;
	(void)type;
}

void Cvar_SetDescription( cvar_t *cv, const char *description )
{
	(void)cv;
	(void)description;
}

void Cmd_AddCommand( const char *cmd_name, xcommand_t function )
{
	(void)cmd_name;
	(void)function;
}

void Cmd_RemoveCommand( const char *cmd_name )
{
	(void)cmd_name;
}

int Cmd_Argc( void )
{
	return 0;
}

const char *Cmd_Argv( int arg )
{
	(void)arg;
	return "";
}

int FS_SV_FOpenFileRead( const char *filename, fileHandle_t *f )
{
	(void)filename;
	(void)f;
	return -1;
}

int FS_Read( void *buffer, int len, fileHandle_t f )
{
	(void)buffer;
	(void)len;
	(void)f;
	return 0;
}

void FS_FCloseFile( fileHandle_t f )
{
	(void)f;
}

qboolean Sys_RandomBytes( byte *string, int len )
{
	int i;
	for ( i = 0; i < len; i++ ) {
		string[i] = (byte)( rand() & 0xff );
	}
	return qtrue;
}
