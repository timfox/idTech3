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

typedef enum rtsCommandType_e {
	RTS_COMMAND_NONE = 0,
	RTS_COMMAND_MOVE,
	RTS_COMMAND_ATTACK,
	RTS_COMMAND_STOP,
	RTS_COMMAND_BUILD
} rtsCommandType_t;

typedef struct rtsCommand_s {
	rtsCommandType_t type;
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
int  RTS_GetPendingCommandCount( void );
int  RTS_GetEntityCount( void );

#ifdef __cplusplus
}
#endif

