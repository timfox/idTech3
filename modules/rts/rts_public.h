/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

RTS simulation public API.
===========================================================================
*/

#pragma once

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int rtsEntityId_t;

typedef enum rtsOwner_e {
	RTS_OWNER_NEUTRAL = 0,
	RTS_OWNER_PLAYER1 = 1,
	RTS_OWNER_PLAYER2 = 2,
	RTS_OWNER_PLAYER3 = 3,
	RTS_OWNER_PLAYER4 = 4
} rtsOwner_t;

typedef enum rtsCommandType_e {
	RTS_COMMAND_NONE = 0,
	RTS_COMMAND_MOVE,
	RTS_COMMAND_ATTACK,
	RTS_COMMAND_STOP,
	RTS_COMMAND_BUILD,
	RTS_COMMAND_GATHER
} rtsCommandType_t;

typedef struct rtsCommand_s {
	rtsCommandType_t type;
	int turn;
	int playerId;
	int sequence;
	rtsEntityId_t entityId;
	rtsEntityId_t targetEntityId;
	int targetX;
	int targetY;
	int data;
} rtsCommand_t;

void RTS_Init( void );
void RTS_Shutdown( void );
void RTS_RunTurn( int msec );
int  RTS_PostCommand( const rtsCommand_t *cmd );

int  RTS_GetTurnMsec( void );
int  RTS_GetCurrentTurn( void );
int  RTS_GetPendingCommandCount( void );
int  RTS_GetExecutedCommandCount( void );
int  RTS_GetEntityCount( void );
int  RTS_GetEntityOwner( rtsEntityId_t id );
int  RTS_GetEntityPosition( rtsEntityId_t id, int *x, int *y );
int  RTS_GetEntityHitpoints( rtsEntityId_t id );
int  RTS_GetPlayerResources( int playerId );
int  RTS_SelectRect( int playerId, int minX, int minY, int maxX, int maxY, rtsEntityId_t *out, int maxOut );
int  RTS_FindGridPath( int width, int height, const unsigned char *blocked, int startX, int startY, int goalX, int goalY, int *outX, int *outY, int maxOut );
unsigned RTS_ComputeStateHash( void );

#ifdef __cplusplus
}
#endif
