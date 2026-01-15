/*
=============================================================================
id Tech 3 - Entity Component System (ECS)

Modern ECS implementation using entt library.
Provides component-based entity management for game objects.
=============================================================================
*/

#pragma once

#ifdef USE_ENTT

#include "../common/q_shared.h"
// Use single-header entt for easier integration
#include "../../external/include/entt/single_include/entt/entt.hpp"

// Forward declarations
struct gentity_t;

// ECS Registry type
using ECSRegistry = entt::registry;

// Core Components
// ===============

// Transform component - position, rotation, scale
struct TransformComponent {
    vec3_t position;
    vec3_t angles;      // Euler angles (pitch, yaw, roll)
    vec3_t scale;
    
    TransformComponent() {
        VectorClear(position);
        VectorClear(angles);
        VectorSet(scale, 1.0f, 1.0f, 1.0f);
    }
};

// Health component
struct HealthComponent {
    int maxHealth;
    int currentHealth;
    int armor;
    qboolean invulnerable;
    
    HealthComponent() : maxHealth(100), currentHealth(100), armor(0), invulnerable(qfalse) {}
};

// Physics component - velocity, acceleration
struct PhysicsComponent {
    vec3_t velocity;
    vec3_t acceleration;
    float mass;
    float friction;
    qboolean onGround;
    
    PhysicsComponent() {
        VectorClear(velocity);
        VectorClear(acceleration);
        mass = 1.0f;
        friction = 0.8f;
        onGround = qfalse;
    }
};

// Render component - model, shader, etc.
struct RenderComponent {
    char model[MAX_QPATH];
    char shader[MAX_QPATH];
    vec3_t color;
    float alpha;
    qboolean visible;
    
    RenderComponent() {
        model[0] = '\0';
        shader[0] = '\0';
        VectorSet(color, 1.0f, 1.0f, 1.0f);
        alpha = 1.0f;
        visible = qtrue;
    }
};

// AI component - for NPCs and bots
struct AIComponent {
    char behavior[MAX_QPATH];
    float reactionTime;
    float awarenessRadius;
    int targetEntity;  // Entity ID of current target
    qboolean active;
    
    AIComponent() {
        behavior[0] = '\0';
        reactionTime = 0.5f;
        awarenessRadius = 512.0f;
        targetEntity = -1;
        active = qtrue;
    }
};

// Weapon component
struct WeaponComponent {
    int weaponType;
    int ammo;
    int maxAmmo;
    float fireRate;
    float lastFireTime;
    qboolean canFire;
    
    WeaponComponent() : weaponType(0), ammo(0), maxAmmo(0), 
                        fireRate(0.1f), lastFireTime(0.0f), canFire(qtrue) {}
};

// Inventory component
struct InventoryComponent {
    int items[32];  // Item IDs
    int itemCount;
    int maxItems;
    
    InventoryComponent() : itemCount(0), maxItems(32) {
        memset(items, 0, sizeof(items));
    }
};

// Trigger component - for interactive objects
struct TriggerComponent {
    char triggerType[MAX_QPATH];
    float radius;
    qboolean activated;
    int activationCount;
    float cooldown;
    float lastActivation;
    
    TriggerComponent() {
        triggerType[0] = '\0';
        radius = 64.0f;
        activated = qfalse;
        activationCount = 0;
        cooldown = 1.0f;
        lastActivation = 0.0f;
    }
};

// Single Player specific components
// =================================

// SP Enemy component - for single player enemies
struct SPEnemyComponent {
    int enemyType;
    float spawnTime;
    float despawnTime;
    qboolean respawnable;
    int respawnDelay;
    vec3_t spawnPosition;
    vec3_t spawnAngles;
    
    SPEnemyComponent() : enemyType(0), spawnTime(0.0f), despawnTime(0.0f),
                         respawnable(qfalse), respawnDelay(0) {
        VectorClear(spawnPosition);
        VectorClear(spawnAngles);
    }
};

// SP Objective component - for mission objectives
struct SPObjectiveComponent {
    char objectiveName[MAX_QPATH];
    char description[MAX_QPATH];
    qboolean completed;
    qboolean required;
    int objectiveType;  // KILL, COLLECT, REACH, etc.
    int targetValue;
    int currentValue;
    
    SPObjectiveComponent() {
        objectiveName[0] = '\0';
        description[0] = '\0';
        completed = qfalse;
        required = qtrue;
        objectiveType = 0;
        targetValue = 0;
        currentValue = 0;
    }
};

// SP Checkpoint component - for save points
struct SPCheckpointComponent {
    char checkpointName[MAX_QPATH];
    qboolean activated;
    float activationTime;
    
    SPCheckpointComponent() {
        checkpointName[0] = '\0';
        activated = qfalse;
        activationTime = 0.0f;
    }
};

// SP Dialog component - for NPC conversations
struct SPDialogComponent {
    char dialogTree[MAX_QPATH];
    int currentDialogNode;
    qboolean inConversation;
    int speakerEntity;
    
    SPDialogComponent() {
        dialogTree[0] = '\0';
        currentDialogNode = 0;
        inConversation = qfalse;
        speakerEntity = -1;
    }
};

// ECS System Functions
// ====================

// Initialize ECS system
void G_ECS_Init(void);

// Shutdown ECS system
void G_ECS_Shutdown(void);

// Update ECS systems
void G_ECS_Update(float deltaTime);

// Entity management
entt::entity G_ECS_CreateEntity(void);
void G_ECS_DestroyEntity(entt::entity entity);
qboolean G_ECS_EntityExists(entt::entity entity);

// Component management
template<typename Component>
void G_ECS_AddComponent(entt::entity entity, const Component& component);

template<typename Component>
void G_ECS_RemoveComponent(entt::entity entity);

template<typename Component>
Component* G_ECS_GetComponent(entt::entity entity);

template<typename Component>
qboolean G_ECS_HasComponent(entt::entity entity);

// Get global registry
ECSRegistry& G_ECS_GetRegistry(void);

// Integration with legacy gentity_t
entt::entity G_ECS_GetEntityFromGentity(gentity_t* gent);
gentity_t* G_ECS_GetGentityFromEntity(entt::entity entity);
entt::entity G_ECS_CreateEntityFromGentity(gentity_t* gent);
void G_ECS_UpdateGentityFromECS(gentity_t* gent, entt::entity entity);
void G_ECS_SyncECSFromGentity(gentity_t* gent, entt::entity entity);

#endif // USE_ENTT
