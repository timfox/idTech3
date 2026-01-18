/*
===============================================================================
OOP Entity Architecture - Implementation

Implementation of the modern C++ entity system replacing gentity_t.
===============================================================================
*/

#ifdef __cplusplus

#include "entity_oop.h"
#include "qcommon.h"
#include <cstring>
#include <algorithm>

//===============================================================================
// Component Implementations
//===============================================================================

std::unique_ptr<EntityComponent> TransformComponent::Clone() const {
    auto clone = std::make_unique<TransformComponent>();
    VectorCopy(position, clone->position);
    VectorCopy(angles, clone->angles);
    VectorCopy(scale, clone->scale);
    return clone;
}

std::unique_ptr<EntityComponent> HealthComponent::Clone() const {
    auto clone = std::make_unique<HealthComponent>();
    clone->maxHealth = maxHealth;
    clone->currentHealth = currentHealth;
    clone->armor = armor;
    clone->invulnerable = invulnerable;
    return clone;
}

std::unique_ptr<EntityComponent> PhysicsComponent::Clone() const {
    auto clone = std::make_unique<PhysicsComponent>();
    VectorCopy(velocity, clone->velocity);
    VectorCopy(acceleration, clone->acceleration);
    clone->mass = mass;
    clone->friction = friction;
    clone->bounce = bounce;
    clone->clipmask = clipmask;
    return clone;
}

std::unique_ptr<EntityComponent> AIComponent::Clone() const {
    auto clone = std::make_unique<AIComponent>();
    clone->aiState = aiState;
    VectorCopy(targetPosition, clone->targetPosition);
    clone->targetEntity = targetEntity;
    clone->moveSpeed = moveSpeed;
    clone->turnSpeed = turnSpeed;
    return clone;
}

//===============================================================================
// Base Entity Implementation
//===============================================================================

BaseEntity::BaseEntity()
    : entityIndex(-1), spawnflags(0), inuse(qfalse), flags(0) {
    memset(&s, 0, sizeof(s));
    memset(&r, 0, sizeof(r));
}

BaseEntity::~BaseEntity() {
    // Detach all components
    for (auto& pair : components) {
        pair.second->OnDetach();
    }
    components.clear();
}

void BaseEntity::Spawn() {
    inuse = qtrue;

    // Initialize default transform if not present
    if (!GetComponent<TransformComponent>()) {
        AddComponent(std::make_unique<TransformComponent>());
    }

    Com_Printf("Entity %d (%s) spawned\n", entityIndex, classname.c_str());
}

void BaseEntity::Think() {
    // Update all components
    for (auto& pair : components) {
        pair.second->Update(0.016f); // Assume 60 FPS for now
    }
}

void BaseEntity::Touch(BaseEntity* other) {
    // Default implementation - override in derived classes
    Com_DPrintf("Entity %d touched entity %d\n", entityIndex, other ? other->GetEntityIndex() : -1);
}

void BaseEntity::Use(BaseEntity* activator, BaseEntity* caller) {
    // Default implementation - override in derived classes
    Com_DPrintf("Entity %d used by entity %d\n", entityIndex, activator ? activator->GetEntityIndex() : -1);
}

void BaseEntity::Pain(BaseEntity* attacker, int damage) {
    // Default implementation - apply damage to health component if present
    auto health = GetComponent<HealthComponent>();
    if (health && !health->invulnerable) {
        health->currentHealth -= damage;
        if (health->currentHealth <= 0) {
            Die(attacker, attacker, damage, MOD_UNKNOWN);
        }
    }
}

void BaseEntity::Die(BaseEntity* inflictor, BaseEntity* attacker, int damage, int mod) {
    // Default implementation - mark as not in use
    Com_Printf("Entity %d died\n", entityIndex);
    inuse = qfalse;
}

void BaseEntity::FreeEntity() {
    inuse = qfalse;
    classname.clear();
    flags = 0;

    // Clear components
    for (auto& pair : components) {
        pair.second->OnDetach();
    }
    components.clear();
}

void BaseEntity::LinkEntity() {
    // TODO: Implement entity linking for collision detection
    Com_DPrintf("Entity %d linked\n", entityIndex);
}

void BaseEntity::UnlinkEntity() {
    // TODO: Implement entity unlinking
    Com_DPrintf("Entity %d unlinked\n", entityIndex);
}

void BaseEntity::SetOrigin(const vec3_t origin) {
    VectorCopy(origin, r.currentOrigin);
    auto transform = GetComponent<TransformComponent>();
    if (transform) {
        VectorCopy(origin, transform->position);
    }
}

void BaseEntity::GetOrigin(vec3_t origin) const {
    VectorCopy(r.currentOrigin, origin);
}

void BaseEntity::SetAngles(const vec3_t angles) {
    VectorCopy(angles, r.currentAngles);
    auto transform = GetComponent<TransformComponent>();
    if (transform) {
        VectorCopy(angles, transform->angles);
    }
}

void BaseEntity::GetAngles(vec3_t angles) const {
    VectorCopy(r.currentAngles, angles);
}

std::unique_ptr<BaseEntity> BaseEntity::CreateEntity(const std::string& classname) {
    if (classname == "player") {
        return std::make_unique<PlayerEntity>();
    } else if (classname == "item") {
        return std::make_unique<ItemEntity>();
    } else if (classname == "mover") {
        return std::make_unique<MoverEntity>();
    } else if (classname == "trigger") {
        return std::make_unique<TriggerEntity>();
    } else {
        // Default to base entity
        return std::make_unique<BaseEntity>();
    }
}

//===============================================================================
// Derived Entity Implementations
//===============================================================================

PlayerEntity::PlayerEntity() : client(nullptr) {
    classname = "player";
}

PlayerEntity::~PlayerEntity() {
    // Player-specific cleanup
}

void PlayerEntity::Spawn() {
    BaseEntity::Spawn();

    // Add player-specific components
    if (!GetComponent<HealthComponent>()) {
        AddComponent(std::make_unique<HealthComponent>());
    }

    Com_Printf("Player entity %d spawned\n", entityIndex);
}

void PlayerEntity::Think() {
    BaseEntity::Think();

    // Player-specific thinking logic would go here
    // Handle movement, input, etc.
}

ItemEntity::ItemEntity() : itemIndex(0), respawnTime(30) {
    classname = "item";
}

ItemEntity::~ItemEntity() {
    // Item-specific cleanup
}

void ItemEntity::Spawn() {
    BaseEntity::Spawn();

    // Items are physics objects by default
    if (!GetComponent<PhysicsComponent>()) {
        auto physics = std::make_unique<PhysicsComponent>();
        physics->mass = 0.1f; // Light weight
        physics->bounce = 0.3f; // Some bounce
        AddComponent(std::move(physics));
    }

    Com_Printf("Item entity %d spawned\n", entityIndex);
}

void ItemEntity::Think() {
    BaseEntity::Think();

    // Item-specific logic (rotation, bobbing, etc.)
}

void ItemEntity::Touch(BaseEntity* other) {
    BaseEntity::Touch(other);

    // Check if touched by a player
    if (dynamic_cast<PlayerEntity*>(other)) {
        // Give item to player and remove
        Com_Printf("Item %d picked up by player %d\n", entityIndex, other->GetEntityIndex());
        inuse = qfalse;
    }
}

MoverEntity::MoverEntity() : moverState(0), soundPos1(0), sound1to2(0), sound2to1(0), soundPos2(0), soundLoop(0) {
    classname = "mover";
    VectorClear(pos1);
    VectorClear(pos2);
}

MoverEntity::~MoverEntity() {
    // Mover-specific cleanup
}

void MoverEntity::Spawn() {
    BaseEntity::Spawn();

    // Add physics component for collision
    if (!GetComponent<PhysicsComponent>()) {
        auto physics = std::make_unique<PhysicsComponent>();
        physics->mass = 0.0f; // Static
        AddComponent(std::move(physics));
    }

    Com_Printf("Mover entity %d spawned\n", entityIndex);
}

void MoverEntity::Think() {
    BaseEntity::Think();

    // Mover-specific logic (moving between pos1 and pos2)
}

TriggerEntity::TriggerEntity() : wait(0), random(0) {
    classname = "trigger";
}

TriggerEntity::~TriggerEntity() {
    // Trigger-specific cleanup
}

void TriggerEntity::Spawn() {
    BaseEntity::Spawn();

    // Triggers don't have physics by default (they're volumetric)
    Com_Printf("Trigger entity %d spawned\n", entityIndex);
}

void TriggerEntity::Touch(BaseEntity* other) {
    BaseEntity::Touch(other);

    // Trigger activation logic
    Com_Printf("Trigger %d activated by entity %d\n", entityIndex, other->GetEntityIndex());

    // Call use function if defined
    Use(other, other);
}

//===============================================================================
// Entity Manager Implementation
//===============================================================================

EntityManager* EntityManager::instance = nullptr;

EntityManager::EntityManager() {
    entities.reserve(1024); // Pre-allocate space
}

EntityManager::~EntityManager() {
    entities.clear();
    activeEntities.clear();
}

EntityManager* EntityManager::GetInstance() {
    if (!instance) {
        instance = new EntityManager();
    }
    return instance;
}

void EntityManager::DestroyInstance() {
    delete instance;
    instance = nullptr;
}

BaseEntity* EntityManager::CreateEntity(const std::string& classname) {
    auto entity = BaseEntity::CreateEntity(classname);
    if (!entity) {
        return nullptr;
    }

    // Find free entity index
    int index = -1;
    for (size_t i = 0; i < entities.size(); ++i) {
        if (!entities[i] || !entities[i]->IsInUse()) {
            index = static_cast<int>(i);
            entities[i] = std::move(entity);
            break;
        }
    }

    if (index == -1) {
        index = static_cast<int>(entities.size());
        entities.push_back(std::move(entity));
    }

    BaseEntity* entityPtr = entities[index].get();
    entityPtr->SetEntityIndex(index);
    entityPtr->SetClassname(classname);

    // Add to active entities list
    activeEntities.push_back(entityPtr);

    Com_Printf("Created entity %s at index %d\n", classname.c_str(), index);
    return entityPtr;
}

void EntityManager::DestroyEntity(BaseEntity* entity) {
    if (!entity) return;

    int index = entity->GetEntityIndex();
    if (index >= 0 && index < static_cast<int>(entities.size())) {
        entities[index].reset();
    }

    // Remove from active entities
    auto it = std::find(activeEntities.begin(), activeEntities.end(), entity);
    if (it != activeEntities.end()) {
        activeEntities.erase(it);
    }
}

void EntityManager::DestroyEntity(int entityIndex) {
    if (entityIndex >= 0 && entityIndex < static_cast<int>(entities.size())) {
        BaseEntity* entity = entities[entityIndex].get();
        if (entity) {
            DestroyEntity(entity);
        }
    }
}

BaseEntity* EntityManager::GetEntity(int index) {
    if (index >= 0 && index < static_cast<int>(entities.size())) {
        return entities[index].get();
    }
    return nullptr;
}

const BaseEntity* EntityManager::GetEntity(int index) const {
    if (index >= 0 && index < static_cast<int>(entities.size())) {
        return entities[index].get();
    }
    return nullptr;
}

int EntityManager::GetEntityCount() const {
    return static_cast<int>(entities.size());
}

const std::vector<BaseEntity*>& EntityManager::GetActiveEntities() const {
    return activeEntities;
}

void EntityManager::UpdateAll(float deltaTime) {
    for (BaseEntity* entity : activeEntities) {
        if (entity && entity->IsInUse()) {
            entity->Think();
        }
    }
}

void EntityManager::LinkEntity(BaseEntity* entity) {
    if (entity) {
        entity->LinkEntity();
    }
}

void EntityManager::UnlinkEntity(BaseEntity* entity) {
    if (entity) {
        entity->UnlinkEntity();
    }
}

//===============================================================================
// Legacy Compatibility Layer Implementation
//===============================================================================

extern "C" {

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

} // extern "C"

#endif // __cplusplus