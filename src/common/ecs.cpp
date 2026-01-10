/*
===========================================================================
ECS (Entity Component System) C++ Implementation

Core EnTT registry wrapper providing C interface for engine use.
===========================================================================
*/

#ifdef USE_ENTT

#include "ecs.h"
#include "ecs_components.h"
#include "ecs_internal.h"
#include <entt/entt.hpp>
#include <unordered_map>
#include <cassert>
#include <optional>
#include <vector>
#include <cstring>

// Global registry instance
static entt::registry *g_registry = nullptr;

// Entity ID mapping (for compatibility with engine entity indices)
// Maps engine entity index -> EnTT entity
static std::unordered_map<int, entt::entity> g_entityMap;
static std::unordered_map<entt::entity, int> g_reverseEntityMap;
static entt::entity g_nextEntity = entt::null;

// Helper to validate registry/entity handles from C-callable wrappers.
static bool ECS_GetRegistryAndEntity(ecs_entity_t entity, entt::registry **outRegistry, entt::entity *outEntity) {
	if (g_registry == nullptr) {
		return false;
	}

	entt::entity enttEntity = static_cast<entt::entity>(entity);
	if (!g_registry->valid(enttEntity)) {
		return false;
	}

	if (outRegistry) {
		*outRegistry = g_registry;
	}
	if (outEntity) {
		*outEntity = enttEntity;
	}
	return true;
}

/*
================
ECS_Init
Initialize the ECS system
================
*/
void ECS_Init(void) {
	if (g_registry != nullptr) {
		return; // Already initialized
	}
	
	g_registry = new entt::registry();
	g_entityMap.clear();
	g_reverseEntityMap.clear();
	g_nextEntity = entt::null;
}

/*
================
ECS_Shutdown
Shutdown the ECS system
================
*/
void ECS_Shutdown(void) {
	if (g_registry == nullptr) {
		return;
	}
	
	// Clear all entities
	g_registry->clear();
	g_entityMap.clear();
	g_reverseEntityMap.clear();
	
	delete g_registry;
	g_registry = nullptr;
}

/*
================
ECS_CreateEntity
Create a new ECS entity
================
*/
ecs_entity_t ECS_CreateEntity(void) {
	if (g_registry == nullptr) {
		return ECS_NULL_ENTITY;
	}

	entt::entity entity = g_registry->create();
	return static_cast<ecs_entity_t>(entity);
}

/*
================
ECS_CreateEntity_Optional

C++23 version using std::optional for better error handling
================
*/
std::optional<ecs_entity_t> ECS_CreateEntity_Optional(void) {
	if (g_registry == nullptr) {
		return std::nullopt;
	}

	entt::entity entity = g_registry->create();
	return static_cast<ecs_entity_t>(entity);
}

/*
================
ECS_DestroyEntity
Destroy an ECS entity
================
*/
void ECS_DestroyEntity(ecs_entity_t entity) {
	if (g_registry == nullptr) {
		return;
	}
	
	entt::entity enttEntity = static_cast<entt::entity>(entity);
	
	// Remove from reverse mapping if exists
	auto it = g_reverseEntityMap.find(enttEntity);
	if (it != g_reverseEntityMap.end()) {
		g_entityMap.erase(it->second);
		g_reverseEntityMap.erase(it);
	}

#ifdef USE_BULLET
	// Tear down Bullet state before destroying the entity so bodies do not leak or keep simulating.
	if (g_registry->valid(enttEntity) && g_registry->all_of<PhysicsComponent>(enttEntity)) {
		auto &physics = g_registry->get<PhysicsComponent>(enttEntity);
		ECS::BulletOnEntityDestroyed(*g_registry, enttEntity, physics);
	}
#endif
	
	g_registry->destroy(enttEntity);
}

/*
================
ECS_IsValid
Check if an entity is valid
================
*/
qboolean ECS_IsValid(ecs_entity_t entity) {
	if (g_registry == nullptr) {
		return qfalse;
	}
	
	entt::entity enttEntity = static_cast<entt::entity>(entity);
	return g_registry->valid(enttEntity) ? qtrue : qfalse;
}

/*
================
ECS_SetTransform
Ensure a TransformComponent exists and update its values
================
*/
qboolean ECS_SetTransform(ecs_entity_t entity, const vec3_t position, const vec3_t rotation, const vec3_t scale) {
	entt::registry *registry = nullptr;
	entt::entity enttEntity = entt::null;
	if (!ECS_GetRegistryAndEntity(entity, &registry, &enttEntity)) {
		return qfalse;
	}

	TransformComponent *transform = registry->try_get<TransformComponent>(enttEntity);
	if (!transform) {
		transform = &registry->emplace<TransformComponent>(enttEntity);
	}

	VectorCopy(position, transform->position);
	VectorCopy(rotation, transform->rotation);
	VectorCopy(scale, transform->scale);

	if (auto net = registry->try_get<NetworkComponent>(enttEntity)) {
		net->needsSync = qtrue;
	}

	return qtrue;
}

/*
================
ECS_SetPhysics
Ensure a PhysicsComponent exists and update its values
================
*/
qboolean ECS_SetPhysics(ecs_entity_t entity, const vec3_t velocity, const vec3_t acceleration, float mass, float friction, qboolean useBullet) {
	entt::registry *registry = nullptr;
	entt::entity enttEntity = entt::null;
	if (!ECS_GetRegistryAndEntity(entity, &registry, &enttEntity)) {
		return qfalse;
	}

	PhysicsComponent *physics = registry->try_get<PhysicsComponent>(enttEntity);
	if (!physics) {
		physics = &registry->emplace<PhysicsComponent>(enttEntity);
	}

	VectorCopy(velocity, physics->velocity);
	VectorCopy(acceleration, physics->acceleration);
	physics->mass = mass;
	physics->friction = friction;

#ifdef USE_BULLET
	// If Bullet was previously enabled and we are turning it off, tear down the body.
	const bool disableBullet = physics->useBullet && (useBullet == qfalse);
	physics->useBullet = useBullet;
	if (disableBullet && physics->body) {
		ECS::BulletOnEntityDestroyed(*registry, enttEntity, *physics);
	}
#else
	(void)useBullet;
#endif

	if (auto net = registry->try_get<NetworkComponent>(enttEntity)) {
		net->needsSync = qtrue;
	}

	return qtrue;
}

/*
================
ECS_SetHealth
Ensure a HealthComponent exists and update its values
================
*/
qboolean ECS_SetHealth(ecs_entity_t entity, int health, int maxHealth, int armor, int maxArmor) {
	entt::registry *registry = nullptr;
	entt::entity enttEntity = entt::null;
	if (!ECS_GetRegistryAndEntity(entity, &registry, &enttEntity)) {
		return qfalse;
	}

	HealthComponent *hc = registry->try_get<HealthComponent>(enttEntity);
	if (!hc) {
		hc = &registry->emplace<HealthComponent>(enttEntity);
	}

	hc->health = health;
	hc->maxHealth = maxHealth;
	hc->armor = armor;
	hc->maxArmor = maxArmor;

	// Clamp to valid ranges
	if (hc->maxHealth < 1) hc->maxHealth = 1;
	if (hc->health < 0) hc->health = 0;
	if (hc->health > hc->maxHealth) hc->health = hc->maxHealth;

	if (hc->maxArmor < 0) hc->maxArmor = 0;
	if (hc->armor < 0) hc->armor = 0;
	if (hc->armor > hc->maxArmor) hc->armor = hc->maxArmor;

	if (auto net = registry->try_get<NetworkComponent>(enttEntity)) {
		net->needsSync = qtrue;
	}

	return qtrue;
}

/*
================
ECS_SetLifetime
Attach or update a LifetimeComponent on the entity
================
*/
qboolean ECS_SetLifetime(ecs_entity_t entity, float seconds) {
	entt::registry *registry = nullptr;
	entt::entity enttEntity = entt::null;
	if (!ECS_GetRegistryAndEntity(entity, &registry, &enttEntity)) {
		return qfalse;
	}

	LifetimeComponent *life = registry->try_get<LifetimeComponent>(enttEntity);
	if (!life) {
		life = &registry->emplace<LifetimeComponent>(enttEntity);
	}

	life->remaining = seconds;
	life->destroyOnExpire = qtrue;
	return qtrue;
}

/*
================
ECS_CreateKey
Create a key or keycard entity (inspired by EntityPlus)
================
*/
ecs_entity_t ECS_CreateKey(KeyColor color, int keyId, qboolean isKeycard, const vec3_t position) {
	entt::registry *registry = nullptr;
	if (!ECS_GetRegistryAndEntity(ECS_NULL_ENTITY, &registry, nullptr)) {
		// Registry not initialized, try to initialize
		ECS_Init();
		registry = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
		if (!registry) {
			return ECS_NULL_ENTITY;
		}
	}
	
	entt::entity entity = registry->create();
	
	// Add TransformComponent
	TransformComponent &transform = registry->emplace<TransformComponent>(entity);
	VectorCopy(position, transform.position);
	
	// Add KeyComponent
	KeyComponent &key = registry->emplace<KeyComponent>(entity);
	key.color = color;
	key.keyId = keyId;
	key.isKeycard = isKeycard;
	
	// Add PickupComponent for pickup behavior
	PickupComponent &pickup = registry->emplace<PickupComponent>(entity);
	pickup.itemType = isKeycard ? ItemType::KEYCARD : ItemType::KEY;
	pickup.itemId = keyId;
	pickup.quantity = 1;
	pickup.autoPickup = qfalse;
	pickup.respawn = qfalse;
	
	// Set model based on color and type
	if (isKeycard) {
		switch (color) {
			case KeyColor::BLUE: Q_strncpyz(key.model, "models/powerups/keys/keycard-b.md3", sizeof(key.model)); break;
			case KeyColor::GREEN: Q_strncpyz(key.model, "models/powerups/keys/keycard-g.md3", sizeof(key.model)); break;
			case KeyColor::RED: Q_strncpyz(key.model, "models/powerups/keys/keycard-r.md3", sizeof(key.model)); break;
			case KeyColor::YELLOW: Q_strncpyz(key.model, "models/powerups/keys/keycard-y.md3", sizeof(key.model)); break;
			default: Q_strncpyz(key.model, "models/powerups/keys/keycard-b.md3", sizeof(key.model)); break;
		}
	} else {
		switch (color) {
			case KeyColor::GOLD: Q_strncpyz(key.model, "models/powerups/keys/key_gold.md3", sizeof(key.model)); break;
			case KeyColor::IRON: Q_strncpyz(key.model, "models/powerups/keys/key_iron.md3", sizeof(key.model)); break;
			case KeyColor::SILVER: Q_strncpyz(key.model, "models/powerups/keys/key_silver.md3", sizeof(key.model)); break;
			case KeyColor::MASTER: Q_strncpyz(key.model, "models/powerups/keys/key_master.md3", sizeof(key.model)); break;
			default: Q_strncpyz(key.model, "models/powerups/keys/key_gold.md3", sizeof(key.model)); break;
		}
	}
	
	return static_cast<ecs_entity_t>(entity);
}

/*
================
ECS_CreateBackpack
Create a backpack entity (inspired by EntityPlus)
================
*/
ecs_entity_t ECS_CreateBackpack(const vec3_t position, int inventorySlots, int ammoCapacity, qboolean permanent) {
	entt::registry *registry = nullptr;
	if (!ECS_GetRegistryAndEntity(ECS_NULL_ENTITY, &registry, nullptr)) {
		ECS_Init();
		registry = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
		if (!registry) {
			return ECS_NULL_ENTITY;
		}
	}
	
	entt::entity entity = registry->create();
	
	// Add TransformComponent
	TransformComponent &transform = registry->emplace<TransformComponent>(entity);
	VectorCopy(position, transform.position);
	
	// Add BackpackComponent
	BackpackComponent &backpack = registry->emplace<BackpackComponent>(entity);
	backpack.inventorySlots = inventorySlots;
	backpack.ammoCapacity = ammoCapacity;
	backpack.permanent = permanent;
	if (!permanent) {
		backpack.duration = 60.0f; // Default 60 seconds
		backpack.timeRemaining = backpack.duration;
	}
	Q_strncpyz(backpack.model, "models/powerups/backpack/backpack.md3", sizeof(backpack.model));
	Q_strncpyz(backpack.pickupSound, "sound/items/backpack_pickup.wav", sizeof(backpack.pickupSound));
	
	// Add PickupComponent
	PickupComponent &pickup = registry->emplace<PickupComponent>(entity);
	pickup.itemType = ItemType::BACKPACK;
	pickup.itemId = 0; // Backpack item ID
	pickup.quantity = 1;
	pickup.autoPickup = qfalse;
	pickup.respawn = qtrue;
	pickup.respawnTime = 30.0f;
	Q_strncpyz(pickup.model, backpack.model, sizeof(pickup.model));
	
	return static_cast<ecs_entity_t>(entity);
}

/*
================
ECS_CreateObjective
Create an objective marker entity (inspired by EntityPlus)
================
*/
ecs_entity_t ECS_CreateObjective(int objectiveId, const char *title, const char *description, const vec3_t position) {
	entt::registry *registry = nullptr;
	if (!ECS_GetRegistryAndEntity(ECS_NULL_ENTITY, &registry, nullptr)) {
		ECS_Init();
		registry = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
		if (!registry) {
			return ECS_NULL_ENTITY;
		}
	}
	
	entt::entity entity = registry->create();
	
	// Add TransformComponent
	TransformComponent &transform = registry->emplace<TransformComponent>(entity);
	VectorCopy(position, transform.position);
	
	// Add ObjectiveComponent
	ObjectiveComponent &objective = registry->emplace<ObjectiveComponent>(entity);
	objective.objectiveId = objectiveId;
	if (title) {
		Q_strncpyz(objective.title, title, sizeof(objective.title));
	}
	if (description) {
		Q_strncpyz(objective.description, description, sizeof(objective.description));
	}
	objective.completed = qfalse;
	objective.required = qtrue;
	objective.progress = 0;
	objective.targetProgress = 100;
	objective.showOnHUD = qtrue;
	
	return static_cast<ecs_entity_t>(entity);
}

/*
================
ECS_CreateDebris
Create a debris entity (inspired by EntityPlus)
================
*/
ecs_entity_t ECS_CreateDebris(const vec3_t position, const vec3_t velocity, const char *model, const char *material, float lifetime) {
	entt::registry *registry = nullptr;
	if (!ECS_GetRegistryAndEntity(ECS_NULL_ENTITY, &registry, nullptr)) {
		ECS_Init();
		registry = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
		if (!registry) {
			return ECS_NULL_ENTITY;
		}
	}
	
	entt::entity entity = registry->create();
	
	// Add TransformComponent
	TransformComponent &transform = registry->emplace<TransformComponent>(entity);
	VectorCopy(position, transform.position);
	
	// Add PhysicsComponent for debris physics
	PhysicsComponent &physics = registry->emplace<PhysicsComponent>(entity);
	VectorCopy(velocity, physics.velocity);
	physics.mass = 1.0f;
	physics.friction = 0.5f;
	
	// Add DebrisComponent
	DebrisComponent &debris = registry->emplace<DebrisComponent>(entity);
	if (model) {
		Q_strncpyz(debris.model, model, sizeof(debris.model));
	}
	if (material) {
		Q_strncpyz(debris.material, material, sizeof(debris.material));
	}
	debris.lifetime = lifetime > 0.0f ? lifetime : 10.0f;
	debris.timeRemaining = debris.lifetime;
	debris.bounceFactor = 0.3f;
	debris.fadeOut = qtrue;
	debris.fadeStartTime = 2.0f;
	
	return static_cast<ecs_entity_t>(entity);
}

/*
================
ECS_CreatePickupItem
Create a generic pickup item entity
================
*/
ecs_entity_t ECS_CreatePickupItem(int itemTypeInt, int itemId, const vec3_t position, const char *model) {
	ItemType itemType = static_cast<ItemType>(itemTypeInt);
	entt::registry *registry = nullptr;
	if (!ECS_GetRegistryAndEntity(ECS_NULL_ENTITY, &registry, nullptr)) {
		ECS_Init();
		registry = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
		if (!registry) {
			return ECS_NULL_ENTITY;
		}
	}
	
	entt::entity entity = registry->create();
	
	// Add TransformComponent
	TransformComponent &transform = registry->emplace<TransformComponent>(entity);
	VectorCopy(position, transform.position);
	
	// Add PickupComponent
	PickupComponent &pickup = registry->emplace<PickupComponent>(entity);
	pickup.itemType = itemType;
	pickup.itemId = itemId;
	pickup.quantity = 1;
	pickup.autoPickup = qfalse;
	pickup.respawn = qtrue;
	pickup.respawnTime = 30.0f;
	if (model) {
		Q_strncpyz(pickup.model, model, sizeof(pickup.model));
	}
	
	// Add ItemComponent for item properties
	ItemComponent &item = registry->emplace<ItemComponent>(entity);
	item.stackable = qtrue;
	item.maxStack = 99;
	item.consumable = (itemType == ItemType::HEALTH || itemType == ItemType::ARMOR || itemType == ItemType::POWERUP);
	
	return static_cast<ecs_entity_t>(entity);
}

/*
================
ECS_CreatePlayerClass
Create a player class entity with class-specific abilities
================
*/
ecs_entity_t ECS_CreatePlayerClass(int classTypeInt, int team, const vec3_t position) {
	PlayerClassType classType = static_cast<PlayerClassType>(classTypeInt);
	entt::registry *registry = nullptr;
	if (!ECS_GetRegistryAndEntity(ECS_NULL_ENTITY, &registry, nullptr)) {
		ECS_Init();
		registry = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
		if (!registry) {
			return ECS_NULL_ENTITY;
		}
	}
	
	entt::entity entity = registry->create();
	
	// Add TransformComponent
	TransformComponent &transform = registry->emplace<TransformComponent>(entity);
	VectorCopy(position, transform.position);
	
	// Add PlayerClassComponent
	PlayerClassComponent &playerClass = registry->emplace<PlayerClassComponent>(entity);
	playerClass.classType = classType;
	playerClass.team = team;
	
	// Set class-specific properties
	switch (classType) {
		case PlayerClassType::SOLDIER:
			Q_strncpyz(playerClass.name, "Soldier", sizeof(playerClass.name));
			playerClass.canDropAmmo = qtrue;
			playerClass.moveSpeedMultiplier = 1.0f;
			playerClass.healthBonus = 0.0f;
			break;
		case PlayerClassType::ENGINEER:
			Q_strncpyz(playerClass.name, "Engineer", sizeof(playerClass.name));
			playerClass.canConstruct = qtrue;
			playerClass.moveSpeedMultiplier = 1.0f;
			break;
		case PlayerClassType::MEDIC:
			Q_strncpyz(playerClass.name, "Medic", sizeof(playerClass.name));
			playerClass.canRevive = qtrue;
			playerClass.moveSpeedMultiplier = 1.1f; // Medics move slightly faster
			break;
		case PlayerClassType::FIELDOPS:
			Q_strncpyz(playerClass.name, "Field Ops", sizeof(playerClass.name));
			playerClass.canDropAmmo = qtrue;
			playerClass.canCallArtillery = qtrue;
			break;
		case PlayerClassType::COVERTOPS:
			Q_strncpyz(playerClass.name, "Covert Ops", sizeof(playerClass.name));
			playerClass.canDisguise = qtrue;
			playerClass.moveSpeedMultiplier = 1.15f; // Fastest movement
			break;
		default:
			break;
	}
	
	// Add SkillComponent for progression
	SkillComponent &skill = registry->emplace<SkillComponent>(entity);
	
	return static_cast<ecs_entity_t>(entity);
}

/*
================
ECS_AddSkillXP
Add experience points to a skill for an entity
================
*/
qboolean ECS_AddSkillXP(ecs_entity_t entity, int skillTypeInt, int xpAmount) {
	SkillType skillType = static_cast<SkillType>(skillTypeInt);
	entt::registry *registry = nullptr;
	entt::entity enttEntity = entt::null;
	if (!ECS_GetRegistryAndEntity(entity, &registry, &enttEntity)) {
		return qfalse;
	}
	
	SkillComponent *skill = registry->try_get<SkillComponent>(enttEntity);
	if (!skill) {
		skill = &registry->emplace<SkillComponent>(enttEntity);
	}
	
	if (skillType >= SkillType::MAX_SKILLS || skillType <= SkillType::NONE) {
		return qfalse;
	}
	
	int skillIndex = static_cast<int>(skillType) - 1;
	if (skillIndex < 0 || skillIndex >= 8) {
		return qfalse;
	}
	
	// Add XP
	skill->skillXP[skillIndex] += xpAmount;
	skill->experiencePoints += xpAmount;
	
	return qtrue;
}

/*
================
ECS_CreateFireteam
Create a new fireteam with a leader
================
*/
int ECS_CreateFireteam(const char *name, int leaderEntityId, int team) {
	entt::registry *registry = nullptr;
	if (!ECS_GetRegistryAndEntity(ECS_NULL_ENTITY, &registry, nullptr)) {
		ECS_Init();
		registry = reinterpret_cast<entt::registry *>(ECS_GetRegistry());
		if (!registry) {
			return -1;
		}
	}
	
	// Find next available fireteam ID
	static int nextFireteamId = 1;
	int fireteamId = nextFireteamId++;
	
	// Get leader entity
	entt::entity leaderEntity = static_cast<entt::entity>(leaderEntityId);
	if (!registry->valid(leaderEntity)) {
		return -1;
	}
	
	// Add or update FireteamComponent for leader
	FireteamComponent *fireteam = registry->try_get<FireteamComponent>(leaderEntity);
	if (!fireteam) {
		fireteam = &registry->emplace<FireteamComponent>(leaderEntity);
	}
	
	fireteam->fireteamId = fireteamId;
	fireteam->leaderId = leaderEntityId;
	fireteam->isLeader = qtrue;
	fireteam->team = team;
	fireteam->numMembers = 1;
	fireteam->memberIds[0] = leaderEntityId;
	if (name) {
		Q_strncpyz(fireteam->name, name, sizeof(fireteam->name));
	}
	
	return fireteamId;
}

/*
================
ECS_JoinFireteam
Add entity to an existing fireteam
================
*/
qboolean ECS_JoinFireteam(ecs_entity_t entity, int fireteamId) {
	entt::registry *registry = nullptr;
	entt::entity enttEntity = entt::null;
	if (!ECS_GetRegistryAndEntity(entity, &registry, &enttEntity)) {
		return qfalse;
	}
	
	// Find fireteam leader
	auto fireteamView = registry->view<FireteamComponent>();
	for (auto ftEntity : fireteamView) {
		auto &ft = fireteamView.get<FireteamComponent>(ftEntity);
		if (ft.fireteamId == fireteamId && ft.isLeader) {
			// Add to fireteam
			if (ft.numMembers < 8) {
				ft.memberIds[ft.numMembers] = static_cast<int>(entity);
				ft.numMembers++;
				
				// Add FireteamComponent to joining entity
				FireteamComponent *memberFt = registry->try_get<FireteamComponent>(enttEntity);
				if (!memberFt) {
					memberFt = &registry->emplace<FireteamComponent>(enttEntity);
				}
				memberFt->fireteamId = fireteamId;
				memberFt->leaderId = static_cast<int>(ftEntity);
				memberFt->isLeader = qfalse;
				memberFt->team = ft.team;
				Q_strncpyz(memberFt->name, ft.name, sizeof(memberFt->name));
				
				return qtrue;
			}
			return qfalse; // Fireteam full
		}
	}
	
	return qfalse; // Fireteam not found
}

/*
================
ECS_LeaveFireteam
Remove entity from its fireteam
================
*/
qboolean ECS_LeaveFireteam(ecs_entity_t entity) {
	entt::registry *registry = nullptr;
	entt::entity enttEntity = entt::null;
	if (!ECS_GetRegistryAndEntity(entity, &registry, &enttEntity)) {
		return qfalse;
	}
	
	FireteamComponent *fireteam = registry->try_get<FireteamComponent>(enttEntity);
	if (!fireteam || fireteam->fireteamId < 0) {
		return qfalse; // Not in a fireteam
	}
	
	// If leader, transfer leadership or disband
	if (fireteam->isLeader) {
		// Find another member to promote
		qboolean foundNewLeader = qfalse;
		for (int i = 0; i < fireteam->numMembers; i++) {
			if (fireteam->memberIds[i] != static_cast<int>(entity) && fireteam->memberIds[i] >= 0) {
				entt::entity newLeader = static_cast<entt::entity>(fireteam->memberIds[i]);
				if (registry->valid(newLeader)) {
					FireteamComponent *newLeaderFt = registry->try_get<FireteamComponent>(newLeader);
					if (newLeaderFt) {
						newLeaderFt->isLeader = qtrue;
						newLeaderFt->leaderId = fireteam->memberIds[i];
						foundNewLeader = qtrue;
						break;
					}
				}
			}
		}
		
		if (!foundNewLeader) {
			// Disband fireteam - remove from all members
			auto fireteamView = registry->view<FireteamComponent>();
			for (auto ftEntity : fireteamView) {
				auto &ft = fireteamView.get<FireteamComponent>(ftEntity);
				if (ft.fireteamId == fireteam->fireteamId) {
					ft.fireteamId = -1;
					ft.leaderId = -1;
					ft.isLeader = qfalse;
					ft.numMembers = 0;
				}
			}
		}
	}
	
	// Remove from fireteam
	fireteam->fireteamId = -1;
	fireteam->leaderId = -1;
	fireteam->isLeader = qfalse;
	fireteam->numMembers = 0;
	
	return qtrue;
}

/*
================
ECS_ClearLifetime
Remove the LifetimeComponent if present
================
*/
void ECS_ClearLifetime(ecs_entity_t entity) {
	entt::registry *registry = nullptr;
	entt::entity enttEntity = entt::null;
	if (!ECS_GetRegistryAndEntity(entity, &registry, &enttEntity)) {
		return;
	}

	if (registry->any_of<LifetimeComponent>(enttEntity)) {
		registry->remove<LifetimeComponent>(enttEntity);
	}
}

/*
================
ECS_GetRegistry
Get the EnTT registry pointer (for C++ code)
================
*/
ecs_registry_t *ECS_GetRegistry(void) {
	return reinterpret_cast<ecs_registry_t *>(g_registry);
}

// Helper functions for C++ code to access the registry
namespace ECS {
	entt::registry &GetRegistry() {
		assert(g_registry != nullptr);
		return *g_registry;
	}
	
	entt::entity GetEntityFromIndex(int index) {
		auto it = g_entityMap.find(index);
		if (it != g_entityMap.end()) {
			return it->second;
		}
		return entt::null;
	}
	
	int GetIndexFromEntity(entt::entity entity) {
		auto it = g_reverseEntityMap.find(entity);
		if (it != g_reverseEntityMap.end()) {
			return it->second;
		}
		return -1;
	}
	
	void MapEntity(int index, entt::entity entity) {
		g_entityMap[index] = entity;
		g_reverseEntityMap[entity] = index;
	}
	
	void UnmapEntity(int index) {
		auto it = g_entityMap.find(index);
		if (it != g_entityMap.end()) {
			g_reverseEntityMap.erase(it->second);
			g_entityMap.erase(it);
		}
	}
}

// C++ helper functions that can be called from other files
entt::entity ECS_GetEntityFromIndex(int index) {
	return ECS::GetEntityFromIndex(index);
}

int ECS_GetIndexFromEntity(entt::entity entity) {
	return ECS::GetIndexFromEntity(entity);
}

void ECS_MapEntity(int index, entt::entity entity) {
	ECS::MapEntity(index, entity);
}

void ECS_UnmapEntity(int index) {
	ECS::UnmapEntity(index);
}

#endif // USE_ENTT

