/*
=============================================================================
id Tech 3 - ECS Integration with Legacy Game System

Bridges ECS system with existing gentity_t system.
=============================================================================
*/

#ifdef USE_ENTT

#include "g_ecs.h"
#include "g_public.h"
#include "../common/qcommon.h"

// External reference to entity map (defined in g_ecs.c)
extern entt::entity g_entityMap[1024];

// Integration functions to convert between gentity_t and ECS entities

/*
===============
G_ECS_CreateEntityFromGentity

Create an ECS entity from a gentity_t
===============
*/
entt::entity G_ECS_CreateEntityFromGentity(gentity_t* gent)
{
    if (gent == NULL) {
        return entt::null;
    }
    
    entt::entity entity = G_ECS_CreateEntity();
    auto& registry = G_ECS_GetRegistry();
    
    // Add TransformComponent from gentity_t
    TransformComponent transform;
    VectorCopy(gent->r.currentOrigin, transform.position);
    VectorCopy(gent->r.currentAngles, transform.angles);
    VectorSet(transform.scale, 1.0f, 1.0f, 1.0f);
    registry.emplace<TransformComponent>(entity, transform);
    
    // Add HealthComponent if entity has health
    if (gent->health > 0) {
        HealthComponent health;
        health.maxHealth = gent->health;
        health.currentHealth = gent->health;
        health.armor = 0;
        health.invulnerable = qfalse;
        registry.emplace<HealthComponent>(entity, health);
    }
    
    // Add RenderComponent if entity has a model
    if (gent->model != NULL) {
        RenderComponent render;
        Q_strncpyz(render.model, gent->model, sizeof(render.model));
        VectorSet(render.color, 1.0f, 1.0f, 1.0f);
        render.alpha = 1.0f;
        render.visible = qtrue;
        registry.emplace<RenderComponent>(entity, render);
    }
    
    // Store mapping (simplified - would need proper bidirectional mapping)
    if (gent->s.number < 1024) {
        g_entityMap[gent->s.number] = entity;
    }
    
    return entity;
}

/*
===============
G_ECS_UpdateGentityFromECS

Update gentity_t from ECS components
===============
*/
void G_ECS_UpdateGentityFromECS(gentity_t* gent, entt::entity entity)
{
    if (gent == NULL || !G_ECS_EntityExists(entity)) {
        return;
    }
    
    auto& registry = G_ECS_GetRegistry();
    
    // Update transform
    if (registry.all_of<TransformComponent>(entity)) {
        const auto& transform = registry.get<TransformComponent>(entity);
        VectorCopy(transform.position, gent->r.currentOrigin);
        VectorCopy(transform.angles, gent->r.currentAngles);
    }
    
    // Update health
    if (registry.all_of<HealthComponent>(entity)) {
        const auto& health = registry.get<HealthComponent>(entity);
        gent->health = health.currentHealth;
    }
}

/*
===============
G_ECS_SyncECSFromGentity

Sync ECS components from gentity_t
===============
*/
void G_ECS_SyncECSFromGentity(gentity_t* gent, entt::entity entity)
{
    if (gent == NULL || !G_ECS_EntityExists(entity)) {
        return;
    }
    
    auto& registry = G_ECS_GetRegistry();
    
    // Update transform component
    if (registry.all_of<TransformComponent>(entity)) {
        auto& transform = registry.get<TransformComponent>(entity);
        VectorCopy(gent->r.currentOrigin, transform.position);
        VectorCopy(gent->r.currentAngles, transform.angles);
    }
    
    // Update health component
    if (registry.all_of<HealthComponent>(entity)) {
        auto& health = registry.get<HealthComponent>(entity);
        health.currentHealth = gent->health;
    }
}

#endif // USE_ENTT
