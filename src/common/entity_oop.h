/*
===============================================================================
OOP Entity Architecture - Modern C++ Entity System

Replaces the legacy gentity_t struct with a modern object-oriented class hierarchy.
Provides proper inheritance, polymorphism, and component-based design while
maintaining compatibility with existing Quake 3 engine code.
===============================================================================
*/

#pragma once

#include "q_shared.h"
#include "qcommon.h"

#ifdef __cplusplus

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>

// Forward declarations
struct entityState_t;
struct entityShared_t;
struct gclient_t;

//===============================================================================
// Component System
//===============================================================================

/**
 * @brief Base component interface for entity components
 */
class EntityComponent {
public:
    virtual ~EntityComponent() = default;

    /**
     * @brief Called when component is attached to an entity
     */
    virtual void OnAttach() {}

    /**
     * @brief Called when component is detached from an entity
     */
    virtual void OnDetach() {}

    /**
     * @brief Called every frame for component updates
     * @param deltaTime Time elapsed since last frame
     */
    virtual void Update(float deltaTime) {}

    /**
     * @brief Get component type identifier
     */
    virtual const char* GetTypeName() const = 0;

    /**
     * @brief Clone this component
     */
    virtual std::unique_ptr<EntityComponent> Clone() const = 0;
};

/**
 * @brief Transform component for position, rotation, and scale
 */
class TransformComponent : public EntityComponent {
public:
    vec3_t position;
    vec3_t angles;      // Euler angles (pitch, yaw, roll)
    vec3_t scale;

    TransformComponent() {
        VectorClear(position);
        VectorClear(angles);
        VectorSet(scale, 1.0f, 1.0f, 1.0f);
    }

    const char* GetTypeName() const override { return "Transform"; }
    std::unique_ptr<EntityComponent> Clone() const override;
};

/**
 * @brief Health component for damageable entities
 */
class HealthComponent : public EntityComponent {
public:
    int maxHealth;
    int currentHealth;
    int armor;
    qboolean invulnerable;

    HealthComponent() : maxHealth(100), currentHealth(100), armor(0), invulnerable(qfalse) {}

    const char* GetTypeName() const override { return "Health"; }
    std::unique_ptr<EntityComponent> Clone() const override;
};

/**
 * @brief Physics component for movement and collision
 */
class PhysicsComponent : public EntityComponent {
public:
    vec3_t velocity;
    vec3_t acceleration;
    float mass;
    float friction;
    float bounce;
    int clipmask;

    PhysicsComponent() : mass(1.0f), friction(1.0f), bounce(0.0f), clipmask(MASK_SOLID) {
        VectorClear(velocity);
        VectorClear(acceleration);
    }

    const char* GetTypeName() const override { return "Physics"; }
    std::unique_ptr<EntityComponent> Clone() const override;
};

/**
 * @brief AI component for intelligent entities
 */
class AIComponent : public EntityComponent {
public:
    int aiState;
    vec3_t targetPosition;
    gentity_t* targetEntity;
    float moveSpeed;
    float turnSpeed;

    AIComponent() : aiState(0), targetEntity(nullptr), moveSpeed(100.0f), turnSpeed(180.0f) {
        VectorClear(targetPosition);
    }

    const char* GetTypeName() const override { return "AI"; }
    std::unique_ptr<EntityComponent> Clone() const override;
};

//===============================================================================
// Base Entity Class
//===============================================================================

/**
 * @brief Base entity class replacing the legacy gentity_t struct
 *
 * Provides modern OOP design with proper inheritance, polymorphism,
 * and component-based architecture while maintaining compatibility
 * with existing Quake 3 engine code.
 */
class BaseEntity {
protected:
    // Legacy compatibility fields (must remain in this order for server communication)
    entityState_t s;       // communicated by server to clients
    entityShared_t r;      // shared by both the server system and game

    // Modern OOP fields
    int entityIndex;       // Index in global entity array
    std::string classname; // Entity class name
    int spawnflags;        // Spawn flags from map editor
    qboolean inuse;        // Whether this entity slot is active
    int flags;            // Entity flags (FL_*)

    // Component system
    std::unordered_map<std::string, std::unique_ptr<EntityComponent>> components;

public:
    // Constructor/Destructor
    BaseEntity();
    virtual ~BaseEntity();

    // Entity lifecycle
    virtual void Spawn();
    virtual void Think();
    virtual void Touch(BaseEntity* other);
    virtual void Use(BaseEntity* activator, BaseEntity* caller);
    virtual void Pain(BaseEntity* attacker, int damage);
    virtual void Die(BaseEntity* inflictor, BaseEntity* attacker, int damage, int mod);

    // Component management
    template<typename T>
    void AddComponent(std::unique_ptr<T> component) {
        static_assert(std::is_base_of<EntityComponent, T>::value, "T must inherit from EntityComponent");
        const char* typeName = component->GetTypeName();
        components[typeName] = std::move(component);
        components[typeName]->OnAttach();
    }

    template<typename T>
    T* GetComponent() {
        static_assert(std::is_base_of<EntityComponent, T>::value, "T must inherit from EntityComponent");
        auto it = components.find(T().GetTypeName());
        return it != components.end() ? static_cast<T*>(it->second.get()) : nullptr;
    }

    template<typename T>
    void RemoveComponent() {
        static_assert(std::is_base_of<EntityComponent, T>::value, "T must inherit from EntityComponent");
        const char* typeName = T().GetTypeName();
        auto it = components.find(typeName);
        if (it != components.end()) {
            it->second->OnDetach();
            components.erase(it);
        }
    }

    // Legacy compatibility accessors
    entityState_t* GetEntityState() { return &s; }
    entityShared_t* GetEntityShared() { return &r; }
    const entityState_t* GetEntityState() const { return &s; }
    const entityShared_t* GetEntityShared() const { return &r; }

    // Modern accessors
    int GetEntityIndex() const { return entityIndex; }
    void SetEntityIndex(int index) { entityIndex = index; }

    const std::string& GetClassname() const { return classname; }
    void SetClassname(const std::string& name) { classname = name; }

    int GetSpawnflags() const { return spawnflags; }
    void SetSpawnflags(int flags) { spawnflags = flags; }

    qboolean IsInUse() const { return inuse; }
    void SetInUse(qboolean use) { inuse = use; }

    int GetFlags() const { return flags; }
    void SetFlags(int f) { flags = f; }

    // Utility methods
    virtual void FreeEntity();
    virtual void LinkEntity();
    virtual void UnlinkEntity();

    // Position and movement
    void SetOrigin(const vec3_t origin);
    void GetOrigin(vec3_t origin) const;
    void SetAngles(const vec3_t angles);
    void GetAngles(vec3_t angles) const;

    // Factory method for creating entities by classname
    static std::unique_ptr<BaseEntity> CreateEntity(const std::string& classname);
};

//===============================================================================
// Derived Entity Classes
//===============================================================================

/**
 * @brief Player entity class
 */
class PlayerEntity : public BaseEntity {
private:
    gclient_t* client;

public:
    PlayerEntity();
    ~PlayerEntity() override;

    void Spawn() override;
    void Think() override;

    gclient_t* GetClient() { return client; }
    void SetClient(gclient_t* c) { client = c; }
};

/**
 * @brief Item entity class for pickups
 */
class ItemEntity : public BaseEntity {
private:
    int itemIndex;
    int respawnTime;

public:
    ItemEntity();
    ~ItemEntity() override;

    void Spawn() override;
    void Think() override;
    void Touch(BaseEntity* other) override;

    int GetItemIndex() const { return itemIndex; }
    void SetItemIndex(int index) { itemIndex = index; }

    int GetRespawnTime() const { return respawnTime; }
    void SetRespawnTime(int time) { respawnTime = time; }
};

/**
 * @brief Mover entity class for doors, platforms, etc.
 */
class MoverEntity : public BaseEntity {
private:
    int moverState;
    vec3_t pos1, pos2;
    int soundPos1, sound1to2, sound2to1, soundPos2, soundLoop;

public:
    MoverEntity();
    ~MoverEntity() override;

    void Spawn() override;
    void Think() override;

    int GetMoverState() const { return moverState; }
    void SetMoverState(int state) { moverState = state; }

    void GetPos1(vec3_t pos) const { VectorCopy(pos1, pos); }
    void SetPos1(const vec3_t pos) { VectorCopy(pos, pos1); }

    void GetPos2(vec3_t pos) const { VectorCopy(pos2, pos); }
    void SetPos2(const vec3_t pos) { VectorCopy(pos, pos2); }
};

/**
 * @brief Trigger entity class for area triggers
 */
class TriggerEntity : public BaseEntity {
private:
    int wait;
    int random;

public:
    TriggerEntity();
    ~TriggerEntity() override;

    void Spawn() override;
    void Touch(BaseEntity* other) override;

    int GetWait() const { return wait; }
    void SetWait(int w) { wait = w; }

    int GetRandom() const { return random; }
    void SetRandom(int r) { random = r; }
};

//===============================================================================
// Entity Manager
//===============================================================================

/**
 * @brief Global entity manager for the OOP entity system
 */
class EntityManager {
private:
    static EntityManager* instance;
    std::vector<std::unique_ptr<BaseEntity>> entities;
    std::vector<BaseEntity*> activeEntities;

    EntityManager();
    ~EntityManager();

public:
    // Singleton access
    static EntityManager* GetInstance();
    static void DestroyInstance();

    // Entity management
    BaseEntity* CreateEntity(const std::string& classname);
    void DestroyEntity(BaseEntity* entity);
    void DestroyEntity(int entityIndex);

    // Entity access
    BaseEntity* GetEntity(int index);
    const BaseEntity* GetEntity(int index) const;
    int GetEntityCount() const;
    const std::vector<BaseEntity*>& GetActiveEntities() const;

    // Update all entities
    void UpdateAll(float deltaTime);

    // Legacy compatibility functions
    void LinkEntity(BaseEntity* entity);
    void UnlinkEntity(BaseEntity* entity);
};

//===============================================================================
// Legacy Compatibility Layer
//===============================================================================

/**
 * @brief Compatibility typedef for legacy code
 * This allows existing code to continue using gentity_t while
 * the underlying implementation uses the new OOP system.
 */
typedef BaseEntity gentity_t;

/**
 * @brief Entity factory function for legacy compatibility
 */
extern "C" {
#endif // __cplusplus

// C interface for legacy compatibility
gentity_t* G_Spawn(void);
void G_FreeEntity(gentity_t* ent);
gentity_t* G_Find(gentity_t* from, int fieldofs, const char* match);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __cplusplus