/*
===========================================================================
RTS entity storage.
===========================================================================
*/

#include "rts_internal.h"

namespace rts {

rtsEntityId_t CreateEntity(int owner, int x, int y) {
	State &state = GetState();
	Entity entity;

	entity.id = state.nextEntityId++;
	entity.owner = owner;
	entity.x = x;
	entity.y = y;
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
