/*
===============================================================================
Legacy Entity Compatibility Implementation

Implements compatibility functions for the OOP entity system.
===============================================================================
*/

#ifdef USE_OOP_ENTITIES

#include "entity_legacy.h"
#include "entity_oop.h"
#include <cstring>

extern "C" {

//===============================================================================
// Legacy Compatibility Functions
//===============================================================================

gentity_t* G_Spawn(void) {
    EntityManager* em = EntityManager::GetInstance();
    return em->CreateEntity("base");
}

void G_FreeEntity(gentity_t* ent) {
    if (ent) {
        EntityManager* em = EntityManager::GetInstance();
        em->DestroyEntity(ent);
    }
}

gentity_t* G_Find(gentity_t* from, int fieldofs, const char* match) {
    EntityManager* em = EntityManager::GetInstance();
    const auto& activeEntities = em->GetActiveEntities();

    // Start from 'from' entity or beginning
    size_t startIndex = 0;
    if (from) {
        startIndex = from->GetEntityIndex() + 1;
    }

    // Simple linear search for now
    // In a real implementation, this would use spatial partitioning
    for (size_t i = startIndex; i < activeEntities.size(); ++i) {
        BaseEntity* entity = activeEntities[i];
        if (entity && entity->IsInUse()) {
            // For now, just compare classnames
            // A full implementation would handle different field types
            if (strcmp(entity->GetClassname().c_str(), match) == 0) {
                return entity;
            }
        }
    }

    return nullptr;
}

//===============================================================================
// Utility Functions
//===============================================================================

qboolean G_ValidEntity(gentity_t* ent) {
    return (ent && ent->IsInUse()) ? qtrue : qfalse;
}

gentity_t* G_SpawnPlayer(void) {
    EntityManager* em = EntityManager::GetInstance();
    return em->CreateEntity("player");
}

gentity_t* G_SpawnItem(int itemIndex) {
    EntityManager* em = EntityManager::GetInstance();
    ItemEntity* item = static_cast<ItemEntity*>(em->CreateEntity("item"));
    if (item) {
        item->SetItemIndex(itemIndex);
    }
    return item;
}

gentity_t* G_SpawnMover(const vec3_t pos1, const vec3_t pos2) {
    EntityManager* em = EntityManager::GetInstance();
    MoverEntity* mover = static_cast<MoverEntity*>(em->CreateEntity("mover"));
    if (mover) {
        mover->SetPos1(pos1);
        mover->SetPos2(pos2);
    }
    return mover;
}

gentity_t* G_SpawnTrigger(const vec3_t mins, const vec3_t maxs) {
    EntityManager* em = EntityManager::GetInstance();
    TriggerEntity* trigger = static_cast<TriggerEntity*>(em->CreateEntity("trigger"));
    if (trigger) {
        // Set up trigger bounds
        VectorCopy(mins, trigger->GetEntityShared()->mins);
        VectorCopy(maxs, trigger->GetEntityShared()->maxs);
    }
    return trigger;
}

} // extern "C"

#endif // USE_OOP_ENTITIES