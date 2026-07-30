/*
===========================================================================
RTS entity storage.
===========================================================================
*/

#include "rts_internal.h"

#include <cstring>

namespace rts {

static void CopyModelPath(char *dst, const char *src) {
	std::size_t i = 0;

	if (!dst) {
		return;
	}
	if (!src || !src[0]) {
		dst[0] = '\0';
		return;
	}
	for (; i + 1u < static_cast<std::size_t>(MAX_QPATH) && src[i] != '\0'; ++i) {
		dst[i] = src[i];
	}
	dst[i] = '\0';
}

bool SetEntityModel(Entity &entity, const char *modelPath, qhandle_t modelHandle) {
	CopyModelPath(entity.modelPath, modelPath);
	entity.modelHandle = modelHandle;
	return entity.modelPath[0] != '\0' || entity.modelHandle != 0;
}

rtsEntityId_t CreateEntity(int owner, int x, int y, int resources) {
	State &state = GetState();
	Entity entity;

	entity.id = state.nextEntityId++;
	entity.owner = owner;
	entity.x = x;
	entity.y = y;
	entity.resources = resources;
	if (owner >= RTS_OWNER_NEUTRAL && owner < kMaxPlayers) {
		const ModelBinding &binding = state.defaultModels[owner];
		SetEntityModel(entity, binding.path, binding.handle);
	}
	state.entities.push_back(entity);
	return entity.id;
}

Entity *FindEntity(rtsEntityId_t id) {
	State &state = GetState();
	for (Entity &entity : state.entities) {
		if (entity.id == id) {
			return &entity;
		}
	}
	return nullptr;
}

const Entity *FindEntityConst(rtsEntityId_t id) {
	const State &state = GetState();
	for (const Entity &entity : state.entities) {
		if (entity.id == id) {
			return &entity;
		}
	}
	return nullptr;
}

}  // namespace rts
