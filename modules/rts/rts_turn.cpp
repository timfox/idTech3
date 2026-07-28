/*
===========================================================================
RTS deterministic turn stepping.
===========================================================================
*/

#include "rts_internal.h"

namespace rts {

void ApplyQueuedCommands(int msec) {
	State &state = GetState();

	for (const rtsCommand_t &cmd : state.pendingCommands) {
		if (cmd.type == RTS_COMMAND_BUILD) {
			CreateEntity(cmd.targetX, cmd.targetY);
		}
	}

	state.pendingCommands.clear();
	state.turnMsec += msec;
}

}  // namespace rts

extern "C" {

void RTS_RunTurn( int msec ) {
	if ( msec <= 0 ) {
		return;
	}
	if ( !rts::GetState().initialized ) {
		RTS_Init();
	}
	rts::ApplyQueuedCommands( msec );
}

}

