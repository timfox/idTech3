#pragma once

#include "rts_public.h"

#include <array>
#include <vector>

namespace rts {

constexpr int kMaxPlayers = RTS_OWNER_PLAYER4 + 1;
struct Entity {
	rtsEntityId_t id = 0;
	int owner = RTS_OWNER_NEUTRAL;
	int x = 0;
	int y = 0;
	int hitpoints = 100;
	int resources = 0;
	qhandle_t modelHandle = 0;
	char modelPath[MAX_QPATH] = {};
};

struct ModelBinding {
	qhandle_t handle = 0;
	char path[MAX_QPATH] = {};
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
	std::array<std::vector<rtsEntityId_t>, kMaxPlayers> guiSelections;
	int playerResources[kMaxPlayers] = {};
	std::array<ModelBinding, kMaxPlayers> defaultModels = {};
};

State &GetState();
void ResetState();
rtsEntityId_t CreateEntity(int owner, int x, int y, int resources = 0);
Entity *FindEntity(rtsEntityId_t id);
const Entity *FindEntityConst(rtsEntityId_t id);
bool SetEntityModel(Entity &entity, const char *modelPath, qhandle_t modelHandle);
void ApplyQueuedCommands(int msec);
unsigned HashState();

}  // namespace rts
