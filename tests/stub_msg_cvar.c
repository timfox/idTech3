/*
 * Minimal cvar stubs for msg.c unit tests (MSG_WriteString touches sv_utf8).
 */
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

static cvar_t sv_utf8_stub;
static char sv_utf8_string[] = "1";

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags )
{
	(void)value;
	(void)flags;
	if ( var_name && strcmp( var_name, "sv_utf8" ) == 0 ) {
		memset( &sv_utf8_stub, 0, sizeof( sv_utf8_stub ) );
		sv_utf8_stub.name = "sv_utf8";
		sv_utf8_stub.string = sv_utf8_string;
		sv_utf8_stub.resetString = sv_utf8_string;
		sv_utf8_stub.integer = 1;
		sv_utf8_stub.value = 1.0f;
		sv_utf8_stub.flags = 0;
		return &sv_utf8_stub;
	}
	return NULL;
}

void Cvar_SetDescription( cvar_t *var, const char *var_description )
{
	(void)var;
	(void)var_description;
}
