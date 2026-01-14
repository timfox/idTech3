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
#include <cstring>

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
                        event.timestamp = (uint64_t)time(NULL); // Use current time as timestamp

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
		// TODO: Call user-registered collision callbacks.
		// Implementation approach:
		//   1. Maintain a registry of user-defined collision callback functions
		//   2. When collision is detected, iterate through registered callbacks
		//   3. Call each callback with collision information (entities, contact points, etc.)
		//   4. Allow callbacks to modify collision response or trigger game events
		// This enables modders to customize collision behavior without modifying core systems.
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
ECS_PickupSystem_Update
Handle item pickup logic - check for player proximity and pickup
================
*/
static void ECS_PickupSystem_Update(float deltaTime) {
	entt::registry &registry = ECS::GetRegistry();
	
	// Get all entities with PickupComponent and TransformComponent
	auto view = registry.view<PickupComponent, TransformComponent>();
	
	for (auto entity : view) {
		auto &pickup = view.get<PickupComponent>(entity);
		auto &transform = view.get<TransformComponent>(entity);
		(void)transform; // Reserved for future use (distance checks, positioning, etc.)
		
		// Skip if already picked up
		if (pickup.isPickedUp) {
			// Handle respawn timer
			if (pickup.respawn) {
				pickup.timeUntilRespawn -= deltaTime;
				if (pickup.timeUntilRespawn <= 0.0f) {
					pickup.isPickedUp = qfalse;
					pickup.timeUntilRespawn = pickup.respawnTime;
					// TODO: Restore entity visibility/rendering.
					// Implementation approach:
					//   1. Check if entity has RenderComponent or similar
					//   2. Set visibility flag to true
					//   3. Trigger renderer update to show the entity again
					//   4. Optionally play respawn sound/effect
					// This ensures pickups become visible again after respawn timer expires.
				}
			}
			continue;
		}
		
		// TODO: Check proximity to players and handle pickup.
		// Implementation approach:
		//   1. Get all entities with PlayerComponent or similar
		//   2. Calculate distance from pickup to each player
		//   3. If within pickup range (e.g., 64 units), trigger pickup
		//   4. Apply pickup effects (health, armor, ammo, etc.) to player
		//   5. Mark pickup as collected and start respawn timer if applicable
		//   6. Play pickup sound and visual effects
		// This integrates with the game's player system to handle item collection.
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
	
	// Get all entities with KeyComponent
	auto view = registry.view<KeyComponent>();
	
	for (auto entity : view) {
		auto &key = view.get<KeyComponent>(entity);
		
		// TODO: Check if key has been picked up and unlock target door.
		// Implementation approach:
		//   1. Check if key.isPickedUp flag is set
		//   2. Find target door entity using key.targetDoorId or similar
		//   3. If door has DoorComponent, set door.locked = false
		//   4. Trigger door unlock animation/sound
		//   5. Update door state in game world
		// This enables key-based door unlocking mechanics in the game.
		(void)key; // Suppress unused warning
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
	
	for (auto entity : view) {
		auto &backpack = view.get<BackpackComponent>(entity);
		
		// Handle temporary backpack duration
		if (!backpack.permanent && backpack.timeRemaining > 0.0f) {
			backpack.timeRemaining -= deltaTime;
			if (backpack.timeRemaining <= 0.0f) {
				// TODO: Remove backpack effects from player.
				// Implementation approach:
				//   1. Find player entity associated with this backpack
				//   2. Remove or modify HealthComponent/ArmorComponent to remove bonuses
				//   3. Restore original health/armor values if they were modified
				//   4. Remove BackpackComponent from entity or mark for removal
				//   5. Play effect removal sound/visual feedback
				// This ensures temporary backpack effects are properly cleaned up.
			}
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
		if (objective.progress >= objective.targetProgress) {
			objective.completed = qtrue;
			// TODO: Play completion sound, trigger completion events.
			// Implementation approach:
			//   1. Play objective completion sound (S_StartLocalSound or similar)
			//   2. Trigger Lua event "objective_completed" with objective data
			//   3. Update UI to show objective completion
			//   4. Award XP or rewards if applicable
			//   5. Check if all objectives are complete (mission completion)
			// This provides feedback and triggers game progression events.
		}
		
		// TODO: Update progress based on game state.
		// Implementation approach:
		//   1. Query game state for objective-relevant data (kills, items collected, etc.)
		//   2. Calculate progress percentage based on current vs. target values
		//   3. Update objective.progress field
		//   4. Trigger progress update events for UI display
		// This integrates with game logic to track objective progress in real-time.
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
			// TODO: Apply fade alpha to rendering.
			// Implementation approach:
			//   1. Calculate fade alpha: alpha = (timeRemaining / fadeStartTime)
			//   2. Get RenderComponent or similar for this entity
			//   3. Set render component alpha/color alpha channel
			//   4. Renderer will use this alpha value for transparency
			//   5. Optionally use shader uniforms for per-entity alpha
			// This creates smooth fade-out effects for debris before removal.
			// Note: Requires renderer integration - see renderer boundary notes.
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
		// TODO: Apply class-specific bonuses to health/armor.
		// Implementation approach:
		//   1. Get HealthComponent for this entity
		//   2. Apply class-specific health/armor multipliers based on playerClass.type
		//   3. Update max health/armor values accordingly
		// This would integrate with HealthComponent to provide class-based gameplay variety.
		(void)playerClass; // Reserved for future use (see TODOs above and below)
		
		// TODO: Apply movement speed modifiers.
		// Implementation approach:
		//   1. Get PhysicsComponent for this entity
		//   2. Apply class-specific speed multipliers based on playerClass.type
		//   3. Update movement parameters (walk speed, run speed, etc.)
		// This would integrate with PhysicsComponent to provide class-based movement variety.
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
		
		// TODO: Handle fireteam communication, shared objectives, etc.
		// Implementation approach:
		//   1. Implement fireteam chat/voice communication system
		//   2. Share objective progress across fireteam members
		//   3. Coordinate fireteam actions (synchronized attacks, etc.)
		//   4. Track fireteam statistics and achievements
		//   5. Handle fireteam member join/leave events
		// This enables cooperative gameplay features for multiplayer modes.
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

