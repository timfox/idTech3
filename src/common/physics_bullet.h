#ifndef __PHYSICS_BULLET_H__
#define __PHYSICS_BULLET_H__

#ifdef USE_BULLET

#include "q_shared.h"

#ifdef __cplusplus
#include <expected>
#include <optional>
#include <string_view>

// C++23 physics result type for better error handling
enum class PhysicsError {
	OK = 0,
	GENERIC_ERROR,
	INVALID_ENTITY,
	NO_BULLET_SUPPORT,
	NOT_INITIALIZED,
	SHAPE_CREATION_FAILED,
	BODY_CREATION_FAILED
};

template<typename T>
using PhysicsResult = std::expected<T, PhysicsError>;

// Optional physics values
template<typename T>
using PhysicsOptional = std::optional<T>;

// Physics string views for read-only operations
using PhysicsString = std::string_view;
#endif

// Forward declarations
typedef struct gentity_s gentity_t;
typedef struct svEntity_s svEntity_t;

#ifdef USE_ENTT
typedef unsigned int ecs_entity_t;
#else
// Stub definition when ECS is not available
typedef unsigned int ecs_entity_t;
#endif

// Physics API return codes
typedef enum {
	PHYSICS_OK = 0,
	PHYSICS_ERROR,
	PHYSICS_INVALID_ENTITY,
	PHYSICS_NO_BULLET,
	PHYSICS_NOT_INITIALIZED
} physicsResult_t;

// Collision shape types (matches ECS version)
typedef enum {
	PHYSICS_SHAPE_NONE = 0,
	PHYSICS_SHAPE_BOX,
	PHYSICS_SHAPE_SPHERE,
	PHYSICS_SHAPE_CAPSULE,
	PHYSICS_SHAPE_CONVEX_HULL,
	PHYSICS_SHAPE_MESH,
	PHYSICS_SHAPE_COMPOUND
} physicsShapeType_t;

// Rigid body creation parameters
typedef struct {
	physicsShapeType_t shapeType;
	vec3_t shapeDimensions;  // Size parameters for the shape
	vec3_t position;         // Initial position
	vec3_t rotation;         // Initial rotation (Euler angles)
	float mass;             // 0 = static, >0 = dynamic
	float friction;         // Friction coefficient
	float restitution;      // Bounciness (0-1)
	qboolean kinematic;     // Kinematic body (moved by code, not physics)
} physicsBodyParams_t;

// Force/impulse application
typedef struct {
	vec3_t force;           // Force vector
	vec3_t position;        // Point of application (for torque)
	qboolean isImpulse;     // Apply as impulse instead of continuous force
} physicsForce_t;

// Physics query results
typedef struct {
	vec3_t position;
	vec3_t rotation;        // Euler angles
	vec3_t linearVelocity;
	vec3_t angularVelocity;
	qboolean isActive;      // Body is active in simulation
} physicsBodyState_t;

// Collision callback function type
typedef void (*physicsCollisionCallback_t)(void *userData, int entityA, int entityB,
                                          const vec3_t contactPoint, const vec3_t normal, float impulse);

// ============================================================================
// UNIFIED PHYSICS API
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Initialization and Shutdown
// ============================================================================

/*
================
Physics_Init
Initialize the physics system
================
*/
physicsResult_t Physics_Init(void);

/*
================
Physics_Shutdown
Shutdown the physics system
================
*/
void Physics_Shutdown(void);

/*
================
Physics_IsInitialized
Check if physics system is initialized
================
*/
qboolean Physics_IsInitialized(void);

// ============================================================================
// Rigid Body Management
// ============================================================================

/*
================
Physics_CreateBody
Create a rigid body with specified parameters
Returns a handle to the created body, or 0 on failure
================
*/
int Physics_CreateBody(const physicsBodyParams_t *params);

/*
================
Physics_DestroyBody
Destroy a rigid body
================
*/
physicsResult_t Physics_DestroyBody(int bodyHandle);

/*
================
Physics_GetBodyState
Get the current state of a rigid body
================
*/
physicsResult_t Physics_GetBodyState(int bodyHandle, physicsBodyState_t *state);

/*
================
Physics_SetBodyState
Set the state of a rigid body
================
*/
physicsResult_t Physics_SetBodyState(int bodyHandle, const physicsBodyState_t *state);

// ============================================================================
// Force and Impulse Application
// ============================================================================

/*
================
Physics_ApplyForce
Apply a force or impulse to a rigid body
================
*/
physicsResult_t Physics_ApplyForce(int bodyHandle, const physicsForce_t *force);

/*
================
Physics_ApplyForceAtPosition
Apply a force at a specific position on a rigid body
================
*/
physicsResult_t Physics_ApplyForceAtPosition(int bodyHandle, const vec3_t force, const vec3_t position);

/*
================
Physics_ApplyTorque
Apply torque to a rigid body
================
*/
physicsResult_t Physics_ApplyTorque(int bodyHandle, const vec3_t torque);

// ============================================================================
// Collision and Queries
// ============================================================================

/*
================
Physics_Raycast
Perform a raycast against the physics world
Returns qtrue if hit, fills in hitInfo
================
*/
qboolean Physics_Raycast(const vec3_t start, const vec3_t end, vec3_t hitPoint,
                        vec3_t hitNormal, int *hitBody);

/*
================
Physics_OverlapSphere
Check for overlaps with a sphere
================
*/
int Physics_OverlapSphere(const vec3_t center, float radius, int *overlappingBodies, int maxResults);

/*
================
Physics_SetCollisionCallback
Set a callback for collision events
================
*/
physicsResult_t Physics_SetCollisionCallback(physicsCollisionCallback_t callback, void *userData);

// ============================================================================
// Entity Integration (Legacy Support)
// ============================================================================

/*
================
Physics_EnableEntityPhysics
Enable physics for a legacy game entity
================
*/
physicsResult_t Physics_EnableEntityPhysics(gentity_t *ent, const physicsBodyParams_t *params);

/*
================
Physics_DisableEntityPhysics
Disable physics for a legacy game entity
================
*/
physicsResult_t Physics_DisableEntityPhysics(gentity_t *ent);

// ============================================================================
// ECS Integration
// ============================================================================

/*
================
Physics_ECS_EnableEntity
Enable physics for an ECS entity
================
*/
physicsResult_t Physics_ECS_EnableEntity(ecs_entity_t entity, const physicsBodyParams_t *params);

/*
================
Physics_ECS_DisableEntity
Disable physics for an ECS entity
================
*/
physicsResult_t Physics_ECS_DisableEntity(ecs_entity_t entity);

// ============================================================================
// World Management
// ============================================================================

/*
================
Physics_StepSimulation
Step the physics simulation forward in time
================
*/
physicsResult_t Physics_StepSimulation(float deltaTime);

/*
================
Physics_SetGravity
Set the global gravity vector
================
*/
physicsResult_t Physics_SetGravity(const vec3_t gravity);

/*
================
Physics_GetGravity
Get the current global gravity vector
================
*/
physicsResult_t Physics_GetGravity(vec3_t gravity);

// ============================================================================
// Debug and Performance
// ============================================================================

/*
================
Physics_DebugDraw
Enable/disable debug drawing of physics shapes
================
*/
physicsResult_t Physics_DebugDraw(qboolean enable);

/*
================
Physics_GetStats
Get physics performance statistics
================
*/
physicsResult_t Physics_GetStats(int *numBodies, int *numConstraints, float *stepTime);

// ============================================================================
// C++23 Enhanced API - Type-safe wrappers with better error handling
// ============================================================================

#ifdef __cplusplus

namespace Physics {

// Type-safe physics operations using std::expected
class API {
public:
    // Initialize physics system
    static PhysicsResult<bool> Initialize() noexcept {
        auto result = Physics_Init();
        if (result == PHYSICS_OK) {
            return true;
        }
        return std::unexpected(static_cast<PhysicsError>(result));
    }

    // Check if initialized
    static PhysicsOptional<bool> IsInitialized() noexcept {
        if (Physics_IsInitialized()) {
            return true;
        }
        return std::nullopt;
    }

    // Get physics statistics
    struct Stats {
        int bodies;
        int constraints;
        float stepTime;
    };

    static PhysicsResult<Stats> GetStatistics() noexcept {
        Stats stats{};
        auto result = Physics_GetStats(&stats.bodies, &stats.constraints, &stats.stepTime);
        if (result == PHYSICS_OK) {
            return stats;
        }
        return std::unexpected(static_cast<PhysicsError>(result));
    }

    // Enable/disable debug drawing
    static PhysicsResult<bool> SetDebugDrawing(bool enable) noexcept {
        auto result = Physics_DebugDraw(enable ? qtrue : qfalse);
        if (result == PHYSICS_OK) {
            return enable;
        }
        return std::unexpected(static_cast<PhysicsError>(result));
    }
};

} // namespace Physics

#endif // __cplusplus

#ifdef __cplusplus
}
#endif

#endif // USE_BULLET

#endif // __PHYSICS_BULLET_H__