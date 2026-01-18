/*
===============================================================================
Legacy Entity Compatibility Layer

Provides backward compatibility for existing Quake 3 code while using the
new OOP entity architecture underneath. This allows gradual migration
from gentity_t to the modern entity system.
===============================================================================
*/

#pragma once

#include "q_shared.h"

#ifdef __cplusplus
#include "entity_oop.h"
#endif

//===============================================================================
// Legacy Compatibility Macros and Types
//===============================================================================

#ifdef USE_OOP_ENTITIES

#ifdef __cplusplus
extern "C" {
#endif

// Legacy gentity_t typedef (now points to BaseEntity)
typedef BaseEntity gentity_t;
typedef gclient_t gclient_t;

// Legacy entity field access macros
#define ent->s                    ent->GetEntityState()
#define ent->r                    ent->GetEntityShared()
#define ent->classname            ent->GetClassname().c_str()
#define ent->spawnflags           ent->GetSpawnflags()
#define ent->flags                ent->GetFlags()
#define ent->inuse                ent->IsInUse()

// Legacy entity manipulation functions
#define G_SetOrigin(ent, origin)  ent->SetOrigin(origin)
#define G_GetOrigin(ent, origin)  ent->GetOrigin(origin)
#define G_SetAngles(ent, angles)  ent->SetAngles(angles)
#define G_GetAngles(ent, angles)  ent->GetAngles(angles)

// Legacy spawn function
gentity_t* G_Spawn(void);

// Legacy free function
#define G_FreeEntity(ent)         G_FreeEntity(ent)

// Legacy find function
gentity_t* G_Find(gentity_t* from, int fieldofs, const char* match);

// Legacy think function
#define G_RunThink(ent)           if (ent && ent->IsInUse()) ent->Think()

// Legacy touch function
#define G_TouchTriggers(ent)      // TODO: Implement trigger touching

#ifdef __cplusplus
} // extern "C"

// C++ helper functions for legacy compatibility
inline void G_SetClassname(gentity_t* ent, const char* classname) {
    if (ent) ent->SetClassname(classname);
}

inline void G_SetSpawnflags(gentity_t* ent, int flags) {
    if (ent) ent->SetSpawnflags(flags);
}

inline void G_SetFlags(gentity_t* ent, int flags) {
    if (ent) ent->SetFlags(flags);
}

inline void G_SetInUse(gentity_t* ent, qboolean inuse) {
    if (ent) ent->SetInUse(inuse);
}

// Component access helpers
template<typename T>
inline T* G_GetComponent(gentity_t* ent) {
    return ent ? ent->GetComponent<T>() : nullptr;
}

template<typename T>
inline void G_AddComponent(gentity_t* ent, std::unique_ptr<T> component) {
    if (ent) ent->AddComponent(std::move(component));
}

template<typename T>
inline void G_RemoveComponent(gentity_t* ent) {
    if (ent) ent->RemoveComponent<T>();
}

#endif // __cplusplus

#else // !USE_OOP_ENTITIES

// Traditional legacy compatibility - include original headers
#include "g_public.h"

// Legacy macros remain unchanged
#define G_SetOrigin               G_SetOrigin
#define G_GetOrigin               G_GetOrigin
#define G_SetAngles               G_SetAngles
#define G_GetAngles               G_GetAngles
#define G_RunThink                G_RunThink
#define G_TouchTriggers           G_TouchTriggers
#define G_SetClassname            G_SetClassname
#define G_SetSpawnflags           G_SetSpawnflags
#define G_SetFlags                G_SetFlags
#define G_SetInUse                G_SetInUse

#endif // USE_OOP_ENTITIES

//===============================================================================
// Utility Functions (available in both modes)
//===============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Entity validation
qboolean G_ValidEntity(gentity_t* ent);

// Entity spawning helpers
gentity_t* G_SpawnPlayer(void);
gentity_t* G_SpawnItem(int itemIndex);
gentity_t* G_SpawnMover(const vec3_t pos1, const vec3_t pos2);
gentity_t* G_SpawnTrigger(const vec3_t mins, const vec3_t maxs);

#ifdef __cplusplus
} // extern "C"
#endif