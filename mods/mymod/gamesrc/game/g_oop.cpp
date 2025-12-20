/*
===========================================================================
Game Module OOP bridge implementation

Implements a C ABI surface for C code (g_spawn/g_utils) while hosting a
C++/EnTT-backed entity registry for new OOP gamecode. Default behavior falls
back to legacy spawn functions unless the g_oopEntities cvar is enabled and
a matching C++ EntityClass is registered.
===========================================================================
*/

#include "g_oop.h"
#include "g_oop.hpp"

#ifdef USE_ENTT

#include <unordered_map>
#include <string>
#include <cassert>

// Legacy spawn implementation we will wrap in the pilot OOP class.
extern "C" void SP_func_door( gentity_t *ent );

extern vmCvar_t g_oopEntities;
extern level_locals_t level;
extern gentity_t g_entities[MAX_GENTITIES];

namespace {

// Simple no-op pilot entity to prove out the OOP path without changing gameplay.
class NoOpEntity : public oop::BaseEntity {
public:
	using oop::BaseEntity::BaseEntity;
	void Spawn() override {
		// Mark that the legacy gentity is “owned” by OOP but do not alter behavior.
		// By default the gentity continues to function as before.
	}
};

// Adapter that reuses the existing C spawn function for func_door.
class FuncDoorEntity : public oop::BaseEntity {
public:
	using oop::BaseEntity::BaseEntity;
	void Spawn() override {
		SP_func_door( self );
	}
};

// Component bridging a gentity to a C++ BaseEntity instance
struct OOPComponent {
	oop::BaseEntity *instance = nullptr;
};

// Registry of classname -> EntityClass
std::unordered_map<std::string, oop::EntityClass> g_classRegistry;
// Owning instances keyed by EnTT entity
std::unordered_map<entt::entity, std::unique_ptr<oop::BaseEntity>> g_instances;
// Active OOP entities count for fast-pathing frame work
int g_oopEntityCount = 0;

bool g_enabled = false;
bool g_builtinsRegistered = false;

entt::registry *GetRegistry() {
	return reinterpret_cast<entt::registry *>( ECS_GetRegistry() );
}

entt::entity GetEnttForGentity( gentity_t *ent ) {
	ecs_entity_t ecs = G_ECS_GetEntityFromGentity( ent );
	if ( ecs == ECS_NULL_ENTITY ) {
		return entt::null;
	}
	return static_cast<entt::entity>( ecs );
}

bool ShouldEnable() {
	return ( g_oopEntities.integer != 0 ) && GetRegistry() != nullptr;
}

void RegisterBuiltins() {
	if ( g_builtinsRegistered ) {
		return;
	}

	// Register a no-op pilot class behind the cvar. It is safe because no stock
	// maps use this classname.
	oop::RegisterClass( oop::EntityClass{
		"class_oop_stub",
		[]( gentity_t *self, entt::entity ) -> std::unique_ptr<oop::BaseEntity> {
			return std::make_unique<NoOpEntity>( self );
		}
	} );

	// Pilot: route func_door through the OOP registry while reusing the legacy spawn.
	oop::RegisterClass( oop::EntityClass{
		"func_door",
		[]( gentity_t *self, entt::entity ) -> std::unique_ptr<oop::BaseEntity> {
			return std::make_unique<FuncDoorEntity>( self );
		}
	} );

	g_builtinsRegistered = true;
}

} // namespace

namespace oop {

void RegisterClass( const EntityClass &cls ) {
	if ( !cls.classname || !cls.factory ) {
		return;
	}
	g_classRegistry[ cls.classname ] = cls;
}

void ClearClasses() {
	g_classRegistry.clear();
}

bool HasClass( std::string_view classname ) {
	return g_classRegistry.find( std::string( classname ) ) != g_classRegistry.end();
}

} // namespace oop

qboolean G_OOP_Enabled( void ) {
	return g_enabled ? qtrue : qfalse;
}

int G_OOP_ActiveCount( void ) {
	return g_enabled ? g_oopEntityCount : 0;
}

void G_OOP_Init( void ) {
	if ( g_enabled ) {
		return;
	}

	g_enabled = ShouldEnable();
	if ( !g_enabled ) {
		return;
	}

	// Build registry for C++ classes
	oop::ClearClasses();
	RegisterBuiltins();
	g_oopEntityCount = 0;
}

void G_OOP_Shutdown( void ) {
	if ( !g_enabled ) {
		return;
	}

	// Destroy instances and remove OOP components
	if ( auto *registry = GetRegistry() ) {
		auto view = registry->view<OOPComponent>();
		for ( auto entity : view ) {
			registry->remove<OOPComponent>( entity );
		}
	}
	g_instances.clear();
	g_oopEntityCount = 0;
	g_enabled = false;
}

static oop::EntityClass *LookupClass( const char *classname ) {
	if ( !classname ) {
		return nullptr;
	}
	auto it = g_classRegistry.find( classname );
	if ( it == g_classRegistry.end() ) {
		return nullptr;
	}
	return &it->second;
}

qboolean G_OOP_CallSpawn( gentity_t *ent, const char *classname ) {
	if ( !ent ) {
		return qfalse;
	}

	if ( !g_enabled ) {
		G_OOP_Init();
	}
	if ( !g_enabled ) {
		return qfalse;
	}

	const char *name = classname ? classname : ent->classname;
	if ( !name ) {
		return qfalse;
	}

	oop::EntityClass *cls = LookupClass( name );
	if ( !cls || !cls->factory ) {
		return qfalse; // fall back to legacy spawn
	}

	entt::registry *registry = GetRegistry();
	assert( registry && "ECS registry must exist when OOP is enabled" );
	if ( !registry ) {
		return qfalse;
	}

	// Ensure an EnTT entity exists for this gentity
	ecs_entity_t ecsHandle = G_ECS_RegisterGentity( ent );
	if ( ecsHandle == ECS_NULL_ENTITY ) {
		return qfalse;
	}
	entt::entity e = static_cast<entt::entity>( ecsHandle );

	// Create the C++ instance
	std::unique_ptr<oop::BaseEntity> instance = cls->factory( ent, e );
	if ( !instance ) {
		return qfalse;
	}

	// Attach component and take ownership
	registry->emplace_or_replace<OOPComponent>( e, OOPComponent{ instance.get() } );
	g_instances[e] = std::move( instance );
	++g_oopEntityCount;

	// Call lifecycle hooks
	g_instances[e]->Precache();
	g_instances[e]->Spawn();

	return qtrue;
}

void G_OOP_OnFreeEntity( gentity_t *ent ) {
	if ( !g_enabled || !ent ) {
		return;
	}

	entt::entity e = GetEnttForGentity( ent );
	if ( e == entt::null ) {
		return;
	}

	if ( auto *registry = GetRegistry() ) {
		if ( registry->all_of<OOPComponent>( e ) ) {
			registry->remove<OOPComponent>( e );
			if ( g_oopEntityCount > 0 ) {
				--g_oopEntityCount;
			}
		}
	}
	assert( g_instances.find( e ) != g_instances.end() || "OOP instance map out of sync" );
	g_instances.erase( e );
	G_ECS_UnregisterGentity( ent );
}

void G_OOP_RunFrame( int msec ) {
	if ( !ShouldEnable() ) {
		if ( g_enabled ) {
			G_OOP_Shutdown();
		}
		return;
	}

	if ( !g_enabled ) {
		G_OOP_Init();
	}

	if ( !g_enabled ) {
		return;
	}

	entt::registry *registry = GetRegistry();
	if ( !registry || msec <= 0 || g_oopEntityCount == 0 ) {
		return;
	}

	const float dt = msec / 1000.0f;
	auto view = registry->view<OOPComponent>();

	for ( auto entity : view ) {
		auto instIt = g_instances.find( entity );
		assert( instIt != g_instances.end() && "OOP component without instance owner" );
		if ( instIt == g_instances.end() ) {
			continue;
		}
		oop::BaseEntity *obj = instIt->second.get();
		if ( !obj ) {
			continue;
		}

		const int nextThink = obj->NextThinkTime();
		if ( nextThink >= 0 && nextThink <= level.time ) {
			obj->ClearNextThink();
			obj->Think( dt );
		}
	}
}

#else // USE_ENTT

// Stubs when EnTT is disabled
qboolean G_OOP_Enabled( void ) { return qfalse; }
int G_OOP_ActiveCount( void ) { return 0; }
void G_OOP_Init( void ) {}
void G_OOP_Shutdown( void ) {}
qboolean G_OOP_CallSpawn( gentity_t *ent, const char *classname ) { (void)ent; (void)classname; return qfalse; }
void G_OOP_OnFreeEntity( gentity_t *ent ) { (void)ent; }
void G_OOP_RunFrame( int msec ) { (void)msec; }

#endif // USE_ENTT

