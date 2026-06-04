#ifndef SV_AUTH_H
#define SV_AUTH_H

#include "../qcommon/q_shared.h"

void SV_Auth_Init( void );
qboolean SV_AuthVerifyToken( const char *token, int clientNum );

#endif
