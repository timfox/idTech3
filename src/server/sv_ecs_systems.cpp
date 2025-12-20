/*
===========================================================================
Server-Side ECS Systems

Server-specific ECS systems for network sync and server-side logic.
===========================================================================
*/

#ifdef USE_ENTT

#include "sv_ecs.h"
#include "../common/ecs_components.h"
#include <entt/entt.hpp>

extern server_t sv;

/*
================
SV_ECS_NetworkSyncSystem
Sync ECS entities to svEntity_t for network snapshots
This ensures all ECS entities are properly represented in network state
================
*/
void SV_ECS_NetworkSyncSystem_Update(void) {
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return;
	entt::registry &registry = *registry_ptr;
	
	auto view = registry.view<NetworkComponent>();
	
	for (auto entity : view) {
		auto &network = view.get<NetworkComponent>(entity);
		
		if (!network.isServer || network.entityIndex < 0 || network.entityIndex >= MAX_GENTITIES) {
			continue;
		}
		
		// Mark entities that need network sync
		// This will be picked up by SV_ECS_SyncToSvEntity
		network.needsSync = qtrue;
	}
}

#endif // USE_ENTT

