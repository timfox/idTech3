#ifndef NET_OSCAR_H
#define NET_OSCAR_H

#include "q_shared.h"

#define OSCAR_MAX_BUDDIES 64

typedef enum {
	OSCAR_STATE_DISABLED = 0,
	OSCAR_STATE_DISCONNECTED,
	OSCAR_STATE_CONNECTING,
	OSCAR_STATE_AUTHENTICATING,
	OSCAR_STATE_ONLINE,
	OSCAR_STATE_RECONNECTING,
	OSCAR_STATE_ERROR
} oscarState_t;

typedef enum {
	OSCAR_EVENT_NONE = 0,
	OSCAR_EVENT_CONNECTED,
	OSCAR_EVENT_DISCONNECTED,
	OSCAR_EVENT_INSTANT_MESSAGE,
	OSCAR_EVENT_ROOM_MESSAGE,
	OSCAR_EVENT_PRESENCE_CHANGED,
	OSCAR_EVENT_REQUEST_COMPLETE,
	OSCAR_EVENT_ERROR
} oscarEventType_t;

typedef struct {
	oscarEventType_t type;
	int requestId;
	qboolean ok;
	char room[MAX_QPATH];
	char screenName[MAX_NAME_LENGTH];
	char status[32];
	char text[MAX_STRING_CHARS];
} oscarEvent_t;

typedef struct {
	char screenName[MAX_NAME_LENGTH];
	char status[32];
	char awayMessage[MAX_STRING_CHARS];
	qboolean online;
} oscarBuddy_t;

void OSCAR_Init( void );
void OSCAR_Shutdown( void );
void OSCAR_Frame( int realtime );
void OSCAR_RegisterCommands( void );

qboolean OSCAR_IsAvailable( void );
qboolean OSCAR_Connect( void );
void OSCAR_Disconnect( const char *reason );

qboolean OSCAR_SendIM( const char *screenName, const char *message );
qboolean OSCAR_JoinRoom( const char *room );
qboolean OSCAR_LeaveRoom( const char *room );
qboolean OSCAR_SendRoomMessage( const char *room, const char *message );
qboolean OSCAR_SetPresence( const char *status, const char *message );
qboolean OSCAR_AddBuddy( const char *screenName );
qboolean OSCAR_RemoveBuddy( const char *screenName );

oscarState_t OSCAR_GetState( void );
const char *OSCAR_GetStatusString( void );
const char *OSCAR_GetLastError( void );
const char *OSCAR_GetCurrentRoom( void );
int OSCAR_GetReconnectAttempt( void );
qboolean OSCAR_PollEvent( oscarEvent_t *eventOut );

int OSCAR_BuddyCount( void );
qboolean OSCAR_BuddyGet( int index, oscarBuddy_t *out );
void OSCAR_BuddyClear( void );
unsigned int OSCAR_GetRosterGeneration( void );

#endif /* NET_OSCAR_H */
