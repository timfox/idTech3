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
	rtsEntityId_t selected[4];
	unsigned hashBeforeMove;
	unsigned hashAfterMove;
	int x = 0;
	int y = 0;

	RTS_Init();
	ASSERT(RTS_GetTurnMsec() == 0, "initial turn time");
	ASSERT(RTS_GetPendingCommandCount() == 0, "initial command queue");
	ASSERT(RTS_GetEntityCount() == 0, "initial entity count");

	cmd.type = RTS_COMMAND_BUILD;
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
	ASSERT(RTS_GetPendingCommandCount() == 0, "queue drained after turn");
	ASSERT(RTS_GetEntityCount() == 2, "build commands create entities");
	ASSERT(RTS_GetEntityOwner(1) == RTS_OWNER_PLAYER1, "first built entity owner");
	ASSERT(RTS_GetEntityPosition(1, &x, &y) == 1, "first built entity position readable");
	ASSERT(x == 16 && y == 32, "stable command ordering by player/sequence");

	ASSERT(RTS_SelectRect(RTS_OWNER_PLAYER1, 0, 0, 80, 140, selected, 4) == 2, "select player entities in rect");
	ASSERT(selected[0] == 1 && selected[1] == 2, "selection order follows entity order");
	ASSERT(RTS_SelectRect(RTS_OWNER_PLAYER2, 0, 0, 80, 140, selected, 4) == 0, "selection filters owner");

	hashBeforeMove = RTS_ComputeStateHash();
	move.type = RTS_COMMAND_MOVE;
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
	ASSERT(RTS_GetPendingCommandCount() == 0, "queue drained after turn");
	ASSERT(RTS_GetEntityPosition(1, &x, &y) == 1, "moved entity position readable");
	ASSERT(x == 512 && y == 256, "move command updates controlled entity");
	hashAfterMove = RTS_ComputeStateHash();
	ASSERT(hashBeforeMove != hashAfterMove, "state hash changes after deterministic state mutation");

	RTS_Shutdown();
	ASSERT(RTS_GetTurnMsec() == 0, "shutdown resets turn time");
	ASSERT(RTS_GetEntityCount() == 0, "shutdown resets entities");

	printf("PASS: unit_rts_module\n");
	return 0;
}
