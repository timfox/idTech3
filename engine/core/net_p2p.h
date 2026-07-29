#ifndef NET_P2P_H
#define NET_P2P_H

#include "q_shared.h"

#define P2P_STATUS_ADDRESS_SIZE MAX_STRING_CHARS

typedef struct p2p_path_status_s {
	qboolean enabled;
	qboolean ready;
	qboolean usingSteamSdr;
	char backend[32];
	char localAddress[P2P_STATUS_ADDRESS_SIZE];
	qboolean iceActive;
	qboolean iceSuccess;
	qboolean iceComplete;
	qboolean connectDeferred;
	qboolean connectReady;
	int iceRemoteCandidates;
	int iceChecksSent;
	int iceTimeoutRemainingMs;
	int iceNominatedTxn;
	char icePeerAddress[P2P_STATUS_ADDRESS_SIZE];
	char iceNominatedAddress[P2P_STATUS_ADDRESS_SIZE];
	int punchActivePeers;
	int punchAcknowledgedPeers;
} p2p_path_status_t;

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
qboolean NET_P2P_GetPathStatus( p2p_path_status_t *status );
void NET_P2P_BeginMasterList( const char *masterAddress );
qboolean NET_P2P_BeginConnectPath( const char *peerAddress );
qboolean NET_P2P_ConnectIsDeferred( void );
qboolean NET_P2P_ConsumeDeferredConnect( char *buffer, int bufferSize );
qboolean NET_P2P_TryHandleNatPacket( const netadr_t *from, const byte *data, int len );
qboolean NET_P2P_TryHandleBrowseOob( const netadr_t *from, const char *cmd, msg_t *msg );
qboolean NET_P2P_HandleOobPacket( const netadr_t *from, const char *cmd );

#endif /* NET_P2P_H */
