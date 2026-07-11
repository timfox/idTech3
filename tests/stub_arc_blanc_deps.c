/*
 * Stubs for unit_arc_blanc tests.
 */
#include <stdlib.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

static cvar_t s_rArcBlanc;

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
const char *Cmd_Argv( int i ) { (void)i; return ""; }

cvar_t *Cvar_Get( const char *name, const char *value, int flags )
{
	(void)flags;
	if ( !Q_stricmp( name, "r_arcBlanc" ) ) {
		if ( !s_rArcBlanc.string ) {
			s_rArcBlanc.string = "1";
			s_rArcBlanc.value = 1.0f;
			s_rArcBlanc.integer = 1;
		}
		return &s_rArcBlanc;
	}
	return &s_rArcBlanc;
}

void Cvar_SetDescription( cvar_t *cv, const char *desc )
{
	(void)cv;
	(void)desc;
}

void Cvar_Set( const char *var_name, const char *value )
{
	(void)var_name;
	(void)value;
}

void *Z_Malloc( int size )
{
	return malloc( (size_t)size );
}

void Z_Free( void *ptr )
{
	free( ptr );
}
