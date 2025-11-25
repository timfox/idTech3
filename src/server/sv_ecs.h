/*
===========================================================================
Server-Side ECS Integration

Server-side ECS integration with svEntity_t bridge for network sync.
===========================================================================
*/

#ifndef __SV_ECS_H__
#define __SV_ECS_H__

#include "../qcommon/q_shared.h"
#include "../qcommon/ecs.h"
#include "server.h"

#ifdef USE_ENTT

#ifdef __cplusplus
// Forward declaration for C++-only function
extern void SV_ECS_NetworkSyncSystem_Update(void);

extern "C" {
#endif

// Initialization
void SV_ECS_Init(void);
void SV_ECS_Shutdown(void);

// Entity bridge functions
ecs_entity_t SV_ECS_RegisterSvEntity(svEntity_t *ent);
void SV_ECS_UnregisterSvEntity(svEntity_t *ent);
ecs_entity_t SV_ECS_GetEntityFromSvEntity(svEntity_t *ent);
svEntity_t *SV_ECS_GetSvEntityFromEntity(ecs_entity_t entity);

// Sync functions
void SV_ECS_SyncToSvEntity(void);  // Sync ECS to svEntity_t for network
void SV_ECS_SyncFromSvEntity(void); // Sync svEntity_t to ECS

// Frame update
void SV_ECS_RunFrame(float deltaTime);

#ifdef __cplusplus
}
#endif

#endif // USE_ENTT

#endif // __SV_ECS_H__

