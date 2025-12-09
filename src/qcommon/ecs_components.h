/*
===========================================================================
ECS Core Components

Core component definitions for the engine-level ECS system.
These components are used throughout the engine for common entity properties.
===========================================================================
*/

#ifndef __ECS_COMPONENTS_H__
#define __ECS_COMPONENTS_H__

#ifdef USE_ENTT

#include <entt/entt.hpp>
#include "q_shared.h"

#ifdef __cplusplus
#ifdef USE_BULLET
// Forward declaration to avoid pulling Bullet headers into every translation unit
class btRigidBody;
#endif
#endif

// Transform Component - Position, rotation, scale
struct TransformComponent {
	vec3_t position;
	vec3_t rotation;  // Euler angles
	vec3_t scale;
	
	TransformComponent() {
		VectorClear(position);
		VectorClear(rotation);
		VectorSet(scale, 1.0f, 1.0f, 1.0f);
	}
	
	TransformComponent(const vec3_t pos) {
		VectorCopy(pos, position);
		VectorClear(rotation);
		VectorSet(scale, 1.0f, 1.0f, 1.0f);
	}
};

// Physics Component - Velocity, acceleration, mass
struct PhysicsComponent {
	vec3_t velocity;
	vec3_t acceleration;
	float mass;
	float friction;
	
#ifdef USE_BULLET
	// When true, this entity will be simulated by Bullet instead of the
	// simple integrator. The Bullet world and rigid bodies are managed
	// by the ECS physics system implementation.
	qboolean	useBullet;
	btRigidBody *body;
#endif
	
	PhysicsComponent() : mass(1.0f), friction(0.0f)
#ifdef USE_BULLET
		, useBullet(qfalse), body(nullptr)
#endif
	{
		VectorClear(velocity);
		VectorClear(acceleration);
	}
};

// Health Component - Health, armor, damage tracking
struct HealthComponent {
	int health;
	int maxHealth;
	int armor;
	int maxArmor;
	
	HealthComponent() : health(100), maxHealth(100), armor(0), maxArmor(0) {}
	HealthComponent(int hp, int maxHP) : health(hp), maxHealth(maxHP), armor(0), maxArmor(0) {}
};

// Network Component - Links ECS entity to engine entity structures
struct NetworkComponent {
	int entityIndex;      // Index into gentity_t or svEntity_t array
	int entityType;       // Entity type (ET_PLAYER, ET_ITEM, etc.)
	qboolean needsSync;   // Whether this entity needs network sync
	qboolean isServer;    // true for svEntity_t, false for gentity_t
	
	NetworkComponent() : entityIndex(-1), entityType(0), needsSync(qfalse), isServer(qfalse) {}
	NetworkComponent(int idx, int type, qboolean server) 
		: entityIndex(idx), entityType(type), needsSync(qtrue), isServer(server) {}
};

// Script Component - Attaches Lua scripts to entities
struct ScriptComponent {
	char script_path[MAX_QPATH];  // Path to Lua script file
	int script_ref;                // Lua reference to loaded script table (-1 = no ref)
	qboolean has_on_spawn;
	qboolean has_on_update;
	qboolean has_on_take_damage;
	qboolean has_on_use;
	qboolean has_on_death;
	
	ScriptComponent() : script_ref(-1), has_on_spawn(qfalse),
		has_on_update(qfalse), has_on_take_damage(qfalse),
		has_on_use(qfalse), has_on_death(qfalse) {
		script_path[0] = '\0';
	}
};

// Lifetime Component - auto-destroys entities after a duration
struct LifetimeComponent {
	float remaining;          // Seconds until expiration
	qboolean destroyOnExpire; // Whether to delete the entity when time elapses

	LifetimeComponent() : remaining(0.0f), destroyOnExpire(qtrue) {}
	LifetimeComponent(float seconds, qboolean destroy = qtrue)
		: remaining(seconds), destroyOnExpire(destroy) {}
};

#endif // USE_ENTT

#endif // __ECS_COMPONENTS_H__

