#ifndef NET_P2P_H
#define NET_P2P_H

#include "q_shared.h"

void NET_P2P_Init( void );
void NET_P2P_Shutdown( void );
void NET_P2P_Frame( void );

qboolean NET_P2P_IsSupported( void );
qboolean NET_P2P_IsEnabled( void );
qboolean NET_P2P_IsReady( void );
const char *NET_P2P_BackendName( void );
qboolean NET_P2P_GetLocalAddressString( char *buffer, int bufferSize );

#endif /* NET_P2P_H */
