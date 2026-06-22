#ifndef CL_APP_CRDT_H
#define CL_APP_CRDT_H

#include "../qcommon/q_shared.h"

struct lua_State;

void CL_AppCrdt_Init( void );
void CL_AppCrdt_Frame( void );
qboolean CL_AppCrdt_TryServerCommand( const char *s );
void CL_AppCrdt_RegisterLua( struct lua_State *L );

#endif
