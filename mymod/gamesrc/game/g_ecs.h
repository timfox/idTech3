/*
===========================================================================
Game Module ECS Integration

Game module ECS integration with gentity_t bridge for game logic.
===========================================================================
*/

#ifndef __G_ECS_H__
#define __G_ECS_H__

#include "g_local.h"
#include "ecs.h"

#ifdef USE_ENTT

// Initialization
void G_ECS_Init(void);
void G_ECS_Shutdown(void);

// Entity bridge functions
ecs_entity_t G_ECS_RegisterGentity(gentity_t *ent);
void G_ECS_UnregisterGentity(gentity_t *ent);
ecs_entity_t G_ECS_GetEntityFromGentity(gentity_t *ent);
gentity_t *G_ECS_GetGentityFromEntity(ecs_entity_t entity);

// Sync functions
void G_ECS_SyncToGentity(void);  // Sync ECS to gentity_t for network
void G_ECS_SyncFromGentity(void); // Sync gentity_t to ECS

// Frame update
void G_ECS_RunFrame(float deltaTime);

#endif // USE_ENTT

#endif // __G_ECS_H__

