/*
===========================================================================
ECS Core Systems

Core system implementations for physics, health, and network sync.
===========================================================================
*/

#ifdef USE_ENTT

#include "ecs.h"
#include "ecs_components.h"
#include "ecs_internal.h"
#include <entt/entt.hpp>

#ifdef USE_BULLET
#include <btBulletDynamicsCommon.h>

// Minimal Cvar accessors used to control Bullet stepping from server cvars.
extern "C" float Cvar_VariableValue( const char *var_name );

// Simple Bullet world wrapper used by the ECS physics system. This keeps
// Bullet usage contained to ECS-driven entities.
namespace {
	struct BulletWorld {
		btBroadphaseInterface					*broadphase		= nullptr;
		btDefaultCollisionConfiguration			*config			= nullptr;
		btCollisionDispatcher					*dispatcher		= nullptr;
		btSequentialImpulseConstraintSolver		*solver			= nullptr;
		btDiscreteDynamicsWorld					*world			= nullptr;
		bool									 initialized		= false;
		
		void Init() {
			if (initialized) {
				return;
			}
			
			broadphase = new btDbvtBroadphase();
			config = new btDefaultCollisionConfiguration();
			dispatcher = new btCollisionDispatcher(config);
			solver = new btSequentialImpulseConstraintSolver();
			world = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, config);
			
			// Basic gravity; game code can still apply its own forces via acceleration
			world->setGravity(btVector3(0.0f, 0.0f, -9.81f));
			
			initialized = true;
		}
		
		~BulletWorld() {
			if (!initialized) {
				return;
			}
			
			// Clean up rigid bodies (shapes/motion states are owned by the caller).
			for (int i = world->getNumCollisionObjects() - 1; i >= 0; --i) {
				btCollisionObject *obj = world->getCollisionObjectArray()[i];
				btRigidBody *body = btRigidBody::upcast(obj);
				if (body && body->getMotionState()) {
					delete body->getMotionState();
				}
				world->removeCollisionObject(obj);
				delete obj;
			}
			
			delete world;
			delete solver;
			delete dispatcher;
			delete config;
			delete broadphase;
			
			world = nullptr;
			solver = nullptr;
			dispatcher = nullptr;
			config = nullptr;
			broadphase = nullptr;
			initialized = false;
		}
	};
	
	static BulletWorld s_bulletWorld;
}

/*
================
ECS_Bullet_Step
Advance Bullet physics for entities that opt into Bullet simulation.
================
*/
static void ECS_Bullet_Step(entt::registry &registry, float deltaTime) {
	if (deltaTime <= 0.0f) {
		return;
	}

	// Global enable switch for Bullet ECS physics
	if (Cvar_VariableValue("sv_bulletEnable") <= 0.0f) {
		return;
	}
	
	s_bulletWorld.Init();
	
	auto view = registry.view<TransformComponent, PhysicsComponent>();
	
	// Ensure Bullet bodies exist and push per-frame forces
	for (auto entity : view) {
		auto &transform = view.get<TransformComponent>(entity);
		auto &physics = view.get<PhysicsComponent>(entity);
		
		if (!physics.useBullet) {
			continue;
		}
		
		// Lazily create a Bullet rigid body for this entity
		if (!physics.body) {
			// For now, use a simple box shape; future work can parameterize this
			btCollisionShape *shape = new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));
			
			const bool isDynamic = (physics.mass > 0.0f);
			btVector3 localInertia(0, 0, 0);
			if (isDynamic) {
				shape->calculateLocalInertia(physics.mass, localInertia);
			}
			
			btTransform startTransform;
			startTransform.setIdentity();
			startTransform.setOrigin(btVector3(transform.position[0], transform.position[1], transform.position[2]));
			
			btDefaultMotionState *motionState = new btDefaultMotionState(startTransform);
			
			btRigidBody::btRigidBodyConstructionInfo rbInfo(
				physics.mass > 0.0f ? physics.mass : 0.0f,
				motionState,
				shape,
				localInertia
			);
			
			btRigidBody *body = new btRigidBody(rbInfo);
			
			// Approximate friction using the existing friction parameter
			body->setFriction(physics.friction);
			
			s_bulletWorld.world->addRigidBody(body);
			physics.body = body;
		}
		
		// Apply acceleration as a force for this step
		if (physics.mass > 0.0f) {
			btVector3 force(
				physics.acceleration[0] * physics.mass,
				physics.acceleration[1] * physics.mass,
				physics.acceleration[2] * physics.mass
			);
			physics.body->applyCentralForce(force);
		}
	}
	
	// Step Bullet simulation
	int maxSubSteps = (int)Cvar_VariableValue("sv_bulletMaxSubSteps");
	if (maxSubSteps < 1) {
		maxSubSteps = 1;
	} else if (maxSubSteps > 16) {
		maxSubSteps = 16;
	}

	float fixedTimestep = Cvar_VariableValue("sv_bulletFixedTimestep");
	if (fixedTimestep <= 0.0f) {
		s_bulletWorld.world->stepSimulation(deltaTime, maxSubSteps);
	} else {
		s_bulletWorld.world->stepSimulation(deltaTime, maxSubSteps, fixedTimestep);
	}
	
	// Sync Bullet transforms back into ECS components
	for (auto entity : view) {
		auto &transform = view.get<TransformComponent>(entity);
		auto &physics = view.get<PhysicsComponent>(entity);
		
		if (!physics.useBullet || !physics.body) {
			continue;
		}
		
		btTransform worldTransform;
		physics.body->getMotionState()->getWorldTransform(worldTransform);
		const btVector3 &origin = worldTransform.getOrigin();
		transform.position[0] = origin.getX();
		transform.position[1] = origin.getY();
		transform.position[2] = origin.getZ();
		
		// Sync linear velocity back into the component for other systems
		const btVector3 &linVel = physics.body->getLinearVelocity();
		physics.velocity[0] = linVel.getX();
		physics.velocity[1] = linVel.getY();
		physics.velocity[2] = linVel.getZ();
		
		// Clear acceleration; it should be re-applied by gameplay each frame
		VectorClear(physics.acceleration);
	}
}
#endif // USE_BULLET

/*
================
PhysicsSystem
Update physics components (velocity, position) using a simple integrator.
Bullet-enabled entities will be overridden by ECS_Bullet_Step when
USE_BULLET is enabled.
================
*/
void ECS_PhysicsSystem_Update(float deltaTime) {
	entt::registry &registry = ECS::GetRegistry();
	
	auto view = registry.view<TransformComponent, PhysicsComponent>();
	
	for (auto entity : view) {
		auto &transform = view.get<TransformComponent>(entity);
		auto &physics = view.get<PhysicsComponent>(entity);
		
		// Simple integration for non-Bullet entities
		if (
#ifdef USE_BULLET
			physics.useBullet
#else
			false
#endif
		) {
			// Bullet-driven entities are handled separately
			continue;
		}
		
		// Update velocity from acceleration
		vec3_t deltaVel;
		VectorScale(physics.acceleration, deltaTime, deltaVel);
		VectorAdd(physics.velocity, deltaVel, physics.velocity);
		
		// Apply friction
		if (physics.friction > 0.0f) {
			float frictionFactor = 1.0f - (physics.friction * deltaTime);
			if (frictionFactor < 0.0f) frictionFactor = 0.0f;
			VectorScale(physics.velocity, frictionFactor, physics.velocity);
		}
		
		// Update position from velocity
		vec3_t deltaPos;
		VectorScale(physics.velocity, deltaTime, deltaPos);
		VectorAdd(transform.position, deltaPos, transform.position);
		
		// Clear acceleration (should be set each frame if needed)
		VectorClear(physics.acceleration);
	}
}

/*
================
HealthSystem
Handle health/damage logic
================
*/
void ECS_HealthSystem_Update(void) {
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return;
	entt::registry &registry = *registry_ptr;
	
	auto view = registry.view<HealthComponent>();
	
	for (auto entity : view) {
		auto &health = view.get<HealthComponent>(entity);
		
		// Clamp health values
		if (health.health > health.maxHealth) {
			health.health = health.maxHealth;
		}
		if (health.health < 0) {
			health.health = 0;
		}
		if (health.armor > health.maxArmor) {
			health.armor = health.maxArmor;
		}
		if (health.armor < 0) {
			health.armor = 0;
		}
	}
}

/*
================
NetworkSyncSystem
Sync ECS entities to engine entity structures for network
This is a placeholder - actual implementation will sync to gentity_t/svEntity_t
================
*/
void ECS_NetworkSyncSystem_Update(void) {
	entt::registry *registry_ptr = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
	if (!registry_ptr) return;
	entt::registry &registry = *registry_ptr;
	
	auto view = registry.view<NetworkComponent, TransformComponent>();
	
	for (auto entity : view) {
		auto &network = view.get<NetworkComponent>(entity);
		// TransformComponent is included in view but not used yet
		// It will be used when syncing to actual gentity_t/svEntity_t
		
		if (!network.needsSync || network.entityIndex < 0) {
			continue;
		}
		
		// This will be implemented to sync to actual gentity_t/svEntity_t
		// For now, just mark that sync is needed
		network.needsSync = qfalse;
	}
}

/*
================
ECS_RunFrame
Run all ECS systems for a frame
================
*/
void ECS_RunFrame(float deltaTime) {
	// Check if registry is initialized by checking if it exists
	extern ecs_registry_t *ECS_GetRegistry(void);
	if (ECS_GetRegistry() == nullptr) {
		return; // System not initialized
	}
	
	// Run systems in order
	ECS_PhysicsSystem_Update(deltaTime);
	// If Bullet is enabled, advance Bullet after the simple integrator so
	// Bullet-enabled entities override the basic integration while
	// non-Bullet entities continue to use the lightweight path.
#ifdef USE_BULLET
	{
		entt::registry &registry = ECS::GetRegistry();
		ECS_Bullet_Step(registry, deltaTime);
	}
#endif
	ECS_HealthSystem_Update();
	ECS_NetworkSyncSystem_Update();
}

#endif // USE_ENTT

