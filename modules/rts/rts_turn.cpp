/*
===========================================================================
RTS deterministic turn stepping.
===========================================================================
*/

#include "rts_internal.h"

#include <algorithm>

namespace rts {

static bool CommandLess(const rtsCommand_t &a, const rtsCommand_t &b) {
	if (a.turn != b.turn) {
		return a.turn < b.turn;
	}
	if (a.playerId != b.playerId) {
		return a.playerId < b.playerId;
	}
	if (a.sequence != b.sequence) {
		return a.sequence < b.sequence;
	}
	if (a.type != b.type) {
		return a.type < b.type;
	}
	return a.entityId < b.entityId;
}

void ApplyQueuedCommands(int msec) {
	State &state = GetState();
	const int turnToRun = state.currentTurn + 1;
	std::vector<rtsCommand_t> remaining;

	std::stable_sort(state.pendingCommands.begin(), state.pendingCommands.end(), CommandLess);

	for (const rtsCommand_t &cmd : state.pendingCommands) {
		if (cmd.turn > turnToRun) {
			remaining.push_back(cmd);
			continue;
		}
		switch (cmd.type) {
		case RTS_COMMAND_CREATE_ENTITY:
			CreateEntity(cmd.playerId, cmd.targetX, cmd.targetY, cmd.data);
			break;
		case RTS_COMMAND_SET_POSITION:
			if (Entity *entity = FindEntity(cmd.entityId)) {
				entity->x = cmd.targetX;
				entity->y = cmd.targetY;
			}
			break;
		case RTS_COMMAND_SET_HITPOINTS:
			if (Entity *entity = FindEntity(cmd.entityId)) {
				entity->hitpoints = cmd.data;
			}
			break;
		case RTS_COMMAND_SET_ENTITY_RESOURCES:
			if (Entity *entity = FindEntity(cmd.entityId)) {
				entity->resources = cmd.data;
			}
			break;
		case RTS_COMMAND_SET_PLAYER_RESOURCES:
			if ( cmd.playerId > RTS_OWNER_NEUTRAL && cmd.playerId < kMaxPlayers ) {
				state.playerResources[cmd.playerId] = cmd.data;
			}
			break;
		case RTS_COMMAND_STOP:
		case RTS_COMMAND_NONE:
		default:
			break;
		}
		state.executedCommands.push_back(cmd);
	}

	state.pendingCommands = remaining;
	state.currentTurn = turnToRun;
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
