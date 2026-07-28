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

int RTS_GetEntityOwner( rtsEntityId_t id ) {
	const rts::Entity *entity = rts::FindEntityConst( id );
	return entity ? entity->owner : RTS_OWNER_NEUTRAL;
}

int RTS_GetEntityPosition( rtsEntityId_t id, int *x, int *y ) {
	const rts::Entity *entity = rts::FindEntityConst( id );
	if ( !entity ) {
		return 0;
	}
	if ( x ) {
		*x = entity->x;
	}
	if ( y ) {
		*y = entity->y;
	}
	return 1;
}

unsigned RTS_ComputeStateHash( void ) {
	return rts::HashState();
}

}
