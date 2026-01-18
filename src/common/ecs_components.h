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

// Physics Component - Velocity, acceleration, mass, materials
struct PhysicsComponent {
	vec3_t velocity;
	vec3_t acceleration;
	float mass;
	float friction;

	// Material properties
	float restitution;    // Bounciness (0.0 = no bounce, 1.0 = perfect bounce)
	float density;        // Density for mass calculation
	float linearDamping;  // Linear velocity damping
	float angularDamping; // Angular velocity damping

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

	PhysicsComponent() : mass(1.0f), friction(0.1f), restitution(0.0f), density(1.0f),
		linearDamping(0.0f), angularDamping(0.0f)
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
	uint64_t timestamp;
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

// Item types inspired by EntityPlus
enum class ItemType : uint8_t {
	NONE = 0,
	KEY,              // Key item (opens doors/locks)
	KEYCARD,          // Keycard (blue, green, red, yellow)
	BACKPACK,         // Backpack (inventory expansion)
	POWERUP,          // Powerup item
	WEAPON,           // Weapon pickup
	AMMO,             // Ammo pickup
	HEALTH,            // Health pickup
	ARMOR,            // Armor pickup
	OBJECTIVE,        // Objective marker
	DEBRIS,           // Debris entity
	CUSTOM            // Custom item type
};

// Enhanced Objective Types (for objective-based gameplay)
enum class ObjectiveType : uint8_t {
	NONE = 0,
	DESTROY,      // Destroy target with explosives
	CONSTRUCT,    // Build/repair structure
	ESCORT,       // Escort vehicle/entity to location
	STEAL,        // Steal item and deliver
	CAPTURE,      // Capture control point
	DEFEND,       // Defend location/entity
	DELIVER       // Deliver item to location
};

// Key/Keycard colors (inspired by EntityPlus)
enum class KeyColor : uint8_t {
	NONE = 0,
	BLUE,
	GREEN,
	RED,
	YELLOW,
	GOLD,
	IRON,
	SILVER,
	MASTER
};

// Pickup Component - Generic pickup behavior for items
struct PickupComponent {
	ItemType itemType;
	int itemId;                    // Item ID (for inventory system)
	int quantity;                  // Quantity to give
	float pickupRadius;            // Radius for pickup detection
	qboolean autoPickup;           // Auto-pickup when in range
	qboolean respawn;              // Whether item respawns after pickup
	float respawnTime;             // Time until respawn (if respawn is true)
	float timeUntilRespawn;        // Current respawn timer
	qboolean isPickedUp;           // Whether item is currently picked up
	char pickupSound[MAX_QPATH];   // Sound to play on pickup
	char model[MAX_QPATH];         // Model path for the item
	char icon[MAX_QPATH];          // Icon path for UI

	PickupComponent() : itemType(ItemType::NONE), itemId(0), quantity(1),
		pickupRadius(32.0f), autoPickup(qfalse), respawn(qfalse),
		respawnTime(30.0f), timeUntilRespawn(0.0f), isPickedUp(qfalse) {
		pickupSound[0] = '\0';
		model[0] = '\0';
		icon[0] = '\0';
	}
};

// Key Component - For key/keycard entities (inspired by EntityPlus)
struct KeyComponent {
	KeyColor color;
	int keyId;                     // Unique key ID (for door matching)
	qboolean isKeycard;            // true for keycard, false for key
	char doorTarget[MAX_QPATH];    // Target door/entity to unlock
	char model[MAX_QPATH];         // Model path
	char pickupSound[MAX_QPATH];   // Sound on pickup
	char description[128];         // Description text

	KeyComponent() : color(KeyColor::NONE), keyId(0), isKeycard(qfalse) {
		doorTarget[0] = '\0';
		model[0] = '\0';
		pickupSound[0] = '\0';
		description[0] = '\0';
	}

	KeyComponent(KeyColor c, int id, qboolean keycard) 
		: color(c), keyId(id), isKeycard(keycard) {
		doorTarget[0] = '\0';
		model[0] = '\0';
		pickupSound[0] = '\0';
		description[0] = '\0';
	}
};

// Backpack Component - Inventory expansion item (inspired by EntityPlus)
struct BackpackComponent {
	int inventorySlots;            // Additional inventory slots granted
	int ammoCapacity;             // Additional ammo capacity
	qboolean permanent;            // Whether backpack is permanent or temporary
	float duration;                // Duration if temporary (-1 = permanent)
	float timeRemaining;          // Time remaining if temporary
	char model[MAX_QPATH];        // Model path
	char pickupSound[MAX_QPATH];  // Sound on pickup
	char effectModel[MAX_QPATH];  // Effect model (spinning backpack)

	BackpackComponent() : inventorySlots(10), ammoCapacity(50),
		permanent(qtrue), duration(-1.0f), timeRemaining(-1.0f) {
		model[0] = '\0';
		pickupSound[0] = '\0';
		effectModel[0] = '\0';
	}
};

// Objective Component - Mission/quest tracking (inspired by EntityPlus)
struct ObjectiveComponent {
	int objectiveId;              // Unique objective ID
	char title[128];              // Objective title
	char description[256];        // Objective description
	qboolean completed;            // Whether objective is completed
	qboolean required;             // Whether objective is required to progress
	int progress;                  // Current progress (0-100)
	int targetProgress;            // Target progress (100 = complete)
	char targetEntity[MAX_QPATH]; // Target entity name
	char completionSound[MAX_QPATH]; // Sound on completion
	char updateSound[MAX_QPATH];  // Sound on progress update
	qboolean showOnHUD;            // Whether to show on HUD
	// Enhanced objective types
	ObjectiveType objectiveType;   // Type of objective
	int requiredClass;             // Required player class (-1 = any)
	float buildTime;               // Time to construct (for CONSTRUCT type)
	float plantTime;               // Time to plant explosives (for DESTROY type)
	float defuseTime;              // Time to defuse (for DESTROY type)
	qboolean isSequential;         // Must complete previous objectives first
	int prerequisiteObjectiveId;   // ID of prerequisite objective (-1 = none)
	int team;                      // Team that can complete (0=red, 1=blue, -1=any)

	ObjectiveComponent() : objectiveId(0), completed(qfalse),
		required(qtrue), progress(0), targetProgress(100),
		showOnHUD(qtrue), objectiveType(ObjectiveType::NONE),
		requiredClass(-1), buildTime(0.0f), plantTime(0.0f),
		defuseTime(0.0f), isSequential(qfalse),
		prerequisiteObjectiveId(-1), team(-1) {
		title[0] = '\0';
		description[0] = '\0';
		targetEntity[0] = '\0';
		completionSound[0] = '\0';
		updateSound[0] = '\0';
	}
};

// Debris Component - Physics-based debris entities (inspired by EntityPlus)
struct DebrisComponent {
	char model[MAX_QPATH];        // Debris model path
	char material[MAX_QPATH];     // Material type (concrete, glass, wood, stone)
	float lifetime;                // How long debris exists before cleanup
	float timeRemaining;          // Time remaining
	float bounceFactor;            // Bounce factor (0.0 = no bounce, 1.0 = full bounce)
	qboolean fadeOut;              // Whether to fade out before removal
	float fadeStartTime;           // When to start fading (seconds before removal)
	int damageOnImpact;            // Damage dealt on impact (0 = no damage)

	DebrisComponent() : lifetime(10.0f), timeRemaining(10.0f),
		bounceFactor(0.3f), fadeOut(qtrue), fadeStartTime(2.0f),
		damageOnImpact(0) {
		model[0] = '\0';
		material[0] = '\0';
	}
};

// Item Component - General item properties (combines with PickupComponent)
struct ItemComponent {
	char name[64];                // Item name
	char description[256];        // Item description
	int value;                    // Item value (for trading/selling)
	qboolean stackable;           // Whether item stacks in inventory
	int maxStack;                 // Maximum stack size
	qboolean consumable;          // Whether item is consumed on use
	char useSound[MAX_QPATH];     // Sound on use
	char icon[MAX_QPATH];         // Icon path

	ItemComponent() : value(0), stackable(qtrue), maxStack(99),
		consumable(qfalse) {
		name[0] = '\0';
		description[0] = '\0';
		useSound[0] = '\0';
		icon[0] = '\0';
	}
};

// Player Class Component - Class-based gameplay with unique abilities
enum class PlayerClassType : uint8_t {
	NONE = 0,
	SOLDIER,      // Heavy weapons specialist
	ENGINEER,     // Construction and explosives
	MEDIC,        // Healing and support
	FIELDOPS,     // Ammunition and artillery
	COVERTOPS     // Infiltration and reconnaissance
};

// Skill types for progression system
enum class SkillType : uint8_t {
	NONE = 0,
	LIGHT_WEAPONS,
	HEAVY_WEAPONS,
	FIRST_AID,
	EXPLOSIVES_AND_CONSTRUCTION,
	SIGNALS,
	BATTLE_SENSE,
	MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS,
	MAX_SKILLS
};

// Player Class Component
struct PlayerClassComponent {
	PlayerClassType classType;
	int team;                    // Team ID (0=red, 1=blue, etc.)
	char characterModel[MAX_QPATH];  // Character model path
	char icon[MAX_QPATH];        // Class icon
	char name[32];               // Class name
	qboolean canRevive;          // Can revive teammates
	qboolean canConstruct;       // Can construct/repair
	qboolean canDisguise;        // Can use disguise
	qboolean canCallArtillery;   // Can call airstrikes/artillery
	qboolean canDropAmmo;        // Can drop ammo packs
	float moveSpeedMultiplier;   // Movement speed modifier
	float healthBonus;           // Health bonus
	float armorBonus;            // Armor bonus

	PlayerClassComponent() : classType(PlayerClassType::NONE), team(0),
		canRevive(qfalse), canConstruct(qfalse), canDisguise(qfalse),
		canCallArtillery(qfalse), canDropAmmo(qfalse),
		moveSpeedMultiplier(1.0f), healthBonus(0.0f), armorBonus(0.0f) {
		characterModel[0] = '\0';
		icon[0] = '\0';
		name[0] = '\0';
	}
};

// Skill Component - XP and skill progression system
struct SkillComponent {
	int experiencePoints;        // Total XP earned
	int skillLevels[8];          // Level for each skill type (0-4)
	int skillXP[8];              // XP for each skill type
	int skillXPRequired[8];     // Required XP for next level
	qboolean skillsUnlocked[8];  // Whether skills are unlocked
	char skillNames[8][32];      // Skill names

	SkillComponent() : experiencePoints(0) {
		for (int i = 0; i < 8; i++) {
			skillLevels[i] = 0;
			skillXP[i] = 0;
			skillXPRequired[i] = 20; // Default: 20 XP for level 1
			skillsUnlocked[i] = qfalse;
			skillNames[i][0] = '\0';
		}
		// Default skill XP requirements (progressive)
		skillXPRequired[0] = 20;   // Level 1
		skillXPRequired[1] = 50;   // Level 2
		skillXPRequired[2] = 90;   // Level 3
		skillXPRequired[3] = 140;  // Level 4
	}
};

// Fireteam Component - Squad/team coordination system
struct FireteamComponent {
	int fireteamId;              // Unique fireteam ID
	int leaderId;                 // Entity ID of fireteam leader
	int memberIds[8];            // Entity IDs of members (max 8)
	int numMembers;               // Current member count
	char name[32];                // Fireteam name
	qboolean isPrivate;           // Private or public fireteam
	qboolean isLeader;            // Whether this entity is the leader
	int team;                     // Team ID

	FireteamComponent() : fireteamId(-1), leaderId(-1), numMembers(0),
		isPrivate(qfalse), isLeader(qfalse), team(0) {
		name[0] = '\0';
		for (int i = 0; i < 8; i++) {
			memberIds[i] = -1;
		}
	}
};

// Constraint types for rigid body joints
enum class ConstraintType : uint8_t {
	NONE = 0,
	POINT_TO_POINT,     // Ball joint - allows rotation around all axes
	HINGE,             // Hinge joint - allows rotation around one axis
	SLIDER,            // Prismatic joint - allows translation along one axis
	CONE_TWIST,        // Cone twist joint - allows rotation within a cone
	GENERIC_6DOF,      // 6 degrees of freedom joint
	FIXED,             // Fixed joint - no relative movement
	SPRING             // Spring constraint
};

// Constraint Component - Defines joints between rigid bodies
struct ConstraintComponent {
	ConstraintType type;
	int targetEntityId;       // Entity ID of the other body in the constraint
	vec3_t pivotA;           // Pivot point in body A's local space
	vec3_t pivotB;           // Pivot point in body B's local space
	vec3_t axisA;            // Primary axis in body A's local space
	vec3_t axisB;            // Primary axis in body B's local space

	// Constraint-specific parameters
	float lowerLimit;        // Lower angular/positional limit
	float upperLimit;        // Upper angular/positional limit
	float softness;          // Constraint softness (0 = rigid, 1 = soft)
	float biasFactor;        // Bias factor for constraint solving
	float relaxationFactor;  // Relaxation factor for constraint solving

#ifdef USE_BULLET
	btTypedConstraint *constraint;  // Bullet constraint object
#endif

	ConstraintComponent() : type(ConstraintType::NONE), targetEntityId(-1),
		lowerLimit(0.0f), upperLimit(0.0f), softness(0.0f),
		biasFactor(0.3f), relaxationFactor(1.0f)
#ifdef USE_BULLET
		, constraint(nullptr)
#endif
	{
		VectorClear(pivotA);
		VectorClear(pivotB);
		VectorClear(axisA);
		VectorClear(axisB);
	}
};

#endif // USE_ENTT

#endif // __ECS_COMPONENTS_H__

