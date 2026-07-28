#pragma once

#include "rts_public.h"

#include <vector>

namespace rts {

struct Entity {
	rtsEntityId_t id = 0;
	int x = 0;
	int y = 0;
};

struct State {
	bool initialized = false;
	int turnMsec = 0;
	rtsEntityId_t nextEntityId = 1;
	std::vector<Entity> entities;
	std::vector<rtsCommand_t> pendingCommands;
};

State &GetState();
void ResetState();
rtsEntityId_t CreateEntity(int x, int y);
void ApplyQueuedCommands(int msec);

}  // namespace rts

