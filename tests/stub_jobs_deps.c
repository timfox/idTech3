/*
 * Stubs for unit_jobs (Cvar + minimal qcommon hooks used by jobs.c).
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

typedef struct stubCvar_s {
	char name[64];
	char string[256];
	int integer;
	int flags;
	struct stubCvar_s *next;
} stubCvar_t;

static stubCvar_t *stubCvars;

void QDECL Com_DPrintf( const char *fmt, ... )
{
	(void)fmt;
}

void Cvar_SetDescription( cvar_t *var, const char *description )
{
	(void)var;
	(void)description;
}

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags )
{
	stubCvar_t *cv;

	for ( cv = stubCvars; cv; cv = cv->next ) {
		if ( !Q_stricmp( cv->name, var_name ) ) {
			return (cvar_t *)cv;
		}
	}

	cv = (stubCvar_t *)calloc( 1, sizeof( *cv ) );
	if ( !cv ) {
		return NULL;
	}
	Q_strncpyz( cv->name, var_name, sizeof( cv->name ) );
	Q_strncpyz( cv->string, value ? value : "", sizeof( cv->string ) );
	cv->integer = value ? atoi( value ) : 0;
	cv->flags = flags;
	cv->next = stubCvars;
	stubCvars = cv;
	return (cvar_t *)cv;
}

int Cvar_VariableIntegerValue( const char *var_name )
{
	cvar_t *cv = Cvar_Get( var_name, "0", 0 );
	return cv ? cv->integer : 0;
}
