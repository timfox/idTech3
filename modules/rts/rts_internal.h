#pragma once

#include "rts_public.h"

#include <vector>

namespace rts {

struct Entity {
	rtsEntityId_t id = 0;
	int owner = RTS_OWNER_NEUTRAL;
	int x = 0;
	int y = 0;
	int hitpoints = 100;
};

struct State {
	bool initialized = false;
	int currentTurn = 0;
	int turnMsec = 0;
	rtsEntityId_t nextEntityId = 1;
	std::vector<Entity> entities;
	std::vector<rtsCommand_t> pendingCommands;
	std::vector<rtsCommand_t> executedCommands;
	std::vector<rtsEntityId_t> selectionScratch;
};

State &GetState();
void ResetState();
rtsEntityId_t CreateEntity(int owner, int x, int y);
Entity *FindEntity(rtsEntityId_t id);
const Entity *FindEntityConst(rtsEntityId_t id);
void ApplyQueuedCommands(int msec);
unsigned HashState();

}  // namespace rts
