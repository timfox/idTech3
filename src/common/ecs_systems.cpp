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
#include "../game/g_local.h"
#include <entt/entt.hpp>
#include <vector>
#include <ctime>
#include <cstring>

#ifdef USE_LUA
extern "C" {
	void Lua_Entity_Update(float deltaTime);
}
#endif

// Forward declarations for internal functions
#ifdef USE_BULLET
static void ECS_IntegrateWithCMSystem(entt::registry &registry);
#endif
static void ECS_DebugDrawPhysics(entt::registry &registry);
#ifdef USE_BULLET
static void ECS_ProcessCollisionEvent(entt::registry &registry, const CollisionEvent &event);
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
                        event.timestamp = static_cast<uint64_t>(time(NULL)); // Use current time as timestamp

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
                rot.setEulerZYX(transform->rotation[2] * static_cast<float>(M_PI) / 180.0f,
                                transform->rotation[1] * static_cast<float>(M_PI) / 180.0f,
                                transform->rotation[0] * static_cast<float>(M_PI) / 180.0f);

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
                transform->rotation[0] = yaw * 180.0f / static_cast<float>(M_PI);
                transform->rotation[1] = pitch * 180.0f / static_cast<float>(M_PI);
                transform->rotation[2] = roll * 180.0f / static_cast<float>(M_PI);
	}
};

	static BulletWorld s_bulletWorld;

// Collision shape factory functions
static btCollisionShape* CreateCollisionShape(CollisionShapeType type, const vec3_t dimensions, const std::vector<btVector3>* vertices = nullptr, const std::vector<int>* indices = nullptr) {
	switch (type) {
		case CollisionShapeType::BOX:
			return new btBoxShape(btVector3(dimensions[0], dimensions[1], dimensions[2]));

		case CollisionShapeType::SPHERE:
			return new btSphereShape(dimensions[0]);  // radius

		case CollisionShapeType::CAPSULE:
			return new btCapsuleShape(dimensions[0], dimensions[1]);  // radius, height

		case CollisionShapeType::CONVEX_HULL: {
			if (vertices && !vertices->empty()) {
				auto* convexHull = new btConvexHullShape();
				for (const auto& vertex : *vertices) {
					convexHull->addPoint(vertex);
				}
				return convexHull;
			} else {
				// Fallback to box if no vertices provided
				return new btBoxShape(btVector3(dimensions[0], dimensions[1], dimensions[2]));
			}
		}

		case CollisionShapeType::MESH: {
			if (vertices && indices && !vertices->empty() && !indices->empty()) {
				auto* triangleMesh = new btTriangleMesh();
				for (size_t i = 0; i < indices->size(); i += 3) {
					if (i + 2 < indices->size()) {
						triangleMesh->addTriangle(
							(*vertices)[(*indices)[i]],
							(*vertices)[(*indices)[i + 1]],
							(*vertices)[(*indices)[i + 2]]
						);
					}
				}
				return new btBvhTriangleMeshShape(triangleMesh, true);
			} else {
				// Fallback to box if no mesh data provided
				return new btBoxShape(btVector3(dimensions[0], dimensions[1], dimensions[2]));
			}
		}

		case CollisionShapeType::COMPOUND:
			return new btCompoundShape();

		default:
			return new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));  // fallback
	}
}

// Create collision shape from model data
btCollisionShape* CreateCollisionShapeFromModel(const char* modelName, CollisionShapeType preferredType) {
	// Suppress unused parameter warnings
	(void)modelName;
	(void)preferredType;

	// This would load model geometry and create appropriate collision shape
	// For now, return a default box shape
	return new btBoxShape(btVector3(1.0f, 1.0f, 1.0f));
}
}

#ifdef USE_BULLET
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
	cvar_t *physics_bullet_enabled = Cvar_Get("physics_bullet_enabled", "1", CVAR_ARCHIVE | CVAR_LATCH);
	Cvar_SetDescription(physics_bullet_enabled, "Enable Bullet physics simulation for ECS entities");

	if (!physics_bullet_enabled->integer) {
		return;
	}
	
	s_bulletWorld.Init();

	// Integrate with CM collision system for world geometry
	ECS_IntegrateWithCMSystem(registry);

	auto view = registry.view<TransformComponent, PhysicsComponent>();
	auto constraintView = registry.view<ConstraintComponent>();

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

			// Set material properties
			body->setFriction(physics.friction);
			body->setRestitution(physics.restitution);
			body->setDamping(physics.linearDamping, physics.angularDamping);

			// Set additional Bullet-specific properties
			body->setSleepingThresholds(0.1f, 0.1f); // Prevent immediate sleeping
			body->setActivationState(DISABLE_DEACTIVATION); // Keep bodies active

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

	// Create and update constraints
	for (auto entity : constraintView) {
		auto &constraintComp = constraintView.get<ConstraintComponent>(entity);

		if (constraintComp.targetEntityId < 0 || !constraintComp.constraint) {
			// Create new constraint
			auto bodyA = registry.try_get<PhysicsComponent>(entity);
			auto bodyB = registry.try_get<PhysicsComponent>(static_cast<entt::entity>(constraintComp.targetEntityId));

			if (bodyA && bodyB && bodyA->body && bodyB->body) {
				btTypedConstraint *newConstraint = nullptr;

				switch (constraintComp.type) {
					case ConstraintType::POINT_TO_POINT: {
						btVector3 pivotA(constraintComp.pivotA[0], constraintComp.pivotA[1], constraintComp.pivotA[2]);
						btVector3 pivotB(constraintComp.pivotB[0], constraintComp.pivotB[1], constraintComp.pivotB[2]);
						newConstraint = new btPoint2PointConstraint(*bodyA->body, *bodyB->body, pivotA, pivotB);
						break;
					}

					case ConstraintType::HINGE: {
						btVector3 pivotA(constraintComp.pivotA[0], constraintComp.pivotA[1], constraintComp.pivotA[2]);
						btVector3 axisA(constraintComp.axisA[0], constraintComp.axisA[1], constraintComp.axisA[2]);
						btVector3 pivotB(constraintComp.pivotB[0], constraintComp.pivotB[1], constraintComp.pivotB[2]);
						btVector3 axisB(constraintComp.axisB[0], constraintComp.axisB[1], constraintComp.axisB[2]);
						newConstraint = new btHingeConstraint(*bodyA->body, *bodyB->body, pivotA, pivotB, axisA, axisB);
						break;
					}

					case ConstraintType::SLIDER: {
						btTransform frameA, frameB;
						frameA.setIdentity();
						frameB.setIdentity();
						newConstraint = new btSliderConstraint(*bodyA->body, *bodyB->body, frameA, frameB, true);
						break;
					}

					case ConstraintType::FIXED: {
						btTransform frameA, frameB;
						frameA.setIdentity();
						frameB.setIdentity();
						newConstraint = new btFixedConstraint(*bodyA->body, *bodyB->body, frameA, frameB);
						break;
					}

					default:
						break;
				}

				if (newConstraint) {
					s_bulletWorld.world->addConstraint(newConstraint, true); // disable collision between linked bodies
					constraintComp.constraint = newConstraint;
				}
			}
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
		// Call user-registered collision callbacks
		ECS_ProcessCollisionEvent(registry, event);

		// Log collision for debugging
		cvar_t *physics_debug_collision = Cvar_Get("physics_debug_collision", "0", CVAR_CHEAT);
		if (physics_debug_collision->integer) {
			Com_Printf("Bullet collision: entity %d <-> entity %d at (%.2f,%.2f,%.2f), impulse %.2f\n",
                                           static_cast<int>(event.entityA), static_cast<int>(event.entityB),
					  event.contactPoint[0], event.contactPoint[1], event.contactPoint[2],
					  event.impulse);
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
#else // !USE_BULLET
// Stub implementation when Bullet is not available
static void ECS_Bullet_Step(entt::registry &registry, float deltaTime) {
	// No-op when Bullet physics is not available
}
#endif // USE_BULLET

// Collision callback system
using CollisionCallbackFunc = void(*)(entt::registry &registry, const CollisionEvent &event, void *userData);
static std::vector<std::tuple<CollisionCallbackFunc, void*>> collisionCallbacks;

// Physics debugging
static void ECS_DebugDrawPhysics(entt::registry &registry) {
	cvar_t *physics_debug_draw = Cvar_Get("physics_debug_draw", "0", CVAR_CHEAT);
	if (!physics_debug_draw->integer) {
		return;
	}

	auto view = registry.view<TransformComponent, PhysicsComponent>();
	for (auto entity : view) {
		auto &transform = view.get<TransformComponent>(entity);
		auto &physics = view.get<PhysicsComponent>(entity);

		// Draw velocity vector
		if (VectorLength(physics.velocity) > 0.1f) {
			Com_DPrintf("Physics entity %d: pos(%.2f,%.2f,%.2f) vel(%.2f,%.2f,%.2f)\n",
                                            static_cast<int>(entity),
					   transform.position[0], transform.position[1], transform.position[2],
					   physics.velocity[0], physics.velocity[1], physics.velocity[2]);
		}
	}
}

#ifdef USE_BULLET
// CM system integration - Add world geometry collision to Bullet world
static void ECS_IntegrateWithCMSystem(entt::registry &registry) {
	// Check if CM Bullet system is initialized
	extern qboolean CM_Bullet_IsInitialized(void);
	extern btDiscreteDynamicsWorld* CM_Bullet_GetWorld(void);

	if (!CM_Bullet_IsInitialized()) {
		return;
	}

	btDiscreteDynamicsWorld *cmWorld = CM_Bullet_GetWorld();
	if (!cmWorld) {
		return;
	}

	cvar_t *physics_world_collision = Cvar_Get("physics_world_collision", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(physics_world_collision, "Enable collision between ECS entities and world geometry");

	if (!physics_world_collision->integer) {
		return;
	}

	// Add ECS rigid bodies to CM world for world collision
	auto view = registry.view<PhysicsComponent>();
	for (auto entity : view) {
		auto &physics = view.get<PhysicsComponent>(entity);

		if (physics.useBullet && physics.body && physics.body->getWorldArrayIndex() < 0) {
			// Body not in any world, add it to CM world for world collision
			cmWorld->addRigidBody(physics.body);
			Com_DPrintf("Added ECS entity to CM collision world\n");
		}
	}
}
#else // !USE_BULLET
// Stub implementation when Bullet is not available
static void ECS_IntegrateWithCMSystem(entt::registry &registry) {
	// No-op when Bullet physics is not available
}
#endif // USE_BULLET

// Register a collision callback function
void ECS_RegisterCollisionCallback(CollisionCallbackFunc callback, void *userData) {
	collisionCallbacks.emplace_back(callback, userData);
}

// Unregister collision callbacks (removes all with matching userData)
void ECS_UnregisterCollisionCallback(void *userData) {
	collisionCallbacks.erase(
		std::remove_if(collisionCallbacks.begin(), collisionCallbacks.end(),
			[userData](const auto &cb) { return std::get<1>(cb) == userData; }),
		collisionCallbacks.end());
}

// Process a collision event by calling all registered callbacks
static void ECS_ProcessCollisionEvent(entt::registry &registry, const CollisionEvent &event) {
	for (const auto &[callback, userData] : collisionCallbacks) {
		callback(registry, event, userData);
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

	// Debug visualization
	ECS_DebugDrawPhysics(registry);
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
	// Sync ECS components to/from engine entity structures
	// This handles bidirectional synchronization between ECS and legacy systems

	auto &registry = ECS::GetRegistry();
	auto networkView = registry.view<NetworkComponent>();

	for (auto entity : networkView) {
		auto &network = networkView.get<NetworkComponent>(entity);

		if (!network.needsSync) {
			continue;
		}

		// Get the corresponding engine entity
		gentity_t *gent = NULL;
		if (network.entityIndex >= 0 && network.entityIndex < MAX_GENTITIES) {
			gent = &g_entities[network.entityIndex];
		}

		if (!gent || !gent->inuse) {
			// Entity no longer exists in engine, remove from ECS
			ECS_DestroyEntity(static_cast<ecs_entity_t>(entity));
			continue;
		}

		// Sync transform component to engine
		if (auto transform = registry.try_get<TransformComponent>(entity)) {
			VectorCopy(transform->position, gent->r.currentOrigin);
			VectorCopy(transform->rotation, gent->r.currentAngles);

			// Update entity state for networking
			VectorCopy(transform->position, gent->s.origin);
			VectorCopy(transform->rotation, gent->s.angles);
		}

		// Sync health component to engine
		if (auto health = registry.try_get<HealthComponent>(entity)) {
			gent->health = health->health;
			if (gent->health <= 0) {
				gent->s.eFlags |= EF_DEAD;
			} else {
				gent->s.eFlags &= ~EF_DEAD;
			}
		}

		// Sync physics component state
		if (auto physics = registry.try_get<PhysicsComponent>(entity)) {
			// Update entity velocity for network sync
			gent->s.pos.trBase[0] = physics->velocity[0];
			gent->s.pos.trBase[1] = physics->velocity[1];
			gent->s.pos.trBase[2] = physics->velocity[2];
			gent->s.pos.trType = TR_LINEAR; // Simple linear motion
		}

		// Sync render component
		if (auto render = registry.try_get<RenderComponent>(entity)) {
			// Update model and skin information
			if (render->model[0]) {
				gent->model = render->model;
			}
			gent->s.modelindex = render->modelIndex;
			// gent->s.skinNum = render->skinIndex; // skinNum not in entityState_t

			// Update visibility and rendering properties
			if (!render->visible) {
				gent->r.svFlags |= SVF_NOCLIENT; // Don't send to clients
			} else {
				gent->r.svFlags &= ~SVF_NOCLIENT;
			}

			// Apply render effects
			if (render->renderFx & RF_GLOW) {
				gent->s.eFlags |= EF_BOUNCE;
			}
		}

		// Sync animation component
		if (auto anim = registry.try_get<AnimationComponent>(entity)) {
			gent->s.legsAnim = anim->legsAnim;
			gent->s.torsoAnim = anim->torsoAnim;
			// gent->s.animMovements = anim->movementFlags; // Field doesn't exist in entityState_t
		}

		// Mark entity as needing network update
		gent->r.svFlags |= SVF_BROADCAST;

		// Reset sync flag
		network.needsSync = qfalse;
	}
}

/*
================
ECS_RenderSystem_Update
Update render components and prepare entities for rendering
================
*/
void ECS_RenderSystem_Update(void) {
	auto &registry = ECS::GetRegistry();

	// Update render components based on other component states
	auto renderView = registry.view<RenderComponent>();

	for (auto entity : renderView) {
		auto &render = renderView.get<RenderComponent>(entity);

		// Update visibility based on health
		if (auto health = registry.try_get<HealthComponent>(entity)) {
			if (health->health <= 0) {
				render.visible = qfalse; // Dead entities are invisible
			} else {
				render.visible = qtrue;
			}

			// Apply damage flash effect (simplified)
			render.alpha = (health->health > 50) ? 1.0f : 0.8f; // Slightly transparent when low health
		}

		// Update animation state for rendering
		if (auto anim = registry.try_get<AnimationComponent>(entity)) {
			// Update animation timing
			anim->frameTime += 16.0f; // Assume 60 FPS (16ms per frame)

			// Simple animation state machine (placeholder)
			// In a full implementation, this would use animation data from models
			if (anim->frameTime >= 1000.0f) { // Reset every second
				anim->frameTime = 0.0f;
				anim->currentFrame = (anim->currentFrame + 1) % 10; // Simple 10-frame loop
			}
		}

		// Handle special render effects
		if (render.renderFx & RF_GLOW) {
			// Pulsing glow effect
			float glowIntensity = sinf(render.alpha * 6.28f) * 0.5f + 0.5f;
			render.alpha = 0.5f + glowIntensity * 0.5f;
		}

		// Update particle effects
		if (auto particles = registry.try_get<ParticleComponent>(entity)) {
			particles->time += 16.0f / 1000.0f; // Convert ms to seconds

			// Update particle system position to follow entity
			if (auto transform = registry.try_get<TransformComponent>(entity)) {
				VectorCopy(transform->position, particles->position);
			}

			// Handle particle system lifetime
			if (!particles->looping && particles->time >= particles->lifetime) {
				// Remove expired particle system
				registry.remove<ParticleComponent>(entity);
			} else {
				// Reset time for looping effects
				if (particles->looping && particles->time >= particles->lifetime) {
					particles->time = 0.0f;
				}
			}

			// Mark for network sync if needed
			if (auto network = registry.try_get<NetworkComponent>(entity)) {
				network->needsSync = qtrue;
			}
		}

		// Mark for network sync if render state changed
		if (auto network = registry.try_get<NetworkComponent>(entity)) {
			network->needsSync = qtrue;
		}
	}

	// Handle advanced rendering features
	auto advancedRenderView = registry.view<RenderComponent, TransformComponent>();

	for (auto entity : advancedRenderView) {
		auto &render = advancedRenderView.get<RenderComponent>(entity);
		auto &transform = advancedRenderView.get<TransformComponent>(entity);

		// Update bounding box based on scale
		render.mins[0] = -16.0f * transform.scale[0];
		render.mins[1] = -16.0f * transform.scale[1];
		render.mins[2] = -16.0f * transform.scale[2];
		render.maxs[0] = 16.0f * transform.scale[0];
		render.maxs[1] = 16.0f * transform.scale[1];
		render.maxs[2] = 16.0f * transform.scale[2];

		// Handle render component culling based on distance/camera
		// This would typically be done in the renderer, but we can prepare data here
		vec3_t renderPos;
		VectorCopy(transform.position, renderPos);

		// Mark entities outside reasonable range as not visible
		// (This is a simplified check - real culling would be more sophisticated)
		float distance = VectorLength(renderPos);
		if (distance > 10000.0f) { // 10km culling distance
			render.visible = qfalse;
		}
	}
}

/*
================
ECS_ParticleSystem_Update
Update particle systems and manage particle effects
================
*/
void ECS_ParticleSystem_Update(float deltaTime) {
	auto &registry = ECS::GetRegistry();
	auto particleView = registry.view<ParticleComponent>();

	for (auto entity : particleView) {
		auto &particles = particleView.get<ParticleComponent>(entity);

		if (!particles.active) {
			continue;
		}

		// Update particle system timing
		particles.time += deltaTime;

		// Handle particle spawning based on spawn rate
		// float particlesToSpawn = particles.spawnRate * deltaTime; // TODO: Implement when renderer integration is complete
		// Note: Actual particle spawning would be handled by the renderer
		// This system manages the logical state

		// Update particle system position from entity transform
		if (auto transform = registry.try_get<TransformComponent>(entity)) {
			// Update position to follow entity
			VectorCopy(transform->position, particles.position);

			// Apply velocity to particle system
			vec3_t velocityDelta;
			VectorScale(particles.velocity, deltaTime, velocityDelta);
			VectorAdd(particles.position, velocityDelta, particles.position);

			// Apply acceleration
			vec3_t accelDelta;
			VectorScale(particles.acceleration, deltaTime, accelDelta);
			VectorAdd(particles.velocity, accelDelta, particles.velocity);
		}

		// Handle system lifetime
		if (!particles.looping && particles.time >= particles.lifetime) {
			particles.active = qfalse;
			// Optionally remove component after effect completes
			// registry.remove<ParticleComponent>(entity);
		}

		// Mark for network sync
		if (auto network = registry.try_get<NetworkComponent>(entity)) {
			network->needsSync = qtrue;
		}
	}
}

/*
================
ECS_DoorSystem_Update
Handle door opening, closing, and locking mechanics
================
*/
void ECS_DoorSystem_Update(float deltaTime) {
	auto &registry = ECS::GetRegistry();
	auto doorView = registry.view<DoorComponent, TransformComponent>();

	for (auto entity : doorView) {
		auto &door = doorView.get<DoorComponent>(entity);
		auto &transform = doorView.get<TransformComponent>(entity);

		// Handle door state transitions
		if (door.isOpen && door.openProgress < 1.0f) {
			// Door is opening
			door.openProgress += door.openSpeed * deltaTime * 0.001f; // Convert to seconds
			if (door.openProgress >= 1.0f) {
				door.openProgress = 1.0f;
				// door.lastUsedTime = Sys_Milliseconds(); // TODO: Implement when Sys_Milliseconds is available
			}
		} else if (!door.isOpen && door.openProgress > 0.0f) {
			// Door is closing
			door.openProgress -= door.openSpeed * deltaTime * 0.001f;
			if (door.openProgress <= 0.0f) {
				door.openProgress = 0.0f;
			}
		}

		// Auto-close doors
		if (door.autoCloseTime > 0 && door.isOpen && door.openProgress >= 1.0f) {
			// int currentTime = Sys_Milliseconds(); // TODO: Implement when Sys_Milliseconds is available
		int currentTime = 0; // Fallback
			if (currentTime - door.lastUsedTime >= door.autoCloseTime) {
				door.isOpen = qfalse; // Start closing
			}
		}

		// Interpolate door position between closed and open positions
		vec3_t targetPos;
		// Simple linear interpolation: pos = start + (end - start) * frac
		targetPos[0] = door.closedPos[0] + (door.openPos[0] - door.closedPos[0]) * door.openProgress;
		targetPos[1] = door.closedPos[1] + (door.openPos[1] - door.closedPos[1]) * door.openProgress;
		targetPos[2] = door.closedPos[2] + (door.openPos[2] - door.closedPos[2]) * door.openProgress;
		VectorCopy(targetPos, transform.position);

		// Mark for network sync if position changed
		if (auto network = registry.try_get<NetworkComponent>(entity)) {
			network->needsSync = qtrue;
		}
	}
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
ECS_PickupSystem_Update
Handle item pickup logic - check for player proximity and pickup
================
*/
static void ECS_PickupSystem_Update(float deltaTime) {
	entt::registry &registry = ECS::GetRegistry();

	// Get all entities with PickupComponent and TransformComponent
	auto pickupView = registry.view<PickupComponent, TransformComponent>();

	for (auto pickupEntity : pickupView) {
		auto &pickup = pickupView.get<PickupComponent>(pickupEntity);
		auto &pickupTransform = pickupView.get<TransformComponent>(pickupEntity);

		// Skip if already picked up
		if (pickup.isPickedUp) {
			// Handle respawn timer
			if (pickup.respawn) {
				pickup.timeUntilRespawn -= deltaTime;
				if (pickup.timeUntilRespawn <= 0.0f) {
					pickup.isPickedUp = qfalse;
					pickup.timeUntilRespawn = pickup.respawnTime;

					// Restore entity visibility/rendering
					// TODO: Add RenderComponent support when renderer integration is complete
					// For now, entity will be visible again after respawn timer

					// Optionally play respawn sound/effect
					// S_StartSound(pickupTransform.position, ENTITYNUM_WORLD, CHAN_AUTO, "sound/items/respawn.wav");
				}
			}
			continue;
		}

		// Check proximity to players and handle pickup
		auto playerView = registry.view<PlayerClassComponent, TransformComponent>();

		for (auto playerEntity : playerView) {
                        (void)playerView; // Suppress unused variable warning
			auto &playerTransform = playerView.get<TransformComponent>(playerEntity);

			// Calculate distance between pickup and player
			vec3_t delta;
			VectorSubtract(pickupTransform.position, playerTransform.position, delta);
			float distance = VectorLength(delta);

			// Check if player is within pickup range
			if (distance <= pickup.pickupRadius) {
				// Handle pickup based on item type
				bool pickupSuccessful = false;

				switch (pickup.itemType) {
					case ItemType::HEALTH: {
						// Give health to player
						if (registry.any_of<HealthComponent>(playerEntity)) {
							auto &health = registry.get<HealthComponent>(playerEntity);
							int oldHealth = health.health;
							health.health = std::min(health.health + pickup.quantity, health.maxHealth);
							if (health.health > oldHealth) {
								pickupSuccessful = true;
							}
						}
						break;
					}

					case ItemType::ARMOR: {
						// Give armor to player
						if (registry.any_of<HealthComponent>(playerEntity)) {
							auto &health = registry.get<HealthComponent>(playerEntity);
							int oldArmor = health.armor;
							health.armor = std::min(health.armor + pickup.quantity, health.maxArmor);
							if (health.armor > oldArmor) {
								pickupSuccessful = true;
							}
						}
						break;
					}

					case ItemType::AMMO:
					case ItemType::WEAPON:
					case ItemType::POWERUP:
					case ItemType::KEY:
					case ItemType::KEYCARD:
					case ItemType::BACKPACK: {
						// These would typically go to inventory system
						// For now, just mark as successful pickup
						pickupSuccessful = true;
						break;
					}

					default:
						break;
				}

				if (pickupSuccessful) {
					// Mark pickup as collected
					pickup.isPickedUp = qtrue;
					pickup.timeUntilRespawn = pickup.respawnTime;

					// Play pickup sound
					if (pickup.pickupSound[0] != '\0') {
						// S_StartSound(pickupTransform.position, ENTITYNUM_WORLD, CHAN_AUTO, pickup.pickupSound);
						Com_DPrintf("Pickup sound: %s\n", pickup.pickupSound);
					}

					// Mark network sync needed
					if (auto net = registry.try_get<NetworkComponent>(pickupEntity)) {
						net->needsSync = qtrue;
					}
					if (auto net = registry.try_get<NetworkComponent>(playerEntity)) {
						net->needsSync = qtrue;
					}

					// Hide the pickup entity (until respawn if applicable)
					// TODO: Set entity visibility to false when RenderComponent is added

					Com_DPrintf("Entity %d picked up item %s (type %d) by player entity %d\n",
                                                            static_cast<int>(pickupEntity), pickup.model, static_cast<int>(pickup.itemType), static_cast<int>(playerEntity));

					// Only allow one player to pick up the item
					break;
				}
			}
		}
	}
}

/*
================
ECS_KeySystem_Update
Handle key/keycard logic - door unlocking, key management
================
*/
static void ECS_KeySystem_Update(float deltaTime) {
	entt::registry &registry = ECS::GetRegistry();

	// Get all entities with KeyComponent and PickupComponent
	auto view = registry.view<KeyComponent, PickupComponent>();

	for (auto entity : view) {
		auto &key = view.get<KeyComponent>(entity);
		auto &pickup = view.get<PickupComponent>(entity);

		// Check if key has been picked up
		if (!pickup.isPickedUp) {
			continue; // Key not yet picked up
		}

		// Check if key has a target door to unlock
		if (key.doorTarget[0] == '\0') {
			continue; // No target door specified
		}

		// Find door entities that match the target
		auto doorView = registry.view<DoorComponent>();
		bool doorFound = false;

		for (auto doorEntity : doorView) {
			auto &door = doorView.get<DoorComponent>(doorEntity);

			// Check if this door requires the key we have
			if (door.requiredKeyId == key.keyId) {
				// Found matching door
				if (door.isLocked) {
					// Unlock the door
					door.isLocked = qfalse;
					Com_Printf("Key '%d' unlocked door '%s'\n", key.keyId, door.doorId);

					// Mark door as needing network sync
					if (auto network = registry.try_get<NetworkComponent>(doorEntity)) {
						network->needsSync = qtrue;
					}

					doorFound = true;
				} else {
					Com_Printf("Door '%s' is already unlocked\n", door.doorId);
					doorFound = true;
				}

				// Play unlock sound if available
				if (key.pickupSound[0] != '\0') {
					Com_DPrintf("Playing key unlock sound: %s\n", key.pickupSound);
				}

				break;
			}
		}

		if (!doorFound) {
			Com_Printf("No door found that requires key '%d'\n", key.keyId);
		}

		// Remove the used key regardless of whether door was found
		// (prevents key duplication exploits)
		registry.remove<KeyComponent>(entity);
	}

	(void)deltaTime; // Suppress unused warning
}

/*
================
ECS_BackpackSystem_Update
Handle backpack effects - inventory expansion, duration tracking
================
*/
static void ECS_BackpackSystem_Update(float deltaTime) {
	entt::registry &registry = ECS::GetRegistry();

	// Get all entities with BackpackComponent
	auto view = registry.view<BackpackComponent>();

	std::vector<entt::entity> expiredBackpacks;

	for (auto entity : view) {
		auto &backpack = view.get<BackpackComponent>(entity);

		// Handle temporary backpack duration
		if (!backpack.permanent && backpack.timeRemaining > 0.0f) {
			backpack.timeRemaining -= deltaTime;
			if (backpack.timeRemaining <= 0.0f) {
				// Backpack has expired - remove effects from player
				expiredBackpacks.push_back(entity);
			}
		}

		// Apply backpack effects to player (permanent or temporary)
		// In a real implementation, this would modify player stats
		// For now, just ensure backpack effects are active
		if (backpack.inventorySlots > 0 || backpack.ammoCapacity > 0) {
			Com_DPrintf("Backpack entity %d active: +%d slots, +%d ammo capacity\n",
                                            static_cast<int>(entity), backpack.inventorySlots, backpack.ammoCapacity);
		}
	}

	// Remove expired backpack effects
	for (auto entity : expiredBackpacks) {
		auto &backpack = registry.get<BackpackComponent>(entity);

		Com_DPrintf("Backpack entity %d expired - removing effects\n", static_cast<int>(entity));

		// TODO: In a full implementation, this would:
		// 1. Find the player entity that owns this backpack
		// 2. Reduce player's inventory slots and ammo capacity
		// 3. Restore original stats if they were modified

		// For now, just remove the BackpackComponent
		registry.remove<BackpackComponent>(entity);

		// Play effect removal sound/visual feedback
		if (backpack.pickupSound[0] != '\0') {
			// S_StartSound(position, ENTITYNUM_WORLD, CHAN_AUTO, backpack.pickupSound);
			Com_DPrintf("Backpack expired sound: %s\n", backpack.pickupSound);
		}

		// Mark network sync needed
		if (auto net = registry.try_get<NetworkComponent>(entity)) {
			net->needsSync = qtrue;
		}
	}
}

/*
================
ECS_ObjectiveSystem_Update
Handle objective tracking - progress updates, completion checks
================
*/
static void ECS_ObjectiveSystem_Update(float deltaTime) {
	entt::registry &registry = ECS::GetRegistry();
	
	// Get all entities with ObjectiveComponent
	auto view = registry.view<ObjectiveComponent>();
	
	for (auto entity : view) {
		auto &objective = view.get<ObjectiveComponent>(entity);
		
		// Skip if already completed
		if (objective.completed) {
			continue;
		}
		
		// Check if objective is complete
		if (objective.progress >= objective.targetProgress && !objective.completed) {
			objective.completed = qtrue;

			Com_DPrintf("Objective %d completed: %s\n", objective.objectiveId, objective.title);

			// Play completion sound
			if (objective.completionSound[0] != '\0') {
				// S_StartLocalSound(objective.completionSound, CHAN_ANNOUNCER);
				Com_DPrintf("Objective completion sound: %s\n", objective.completionSound);
			}

			// Trigger completion events
					// Trigger objective completion events
					Com_Printf("Objective '%s' completed!\n", objective.title);

					// Award experience/rewards (placeholder)
					// TODO: Implement proper reward system
					if (auto playerStats = registry.try_get<HealthComponent>(entity)) {
						// Simple XP award
						playerStats->experience += objective.rewardXP;
						Com_DPrintf("Awarded %d XP\n", objective.rewardXP);
					}

					// Check for mission completion
					bool missionComplete = true;
					auto allObjectives = registry.view<ObjectiveComponent>();
					for (auto objEntity : allObjectives) {
						auto &obj = allObjectives.get<ObjectiveComponent>(objEntity);
						if (!obj.completed) {
							missionComplete = false;
							break;
						}
					}

					if (missionComplete) {
						Com_Printf("MISSION COMPLETE!\n");
						// TODO: Trigger mission completion sequence
						// - Play victory music
						// - Show mission complete screen
						// - Unlock next mission
						// - Save progress
					}

					// Mark network sync needed
					if (auto network = registry.try_get<NetworkComponent>(entity)) {
						network->needsSync = qtrue;
					}

			// Mark network sync needed
			if (auto net = registry.try_get<NetworkComponent>(entity)) {
				net->needsSync = qtrue;
			}
		}

		// Store old progress for comparison
		int oldProgress = objective.progress;

		// Update progress based on objective type and game state

		switch (objective.objectiveType) {
			case ObjectiveType::DESTROY: {
				// Check for destruction of target entities
				if (objective.targetEntity[0] != '\0') {
					// Count destroyed entities matching target
					// This is a simplified implementation - real game would track specific entities
					// For now, just simulate progress based on time (for demo purposes)
					if (objective.buildTime > 0.0f) {
						objective.progress = std::min(objective.targetProgress,
													static_cast<int>(oldProgress + deltaTime * 10.0f));
					}
				}
				break;
			}

			case ObjectiveType::CONSTRUCT: {
				// Check construction progress
				if (objective.buildTime > 0.0f) {
					// Simulate construction progress
					objective.progress = std::min(objective.targetProgress,
												static_cast<int>(oldProgress + deltaTime * 5.0f));
				}
				break;
			}

			case ObjectiveType::ESCORT: {
				// Check if escorted entity is still alive and in position
				// Simplified: just maintain current progress
				break;
			}

			case ObjectiveType::STEAL: {
				// Check if item has been stolen (picked up)
				auto pickupView = registry.view<PickupComponent>();
				for (auto pickupEntity : pickupView) {
					auto &pickup = pickupView.get<PickupComponent>(pickupEntity);
					if (pickup.isPickedUp && pickup.itemId == objective.objectiveId) {
						objective.progress = objective.targetProgress; // Item stolen
						break;
					}
				}
				break;
			}

			case ObjectiveType::CAPTURE: {
				// Check control point capture status
				// Simplified: maintain progress
				break;
			}

			case ObjectiveType::DEFEND: {
				// Check if defended entity/area is still intact
				// Simplified: maintain progress
				break;
			}

			case ObjectiveType::DELIVER: {
				// Check if item has been delivered to target location
				// Simplified: maintain progress
				break;
			}

			default:
				// Generic objective - progress based on time for demo
				objective.progress = std::min(objective.targetProgress,
											static_cast<int>(oldProgress + deltaTime * 2.0f));
				break;
		}
	}
	
	(void)deltaTime; // Suppress unused warning
}

/*
================
ECS_DebrisSystem_Update
Handle debris physics and cleanup - lifetime, fade out, cleanup
================
*/
static void ECS_DebrisSystem_Update(float deltaTime) {
	entt::registry &registry = ECS::GetRegistry();
	
	// Get all entities with DebrisComponent
	auto view = registry.view<DebrisComponent>();
	
	std::vector<entt::entity> toRemove;
	
	for (auto entity : view) {
		auto &debris = view.get<DebrisComponent>(entity);
		
		// Update lifetime
		debris.timeRemaining -= deltaTime;
		
		// Check if debris should be removed
		if (debris.timeRemaining <= 0.0f) {
			toRemove.push_back(entity);
			continue;
		}
		
		// Handle fade out
		if (debris.fadeOut && debris.timeRemaining <= debris.fadeStartTime) {
			// Calculate fade alpha: alpha = (timeRemaining / fadeStartTime)
			float fadeAlpha = debris.timeRemaining / debris.fadeStartTime;
			fadeAlpha = std::max(0.0f, std::min(1.0f, fadeAlpha));

			// TODO: Apply fade alpha to rendering when RenderComponent is available
			// For now, just log the fade progress
			Com_DPrintf("Debris entity %d fading: alpha=%.2f\n", static_cast<int>(entity), fadeAlpha);

			// Mark network sync needed for fade updates
			if (auto net = registry.try_get<NetworkComponent>(entity)) {
				net->needsSync = qtrue;
			}
		}
	}
	
	// Remove expired debris entities
	for (auto entity : toRemove) {
		ECS_DestroyEntity(static_cast<ecs_entity_t>(entity));
	}
}

/*
================
ECS_PlayerClassSystem_Update
Handle player class abilities and restrictions
================
*/
static void ECS_PlayerClassSystem_Update(float deltaTime) {
	entt::registry &registry = ECS::GetRegistry();
	
	// Get all entities with PlayerClassComponent
	auto view = registry.view<PlayerClassComponent>();
	
	for (auto entity : view) {
		auto &playerClass = view.get<PlayerClassComponent>(entity);

		// Apply class-specific bonuses to health/armor
		if (registry.any_of<HealthComponent>(entity)) {
			auto &health = registry.get<HealthComponent>(entity);

			// Base values (will be modified by class)
			int baseMaxHealth = 100;
			int baseMaxArmor = 50;

			// Apply class-specific health bonuses
			switch (playerClass.classType) {
				case PlayerClassType::SOLDIER:
					// Soldiers get extra armor
					baseMaxArmor += 25;
					break;

				case PlayerClassType::ENGINEER:
					// Engineers get moderate health bonus
					baseMaxHealth += 10;
					break;

				case PlayerClassType::MEDIC:
					// Medics get significant health bonus
					baseMaxHealth += 20;
					break;

				case PlayerClassType::FIELDOPS:
					// Field Ops get moderate armor bonus
					baseMaxArmor += 15;
					break;

				case PlayerClassType::COVERTOPS:
					// Covert Ops get stealth bonuses (no direct stat changes)
					break;

				default:
					break;
			}

			// Apply health bonus multiplier
			if (playerClass.healthBonus > 0.0f) {
				baseMaxHealth = static_cast<int>(baseMaxHealth * (1.0f + playerClass.healthBonus));
			}

			// Apply armor bonus multiplier
			if (playerClass.armorBonus > 0.0f) {
				baseMaxArmor = static_cast<int>(baseMaxArmor * (1.0f + playerClass.armorBonus));
			}

			// Update max values if they have changed
			if (health.maxHealth != baseMaxHealth) {
				health.maxHealth = baseMaxHealth;
				// Clamp current health to new max
				if (health.health > health.maxHealth) {
					health.health = health.maxHealth;
				}
			}

			if (health.maxArmor != baseMaxArmor) {
				health.maxArmor = baseMaxArmor;
				// Clamp current armor to new max
				if (health.armor > health.maxArmor) {
					health.armor = health.maxArmor;
				}
			}

			Com_DPrintf("Player class %s: Health=%d/%d, Armor=%d/%d\n",
					   playerClass.name, health.health, health.maxHealth,
					   health.armor, health.maxArmor);
		}

		// Apply movement speed modifiers
		if (registry.any_of<PhysicsComponent>(entity)) {
                        // Physics component accessed but not used directly

			// Apply class-specific speed multipliers
			float baseSpeed = 320.0f; // Default Quake 3 speed
			float modifiedSpeed = baseSpeed * playerClass.moveSpeedMultiplier;

			// TODO: Set movement speed in appropriate component
			// For now, just log the speed modification
			Com_DPrintf("Player class %s speed multiplier: %.2f (%.0f units/sec)\n",
					   playerClass.name, playerClass.moveSpeedMultiplier, modifiedSpeed);
		}

		// Apply class-specific abilities
		switch (playerClass.classType) {
			case PlayerClassType::SOLDIER:
				// Soldiers can drop ammo - this would be handled in game logic
				break;

			case PlayerClassType::ENGINEER:
				// Engineers can construct - this would be handled in game logic
				break;

			case PlayerClassType::MEDIC:
				// Medics can revive - this would be handled in game logic
				break;

			case PlayerClassType::FIELDOPS:
				// Field Ops can call artillery - this would be handled in game logic
				break;

			case PlayerClassType::COVERTOPS:
				// Covert Ops can disguise - this would be handled in game logic
				break;

			default:
				break;
		}
	}
	
	(void)deltaTime; // Suppress unused warning
}

/*
================
ECS_SkillSystem_Update
Handle skill progression and XP gain
================
*/
static void ECS_SkillSystem_Update(float deltaTime) {
	entt::registry &registry = ECS::GetRegistry();
	
	// Get all entities with SkillComponent
	auto view = registry.view<SkillComponent>();
	
	for (auto entity : view) {
		auto &skill = view.get<SkillComponent>(entity);
		
		// Check each skill for level progression
		for (int i = 0; i < 8; i++) {
			if (skill.skillLevels[i] < 4 && skill.skillXP[i] >= skill.skillXPRequired[i]) {
				// Level up
				skill.skillLevels[i]++;
				skill.skillXP[i] = 0;
				skill.skillsUnlocked[i] = qtrue;
				
				// Update XP requirement for next level
				if (skill.skillLevels[i] < 4) {
					switch (skill.skillLevels[i]) {
						case 1: skill.skillXPRequired[i] = 50; break;
						case 2: skill.skillXPRequired[i] = 90; break;
						case 3: skill.skillXPRequired[i] = 140; break;
						default: skill.skillXPRequired[i] = 999; break;
					}
				}
			}
		}
	}
	
	(void)deltaTime; // Suppress unused warning
}

/*
================
ECS_FireteamSystem_Update
Handle fireteam coordination and management
================
*/
static void ECS_FireteamSystem_Update(float deltaTime) {
	entt::registry &registry = ECS::GetRegistry();
	
	// Get all entities with FireteamComponent
	auto view = registry.view<FireteamComponent>();
	
	for (auto entity : view) {
		auto &fireteam = view.get<FireteamComponent>(entity);
		
		// Validate fireteam membership
		if (fireteam.isLeader && fireteam.leaderId != static_cast<int>(entity)) {
			// Leader changed, update
			fireteam.leaderId = static_cast<int>(entity);
		}
		
		// Handle fireteam coordination and management
		if (fireteam.isLeader && fireteam.numMembers > 1) {
			// Leader-specific logic
			Com_DPrintf("Fireteam %d led by entity %d has %d members\n",
					   fireteam.fireteamId, fireteam.leaderId, fireteam.numMembers);

			// TODO: Implement fireteam communication system
			// - Shared voice chat
			// - Team coordination commands
			// - Shared minimap/objective data

			// TODO: Share objective progress across fireteam members
			// - Synchronize objective completion
			// - Coordinate objective assignments
			// - Track team-based achievements

			// TODO: Coordinate fireteam actions
			// - Formation movement
			// - Synchronized attacks
			// - Cover fire mechanics

		} else if (!fireteam.isLeader && fireteam.fireteamId >= 0) {
			// Member-specific logic
			Com_DPrintf("Entity %d is member of fireteam %d (leader: %d)\n",
                                            static_cast<int>(entity), fireteam.fireteamId, fireteam.leaderId);

			// TODO: Follow leader commands
			// - Execute coordinated actions
			// - Maintain formation
			// - Provide supporting fire
		}

		// Validate member connections (check if members still exist)
		for (int i = 0; i < fireteam.numMembers; i++) {
			int memberId = fireteam.memberIds[i];
			if (memberId >= 0) {
				entt::entity memberEntity = static_cast<entt::entity>(memberId);
				if (!registry.valid(memberEntity)) {
					// Member entity no longer exists, remove from fireteam
					Com_DPrintf("Fireteam %d member %d disconnected\n", fireteam.fireteamId, memberId);
					fireteam.memberIds[i] = -1;

					// Shift remaining members down
					for (int j = i; j < fireteam.numMembers - 1; j++) {
						fireteam.memberIds[j] = fireteam.memberIds[j + 1];
						fireteam.memberIds[j + 1] = -1;
					}
					fireteam.numMembers--;
					i--; // Recheck this slot
				}
			}
		}

		// If leader leaves and there are other members, promote someone
		if (fireteam.isLeader && fireteam.numMembers <= 1) {
			Com_DPrintf("Fireteam %d disbanded (leader left, no members)\n", fireteam.fireteamId);
			fireteam.fireteamId = -1;
			fireteam.leaderId = -1;
			fireteam.isLeader = qfalse;
			fireteam.numMembers = 0;
		}
	}
	
	(void)deltaTime; // Suppress unused warning
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
	
	// EntityPlus-inspired systems
	ECS_PickupSystem_Update(deltaTime);
	ECS_KeySystem_Update(deltaTime);
	ECS_BackpackSystem_Update(deltaTime);
	ECS_ObjectiveSystem_Update(deltaTime);
	ECS_DebrisSystem_Update(deltaTime);
	
	// Class-based gameplay systems
	ECS_PlayerClassSystem_Update(deltaTime);
	ECS_SkillSystem_Update(deltaTime);
	ECS_FireteamSystem_Update(deltaTime);
	
	ECS_NetworkSyncSystem_Update();
}

#endif // USE_ENTT

