/*
 * Stubs for unit_sector_graph tests.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "qcommon/cm_stream.h"
#include "world/world_open.h"

int com_frameTime = 0;

void QDECL Com_DPrintf( const char *fmt, ... )
{
	(void)fmt;
}

void Cmd_AddCommand( const char *cmd, void ( *f )( void ) )
{
	(void)cmd;
	(void)f;
}

void Cmd_RemoveCommand( const char *cmd )
{
	(void)cmd;
}

int Cmd_Argc( void ) { return 0; }
const char *Cmd_Argv( int arg ) { (void)arg; return ""; }

cvar_t *Cvar_Get( const char *name, const char *value, int flags )
{
	static cvar_t cv;
	(void)flags;
	(void)name;
	cv.value = atof( value );
	cv.integer = atoi( value );
	return &cv;
}

void Cvar_SetDescription( cvar_t *var, const char *description )
{
	(void)var;
	(void)description;
}

float Cvar_VariableValue( const char *var_name )
{
	if ( !Q_stricmp( var_name, "r_openWorldSectorSize" ) ) {
		return 4096.0f;
	}
	return 0.0f;
}

int Cvar_VariableIntegerValue( const char *var_name )
{
	return (int)Cvar_VariableValue( var_name );
}

void CM_Stream_WorldToCell( const vec3_t origin, float sectorSize, int *cellX, int *cellY )
{
	if ( cellX ) {
		*cellX = (int)floor( origin[0] / sectorSize );
	}
	if ( cellY ) {
		*cellY = (int)floor( origin[1] / sectorSize );
	}
}

qboolean CM_Stream_IsSectorLoaded( int cellX, int cellY )
{
	(void)cellX;
	(void)cellY;
	return qfalse;
}

int WorldOpen_GetSectorCount( void ) { return 0; }
const worldOpenSector_t *WorldOpen_GetSector( int index ) { (void)index; return NULL; }
