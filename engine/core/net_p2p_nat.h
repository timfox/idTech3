#ifndef NET_P2P_NAT_H
#define NET_P2P_NAT_H

#include "q_shared.h"
#include "net_p2p_stun_codec.h"

void NET_P2P_NatInit( void );
void NET_P2P_NatShutdown( void );
void NET_P2P_NatFrame( void );
qboolean NET_P2P_NatTryHandlePacket( const netadr_t *from, const byte *data, int len );
qboolean NET_P2P_NatTryHandleConnectionless( const netadr_t *from, const char *cmd, msg_t *msg );
qboolean NET_P2P_NatGetAdvertiseAddress( char *buffer, int bufferSize );
qboolean NET_P2P_NatGetCandidateText( p2p_candidate_type_t type, char *buffer, int bufferSize );
void NET_P2P_NatGrantTurnPermission( const netadr_t *peer );
void NET_P2P_NatPrintCandidates( void );
void NET_P2P_NatBeginMasterList( const char *masterAddress );
void NET_P2P_NatPrintMasterList( void );

#endif /* NET_P2P_NAT_H */
