/*
 * Cvar + FS stubs for open-world nav unit tests (recast_nav without full common.c).
 */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

void QDECL Com_DPrintf( const char *fmt, ... )
{
	(void)fmt;
}

int FS_ReadFile( const char *qpath, void **buffer )
{
	(void)qpath;
	if ( buffer ) {
		*buffer = NULL;
	}
	return -1;
}

void FS_FreeFile( void *buffer )
{
	if ( buffer ) {
		free( buffer );
	}
}

void FS_WriteFile( const char *qpath, const void *buffer, int size )
{
	(void)qpath;
	(void)buffer;
	(void)size;
}

typedef struct {
	const char *name;
	const char *string;
	cvar_t cv;
} stubCvar_t;

static stubCvar_t stubCvars[] = {
	{ "nav_enabled", "1", { 0 } },
	{ "nav_debugDraw", "0", { 0 } },
	{ "nav_cellSize", "0.3", { 0 } },
	{ "nav_agentRadius", "0.6", { 0 } },
	{ "nav_agentHeight", "2.0", { 0 } },
	{ "r_openWorldSectorSize", "4096", { 0 } },
};

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags )
{
	size_t i;

	(void)value;
	(void)flags;
	for ( i = 0; i < sizeof( stubCvars ) / sizeof( stubCvars[0] ); i++ ) {
		if ( var_name && !strcmp( var_name, stubCvars[i].name ) ) {
			stubCvars[i].cv.name = (char *)(uintptr_t)stubCvars[i].name;
			stubCvars[i].cv.string = (char *)(uintptr_t)stubCvars[i].string;
			stubCvars[i].cv.resetString = (char *)(uintptr_t)stubCvars[i].string;
			stubCvars[i].cv.integer = atoi( stubCvars[i].string );
			stubCvars[i].cv.value = (float)atof( stubCvars[i].string );
			return &stubCvars[i].cv;
		}
	}
	return NULL;
}

void Cvar_SetDescription( cvar_t *var, const char *var_description )
{
	(void)var;
	(void)var_description;
}

float Cvar_VariableValue( const char *var_name )
{
	cvar_t *cv = Cvar_Get( var_name, "", 0 );
	return cv ? cv->value : 0.0f;
}

int Cvar_VariableIntegerValue( const char *var_name )
{
	cvar_t *cv = Cvar_Get( var_name, "", 0 );
	return cv ? cv->integer : 0;
}
