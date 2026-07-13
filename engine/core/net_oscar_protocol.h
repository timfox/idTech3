#ifndef NET_OSCAR_PROTOCOL_H
#define NET_OSCAR_PROTOCOL_H

#include "q_shared.h"
#include "net_oscar.h"

#define OSCAR_MAX_JSON_FRAME 4096

qboolean OSCAR_ProtocolBuildAuth( char *out, int outSize, int requestId, const char *account, const char *token );
qboolean OSCAR_ProtocolBuildJoinRoom( char *out, int outSize, int requestId, const char *room );
qboolean OSCAR_ProtocolBuildLeaveRoom( char *out, int outSize, int requestId, const char *room );
qboolean OSCAR_ProtocolBuildRoomMessage( char *out, int outSize, int requestId, const char *room, const char *sender, const char *text );
qboolean OSCAR_ProtocolBuildIM( char *out, int outSize, int requestId, const char *screenName, const char *text );
qboolean OSCAR_ProtocolBuildPresence( char *out, int outSize, int requestId, const char *status, const char *message );
qboolean OSCAR_ProtocolParseEvent( const char *json, oscarEvent_t *eventOut );

#endif /* NET_OSCAR_PROTOCOL_H */
