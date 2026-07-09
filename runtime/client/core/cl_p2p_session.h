#ifndef CL_P2P_SESSION_H
#define CL_P2P_SESSION_H

#include "q_shared.h"

void CL_P2P_SessionInit( void );
void CL_P2P_SessionShutdown( void );
void CL_P2P_SessionFrame( void );

void CL_P2P_SessionOnConnect( const char *sessionId, const char *p2pAddr, const char *failover, int reconnectWindowSec );
void CL_P2P_SessionOnDisconnect( qboolean serverInitiated );
void CL_P2P_SessionOnMigrate( const char *sessionId, const char *newP2pAddr );
qboolean CL_P2P_SessionHandleOobPacket( const netadr_t *from, const char *cmd );
qboolean CL_P2P_SessionIsBackupHostEligible( void );
const char *CL_P2P_SessionId( void );

#endif /* CL_P2P_SESSION_H */
