/*
 * Stubs for unit_cluster_graph tests.
 */
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "qcommon/cm_local.h"

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
	static cvar_t cvReach;
	static cvar_t cvHops;
	(void)flags;
	if ( !Q_stricmp( name, "r_graphClusterReach" ) ) {
		cvReach.value = atof( value );
		cvReach.integer = 1;
		return &cvReach;
	}
	if ( !Q_stricmp( name, "r_graphClusterHops" ) ) {
		cvHops.value = atof( value );
		cvHops.integer = atoi( value );
		return &cvHops;
	}
	{
		static cvar_t cv;
		cv.value = atof( value );
		cv.integer = atoi( value );
		return &cv;
	}
}

void Cvar_SetDescription( cvar_t *var, const char *description )
{
	(void)var;
	(void)description;
}

clipMap_t cm;
