/*
 * Minimal qcommon stubs for unit_sfca.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

void *Z_Malloc( int size )
{
	return malloc( (size_t)size );
}

void Z_Free( void *ptr )
{
	free( ptr );
}

void QDECL Com_Printf( const char *fmt, ... )
{
	(void)fmt;
}

void QDECL Com_DPrintf( const char *fmt, ... )
{
	(void)fmt;
}
