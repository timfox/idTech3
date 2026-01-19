/*
=============================================================================
id Tech 3 - Entity Component System (ECS) Implementation

Modern ECS implementation using entt library.
=============================================================================
*/

#ifdef USE_ENTT

#include "g_ecs.h"
#include "g_public.h"
#include "g_local.h"
#include "../common/qcommon.h"

// Forward declaration - g_local.h may not exist in all builds
#ifndef MAX_GENTITIES
#define MAX_GENTITIES 1024
#endif

// Define the global entity array
struct gentity_t g_entities[MAX_GENTITIES];

// Define the global level structure
level_locals_t level;

// Global ECS registry
static ECSRegistry* g_ecsRegistry = NULL;

// Mapping between entt entities and gentity_t
// Note: This is a simplified mapping - full integration would require
// bidirectional mapping with proper lifecycle management
entt::entity g_entityMap[1024];  // Made non-static for integration file access
static int g_entityMapCount = 0;

/*
===============
G_ECS_Init

Initialize the ECS system
===============
*/
void G_ECS_Init(void)
{
    if (g_ecsRegistry != NULL) {
        Com_Printf("ECS system already initialized\n");
        return;
    }
    
    g_ecsRegistry = new ECSRegistry();
    g_entityMapCount = 0;
    
    Com_Printf("ECS system initialized\n");
}

/*
===============
G_ECS_Shutdown

Shutdown the ECS system
===============
*/
void G_ECS_Shutdown(void)
{
    if (g_ecsRegistry == NULL) {
        return;
    }
    
    // Clear all entities
    g_ecsRegistry->clear();
    
    delete g_ecsRegistry;
    g_ecsRegistry = NULL;
    g_entityMapCount = 0;
    
    Com_Printf("ECS system shutdown\n");
}

/*
===============
G_ECS_Update

Update all ECS systems
===============
*/
void G_ECS_Update(float deltaTime)
{
    if (g_ecsRegistry == NULL) {
        return;
    }
    
    // Update physics system
    auto physicsView = g_ecsRegistry->view<PhysicsComponent, TransformComponent>();
    for (auto entity : physicsView) {
        auto& physics = physicsView.get<PhysicsComponent>(entity);
        auto& transform = physicsView.get<TransformComponent>(entity);
        
        // Apply acceleration to velocity
        VectorMA(physics.velocity, deltaTime, physics.acceleration, physics.velocity);
        
        // Apply friction
        VectorScale(physics.velocity, physics.friction, physics.velocity);
        
        // Update position
        VectorMA(transform.position, deltaTime, physics.velocity, transform.position);
        
        // Clear acceleration (will be set by systems)
        VectorClear(physics.acceleration);
    }
    
    // Update AI system
    auto aiView = g_ecsRegistry->view<AIComponent, TransformComponent>();
    for (auto entity : aiView) {
        auto& ai = aiView.get<AIComponent>(entity);
        
        if (!ai.active) {
            continue;
        }
        
        // AI logic would go here
        // This is a placeholder for AI system integration
    }
    
    // Update weapon system
    auto weaponView = g_ecsRegistry->view<WeaponComponent>();
    for (auto entity : weaponView) {
        auto& weapon = weaponView.get<WeaponComponent>(entity);
        
        // Update fire cooldown
        if (weapon.lastFireTime > 0.0f) {
            weapon.lastFireTime -= deltaTime;
            if (weapon.lastFireTime <= 0.0f) {
                weapon.canFire = qtrue;
                weapon.lastFireTime = 0.0f;
            }
        }
    }
    
    // Update trigger system
    auto triggerView = g_ecsRegistry->view<TriggerComponent, TransformComponent>();
    for (auto entity : triggerView) {
        // auto& trigger = triggerView.get<TriggerComponent>(entity); // TODO: Implement trigger logic
        // auto& transform = triggerView.get<TransformComponent>(entity); // TODO: Implement trigger logic

        // Trigger logic would go here
        // Check for player proximity, etc.
        (void)entity; // Suppress unused variable warning
    }
}

/*
===============
G_ECS_CreateEntity

Create a new entity
===============
*/
entt::entity G_ECS_CreateEntity(void)
{
    if (g_ecsRegistry == NULL) {
        G_ECS_Init();
    }
    
    return g_ecsRegistry->create();
}

/*
===============
G_ECS_DestroyEntity

Destroy an entity
===============
*/
void G_ECS_DestroyEntity(entt::entity entity)
{
    if (g_ecsRegistry == NULL) {
        return;
    }
    
    g_ecsRegistry->destroy(entity);
}

/*
===============
G_ECS_EntityExists

Check if entity exists
===============
*/
qboolean G_ECS_EntityExists(entt::entity entity)
{
    if (g_ecsRegistry == NULL) {
        return qfalse;
    }
    
    return g_ecsRegistry->valid(entity) ? qtrue : qfalse;
}

/*
===============
G_ECS_GetRegistry

Get the global ECS registry
===============
*/
ECSRegistry& G_ECS_GetRegistry(void)
{
    if (g_ecsRegistry == NULL) {
        G_ECS_Init();
    }
    
    return *g_ecsRegistry;
}

/*
===============
G_ECS_GetEntityFromGentity

Get ECS entity from legacy gentity_t
===============
*/
entt::entity G_ECS_GetEntityFromGentity(gentity_t* gent)
{
    if (gent == NULL || gent->s.number < 0 || gent->s.number >= 1024) {
        return entt::null;
    }
    
    // Check if we already have a mapping
    entt::entity entity = g_entityMap[gent->s.number];
    if (G_ECS_EntityExists(entity)) {
        return entity;
    }
    
    // Create new entity and map it
    return G_ECS_CreateEntityFromGentity(gent);
}

/*
===============
G_ECS_GetGentityFromEntity

Get legacy gentity_t from ECS entity
===============
*/
gentity_t* G_ECS_GetGentityFromEntity(entt::entity entity)
{
    // TODO: Implement reverse mapping using a hash map or similar
    // For now, this requires iterating through entities
    // In a full implementation, we'd maintain a bidirectional map
    (void)entity;
    return NULL;
}

#endif // USE_ENTT
