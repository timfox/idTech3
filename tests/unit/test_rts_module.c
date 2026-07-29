/*
 * Unit test: RTS simulation module public ABI.
 * Run: ctest -R unit_rts_module
 */
#include <stdio.h>

#include "rts_public.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

int main(void) {
	rtsCommand_t cmd;
	rtsCommand_t move;
	rtsCommand_t enemyBuild;
	rtsCommand_t attack;
	rtsCommand_t gather;
	rtsCommand_t resource;
	rtsEntityId_t selected[4];
	unsigned char blocked[25] = { 0 };
	int pathX[16];
	int pathY[16];
	unsigned hashBeforeMove;
	unsigned hashAfterMove;
	int pathCount;
	int i;
	int x = 0;
	int y = 0;

	RTS_Init();
	ASSERT(RTS_GetTurnMsec() == 0, "initial turn time");
	ASSERT(RTS_GetCurrentTurn() == 0, "initial turn number");
	ASSERT(RTS_GetPendingCommandCount() == 0, "initial command queue");
	ASSERT(RTS_GetExecutedCommandCount() == 0, "initial replay command log");
	ASSERT(RTS_GetEntityCount() == 0, "initial entity count");
	ASSERT(RTS_GetPlayerResources(RTS_OWNER_PLAYER1) == 0, "initial player resources");

	cmd.type = RTS_COMMAND_CREATE_ENTITY;
	cmd.turn = 1;
	cmd.playerId = RTS_OWNER_PLAYER1;
	cmd.sequence = 2;
	cmd.entityId = 0;
	cmd.targetEntityId = 0;
	cmd.targetX = 64;
	cmd.targetY = 128;
	cmd.data = 0;

	ASSERT(RTS_PostCommand(&cmd) == 1, "post build command");
	cmd.sequence = 1;
	cmd.targetX = 16;
	cmd.targetY = 32;
	ASSERT(RTS_PostCommand(&cmd) == 1, "post second build command");
	ASSERT(RTS_GetPendingCommandCount() == 2, "queued command count");

	RTS_RunTurn(50);
	ASSERT(RTS_GetTurnMsec() == 50, "turn time after step");
	ASSERT(RTS_GetCurrentTurn() == 1, "turn number after first step");
	ASSERT(RTS_GetPendingCommandCount() == 0, "queue drained after turn");
	ASSERT(RTS_GetExecutedCommandCount() == 2, "replay command log after first step");
	ASSERT(RTS_GetEntityCount() == 2, "create commands create entities");
	ASSERT(RTS_GetPlayerResources(RTS_OWNER_PLAYER1) == 0, "create commands do not apply game economy");
	ASSERT(RTS_GetEntityOwner(1) == RTS_OWNER_PLAYER1, "first built entity owner");
	ASSERT(RTS_GetEntityHitpoints(1) == 100, "first built entity starts with full hitpoints");
	ASSERT(RTS_GetEntityResources(1) == 0, "combat entity has no resources");
	ASSERT(RTS_GetEntityPosition(1, &x, &y) == 1, "first built entity position readable");
	ASSERT(x == 16 && y == 32, "stable command ordering by player/sequence");

	ASSERT(RTS_SelectRect(RTS_OWNER_PLAYER1, 0, 0, 80, 140, selected, 4) == 2, "select player entities in rect");
	ASSERT(selected[0] == 1 && selected[1] == 2, "selection order follows entity order");
	ASSERT(RTS_SelectRect(RTS_OWNER_PLAYER2, 0, 0, 80, 140, selected, 4) == 0, "selection filters owner");

	hashBeforeMove = RTS_ComputeStateHash();
	move.type = RTS_COMMAND_SET_POSITION;
	move.turn = 3;
	move.playerId = RTS_OWNER_PLAYER1;
	move.sequence = 3;
	move.entityId = 1;
	move.targetEntityId = 0;
	move.targetX = 512;
	move.targetY = 256;
	move.data = 0;

	ASSERT(RTS_PostCommand(&move) == 1, "post move command");
	ASSERT(RTS_GetPendingCommandCount() == 1, "queued command count");

	RTS_RunTurn(50);
	ASSERT(RTS_GetTurnMsec() == 100, "turn time after second step");
	ASSERT(RTS_GetCurrentTurn() == 2, "turn number after second step");
	ASSERT(RTS_GetPendingCommandCount() == 1, "future command remains queued");
	ASSERT(RTS_GetEntityPosition(1, &x, &y) == 1, "future move did not execute early");
	ASSERT(x == 16 && y == 32, "future move leaves position unchanged");

	RTS_RunTurn(50);
	ASSERT(RTS_GetTurnMsec() == 150, "turn time after third step");
	ASSERT(RTS_GetCurrentTurn() == 3, "turn number after third step");
	ASSERT(RTS_GetPendingCommandCount() == 0, "queue drained after scheduled turn");
	ASSERT(RTS_GetExecutedCommandCount() == 3, "replay command log after move");
	ASSERT(RTS_GetEntityPosition(1, &x, &y) == 1, "moved entity position readable");
	ASSERT(x == 512 && y == 256, "move command updates controlled entity");
	hashAfterMove = RTS_ComputeStateHash();
	ASSERT(hashBeforeMove != hashAfterMove, "state hash changes after deterministic state mutation");

	enemyBuild = cmd;
	enemyBuild.turn = 4;
	enemyBuild.playerId = RTS_OWNER_PLAYER2;
	enemyBuild.sequence = 4;
	enemyBuild.targetX = 1024;
	enemyBuild.targetY = 256;
	ASSERT(RTS_PostCommand(&enemyBuild) == 1, "post enemy create command");
	RTS_RunTurn(50);
	ASSERT(RTS_GetEntityCount() == 3, "enemy create creates entity");
	ASSERT(RTS_GetEntityOwner(3) == RTS_OWNER_PLAYER2, "enemy entity owner");
	ASSERT(RTS_GetEntityHitpoints(3) == 100, "enemy entity starts with full hitpoints");

	attack.type = RTS_COMMAND_SET_HITPOINTS;
	attack.turn = 5;
	attack.playerId = RTS_OWNER_NEUTRAL;
	attack.sequence = 5;
	attack.entityId = 3;
	attack.targetEntityId = 0;
	attack.targetX = 0;
	attack.targetY = 0;
	attack.data = 90;
	ASSERT(RTS_PostCommand(&attack) == 1, "post hitpoint set command");
	RTS_RunTurn(50);
	ASSERT(RTS_GetEntityHitpoints(3) == 90, "hitpoint set updates enemy hitpoints");

	resource.type = RTS_COMMAND_CREATE_ENTITY;
	resource.turn = 6;
	resource.playerId = RTS_OWNER_NEUTRAL;
	resource.sequence = 6;
	resource.entityId = 0;
	resource.targetEntityId = 0;
	resource.targetX = 256;
	resource.targetY = 256;
	resource.data = 100;
	ASSERT(RTS_PostCommand(&resource) == 1, "post resource spawn command");
	RTS_RunTurn(50);
	ASSERT(RTS_GetEntityCount() == 4, "resource spawn creates neutral entity");
	ASSERT(RTS_GetEntityOwner(4) == RTS_OWNER_NEUTRAL, "resource entity owner");
	ASSERT(RTS_GetEntityResources(4) == 100, "resource entity starts with stock");

	gather.type = RTS_COMMAND_SET_ENTITY_RESOURCES;
	gather.turn = 7;
	gather.playerId = RTS_OWNER_NEUTRAL;
	gather.sequence = 7;
	gather.entityId = 4;
	gather.targetEntityId = 0;
	gather.targetX = 0;
	gather.targetY = 0;
	gather.data = 25;
	ASSERT(RTS_PostCommand(&gather) == 1, "post resource stock set command");
	RTS_RunTurn(50);
	ASSERT(RTS_GetPlayerResources(RTS_OWNER_PLAYER1) == 0, "resource stock set does not apply game economy");
	ASSERT(RTS_GetEntityResources(4) == 25, "resource stock set updates resource stock");

	gather.type = RTS_COMMAND_SET_PLAYER_RESOURCES;
	gather.turn = 8;
	gather.playerId = RTS_OWNER_PLAYER1;
	gather.sequence = 8;
	gather.entityId = 0;
	gather.data = 400;
	ASSERT(RTS_PostCommand(&gather) == 1, "post player resource set command");
	RTS_RunTurn(50);
	ASSERT(RTS_GetPlayerResources(RTS_OWNER_PLAYER1) == 400, "player resource set updates resources");
	ASSERT(RTS_GetEntityResources(4) == 25, "player resource set leaves resource stock unchanged");

	blocked[1 * 5 + 2] = 1;
	blocked[2 * 5 + 2] = 1;
	blocked[3 * 5 + 2] = 1;
	pathCount = RTS_FindGridPath(5, 5, blocked, 0, 2, 4, 2, pathX, pathY, 16);
	ASSERT(pathCount == 9, "path grid routes around vertical obstruction");
	ASSERT(pathX[0] == 0 && pathY[0] == 2, "path starts at requested cell");
	ASSERT(pathX[pathCount - 1] == 4 && pathY[pathCount - 1] == 2, "path ends at requested cell");
	for (i = 0; i < pathCount; ++i) {
		ASSERT(!(pathX[i] == 2 && pathY[i] >= 1 && pathY[i] <= 3), "path avoids blocked cells");
	}
	ASSERT(RTS_FindGridPath(5, 5, blocked, 0, 2, 4, 2, NULL, NULL, 0) == pathCount, "path query returns required length without output arrays");
	blocked[2 * 5 + 0] = 1;
	ASSERT(RTS_FindGridPath(5, 5, blocked, 0, 2, 4, 2, pathX, pathY, 16) == 0, "blocked start rejects path");
	blocked[2 * 5 + 0] = 0;
	blocked[0 * 5 + 2] = 1;
	blocked[4 * 5 + 2] = 1;
	ASSERT(RTS_FindGridPath(5, 5, blocked, 0, 2, 4, 2, pathX, pathY, 16) == 0, "sealed wall has no path");

	RTS_Shutdown();
	ASSERT(RTS_GetTurnMsec() == 0, "shutdown resets turn time");
	ASSERT(RTS_GetCurrentTurn() == 0, "shutdown resets turn number");
	ASSERT(RTS_GetEntityCount() == 0, "shutdown resets entities");

	printf("PASS: unit_rts_module\n");
	return 0;
}
