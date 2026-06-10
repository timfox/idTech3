#ifndef SV_AUTH_H
#define SV_AUTH_H

#include "../qcommon/q_shared.h"

struct client_s;

void        SV_Auth_Init( void );
void        SV_Auth_Shutdown( void );

qboolean    SV_AuthVerifyToken( const char *token, int clientNum );
qboolean    SV_Auth_CheckClient( struct client_s *cl );
const char *SV_Auth_UserinfoKey( void );

void        SV_Auth_MakeToken_f( void );

#endif
