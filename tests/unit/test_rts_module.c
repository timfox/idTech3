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

	RTS_Init();
	ASSERT(RTS_GetTurnMsec() == 0, "initial turn time");
	ASSERT(RTS_GetPendingCommandCount() == 0, "initial command queue");
	ASSERT(RTS_GetEntityCount() == 0, "initial entity count");

	cmd.type = RTS_COMMAND_BUILD;
	cmd.entityId = 0;
	cmd.targetEntityId = 0;
	cmd.targetX = 64;
	cmd.targetY = 128;
	cmd.data = 0;

	ASSERT(RTS_PostCommand(&cmd) == 1, "post build command");
	ASSERT(RTS_GetPendingCommandCount() == 1, "queued command count");

	RTS_RunTurn(50);
	ASSERT(RTS_GetTurnMsec() == 50, "turn time after step");
	ASSERT(RTS_GetPendingCommandCount() == 0, "queue drained after turn");
	ASSERT(RTS_GetEntityCount() == 1, "build command creates entity");

	RTS_Shutdown();
	ASSERT(RTS_GetTurnMsec() == 0, "shutdown resets turn time");
	ASSERT(RTS_GetEntityCount() == 0, "shutdown resets entities");

	printf("PASS: unit_rts_module\n");
	return 0;
}

