/*
===========================================================================
RTS replay / OOS state hash helpers.
===========================================================================
*/

#include "rts_internal.h"

namespace rts {

static unsigned HashMix(unsigned hash, unsigned value) {
	hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
	return hash;
}

unsigned HashState() {
	const State &state = GetState();
	unsigned hash = 2166136261u;

	hash = HashMix(hash, static_cast<unsigned>(state.turnMsec));
	hash = HashMix(hash, static_cast<unsigned>(state.nextEntityId));
	hash = HashMix(hash, static_cast<unsigned>(state.entities.size()));
	for (const Entity &entity : state.entities) {
		hash = HashMix(hash, static_cast<unsigned>(entity.id));
		hash = HashMix(hash, static_cast<unsigned>(entity.owner));
		hash = HashMix(hash, static_cast<unsigned>(entity.x));
		hash = HashMix(hash, static_cast<unsigned>(entity.y));
		hash = HashMix(hash, static_cast<unsigned>(entity.hitpoints));
	}
	return hash;
}

}  // namespace rts
