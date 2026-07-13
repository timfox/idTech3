#ifndef NET_P2P_H
#define NET_P2P_H

#include "q_shared.h"

void NET_P2P_Init( void );
void NET_P2P_Shutdown( void );
void NET_P2P_Frame( void );

qboolean NET_P2P_IsSupported( void );
qboolean NET_P2P_IsEnabled( void );
qboolean NET_P2P_IsReady( void );
qboolean NET_P2P_UsesSteamSdrBackend( void );
const char *NET_P2P_BackendName( void );
qboolean NET_P2P_GetLocalAddressString( char *buffer, int bufferSize );
qboolean NET_P2P_IsAddressString( const char *address );
qboolean NET_P2P_NormalizeAddressString( const char *address, char *buffer, int bufferSize );
void NET_P2P_BeginPunchForAddress( const char *address );
void NET_P2P_PrintPunchStatus( void );
void NET_P2P_PrintIceCandidates( void );
void NET_P2P_PrintPathStatus( void );
void NET_P2P_BeginMasterList( const char *masterAddress );
qboolean NET_P2P_BeginConnectPath( const char *peerAddress );
qboolean NET_P2P_TryHandleNatPacket( const netadr_t *from, const byte *data, int len );
qboolean NET_P2P_TryHandleBrowseOob( const netadr_t *from, const char *cmd, msg_t *msg );
qboolean NET_P2P_HandleOobPacket( const netadr_t *from, const char *cmd );

#endif /* NET_P2P_H */
