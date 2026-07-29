#ifndef NET_P2P_ICE_H
#define NET_P2P_ICE_H

#include "q_shared.h"
#include "net_p2p.h"

void NET_P2P_IceInit( void );
void NET_P2P_IceShutdown( void );
void NET_P2P_IceFrame( void );

qboolean NET_P2P_IceBeginConnectPath( const char *peerAddress );
qboolean NET_P2P_IceHandleOobPacket( const netadr_t *from, const char *cmd );
void NET_P2P_IcePrintStatus( void );
void NET_P2P_IceGetStatus( p2p_path_status_t *status );
qboolean NET_P2P_IceConnectIsDeferred( void );
qboolean NET_P2P_IceConsumeDeferredConnect( char *buffer, int bufferSize );

#endif /* NET_P2P_ICE_H */
