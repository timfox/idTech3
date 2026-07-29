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

static bool CanControl(const rtsCommand_t &cmd, const Entity &entity) {
	return cmd.playerId > RTS_OWNER_NEUTRAL && entity.owner == cmd.playerId;
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
		case RTS_COMMAND_BUILD:
			if ( cmd.playerId > RTS_OWNER_NEUTRAL && cmd.playerId < kMaxPlayers &&
					state.playerResources[cmd.playerId] >= kBuildCost ) {
				state.playerResources[cmd.playerId] -= kBuildCost;
				CreateEntity(cmd.playerId, cmd.targetX, cmd.targetY);
			}
			break;
		case RTS_COMMAND_SPAWN_RESOURCE:
			if ( cmd.playerId == RTS_OWNER_NEUTRAL ) {
				const int amount = cmd.data > 0 ? cmd.data : kDefaultResourceNodeAmount;
				CreateEntity(RTS_OWNER_NEUTRAL, cmd.targetX, cmd.targetY, amount);
			}
			break;
		case RTS_COMMAND_MOVE:
			if (Entity *entity = FindEntity(cmd.entityId)) {
				if (CanControl(cmd, *entity)) {
					entity->x = cmd.targetX;
					entity->y = cmd.targetY;
				}
			}
			break;
		case RTS_COMMAND_ATTACK:
			if (Entity *attacker = FindEntity(cmd.entityId)) {
				Entity *target = FindEntity(cmd.targetEntityId);
				if (target && CanControl(cmd, *attacker) && target->owner != attacker->owner) {
					target->hitpoints -= 10;
				}
			}
			break;
		case RTS_COMMAND_GATHER:
			if ( cmd.playerId > RTS_OWNER_NEUTRAL && cmd.playerId < kMaxPlayers ) {
				if ( Entity *entity = FindEntity(cmd.entityId) ) {
					Entity *resource = FindEntity(cmd.targetEntityId);
					if ( CanControl(cmd, *entity) && resource && resource->owner == RTS_OWNER_NEUTRAL && resource->resources > 0 ) {
						const int amount = cmd.data > 0 ? cmd.data : kDefaultGatherAmount;
						const int gathered = std::min(amount, resource->resources);
						resource->resources -= gathered;
						state.playerResources[cmd.playerId] += gathered;
					}
				}
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
