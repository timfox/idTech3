/*
===========================================================================
RTS simulation world lifetime and shared state.
===========================================================================
*/

#include "rts_internal.h"

#include <cstring>

namespace rts {

static State s_state;

State &GetState() {
	return s_state;
}

void ResetState() {
	s_state = State{};
}

}  // namespace rts

static void RTS_CopyPath( char *dst, int dstSize, const char *src ) {
	int i = 0;

	if ( !dst || dstSize <= 0 ) {
		return;
	}
	if ( !src || !src[0] ) {
		dst[0] = '\0';
		return;
	}
	for ( ; i + 1 < dstSize && src[i] != '\0'; ++i ) {
		dst[i] = src[i];
	}
	dst[i] = '\0';
}

extern "C" {

void RTS_Init( void ) {
	rts::ResetState();
	rts::GetState().initialized = true;
}

void RTS_Shutdown( void ) {
	rts::ResetState();
}

int RTS_GetTurnMsec( void ) {
	return rts::GetState().turnMsec;
}

int RTS_GetCurrentTurn( void ) {
	return rts::GetState().currentTurn;
}

int RTS_GetPendingCommandCount( void ) {
	return static_cast<int>( rts::GetState().pendingCommands.size() );
}

int RTS_GetExecutedCommandCount( void ) {
	return static_cast<int>( rts::GetState().executedCommands.size() );
}

int RTS_GetEntityCount( void ) {
	return static_cast<int>( rts::GetState().entities.size() );
}

int RTS_GetEntityOwner( rtsEntityId_t id ) {
	const rts::Entity *entity = rts::FindEntityConst( id );
	return entity ? entity->owner : RTS_OWNER_NEUTRAL;
}

int RTS_GetEntityPosition( rtsEntityId_t id, int *x, int *y ) {
	const rts::Entity *entity = rts::FindEntityConst( id );
	if ( !entity ) {
		return 0;
	}
	if ( x ) {
		*x = entity->x;
	}
	if ( y ) {
		*y = entity->y;
	}
	return 1;
}

int RTS_GetEntityHitpoints( rtsEntityId_t id ) {
	const rts::Entity *entity = rts::FindEntityConst( id );
	return entity ? entity->hitpoints : 0;
}

int RTS_GetEntityResources( rtsEntityId_t id ) {
	const rts::Entity *entity = rts::FindEntityConst( id );
	return entity ? entity->resources : 0;
}

qhandle_t RTS_GetEntityModelHandle( rtsEntityId_t id ) {
	const rts::Entity *entity = rts::FindEntityConst( id );
	return entity ? entity->modelHandle : 0;
}

int RTS_GetEntityModelPath( rtsEntityId_t id, char *outPath, int outPathSize ) {
	const rts::Entity *entity = rts::FindEntityConst( id );
	if ( !entity || !outPath || outPathSize <= 0 ) {
		return 0;
	}
	RTS_CopyPath( outPath, outPathSize, entity->modelPath );
	return entity->modelPath[0] != '\0' ? 1 : 0;
}

int RTS_SetEntityModel( rtsEntityId_t id, const char *modelPath, qhandle_t modelHandle ) {
	rts::Entity *entity = rts::FindEntity( id );
	if ( !entity ) {
		return 0;
	}
	rts::SetEntityModel( *entity, modelPath, modelHandle );
	return 1;
}

int RTS_SetDefaultModelForOwner( int owner, const char *modelPath, qhandle_t modelHandle ) {
	if ( owner < RTS_OWNER_NEUTRAL || owner >= rts::kMaxPlayers ) {
		return 0;
	}
	rts::ModelBinding &binding = rts::GetState().defaultModels[owner];
	RTS_CopyPath( binding.path, sizeof( binding.path ), modelPath );
	binding.handle = modelHandle;
	return 1;
}

int RTS_BuildRenderEntities( rtsRenderEntity_t *outEntities, int maxOut, float zOrigin, float unitScale ) {
	const rts::State &state = rts::GetState();
	int count = 0;
	float scale = unitScale > 0.0f ? unitScale : 1.0f;

	if ( maxOut < 0 ) {
		return 0;
	}
	for ( const rts::Entity &entity : state.entities ) {
		if ( count < maxOut && outEntities ) {
			rtsRenderEntity_t *out = &outEntities[count];
			std::memset( out, 0, sizeof( *out ) );
			out->entityId = entity.id;
			out->owner = entity.owner;
			out->hitpoints = entity.hitpoints;
			out->resources = entity.resources;
			out->modelHandle = entity.modelHandle;
			RTS_CopyPath( out->modelPath, sizeof( out->modelPath ), entity.modelPath );
			out->origin[0] = static_cast<float>( entity.x ) * scale;
			out->origin[1] = static_cast<float>( entity.y ) * scale;
			out->origin[2] = zOrigin;
			out->yawDegrees = 0.0f;
			out->scale = scale;
		}
		count++;
	}
	return count;
}

int RTS_GetPlayerResources( int playerId ) {
	if ( playerId <= RTS_OWNER_NEUTRAL || playerId >= rts::kMaxPlayers ) {
		return 0;
	}
	return rts::GetState().playerResources[playerId];
}

unsigned RTS_ComputeStateHash( void ) {
	return rts::HashState();
}

}
