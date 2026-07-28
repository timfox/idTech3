/*
===========================================================================
RTS command queue.
===========================================================================
*/

#include "rts_internal.h"

extern "C" {

int RTS_PostCommand( const rtsCommand_t *cmd ) {
	if ( !cmd || cmd->type == RTS_COMMAND_NONE ) {
		return 0;
	}
	if ( cmd->playerId <= RTS_OWNER_NEUTRAL ) {
		return 0;
	}
	if ( !rts::GetState().initialized ) {
		RTS_Init();
	}

	rtsCommand_t queued = *cmd;
	if ( queued.turn <= rts::GetState().currentTurn ) {
		queued.turn = rts::GetState().currentTurn + 1;
	}

	rts::GetState().pendingCommands.push_back( queued );
	return 1;
}

}
