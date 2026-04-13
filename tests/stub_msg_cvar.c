/*
 * Minimal cvar stubs for msg.c unit tests (MSG_WriteString touches sv_utf8).
 * gSTUB_SV_UTF8: 0 = legacy strip high bytes + '%'; 1 = UTF-8 on wire (still maps '%' to '.').
 */
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

int gSTUB_SV_UTF8 = 1;

static cvar_t sv_utf8_stub;
static char sv_utf8_string[4] = "1";

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags )
{
	(void)value;
	(void)flags;
	if ( var_name && strcmp( var_name, "sv_utf8" ) == 0 ) {
		memset( &sv_utf8_stub, 0, sizeof( sv_utf8_stub ) );
		sv_utf8_stub.name = "sv_utf8";
		sv_utf8_string[0] = (char)( '0' + ( gSTUB_SV_UTF8 ? 1 : 0 ) );
		sv_utf8_string[1] = '\0';
		sv_utf8_stub.string = sv_utf8_string;
		sv_utf8_stub.resetString = sv_utf8_string;
		sv_utf8_stub.integer = gSTUB_SV_UTF8 ? 1 : 0;
		sv_utf8_stub.value = gSTUB_SV_UTF8 ? 1.0f : 0.0f;
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
