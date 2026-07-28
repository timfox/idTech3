/*
===========================================================================
RTS selection queries.
===========================================================================
*/

#include "rts_internal.h"

#include <algorithm>

namespace rts {

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

int RTS_SelectRect( int playerId, int minX, int minY, int maxX, int maxY, rtsEntityId_t *out, int maxOut ) {
	if ( !rts::GetState().initialized ) {
		RTS_Init();
	}
	if ( playerId <= RTS_OWNER_NEUTRAL || maxOut < 0 ) {
		return 0;
	}

	rts::NormalizeRect( minX, minY, maxX, maxY );
	int count = 0;
	for (const rts::Entity &entity : rts::GetState().entities) {
		if (entity.owner != playerId) {
			continue;
		}
		if (entity.x < minX || entity.x > maxX || entity.y < minY || entity.y > maxY) {
			continue;
		}
		if (out && count < maxOut) {
			out[count] = entity.id;
		}
		++count;
	}
	return count;
}

}
