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

// C++23 type safety enhancements
#include <optional>
#include <expected>
#include <string_view>
#include <span>

// Strong typedef for entity IDs to prevent accidental mixing
using EntityID = entt::entity;

// Result type for operations that might fail
template<typename T, typename E>
using Result = std::expected<T, E>;

// Optional values
template<typename T>
using Optional = std::optional<T>;

// String view for read-only string operations
using StringView = std::string_view;

// Span for contiguous memory access
template<typename T>
using Span = std::span<T>;

// Constexpr string validation
consteval bool is_valid_component_name(const char* name) {
    return name != nullptr && name[0] != '\0';
}

#ifdef __cplusplus
#ifdef USE_BULLET
#include <LinearMath/btVector3.h>
#include <LinearMath/btQuaternion.h>
#include <LinearMath/btTransform.h>
#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <BulletCollision/CollisionShapes/btCollisionShape.h>
#include <BulletDynamics/Dynamics/btDiscreteDynamicsWorld.h>
#include <BulletDynamics/Dynamics/btDynamicsWorld.h>
#include <LinearMath/btMotionState.h>

// Collision shape types for Bullet physics
enum class CollisionShapeType : uint8_t {
	NONE = 0,
	BOX,           // Rectangular box
	SPHERE,        // Spherical shape
	CAPSULE,       // Capsule (cylinder with hemispherical ends)
	CONVEX_HULL,   // Convex hull from vertices
	MESH,          // Triangle mesh (static only)
	COMPOUND       // Compound shape (multiple shapes)
};

// Type-safe collision shape operations
class CollisionShapeTypeOps {
public:
	// Check if shape type is valid
	static constexpr bool is_valid(CollisionShapeType type) noexcept {
		return type >= CollisionShapeType::NONE && type <= CollisionShapeType::COMPOUND;
	}

	// Check if shape requires dimensions
	static constexpr bool requires_dimensions(CollisionShapeType type) noexcept {
		return type != CollisionShapeType::NONE;
	}

	// Check if shape is static-only
	static constexpr bool is_static_only(CollisionShapeType type) noexcept {
		return type == CollisionShapeType::MESH;
	}

	// Get string representation
	static constexpr StringView to_string(CollisionShapeType type) noexcept {
		switch (type) {
			case CollisionShapeType::NONE: return "NONE";
			case CollisionShapeType::BOX: return "BOX";
			case CollisionShapeType::SPHERE: return "SPHERE";
			case CollisionShapeType::CAPSULE: return "CAPSULE";
			case CollisionShapeType::CONVEX_HULL: return "CONVEX_HULL";
			case CollisionShapeType::MESH: return "MESH";
			case CollisionShapeType::COMPOUND: return "COMPOUND";
			default: return "UNKNOWN";
		}
	}
};
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

	// Collision shape configuration
	CollisionShapeType shapeType;
	vec3_t shapeDimensions;  // For box: half-extents, for sphere: radius, for capsule: radius/height
	btCollisionShape *collisionShape;  // Owned by the component

	// Motion state for synchronization with TransformComponent
        btMotionState *motionState;
#endif

	// Type-safe validation methods
	[[nodiscard]] constexpr bool is_valid() const noexcept {
		return mass > 0.0f && friction >= 0.0f && friction <= 1.0f;
	}

	[[nodiscard]] constexpr bool is_static() const noexcept {
		return mass >= std::numeric_limits<float>::max() / 2.0f; // Very large mass = static
	}

	[[nodiscard]] constexpr bool is_kinematic() const noexcept {
		return !is_static() && (velocity[0] != 0.0f || velocity[1] != 0.0f || velocity[2] != 0.0f);
	}

#ifdef USE_BULLET
	// Bullet-specific validation
	[[nodiscard]] constexpr bool has_valid_bullet_shape() const noexcept {
		return CollisionShapeTypeOps::is_valid(shapeType) &&
			   (!CollisionShapeTypeOps::requires_dimensions(shapeType) ||
				(shapeDimensions[0] > 0.0f && shapeDimensions[1] > 0.0f && shapeDimensions[2] > 0.0f));
	}

	[[nodiscard]] constexpr bool can_use_bullet() const noexcept {
		return is_valid() && has_valid_bullet_shape() &&
			   !CollisionShapeTypeOps::is_static_only(shapeType);
	}
#endif

	PhysicsComponent() : mass(1.0f), friction(0.0f)
#ifdef USE_BULLET
		, useBullet(qfalse), body(nullptr), shapeType(CollisionShapeType::BOX), collisionShape(nullptr), motionState(nullptr)
#endif
	{
		VectorClear(velocity);
		VectorClear(acceleration);
#ifdef USE_BULLET
		VectorSet(shapeDimensions, 0.5f, 0.5f, 0.5f);  // Default box half-extents
#endif
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

// Collision callback types
#ifdef USE_BULLET
using CollisionCallback = void(*)(ecs_entity_t entityA, ecs_entity_t entityB,
                                 const btVector3 &contactPoint,
                                 const btVector3 &normal, float impulse);

// Collision event structure for game code
struct CollisionEvent {
	ecs_entity_t entityA;
	ecs_entity_t entityB;
	vec3_t contactPoint;
	vec3_t normal;
	float impulse;
	int timestamp;
};
#endif

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

