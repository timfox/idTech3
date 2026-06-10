#include "qcommon/q_shared.h"
#include <stdarg.h>
#include <stdlib.h>

void Com_Printf( const char *fmt, ... )
{
	(void)fmt;
}

void QDECL Com_Error( errorParm_t code, const char *fmt, ... )
{
	(void)code;
	(void)fmt;
	exit( 1 );
}
