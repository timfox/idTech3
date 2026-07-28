/*
===========================================================================
RTS entity storage.
===========================================================================
*/

#include "rts_internal.h"

namespace rts {

rtsEntityId_t CreateEntity(int x, int y) {
	State &state = GetState();
	Entity entity;

	entity.id = state.nextEntityId++;
	entity.x = x;
	entity.y = y;
	state.entities.push_back(entity);
	return entity.id;
}

}  // namespace rts

