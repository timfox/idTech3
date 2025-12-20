/*
===========================================================================
Game Module ECS Systems

Game module-specific ECS systems for game logic.
===========================================================================
*/

#ifdef USE_ENTT

#include "g_ecs.h"
#include "../../../src/common/ecs_components.h"
#include <entt/entt.hpp>

extern gentity_t g_entities[MAX_GENTITIES];

/*
================
G_ECS_NetworkSyncSystem
Sync ECS entities to gentity_t for network snapshots
This ensures all ECS entities are properly represented in network state
================
*/
void G_ECS_NetworkSyncSystem_Update(void) {
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return;
	entt::registry &registry = *registry_ptr;
	
	auto view = registry.view<NetworkComponent>();
	
	for (auto entity : view) {
		auto &network = view.get<NetworkComponent>(entity);
		
		if (network.isServer || network.entityIndex < 0 || network.entityIndex >= MAX_GENTITIES) {
			continue;
		}
		
		// Mark entities that need network sync
		// This will be picked up by G_ECS_SyncToGentity
		network.needsSync = qtrue;
	}
}

#endif // USE_ENTT

