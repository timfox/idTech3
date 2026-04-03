/*
 * Minimal stubs so q_shared.c can link in unit tests without full common.c.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "qcommon/q_shared.h"

void NORETURN QDECL Com_Error( errorParm_t level, const char *fmt, ... )
{
	va_list ap;
	(void)level;
	fprintf( stderr, "Com_Error: " );
	va_start( ap, fmt );
	vfprintf( stderr, fmt, ap );
	va_end( ap );
	fprintf( stderr, "\n" );
	abort();
}

void QDECL Com_Printf( const char *fmt, ... )
{
	(void)fmt;
}
