/*
===========================================================================
RTS simulation world lifetime and shared state.
===========================================================================
*/

#include "rts_internal.h"

namespace rts {

static State s_state;

State &GetState() {
	return s_state;
}

void ResetState() {
	s_state = State{};
}

}  // namespace rts

extern "C" {

void RTS_Init( void ) {
	rts::ResetState();
	rts::GetState().initialized = true;
}

void RTS_Shutdown( void ) {
	rts::ResetState();
}

int RTS_GetTurnMsec( void ) {
	return rts::GetState().turnMsec;
}

int RTS_GetPendingCommandCount( void ) {
	return static_cast<int>( rts::GetState().pendingCommands.size() );
}

int RTS_GetEntityCount( void ) {
	return static_cast<int>( rts::GetState().entities.size() );
}

}

