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
	if ( !rts::GetState().initialized ) {
		RTS_Init();
	}

	rts::GetState().pendingCommands.push_back( *cmd );
	return 1;
}

}

