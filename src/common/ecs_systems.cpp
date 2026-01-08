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
#include "q_shared.h"
#include "qcommon.h"
#include <entt/entt.hpp>
#include <vector>
#include <ctime>

#ifdef USE_LUA
extern "C" {
	void Lua_Entity_Update(float deltaTime);
}
#endif

#ifdef USE_BULLET
#include <btBulletDynamicsCommon.h>

// Bullet physics uses hardcoded values for now

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

// Custom contact result callback for collision events
struct CollisionContactResultCallback : public btCollisionWorld::ContactResultCallback {
	std::vector<CollisionEvent> &events;
	entt::registry &registry;

	CollisionContactResultCallback(std::vector<CollisionEvent> &e, entt::registry &r)
		: events(e), registry(r) {}

	virtual btScalar addSingleResult(btManifoldPoint &cp,
									const btCollisionObjectWrapper *colObj0Wrap,
									int partId0, int index0,
									const btCollisionObjectWrapper *colObj1Wrap,
									int partId1, int index1) override {
		// Suppress unused parameter warnings - these are required by Bullet interface
		(void)partId0; (void)index0; (void)partId1; (void)index1;

		// Find ECS entities from Bullet collision objects
		ecs_entity_t entityA = FindEntityFromBulletBody(colObj0Wrap->getCollisionObject());
		ecs_entity_t entityB = FindEntityFromBulletBody(colObj1Wrap->getCollisionObject());

		if (entityA != entt::null && entityB != entt::null) {
			CollisionEvent event;
			event.entityA = entityA;
			event.entityB = entityB;
			event.contactPoint[0] = cp.getPositionWorldOnA().x();
			event.contactPoint[1] = cp.getPositionWorldOnA().y();
			event.contactPoint[2] = cp.getPositionWorldOnA().z();
			event.normal[0] = cp.m_normalWorldOnB.x();
			event.normal[1] = cp.m_normalWorldOnB.y();
			event.normal[2] = cp.m_normalWorldOnB.z();
			event.impulse = cp.getAppliedImpulse();
                        event.timestamp = (int)time(NULL); // Use current time as timestamp

			events.push_back(event);
		}

		return 0.0f;
	}

private:
	ecs_entity_t FindEntityFromBulletBody(const btCollisionObject *body) {
		// Search through physics components to find matching body
		auto view = registry.view<PhysicsComponent>();
		for (auto entity : view) {
			auto &physics = view.get<PhysicsComponent>(entity);
			if (physics.body == body) {
				return static_cast<ecs_entity_t>(entity);
			}
		}
		return static_cast<ecs_entity_t>(entt::null);
	}
};

// Custom motion state to synchronize Bullet transforms with ECS TransformComponent
class TransformMotionState : public btMotionState {
private:
	TransformComponent *transform;

public:
	TransformMotionState(TransformComponent *t) : transform(t) {}

	virtual void getWorldTransform(btTransform &worldTrans) const override {
		// Convert ECS transform to Bullet transform
		btVector3 pos(transform->position[0], transform->position[1], transform->position[2]);

		// Convert Euler angles to quaternion (simplified - assumes ZYX order)
		btQuaternion rot;
		rot.setEulerZYX(transform->rotation[2] * M_PI / 180.0f,
					   transform->rotation[1] * M_PI / 180.0f,
					   transform->rotation[0] * M_PI / 180.0f);

		worldTrans.setOrigin(pos);
		worldTrans.setRotation(rot);
	}

	virtual void setWorldTransform(const btTransform &worldTrans) override {
		// Convert Bullet transform back to ECS transform
		const btVector3 &pos = worldTrans.getOrigin();
		transform->position[0] = pos.x();
		transform->position[1] = pos.y();
		transform->position[2] = pos.z();

		// Convert quaternion back to Euler angles (simplified)
		btQuaternion rot = worldTrans.getRotation();
		btScalar yaw, pitch, roll;
		rot.getEulerZYX(yaw, pitch, roll);
		transform->rotation[0] = yaw * 180.0f / M_PI;
		transform->rotation[1] = pitch * 180.0f / M_PI;
		transform->rotation[2] = roll * 180.0f / M_PI;
	}
};

	static BulletWorld s_bulletWorld;

// Collision shape factory functions
static btCollisionShape* CreateCollisionShape(CollisionShapeType type, const vec3_t dimensions) {
	switch (type) {
		case CollisionShapeType::BOX:
			return new btBoxShape(btVector3(dimensions[0], dimensions[1], dimensions[2]));

		case CollisionShapeType::SPHERE:
			return new btSphereShape(dimensions[0]);  // radius

		case CollisionShapeType::CAPSULE:
			return new btCapsuleShape(dimensions[0], dimensions[1]);  // radius, height

		case CollisionShapeType::CONVEX_HULL:
			// Placeholder - would need vertex data
			return new btBoxShape(btVector3(dimensions[0], dimensions[1], dimensions[2]));

		case CollisionShapeType::MESH:
			// Static triangle mesh - placeholder
			return new btBoxShape(btVector3(dimensions[0], dimensions[1], dimensions[2]));

		case CollisionShapeType::COMPOUND:
			// Compound shape - placeholder
			return new btCompoundShape();

		default:
			return new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));  // fallback
	}
}
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
	if (false) { // Bullet physics disabled by default
		return;
	}
	
	s_bulletWorld.Init();

	auto view = registry.view<TransformComponent, PhysicsComponent>();

	// Collect collision events
	std::vector<CollisionEvent> collisionEvents;
	
	// Ensure Bullet bodies exist and push per-frame forces
	for (auto entity : view) {
		auto &transform = view.get<TransformComponent>(entity);
		auto &physics = view.get<PhysicsComponent>(entity);
		
		if (!physics.useBullet) {
			continue;
		}
		
		// Lazily create a Bullet rigid body for this entity
		if (!physics.body) {
			// Create collision shape based on configured type
			if (!physics.collisionShape) {
				physics.collisionShape = CreateCollisionShape(physics.shapeType, physics.shapeDimensions);
			}

			const bool isDynamic = (physics.mass > 0.0f);
			btVector3 localInertia(0, 0, 0);
			if (isDynamic) {
				physics.collisionShape->calculateLocalInertia(physics.mass, localInertia);
			}

			// Create custom motion state for TransformComponent sync
			if (!physics.motionState) {
				physics.motionState = static_cast<btMotionState*>(new TransformMotionState(&transform));
			}

			btRigidBody::btRigidBodyConstructionInfo rbInfo(
				physics.mass > 0.0f ? physics.mass : 0.0f,
				static_cast<btMotionState*>(physics.motionState),
				physics.collisionShape,
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
	int maxSubSteps = 10; // Default max sub-steps
	float fixedTimestep = 1.0f / 60.0f; // Default 60 FPS fixed timestep

	s_bulletWorld.world->stepSimulation(deltaTime, maxSubSteps, fixedTimestep);

	// Perform collision detection and generate events
	CollisionContactResultCallback collisionCallback(collisionEvents, registry);
	s_bulletWorld.world->performDiscreteCollisionDetection();

	// Check for collisions using contact manifolds
	int numManifolds = s_bulletWorld.world->getDispatcher()->getNumManifolds();
	for (int i = 0; i < numManifolds; i++) {
		btPersistentManifold *contactManifold =
			s_bulletWorld.world->getDispatcher()->getManifoldByIndexInternal(i);

		int numContacts = contactManifold->getNumContacts();
		if (numContacts > 0) {
			const btCollisionObject *objA = contactManifold->getBody0();
			const btCollisionObject *objB = contactManifold->getBody1();

			// Create collision wrappers for the callback
			btCollisionObjectWrapper objAWrap(0, objA->getCollisionShape(), objA, btTransform::getIdentity(), -1, -1);
			btCollisionObjectWrapper objBWrap(0, objB->getCollisionShape(), objB, btTransform::getIdentity(), -1, -1);

			for (int j = 0; j < numContacts; j++) {
				btManifoldPoint &pt = contactManifold->getContactPoint(j);
				if (pt.getDistance() < 0.f) {
					collisionCallback.addSingleResult(pt, &objAWrap, 0, 0, &objBWrap, 0, 0);
				}
			}
		}
	}

	// Process collision events (call user callbacks if registered)
	for (const auto &event : collisionEvents) {
		// TODO: Call user-registered collision callbacks
		// For now, just log the collision
		if (false) { // Debug drawing disabled by default
			Com_Printf("Bullet collision: entity %d <-> entity %d, impulse %.2f\n",
					  (int)event.entityA, (int)event.entityB, event.impulse);
		}
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

		if (auto net = registry.try_get<NetworkComponent>(entity)) {
			net->needsSync = qtrue;
		}
	}
}

// Called by ECS_DestroyEntity to tear down Bullet state before entity destruction.
void ECS::BulletOnEntityDestroyed(entt::registry &registry, entt::entity entity, PhysicsComponent &physics) {
	if (!physics.useBullet || !physics.body || !s_bulletWorld.initialized || s_bulletWorld.world == nullptr) {
		return;
	}

	btRigidBody *body = physics.body;
	physics.body = nullptr;

	if (body->getMotionState()) {
		delete body->getMotionState();
	}
	btCollisionShape *shape = body->getCollisionShape();
	if (shape) {
		s_bulletWorld.world->removeCollisionObject(body);
		delete shape;
	} else {
		s_bulletWorld.world->removeRigidBody(body);
	}
	delete body;

	// Ensure the registry still owns the entity before removing components.
	if (registry.valid(entity) && registry.all_of<PhysicsComponent>(entity)) {
		physics.useBullet = qfalse;
	}
}
#endif // USE_BULLET

/*
================
ECS_LifetimeSystem_Update
Expire entities that have a LifetimeComponent
================
*/
static void ECS_LifetimeSystem_Update(float deltaTime) {
	if (deltaTime <= 0.0f) {
		return;
	}

	entt::registry &registry = ECS::GetRegistry();
	auto view = registry.view<LifetimeComponent>();

	std::vector<entt::entity> toDestroy;
	toDestroy.reserve(view.size());

	for (auto entity : view) {
		auto &life = view.get<LifetimeComponent>(entity);
		life.remaining -= deltaTime;
		if (life.remaining <= 0.0f && life.destroyOnExpire) {
			toDestroy.push_back(entity);
		}
	}

	for (auto entity : toDestroy) {
		ECS_DestroyEntity(static_cast<ecs_entity_t>(entity));
	}
}

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

		if (auto net = registry.try_get<NetworkComponent>(entity)) {
			net->needsSync = qtrue;
		}
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
		const int prevHealth = health.health;
		const int prevArmor = health.armor;
		
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

		if ((health.health != prevHealth || health.armor != prevArmor)) {
			if (auto net = registry.try_get<NetworkComponent>(entity)) {
				net->needsSync = qtrue;
			}
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
	// Placeholder intentionally left as a no-op. Actual syncing is handled
	// in the game/server bridge layers (G_ECS_SyncToGentity, SV_ECS_SyncToSvEntity).
}

/*
================
ECS_ScriptSystem_Update
Update entity scripts (call OnUpdate hooks)
================
*/
void ECS_ScriptSystem_Update(float deltaTime) {
#ifdef USE_LUA
	Lua_Entity_Update(deltaTime);
#endif
	(void)deltaTime;  // Suppress unused warning if USE_LUA is off
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
	ECS_LifetimeSystem_Update(deltaTime);
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
	ECS_ScriptSystem_Update(deltaTime);
	ECS_NetworkSyncSystem_Update();
}

#endif // USE_ENTT

