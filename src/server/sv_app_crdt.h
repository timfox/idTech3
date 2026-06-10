#ifndef SV_APP_CRDT_H
#define SV_APP_CRDT_H

#include "../qcommon/q_shared.h"

typedef struct client_s client_t;

void SV_AppCrdt_Init( void );
void SV_AppCrdt_ClientEnterWorld( client_t *client );
void SV_AppCrdt_OnMapReady( void );
qboolean SV_AppCrdt_TryClientCommand( client_t *cl, const char *s );

#endif
