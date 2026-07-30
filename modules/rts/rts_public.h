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
	RTS_COMMAND_CREATE_ENTITY,
	RTS_COMMAND_SET_POSITION,
	RTS_COMMAND_SET_HITPOINTS,
	RTS_COMMAND_SET_ENTITY_RESOURCES,
	RTS_COMMAND_SET_PLAYER_RESOURCES,
	RTS_COMMAND_STOP
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

typedef struct rtsRenderEntity_s {
	rtsEntityId_t entityId;
	int owner;
	int hitpoints;
	int resources;
	qhandle_t modelHandle;
	char modelPath[MAX_QPATH];
	float origin[3];
	float yawDegrees;
	float scale;
} rtsRenderEntity_t;

#define RTS_GUI_MAX_SELECTION 32

/*
 * Client-facing session state modelled after 0 A.D.'s session GUI queries.
 * It is data only: presentation belongs to the native client shell or Lua.
 */
typedef struct rtsGuiState_s {
	int playerId;
	int currentTurn;
	int entityCount;
	int pendingCommands;
	int playerResources;
	int selectedCount;
	rtsEntityId_t primarySelection;
	int primaryHitpoints;
	int primaryResources;
} rtsGuiState_t;

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
int  RTS_GetEntityResources( rtsEntityId_t id );
qhandle_t RTS_GetEntityModelHandle( rtsEntityId_t id );
int  RTS_GetEntityModelPath( rtsEntityId_t id, char *outPath, int outPathSize );
int  RTS_SetEntityModel( rtsEntityId_t id, const char *modelPath, qhandle_t modelHandle );
int  RTS_SetDefaultModelForOwner( int owner, const char *modelPath, qhandle_t modelHandle );
int  RTS_BuildRenderEntities( rtsRenderEntity_t *outEntities, int maxOut, float zOrigin, float unitScale );
int  RTS_GetPlayerResources( int playerId );
int  RTS_SelectRect( int playerId, int minX, int minY, int maxX, int maxY, rtsEntityId_t *out, int maxOut );
void RTS_GuiClearSelection( int playerId );
int  RTS_GuiSelectRect( int playerId, int minX, int minY, int maxX, int maxY );
int  RTS_GuiGetSelection( int playerId, rtsEntityId_t *out, int maxOut );
int  RTS_GuiGetState( int playerId, rtsGuiState_t *outState );
int  RTS_GuiIssueMoveSelected( int playerId, int targetX, int targetY );
int  RTS_FindGridPath( int width, int height, const unsigned char *blocked, int startX, int startY, int goalX, int goalY, int *outX, int *outY, int maxOut );
unsigned RTS_ComputeStateHash( void );

#ifdef __cplusplus
}
#endif
