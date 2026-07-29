/*
 * Minimal DAP unit-test stubs for engine command/cvar registration.
 */
#include <stdlib.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"

static cvar_t stubCvar;
static char stubCvarString[128];

cvar_t *Cvar_Get( const char *var_name, const char *var_value, int flags ) {
	(void)var_name;
	(void)flags;
	memset( &stubCvar, 0, sizeof( stubCvar ) );
	Q_strncpyz( stubCvarString, var_value ? var_value : "", sizeof( stubCvarString ) );
	stubCvar.string = stubCvarString;
	stubCvar.integer = atoi( stubCvar.string );
	return &stubCvar;
}

void Cmd_AddCommand( const char *cmd_name, xcommand_t function ) {
	(void)cmd_name;
	(void)function;
}

void Cmd_RemoveCommand( const char *cmd_name ) {
	(void)cmd_name;
}
