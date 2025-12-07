/*
===========================================================================
Game Module OOP types (C++ only)

Half-Life–style entity OOP model built on EnTT. This header is consumed by
new C++ gamecode and should not be included from C translation units.
===========================================================================
*/

#pragma once

#if defined(__cplusplus) && defined(USE_ENTT)

#include "g_local.h"
#include "g_ecs.h"
#include "../../../src/qcommon/ecs_components.h"
#include <entt/entt.hpp>
#include <functional>
#include <memory>
#include <string_view>

namespace oop {

struct DamageInfo {
	gentity_t *inflictor = nullptr;
	gentity_t *attacker = nullptr;
	int damage = 0;
	int mod = 0;
	vec3_t direction = { 0.0f, 0.0f, 0.0f };
};

class BaseEntity {
public:
	explicit BaseEntity( gentity_t *self ) : self( self ) {}
	virtual ~BaseEntity() = default;

	virtual void Precache() {}
	virtual void Spawn() {}
	virtual void Think( float deltaSeconds ) { (void)deltaSeconds; }
	virtual void ScheduleNextThink( int msecFromNow );
	virtual void Touch( gentity_t *other, trace_t *trace ) { (void)other; (void)trace; }
	virtual void Use( gentity_t *other, gentity_t *activator ) { (void)other; (void)activator; }
	virtual void TakeDamage( const DamageInfo &info ) { (void)info; }
	virtual void OnPain( const DamageInfo &info ) { (void)info; }
	virtual void OnDeath( const DamageInfo &info ) { (void)info; }
	virtual void Save( /* TODO: SaveWriter& */ ) {}
	virtual void Load( /* TODO: SaveReader& */ ) {}

	gentity_t *GetEntity() const { return self; }
	int NextThinkTime() const { return nextThinkTime; }
	void ClearNextThink() { nextThinkTime = -1; }

protected:
	gentity_t *self = nullptr;
	int nextThinkTime = -1; // server time in msec
};

inline void BaseEntity::ScheduleNextThink( int msecFromNow ) {
	if ( !self ) {
		return;
	}
	extern level_locals_t level;
	nextThinkTime = level.time + msecFromNow;
}

struct EntityClass {
	const char *classname;
	std::function<std::unique_ptr<BaseEntity>( gentity_t *self, entt::entity e )> factory;
	int flags = 0; // reserved for future caps/mixins
};

// Registration API for C++ gamecode
void RegisterClass( const EntityClass &cls );
void ClearClasses();
bool HasClass( std::string_view classname );

} // namespace oop

#endif // defined(__cplusplus) && defined(USE_ENTT)

