/*
===========================================================================
RTS session GUI state.

This is the native C++ equivalent of the small query/action surface used by
0 A.D.'s session scripts. It deliberately owns no widgets or renderer data.
===========================================================================
*/

#include "rts_internal.h"

#include <algorithm>
#include <cstring>

namespace rts {

static bool IsControllablePlayer(int playerId) {
	return playerId > RTS_OWNER_NEUTRAL && playerId < kMaxPlayers;
}

static void NormalizeRect(int &minX, int &minY, int &maxX, int &maxY) {
	if (minX > maxX) {
		std::swap(minX, maxX);
	}
	if (minY > maxY) {
		std::swap(minY, maxY);
	}
}

}  // namespace rts

extern "C" {

void RTS_GuiClearSelection( int playerId ) {
	if ( !rts::IsControllablePlayer( playerId ) ) {
		return;
	}
	rts::GetState().guiSelections[playerId].clear();
}

int RTS_GuiSelectRect( int playerId, int minX, int minY, int maxX, int maxY ) {
	if ( !rts::IsControllablePlayer( playerId ) ) {
		return 0;
	}
	if ( !rts::GetState().initialized ) {
		RTS_Init();
	}
	rts::State &state = rts::GetState();
	std::vector<rtsEntityId_t> &selection = state.guiSelections[playerId];

	rts::NormalizeRect( minX, minY, maxX, maxY );
	selection.clear();
	for ( const rts::Entity &entity : state.entities ) {
		if ( entity.owner == playerId && entity.x >= minX && entity.x <= maxX &&
			entity.y >= minY && entity.y <= maxY ) {
			selection.push_back( entity.id );
		}
	}
	return static_cast<int>( selection.size() );
}

int RTS_GuiGetSelection( int playerId, rtsEntityId_t *out, int maxOut ) {
	const rts::State &state = rts::GetState();
	const std::vector<rtsEntityId_t> *selection;
	int count;

	if ( !rts::IsControllablePlayer( playerId ) || maxOut < 0 ) {
		return 0;
	}
	selection = &state.guiSelections[playerId];
	count = static_cast<int>( selection->size() );
	if ( out ) {
		const int limit = std::min( count, maxOut );
		for ( int i = 0; i < limit; ++i ) {
			out[i] = ( *selection )[i];
		}
	}
	return count;
}

int RTS_GuiGetState( int playerId, rtsGuiState_t *outState ) {
	const rts::State &state = rts::GetState();
	const std::vector<rtsEntityId_t> *selection;
	const rts::Entity *primary = nullptr;

	if ( !outState || !rts::IsControllablePlayer( playerId ) ) {
		return 0;
	}
	std::memset( outState, 0, sizeof( *outState ) );
	selection = &state.guiSelections[playerId];
	if ( !selection->empty() ) {
		primary = rts::FindEntityConst( selection->front() );
	}

	outState->playerId = playerId;
	outState->currentTurn = state.currentTurn;
	outState->entityCount = static_cast<int>( state.entities.size() );
	outState->pendingCommands = static_cast<int>( state.pendingCommands.size() );
	outState->playerResources = state.playerResources[playerId];
	outState->selectedCount = static_cast<int>( selection->size() );
	if ( primary ) {
		outState->primarySelection = primary->id;
		outState->primaryHitpoints = primary->hitpoints;
		outState->primaryResources = primary->resources;
	}
	return 1;
}

int RTS_GuiIssueMoveSelected( int playerId, int targetX, int targetY ) {
	const rts::State &state = rts::GetState();
	const std::vector<rtsEntityId_t> selection =
		rts::IsControllablePlayer( playerId ) ? state.guiSelections[playerId] : std::vector<rtsEntityId_t>{};
	int posted = 0;

	for ( rtsEntityId_t entityId : selection ) {
		if ( !rts::FindEntityConst( entityId ) ) {
			continue;
		}
		rtsCommand_t command = {};
		command.type = RTS_COMMAND_SET_POSITION;
		command.turn = state.currentTurn + 1;
		command.playerId = playerId;
		command.sequence = entityId;
		command.entityId = entityId;
		command.targetX = targetX;
		command.targetY = targetY;
		posted += RTS_PostCommand( &command );
	}
	return posted;
}

}
