/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Lua scripting bindings implementation.
Exposes all engine subsystems to Lua game scripts via a clean
function registration API. Each system gets a Lua table with
methods matching the C API.

Usage from Lua:
  Engine.Director.getIntensity(playerNum)
  Engine.Nav.findPath(startX, startY, startZ, endX, endY, endZ)
  Engine.BT.createTree() / addNode(tree, type, {childIds}) / createAgent(tree)
  Engine.Physics.createBody({shape="box", mass=10, ...})
  Engine.Particles.emitSmoke(x, y, z, ...)
  Engine.Music.addLayer("music/combat.ogg", "action", 0.5, 1.0)
  Engine.Face.create(entityNum)
  Engine.Horde.spawn(x, y, z, health, speed)
  Engine.Dismember.applyDamage(handle, limb, damage, woundType)
  Engine.Choreo.play(sceneHandle)
  Engine.Response.trigger("enemy_spotted", context)
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "g_lua_bindings.h"

#ifdef USE_LUA
#include "lua_compat.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#ifdef USE_GAME_AI_MIDDLEWARE
#include "g_director.h"
#include "g_facial.h"
#include "g_horde.h"
#include "g_dismember.h"
#include "g_choreography.h"
#include "g_response.h"
#endif
#include "ecs.h"
#include "../physics/phys_bullet.h"
#include "../physics/phys_events.h"
#include "../physics/phys_materials.h"
#include "../physics/phys_props.h"
#include "../physics/phys_volumes.h"
#include "../world/fog_biology.h"
#include "../world/genetic_gan.h"
#ifdef USE_ARC_BLANC
#include "../world/arc_blanc/arc_blanc.h"
#endif
#include "../physics/phys_procedural_anim.h"
#include "../physics/phys_ik.h"
#include "../navigation/nav_recast.h"
#include "../client/cl_particles.h"
#include "../client/cl_engine_sprites.h"
#include "../client/cl_engine_decals.h"
#include "../client/core/cl_p2p_session.h"
#include "../physics/phys_character.h"
#include "../physics/phys_ragdoll_bind.h"
#include "../physics/phys_character.h"
#include "../physics/phys_dmm.h"
#include "../physics/phys_middleware.h"
#include "g_animgraph.h"
#include "../renderers/common/tr_public.h"
#include "../audio/snd_music_adaptive.h"

extern refexport_t re;

#ifdef USE_GAME_AI_MIDDLEWARE

/* ========== Director bindings ========== */

static int l_director_init(lua_State *L) { Director_Init(); return 0; }
static int l_director_update(lua_State *L) { Director_Update((float)luaL_checknumber(L, 1)); return 0; }
static int l_director_getPhase(lua_State *L) { lua_pushinteger(L, Director_GetPhase()); return 1; }
static int l_director_forcePhase(lua_State *L) { Director_ForcePhase((dirPhase_t)luaL_checkinteger(L, 1)); return 0; }
static int l_director_getIntensity(lua_State *L) { lua_pushnumber(L, Director_GetGlobalIntensity()); return 1; }
static int l_director_getPlayerIntensity(lua_State *L) { lua_pushnumber(L, Director_GetPlayerIntensity((int)luaL_checkinteger(L, 1))); return 1; }
static int l_director_getPlayerStress(lua_State *L) { lua_pushnumber(L, Director_GetPlayerStress((int)luaL_checkinteger(L, 1))); return 1; }
static int l_director_triggerWave(lua_State *L) { Director_TriggerWave((float)luaL_checknumber(L, 1)); return 0; }
static int l_director_updatePlayer(lua_State *L) {
	int cn = (int)luaL_checkinteger(L, 1);
	vec3_t pos; pos[0]=(float)luaL_checknumber(L,2); pos[1]=(float)luaL_checknumber(L,3); pos[2]=(float)luaL_checknumber(L,4);
	Director_UpdatePlayer(cn, pos, (float)luaL_checknumber(L,5), (float)luaL_checknumber(L,6), lua_toboolean(L,7));
	return 0;
}
static int l_director_playerKill(lua_State *L) { Director_PlayerKill((int)luaL_checkinteger(L,1)); return 0; }
static int l_director_playerDeath(lua_State *L) { Director_PlayerDeath((int)luaL_checkinteger(L,1)); return 0; }
static int l_director_playerDamage(lua_State *L) { Director_PlayerDamage((int)luaL_checkinteger(L,1),(float)luaL_checknumber(L,2)); return 0; }
static int l_director_addSpawnType(lua_State *L) {
	lua_pushinteger(L, Director_AddSpawnType(luaL_checkstring(L,1),(int)luaL_checkinteger(L,2),
		(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(float)luaL_checknumber(L,5),(float)luaL_checknumber(L,6)));
	return 1;
}
static int l_director_shouldSpawn(lua_State *L) { lua_pushboolean(L, Director_ShouldSpawn((int)luaL_checkinteger(L,1))); return 1; }
static int l_director_pickSpawnType(lua_State *L) { lua_pushinteger(L, Director_PickSpawnType()); return 1; }
static int l_director_addZone(lua_State *L) {
	vec3_t mn,mx;
	mn[0]=(float)luaL_checknumber(L,2); mn[1]=(float)luaL_checknumber(L,3); mn[2]=(float)luaL_checknumber(L,4);
	mx[0]=(float)luaL_checknumber(L,5); mx[1]=(float)luaL_checknumber(L,6); mx[2]=(float)luaL_checknumber(L,7);
	lua_pushinteger(L, Director_AddZone(luaL_checkstring(L,1),mn,mx,(dirThreat_t)luaL_checkinteger(L,8),(float)luaL_checknumber(L,9)));
	return 1;
}

/* ========== Navigation bindings ========== */

static int l_nav_init(lua_State *L) { Nav_Init(); return 0; }
static int l_nav_buildFromBSP(lua_State *L) { lua_pushinteger(L, Nav_BuildFromBSP(luaL_checkstring(L,1), NULL)); return 1; }
static int l_nav_findPath(lua_State *L) {
	vec3_t s,e; navPath_t path;
	s[0]=(float)luaL_checknumber(L,2); s[1]=(float)luaL_checknumber(L,3); s[2]=(float)luaL_checknumber(L,4);
	e[0]=(float)luaL_checknumber(L,5); e[1]=(float)luaL_checknumber(L,6); e[2]=(float)luaL_checknumber(L,7);
	if (Nav_FindPath((int)luaL_checkinteger(L,1), s, e, &path) && path.valid) {
		lua_newtable(L);
		int i;
		for (i = 0; i < path.numPoints; i++) {
			lua_newtable(L);
			lua_pushnumber(L,path.points[i][0]); lua_rawseti(L,-2,1);
			lua_pushnumber(L,path.points[i][1]); lua_rawseti(L,-2,2);
			lua_pushnumber(L,path.points[i][2]); lua_rawseti(L,-2,3);
			lua_rawseti(L,-2,i+1);
		}
		return 1;
	}
	lua_pushnil(L); return 1;
}
static int l_nav_addAgent(lua_State *L) {
	vec3_t pos; navAgentParams_t p;
	pos[0]=(float)luaL_checknumber(L,1); pos[1]=(float)luaL_checknumber(L,2); pos[2]=(float)luaL_checknumber(L,3);
	Com_Memset(&p,0,sizeof(p)); p.radius=0.6f; p.height=2.0f; p.maxSpeed=3.5f; p.maxAcceleration=8.0f;
	lua_pushinteger(L, Nav_AddAgent(0, pos, &p));
	return 1;
}

/* ========== Physics bindings ========== */

static qboolean Lua_GetVec3Field( lua_State *L, int index, const char *field, vec3_t out )
{
	int absIndex;

	if ( !lua_istable( L, index ) ) {
		return qfalse;
	}

	absIndex = ID3_LUA_ABSINDEX( L, index );
	lua_getfield( L, absIndex, field );
	if ( !lua_istable( L, -1 ) ) {
		lua_pop( L, 1 );
		return qfalse;
	}

	lua_rawgeti( L, -1, 1 );
	out[0] = (float)luaL_optnumber( L, -1, 0.0 );
	lua_pop( L, 1 );
	lua_rawgeti( L, -1, 2 );
	out[1] = (float)luaL_optnumber( L, -1, 0.0 );
	lua_pop( L, 1 );
	lua_rawgeti( L, -1, 3 );
	out[2] = (float)luaL_optnumber( L, -1, 0.0 );
	lua_pop( L, 1 );

	lua_pop( L, 1 );
	return qtrue;
}

static float Lua_GetNumberField( lua_State *L, int index, const char *field, float defaultValue )
{
	float value;
	int absIndex = ID3_LUA_ABSINDEX( L, index );

	lua_getfield( L, absIndex, field );
	value = (float)luaL_optnumber( L, -1, defaultValue );
	lua_pop( L, 1 );
	return value;
}

static int Lua_GetIntegerField( lua_State *L, int index, const char *field, int defaultValue )
{
	int value;
	int absIndex = ID3_LUA_ABSINDEX( L, index );

	lua_getfield( L, absIndex, field );
	value = (int)luaL_optinteger( L, -1, defaultValue );
	lua_pop( L, 1 );
	return value;
}

static qboolean Lua_GetBooleanField( lua_State *L, int index, const char *field, qboolean defaultValue )
{
	qboolean value = defaultValue;
	int absIndex = ID3_LUA_ABSINDEX( L, index );

	lua_getfield( L, absIndex, field );
	if ( !lua_isnoneornil( L, -1 ) ) {
		value = lua_toboolean( L, -1 ) ? qtrue : qfalse;
	}
	lua_pop( L, 1 );
	return value;
}

static physShape_t Lua_ParsePhysShape( const char *shapeName )
{
	if ( !shapeName || !shapeName[0] || !Q_stricmp( shapeName, "box" ) ) {
		return PHYS_SHAPE_BOX;
	}
	if ( !Q_stricmp( shapeName, "sphere" ) ) {
		return PHYS_SHAPE_SPHERE;
	}
	if ( !Q_stricmp( shapeName, "capsule" ) ) {
		return PHYS_SHAPE_CAPSULE;
	}
	if ( !Q_stricmp( shapeName, "cylinder" ) ) {
		return PHYS_SHAPE_CYLINDER;
	}
	if ( !Q_stricmp( shapeName, "hull" ) || !Q_stricmp( shapeName, "convexHull" ) || !Q_stricmp( shapeName, "convex_hull" ) ) {
		return PHYS_SHAPE_CONVEX_HULL;
	}
	return PHYS_SHAPE_BOX;
}

static physBodyType_t Lua_ParsePhysBodyType( const char *typeName )
{
	if ( !typeName || !typeName[0] || !Q_stricmp( typeName, "dynamic" ) ) {
		return PHYS_BODY_DYNAMIC;
	}
	if ( !Q_stricmp( typeName, "static" ) ) {
		return PHYS_BODY_STATIC;
	}
	if ( !Q_stricmp( typeName, "kinematic" ) ) {
		return PHYS_BODY_KINEMATIC;
	}
	return PHYS_BODY_DYNAMIC;
}

static void Lua_ParsePhysBodyDefTable( lua_State *L, int index, physBodyDef_t *def, float **tempHullPointsOut )
{
	vec3_t v;
	const char *shapeName;
	const char *typeName;
	int absIndex;

	absIndex = ID3_LUA_ABSINDEX( L, index );
	shapeName = NULL;
	typeName = NULL;

	if ( tempHullPointsOut ) {
		*tempHullPointsOut = NULL;
	}

	lua_getfield( L, absIndex, "shape" );
	shapeName = lua_isstring( L, -1 ) ? lua_tostring( L, -1 ) : NULL;
	def->shape = Lua_ParsePhysShape( shapeName );
	lua_pop( L, 1 );

	lua_getfield( L, absIndex, "type" );
	typeName = lua_isstring( L, -1 ) ? lua_tostring( L, -1 ) : NULL;
	def->type = Lua_ParsePhysBodyType( typeName );
	lua_pop( L, 1 );

	if ( Lua_GetVec3Field( L, absIndex, "position", v ) ) {
		VectorCopy( v, def->position );
	} else {
		def->position[0] = Lua_GetNumberField( L, absIndex, "x", 0.0f );
		def->position[1] = Lua_GetNumberField( L, absIndex, "y", 0.0f );
		def->position[2] = Lua_GetNumberField( L, absIndex, "z", 0.0f );
	}

	if ( Lua_GetVec3Field( L, absIndex, "rotation", v ) || Lua_GetVec3Field( L, absIndex, "angles", v ) ) {
		VectorCopy( v, def->rotation );
	} else {
		def->rotation[0] = Lua_GetNumberField( L, absIndex, "pitch", 0.0f );
		def->rotation[1] = Lua_GetNumberField( L, absIndex, "yaw", 0.0f );
		def->rotation[2] = Lua_GetNumberField( L, absIndex, "roll", 0.0f );
	}

	if ( Lua_GetVec3Field( L, absIndex, "halfExtents", v ) || Lua_GetVec3Field( L, absIndex, "extents", v ) ) {
		VectorCopy( v, def->halfExtents );
	} else {
		float size = Lua_GetNumberField( L, absIndex, "size", 8.0f );
		def->halfExtents[0] = Lua_GetNumberField( L, absIndex, "hx", size );
		def->halfExtents[1] = Lua_GetNumberField( L, absIndex, "hy", size );
		def->halfExtents[2] = Lua_GetNumberField( L, absIndex, "hz", size );
	}

	def->radius = Lua_GetNumberField( L, absIndex, "radius", 8.0f );
	def->height = Lua_GetNumberField( L, absIndex, "height", 32.0f );
	def->mass = Lua_GetNumberField( L, absIndex, "mass", def->type == PHYS_BODY_DYNAMIC ? 10.0f : 0.0f );
	def->friction = Lua_GetNumberField( L, absIndex, "friction", 0.5f );
	def->restitution = Lua_GetNumberField( L, absIndex, "restitution", 0.3f );
	def->linearDamping = Lua_GetNumberField( L, absIndex, "linearDamping", 0.0f );
	def->angularDamping = Lua_GetNumberField( L, absIndex, "angularDamping", 0.0f );
	def->gravityScale = Lua_GetNumberField( L, absIndex, "gravityScale", 1.0f );
	def->motionLocks = Lua_GetIntegerField( L, absIndex, "motionLocks", 0 );
	def->isSensor = Lua_GetBooleanField( L, absIndex, "isSensor",
		Lua_GetBooleanField( L, absIndex, "sensor", qfalse ) );
	def->collisionGroup = Lua_GetIntegerField( L, absIndex, "collisionGroup", 1 );
	def->collisionMask = Lua_GetIntegerField( L, absIndex, "collisionMask", -1 );
	def->materialId = Lua_GetIntegerField( L, absIndex, "materialId", PHYS_MAT_DEFAULT );

	lua_getfield( L, absIndex, "material" );
	if ( lua_isstring( L, -1 ) ) {
		def->materialId = PhysMat_FindByName( lua_tostring( L, -1 ) );
	} else if ( lua_isnumber( L, -1 ) ) {
		def->materialId = (int)lua_tointeger( L, -1 );
	}
	lua_pop( L, 1 );

	if ( def->materialId >= 0 && def->materialId < PHYS_MAT_COUNT ) {
		PhysMat_ApplyToBodyDef( def, def->materialId );
	}

	lua_getfield( L, absIndex, "hullPoints" );
	if ( lua_istable( L, -1 ) ) {
		size_t rawCount = ID3_LUA_RAWLEN( L, -1 );
		int floatCount = (int)rawCount;
		int pointCount = floatCount / 3;
		float *pts;
		int i;

		if ( pointCount >= 4 && floatCount == pointCount * 3 ) {
			pts = (float *)Z_Malloc( (size_t)floatCount * sizeof( float ) );
			for ( i = 0; i < floatCount; i++ ) {
				lua_rawgeti( L, -1, i + 1 );
				pts[i] = (float)luaL_optnumber( L, -1, 0.0 );
				lua_pop( L, 1 );
			}
			def->shape = PHYS_SHAPE_CONVEX_HULL;
			def->hullPoints = pts;
			def->hullPointCount = pointCount;
			if ( tempHullPointsOut ) {
				*tempHullPointsOut = pts;
			}
		}
	}
	lua_pop( L, 1 );
}

static int l_phys_init(lua_State *L) { lua_pushboolean(L, Phys_Init()); return 1; }
static int l_phys_step(lua_State *L) { Phys_StepSimulation((float)luaL_checknumber(L,1)); return 0; }
static int l_phys_createBody(lua_State *L) {
	physBodyDef_t def;
	physBodyHandle_t handle;
	float *tempHullPoints = NULL;

	Com_Memset( &def, 0, sizeof( def ) );
	def.shape = PHYS_SHAPE_BOX;
	def.type = PHYS_BODY_DYNAMIC;
	def.mass = 10.0f;
	def.radius = 8.0f;
	def.height = 32.0f;
	def.gravityScale = 1.0f;
	def.halfExtents[0] = def.halfExtents[1] = def.halfExtents[2] = 8.0f;
	def.friction = 0.5f;
	def.restitution = 0.3f;
	def.collisionGroup = 1;
	def.collisionMask = -1;
	def.materialId = PHYS_MAT_DEFAULT;

	if ( lua_istable( L, 1 ) ) {
		Lua_ParsePhysBodyDefTable( L, 1, &def, &tempHullPoints );
	} else {
		def.position[0]=(float)luaL_checknumber(L,1); def.position[1]=(float)luaL_checknumber(L,2); def.position[2]=(float)luaL_checknumber(L,3);
		def.mass=(float)luaL_optnumber(L,4,10); def.halfExtents[0]=def.halfExtents[1]=def.halfExtents[2]=(float)luaL_optnumber(L,5,8);
	}

	handle = Phys_CreateBody( &def );
	if ( tempHullPoints ) {
		Z_Free( tempHullPoints );
	}
	lua_pushinteger( L, handle );
	return 1;
}
static int l_phys_destroyBody(lua_State *L) { Phys_DestroyBody((int)luaL_checkinteger(L,1)); return 0; }
static int l_phys_applyImpulse(lua_State *L) {
	vec3_t imp,pt;
	imp[0]=(float)luaL_checknumber(L,2); imp[1]=(float)luaL_checknumber(L,3); imp[2]=(float)luaL_checknumber(L,4);
	pt[0]=(float)luaL_optnumber(L,5,0); pt[1]=(float)luaL_optnumber(L,6,0); pt[2]=(float)luaL_optnumber(L,7,0);
	Phys_ApplyImpulse((int)luaL_checkinteger(L,1), imp, pt);
	return 0;
}
static int l_phys_applyImpulseRadius(lua_State *L) {
	vec3_t center;
	center[0]=(float)luaL_checknumber(L,1); center[1]=(float)luaL_checknumber(L,2); center[2]=(float)luaL_checknumber(L,3);
	lua_pushinteger(L, Phys_ApplyImpulseRadius(center, (float)luaL_checknumber(L,4), (float)luaL_checknumber(L,5),
		(float)luaL_optnumber(L,6,1.0)));
	return 1;
}
static int l_phys_createBox(lua_State *L) {
	vec3_t origin, half;
	origin[0]=(float)luaL_checknumber(L,1); origin[1]=(float)luaL_checknumber(L,2); origin[2]=(float)luaL_checknumber(L,3);
	half[0]=half[1]=half[2]=(float)luaL_optnumber(L,4,8);
	lua_pushinteger(L, PhysProp_CreateBox(origin, half, PHYS_BODY_DYNAMIC, (float)luaL_optnumber(L,5,20), (int)luaL_optinteger(L,6,0)));
	return 1;
}
static int l_phys_createSphere(lua_State *L) {
	vec3_t origin;
	origin[0]=(float)luaL_checknumber(L,1); origin[1]=(float)luaL_checknumber(L,2); origin[2]=(float)luaL_checknumber(L,3);
	lua_pushinteger(L, PhysProp_CreateSphere(origin, (float)luaL_optnumber(L,4,10), PHYS_BODY_DYNAMIC,
		(float)luaL_optnumber(L,5,15), (int)luaL_optinteger(L,6,0)));
	return 1;
}
static int l_phys_createStatic(lua_State *L) {
	vec3_t origin, half;
	origin[0]=(float)luaL_checknumber(L,1); origin[1]=(float)luaL_checknumber(L,2); origin[2]=(float)luaL_checknumber(L,3);
	half[0]=(float)luaL_optnumber(L,4,32); half[1]=(float)luaL_optnumber(L,5,32); half[2]=(float)luaL_optnumber(L,6,4);
	lua_pushinteger(L, PhysProp_CreateBox(origin, half, PHYS_BODY_STATIC, 0.0f, (int)luaL_optinteger(L,7,0)));
	return 1;
}
static int l_phys_createShadow(lua_State *L) {
	physShadowDef_t def;
	Com_Memset(&def, 0, sizeof(def));
	def.origin[0]=(float)luaL_checknumber(L,1); def.origin[1]=(float)luaL_checknumber(L,2); def.origin[2]=(float)luaL_checknumber(L,3);
	def.halfExtents[0]=(float)luaL_optnumber(L,4,16); def.halfExtents[1]=(float)luaL_optnumber(L,5,16); def.halfExtents[2]=(float)luaL_optnumber(L,6,32);
	def.shape = PHYS_SHAPE_BOX;
	def.entityNum = (int)luaL_optinteger(L,7,-1);
	def.allowMovement = qtrue;
	def.allowRotation = qtrue;
	lua_pushinteger(L, PhysProp_CreateShadow(&def));
	return 1;
}
static int l_phys_setShadowPose(lua_State *L) {
	vec3_t origin, angles;
	origin[0]=(float)luaL_checknumber(L,2); origin[1]=(float)luaL_checknumber(L,3); origin[2]=(float)luaL_checknumber(L,4);
	angles[0]=(float)luaL_optnumber(L,5,0); angles[1]=(float)luaL_optnumber(L,6,0); angles[2]=(float)luaL_optnumber(L,7,0);
	PhysProp_SetShadowPose((int)luaL_checkinteger(L,1), origin, angles);
	return 0;
}
static int l_phys_createBuoyancy(lua_State *L) {
	physVolumeDef_t def;
	Com_Memset(&def, 0, sizeof(def));
	def.type = PHYS_VOLUME_BUOYANCY;
	def.center[0]=(float)luaL_checknumber(L,1); def.center[1]=(float)luaL_checknumber(L,2); def.center[2]=(float)luaL_checknumber(L,3);
	def.halfExtents[0]=(float)luaL_optnumber(L,4,128); def.halfExtents[1]=(float)luaL_optnumber(L,5,128); def.halfExtents[2]=(float)luaL_optnumber(L,6,48);
	def.density=(float)luaL_optnumber(L,7,1.0); def.linearDrag=(float)luaL_optnumber(L,8,0.4); def.angularDrag=(float)luaL_optnumber(L,9,0.2);
	lua_pushinteger(L, PhysVolume_Create(&def));
	return 1;
}
static int l_phys_getTransform(lua_State *L) {
	physTransform_t t; Phys_GetBodyTransform((int)luaL_checkinteger(L,1), &t);
	lua_pushnumber(L,t.position[0]); lua_pushnumber(L,t.position[1]); lua_pushnumber(L,t.position[2]);
	lua_pushnumber(L,t.rotation[0]); lua_pushnumber(L,t.rotation[1]); lua_pushnumber(L,t.rotation[2]);
	return 6;
}
static int l_phys_setGravity(lua_State *L) {
	vec3_t gravity;
	gravity[0] = (float)luaL_checknumber( L, 1 );
	gravity[1] = (float)luaL_checknumber( L, 2 );
	gravity[2] = (float)luaL_checknumber( L, 3 );
	Phys_SetGravity( gravity );
	return 0;
}
static int l_phys_setTransform(lua_State *L) {
	vec3_t pos, rot;
	pos[0]=(float)luaL_checknumber(L,2); pos[1]=(float)luaL_checknumber(L,3); pos[2]=(float)luaL_checknumber(L,4);
	rot[0]=(float)luaL_optnumber(L,5,0); rot[1]=(float)luaL_optnumber(L,6,0); rot[2]=(float)luaL_optnumber(L,7,0);
	Phys_SetBodyTransform((int)luaL_checkinteger(L,1), pos, rot);
	return 0;
}
static int l_phys_rayCast(lua_State *L) {
	vec3_t from, to;
	physRayResult_t hit;
	physQueryFilter_t filter;
	const physQueryFilter_t *filterPtr = NULL;
	from[0]=(float)luaL_checknumber(L,1); from[1]=(float)luaL_checknumber(L,2); from[2]=(float)luaL_checknumber(L,3);
	to[0]=(float)luaL_checknumber(L,4); to[1]=(float)luaL_checknumber(L,5); to[2]=(float)luaL_checknumber(L,6);
	if ( lua_gettop( L ) >= 8 ) {
		filter.categoryBits = (unsigned)luaL_checkinteger( L, 7 );
		filter.maskBits = (unsigned)luaL_checkinteger( L, 8 );
		filterPtr = &filter;
	}
	if ( !Phys_RayCastFiltered( from, to, &hit, filterPtr ) || !hit.hit ) {
		lua_pushboolean(L, 0);
		return 1;
	}
	lua_pushboolean(L, 1);
	lua_pushnumber(L, hit.hitPoint[0]); lua_pushnumber(L, hit.hitPoint[1]); lua_pushnumber(L, hit.hitPoint[2]);
	lua_pushnumber(L, hit.fraction);
	lua_pushinteger(L, hit.body);
	lua_pushinteger(L, (lua_Integer)hit.userMaterialId);
	lua_pushinteger(L, hit.triangleIndex);
	return 7;
}
static int l_phys_createSensor(lua_State *L) {
	physBodyDef_t def;
	Com_Memset(&def, 0, sizeof(def));
	def.shape = PHYS_SHAPE_BOX;
	def.type = PHYS_BODY_STATIC;
	def.isSensor = qtrue;
	def.position[0]=(float)luaL_checknumber(L,1); def.position[1]=(float)luaL_checknumber(L,2); def.position[2]=(float)luaL_checknumber(L,3);
	def.halfExtents[0]=def.halfExtents[1]=def.halfExtents[2]=(float)luaL_optnumber(L,4,32);
	lua_pushinteger(L, Phys_CreateBody(&def));
	return 1;
}
static int l_phys_createConstraint(lua_State *L) {
	physConstraintDef_t def;
	const char *typeName;
	Com_Memset(&def, 0, sizeof(def));
	typeName = luaL_checkstring(L, 1);
	if ( !Q_stricmp( typeName, "hinge" ) ) {
		def.type = PHYS_CONSTRAINT_HINGE;
	} else if ( !Q_stricmp( typeName, "slider" ) ) {
		def.type = PHYS_CONSTRAINT_SLIDER;
	} else if ( !Q_stricmp( typeName, "distance" ) ) {
		def.type = PHYS_CONSTRAINT_DISTANCE;
	} else if ( !Q_stricmp( typeName, "wheel" ) ) {
		def.type = PHYS_CONSTRAINT_WHEEL;
	} else if ( !Q_stricmp( typeName, "motor" ) ) {
		def.type = PHYS_CONSTRAINT_MOTOR;
	} else if ( !Q_stricmp( typeName, "fixed" ) ) {
		def.type = PHYS_CONSTRAINT_FIXED;
	} else if ( !Q_stricmp( typeName, "filter" ) ) {
		def.type = PHYS_CONSTRAINT_FILTER;
	} else if ( !Q_stricmp( typeName, "parallel" ) ) {
		def.type = PHYS_CONSTRAINT_PARALLEL;
	} else if ( !Q_stricmp( typeName, "cone" ) || !Q_stricmp( typeName, "spherical" ) ) {
		def.type = PHYS_CONSTRAINT_CONE_TWIST;
	} else {
		def.type = PHYS_CONSTRAINT_POINT;
	}
	def.bodyA = (physBodyHandle_t)luaL_checkinteger(L, 2);
	def.bodyB = (physBodyHandle_t)luaL_checkinteger(L, 3);
	def.lowerLimit = (float)luaL_optnumber(L, 4, 0);
	def.upperLimit = (float)luaL_optnumber(L, 5, 0);
	def.coneAngle = (float)luaL_optnumber(L, 6, 0);
	def.springHertz = (float)luaL_optnumber(L, 7, 0);
	def.springDamping = (float)luaL_optnumber(L, 8, 0);
	def.axisA[2] = 1.0f;
	def.disableCollision = qtrue;
	lua_pushinteger(L, Phys_CreateConstraint(&def));
	return 1;
}
static int l_phys_moverStep(lua_State *L) {
	vec3_t origin, velocity, wish;
	qboolean grounded = qfalse;
	origin[0]=(float)luaL_checknumber(L,1); origin[1]=(float)luaL_checknumber(L,2); origin[2]=(float)luaL_checknumber(L,3);
	velocity[0]=(float)luaL_checknumber(L,4); velocity[1]=(float)luaL_checknumber(L,5); velocity[2]=(float)luaL_checknumber(L,6);
	wish[0]=(float)luaL_optnumber(L,7,0); wish[1]=(float)luaL_optnumber(L,8,0); wish[2]=(float)luaL_optnumber(L,9,0);
	lua_pushboolean(L, Phys_MoverStep(origin, velocity, (float)luaL_optnumber(L,10,15), (float)luaL_optnumber(L,11,56),
		wish, (float)luaL_optnumber(L,12,320), (float)luaL_optnumber(L,13,0.016), lua_toboolean(L,14), &grounded));
	lua_pushnumber(L, origin[0]); lua_pushnumber(L, origin[1]); lua_pushnumber(L, origin[2]);
	lua_pushnumber(L, velocity[0]); lua_pushnumber(L, velocity[1]); lua_pushnumber(L, velocity[2]);
	lua_pushboolean(L, grounded);
	return 8;
}
static int l_phys_pmoveCorrect(lua_State *L) {
	vec3_t origin, velocity;
	origin[0]=(float)luaL_checknumber(L,1); origin[1]=(float)luaL_checknumber(L,2); origin[2]=(float)luaL_checknumber(L,3);
	velocity[0]=(float)luaL_checknumber(L,4); velocity[1]=(float)luaL_checknumber(L,5); velocity[2]=(float)luaL_checknumber(L,6);
	lua_pushinteger(L, Phys_PmoveCorrect(origin, velocity, (float)luaL_optnumber(L,7,15),
		(float)luaL_optnumber(L,8,56), (float)luaL_optnumber(L,9,0.016)));
	lua_pushnumber(L, origin[0]); lua_pushnumber(L, origin[1]); lua_pushnumber(L, origin[2]);
	lua_pushnumber(L, velocity[0]); lua_pushnumber(L, velocity[1]); lua_pushnumber(L, velocity[2]);
	return 7;
}
static int l_phys_addHeightField(lua_State *L) {
	/* Lua: heights table flat row-major, countX, countY, cellSize, origin */
	float heights[256];
	vec3_t origin;
	int countX, countY, n, i;
	countX = (int)luaL_checkinteger(L, 2);
	countY = (int)luaL_checkinteger(L, 3);
	n = countX * countY;
	if ( n < 4 || n > 256 || !lua_istable(L, 1) ) {
		lua_pushinteger(L, -1);
		return 1;
	}
	for ( i = 0; i < n; i++ ) {
		lua_rawgeti(L, 1, i + 1);
		heights[i] = (float)luaL_optnumber(L, -1, 0);
		lua_pop(L, 1);
	}
	origin[0]=(float)luaL_optnumber(L,5,0); origin[1]=(float)luaL_optnumber(L,6,0); origin[2]=(float)luaL_optnumber(L,7,0);
	lua_pushinteger(L, Phys_AddStaticHeightField(heights, countX, countY, (float)luaL_optnumber(L,4,32), 1.0f, origin));
	return 1;
}
static int l_phys_backend(lua_State *L) {
	lua_pushstring(L, Phys_GetBackendName());
	return 1;
}
static int l_phys_createRagdoll(lua_State *L) {
	physBoundRagdoll_t bound;
	vec3_t origin;
	const char *path = luaL_optstring(L, 1, NULL);
	origin[0] = (float)luaL_optnumber(L, 2, 0);
	origin[1] = (float)luaL_optnumber(L, 3, 0);
	origin[2] = (float)luaL_optnumber(L, 4, 64);
	if ( !Phys_RagdollSpawnBound( path, origin, &bound ) ) {
		lua_pushinteger(L, -1);
		return 1;
	}
	lua_pushinteger(L, bound.ragdoll);
	lua_pushinteger(L, bound.anim);
	lua_pushinteger(L, bound.motor);
	return 3;
}
static int l_phys_loadRagdoll(lua_State *L) {
	physRagdollDef_t def;
	lua_pushboolean(L, Phys_RagdollLoadDef(luaL_checkstring(L, 1), &def));
	lua_pushinteger(L, def.numBones);
	return 2;
}
static int l_phys_setBoneAnimTarget(lua_State *L) {
	vec3_t pos, rot;
	pos[0]=(float)luaL_checknumber(L,3); pos[1]=(float)luaL_checknumber(L,4); pos[2]=(float)luaL_checknumber(L,5);
	rot[0]=(float)luaL_optnumber(L,6,0); rot[1]=(float)luaL_optnumber(L,7,0); rot[2]=(float)luaL_optnumber(L,8,0);
	Phys_RagdollSetBoneAnimTarget((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2), pos, rot);
	return 0;
}
static int l_phys_subscribe(lua_State *L) {
	const char *typeName = luaL_optstring(L, 1, "impact");
	Com_Printf( "[physics] Engine.Physics.subscribe(%s) — use pollEvent() for Soft Step bus\n", typeName );
	lua_pushboolean(L, 1);
	return 1;
}
static int l_phys_setFriction(lua_State *L) {
	Phys_SetBodyFriction( (physBodyHandle_t)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_phys_setRestitution(lua_State *L) {
	Phys_SetBodyRestitution( (physBodyHandle_t)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_phys_validateReplay(lua_State *L) {
	lua_pushboolean( L, Phys_ValidateReplay( luaL_optstring( L, 1, "phys_recording.bin" ) ) );
	return 1;
}
static int l_phys_setFilter(lua_State *L) {
	Phys_SetBodyFilterEx( (physBodyHandle_t)luaL_checkinteger( L, 1 ),
		(int)luaL_checkinteger( L, 2 ), (int)luaL_checkinteger( L, 3 ),
		(int)luaL_optinteger( L, 4, 0 ) );
	return 0;
}
static int l_phys_attachShape(lua_State *L) {
	physBodyDef_t def;
	Com_Memset( &def, 0, sizeof( def ) );
	def.shape = PHYS_SHAPE_BOX;
	def.halfExtents[0] = def.halfExtents[1] = def.halfExtents[2] = (float)luaL_optnumber( L, 2, 8 );
	if ( lua_gettop( L ) >= 4 ) {
		def.halfExtents[0] = (float)luaL_checknumber( L, 2 );
		def.halfExtents[1] = (float)luaL_checknumber( L, 3 );
		def.halfExtents[2] = (float)luaL_checknumber( L, 4 );
	}
	lua_pushinteger( L, Phys_AttachShape( (physBodyHandle_t)luaL_checkinteger( L, 1 ), &def ) );
	return 1;
}
static int l_phys_convexSweep(lua_State *L) {
	physBodyDef_t def;
	physRayResult_t hit;
	vec3_t from, to, rot;
	Com_Memset( &def, 0, sizeof( def ) );
	def.shape = PHYS_SHAPE_SPHERE;
	def.radius = (float)luaL_optnumber( L, 7, 8 );
	from[0]=(float)luaL_checknumber(L,1); from[1]=(float)luaL_checknumber(L,2); from[2]=(float)luaL_checknumber(L,3);
	to[0]=(float)luaL_checknumber(L,4); to[1]=(float)luaL_checknumber(L,5); to[2]=(float)luaL_checknumber(L,6);
	VectorClear( rot );
	if ( !Phys_ConvexSweep( &def, from, to, rot, &hit ) || !hit.hit ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	lua_pushboolean( L, 1 );
	lua_pushnumber( L, hit.fraction );
	lua_pushinteger( L, hit.body );
	lua_pushnumber( L, hit.hitPoint[0] ); lua_pushnumber( L, hit.hitPoint[1] ); lua_pushnumber( L, hit.hitPoint[2] );
	return 6;
}
static int l_phys_overlapSphere(lua_State *L) {
	vec3_t center;
	physBodyHandle_t hits[32];
	int n, i;
	center[0]=(float)luaL_checknumber(L,1); center[1]=(float)luaL_checknumber(L,2); center[2]=(float)luaL_checknumber(L,3);
	n = Phys_OverlapSphere( center, (float)luaL_optnumber( L, 4, 32 ), hits, 32 );
	lua_createtable( L, n, 0 );
	for ( i = 0; i < n; i++ ) {
		lua_pushinteger( L, hits[i] );
		lua_rawseti( L, -2, i + 1 );
	}
	return 1;
}
static int l_phys_overlapBox(lua_State *L) {
	vec3_t center, halfExtents;
	physBodyHandle_t hits[32];
	int n, i;
	center[0]=(float)luaL_checknumber(L,1); center[1]=(float)luaL_checknumber(L,2); center[2]=(float)luaL_checknumber(L,3);
	halfExtents[0]=(float)luaL_optnumber(L,4,32); halfExtents[1]=(float)luaL_optnumber(L,5,32); halfExtents[2]=(float)luaL_optnumber(L,6,32);
	n = Phys_OverlapBox( center, halfExtents, hits, 32 );
	lua_createtable( L, n, 0 );
	for ( i = 0; i < n; i++ ) {
		lua_pushinteger( L, hits[i] );
		lua_rawseti( L, -2, i + 1 );
	}
	return 1;
}
static int l_phys_getContacts(lua_State *L) {
	physContact_t contacts[16];
	int n, i;
	n = Phys_GetBodyContacts( (physBodyHandle_t)luaL_checkinteger( L, 1 ), contacts, 16 );
	lua_createtable( L, n, 0 );
	for ( i = 0; i < n; i++ ) {
		lua_createtable( L, 0, 5 );
		lua_pushinteger( L, contacts[i].otherBody ); lua_setfield( L, -2, "other" );
		lua_pushnumber( L, contacts[i].point[0] ); lua_setfield( L, -2, "x" );
		lua_pushnumber( L, contacts[i].point[1] ); lua_setfield( L, -2, "y" );
		lua_pushnumber( L, contacts[i].point[2] ); lua_setfield( L, -2, "z" );
		lua_pushnumber( L, contacts[i].normalImpulse ); lua_setfield( L, -2, "impulse" );
		lua_rawseti( L, -2, i + 1 );
	}
	return 1;
}
static int l_phys_setConstraintSpring(lua_State *L) {
	Phys_SetConstraintSpring( (physConstraintHandle_t)luaL_checkinteger( L, 1 ),
		lua_toboolean( L, 2 ) ? qtrue : qfalse,
		(float)luaL_optnumber( L, 3, 8 ), (float)luaL_optnumber( L, 4, 0.7 ) );
	return 0;
}
static int l_phys_setSphericalLimits(lua_State *L) {
	Phys_SetSphericalLimits( (physConstraintHandle_t)luaL_checkinteger( L, 1 ),
		(float)luaL_checknumber( L, 2 ), (float)luaL_optnumber( L, 3, -1 ), (float)luaL_optnumber( L, 4, 1 ) );
	return 0;
}
static int l_phys_setWheelSteering(lua_State *L) {
	Phys_SetWheelSteering( (physConstraintHandle_t)luaL_checkinteger( L, 1 ),
		(float)luaL_checknumber( L, 2 ), (float)luaL_optnumber( L, 3, 500 ) );
	return 0;
}
static int l_phys_pollEvent(lua_State *L) {
	phys_event_t ev;
	if ( !PhysEvent_Poll( &ev ) ) {
		lua_pushnil( L );
		return 1;
	}
	lua_createtable( L, 0, 10 );
	lua_pushinteger( L, (int)ev.type ); lua_setfield( L, -2, "type" );
	lua_pushinteger( L, ev.bodyA ); lua_setfield( L, -2, "bodyA" );
	lua_pushinteger( L, ev.bodyB ); lua_setfield( L, -2, "bodyB" );
	lua_pushinteger( L, ev.ragdoll ); lua_setfield( L, -2, "ragdoll" );
	lua_pushinteger( L, ev.bone ); lua_setfield( L, -2, "bone" );
	lua_pushnumber( L, ev.point[0] ); lua_setfield( L, -2, "x" );
	lua_pushnumber( L, ev.point[1] ); lua_setfield( L, -2, "y" );
	lua_pushnumber( L, ev.point[2] ); lua_setfield( L, -2, "z" );
	lua_pushnumber( L, ev.magnitude ); lua_setfield( L, -2, "magnitude" );
	return 1;
}
static int l_phys_forceAnimState(lua_State *L) {
	ProcAnim_ForceState( (procAnimHandle_t)luaL_checkinteger( L, 1 ),
		(procAnimState_t)luaL_checkinteger( L, 2 ) );
	return 0;
}
static int l_phys_hitRagdoll(lua_State *L) {
	vec3_t point, impulse;
	procAnimHandle_t anim = (procAnimHandle_t)luaL_checkinteger( L, 1 );
	physMotorHandle_t motor = (physMotorHandle_t)luaL_optinteger( L, 2, -1 );
	int bone = (int)luaL_optinteger( L, 3, PROCANIM_BONE_SPINE );
	impulse[0] = (float)luaL_optnumber( L, 4, 200 );
	impulse[1] = (float)luaL_optnumber( L, 5, 0 );
	impulse[2] = (float)luaL_optnumber( L, 6, 100 );
	point[0] = (float)luaL_optnumber( L, 7, 0 );
	point[1] = (float)luaL_optnumber( L, 8, 0 );
	point[2] = (float)luaL_optnumber( L, 9, 0 );
	PhysMiddleware_DispatchHit( -1, anim, motor, bone, 0, point, impulse );
	return 0;
}
static int l_phys_createDmm(lua_State *L) {
	dmmObjectDef_t def;
	dmmFracturePattern_t pattern;
	dmmObjectHandle_t h;
	float edge;
	Com_Memset( &def, 0, sizeof( def ) );
	def.position[0] = (float)luaL_checknumber( L, 1 );
	def.position[1] = (float)luaL_checknumber( L, 2 );
	def.position[2] = (float)luaL_checknumber( L, 3 );
	edge = (float)luaL_optnumber( L, 4, 48 );
	VectorSet( def.dimensions, edge, edge, edge );
	def.material = DMM_CONCRETE;
	def.density = (float)luaL_optnumber( L, 5, 2.4 );
	def.gridResolution = (int)luaL_optinteger( L, 6, 6 );
	def.deformability = 1.0f;
	Dmm_GenerateVoronoiPattern( def.position, edge * 0.5f, 8, &pattern );
	h = Dmm_CreateEnhanced( &def, &pattern );
	lua_pushinteger( L, h );
	return 1;
}
static int l_phys_fractureDmm(lua_State *L) {
	vec3_t point;
	int n;
	point[0] = (float)luaL_optnumber( L, 2, 0 );
	point[1] = (float)luaL_optnumber( L, 3, 0 );
	point[2] = (float)luaL_optnumber( L, 4, 0 );
	n = Dmm_Fracture( (dmmObjectHandle_t)luaL_checkinteger( L, 1 ), point,
		(float)luaL_optnumber( L, 5, 800 ) );
	lua_pushinteger( L, n );
	return 1;
}
static int l_phys_dmmStatus(lua_State *L) {
	lua_pushinteger( L, Dmm_GetActiveCount() );
	return 1;
}
static int l_phys_spawnBoundAlive(lua_State *L) {
	physBoundRagdoll_t bound;
	vec3_t origin;
	const char *path = luaL_optstring( L, 1, NULL );
	origin[0] = (float)luaL_optnumber( L, 2, 0 );
	origin[1] = (float)luaL_optnumber( L, 3, 0 );
	origin[2] = (float)luaL_optnumber( L, 4, 64 );
	if ( !Phys_RagdollSpawnBoundEx( path, origin, &bound, qfalse ) ) {
		lua_pushinteger( L, -1 );
		return 1;
	}
	lua_pushinteger( L, bound.ragdoll );
	lua_pushinteger( L, bound.anim );
	lua_pushinteger( L, bound.motor );
	return 3;
}
static int l_phys_getClosestPoint(lua_State *L) {
	vec3_t target, closest;
	float dist;
	target[0] = (float)luaL_checknumber( L, 2 );
	target[1] = (float)luaL_checknumber( L, 3 );
	target[2] = (float)luaL_checknumber( L, 4 );
	if ( !Phys_GetClosestPoint( (physBodyHandle_t)luaL_checkinteger( L, 1 ), target, closest, &dist ) ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	lua_pushboolean( L, 1 );
	lua_pushnumber( L, closest[0] ); lua_pushnumber( L, closest[1] ); lua_pushnumber( L, closest[2] );
	lua_pushnumber( L, dist );
	return 5;
}
static int l_phys_sphereTOI(lua_State *L) {
	vec3_t from, to;
	physRayResult_t hit;
	from[0]=(float)luaL_checknumber(L,1); from[1]=(float)luaL_checknumber(L,2); from[2]=(float)luaL_checknumber(L,3);
	to[0]=(float)luaL_checknumber(L,4); to[1]=(float)luaL_checknumber(L,5); to[2]=(float)luaL_checknumber(L,6);
	if ( !Phys_SphereTimeOfImpact( from, to, (float)luaL_optnumber( L, 7, 8 ),
			(physBodyHandle_t)luaL_optinteger( L, 8, -1 ), &hit ) || !hit.hit ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	lua_pushboolean( L, 1 );
	lua_pushnumber( L, hit.fraction );
	lua_pushinteger( L, hit.body );
	lua_pushnumber( L, hit.hitPoint[0] ); lua_pushnumber( L, hit.hitPoint[1] ); lua_pushnumber( L, hit.hitPoint[2] );
	return 6;
}
static int l_phys_setContinuous(lua_State *L) {
	Phys_SetBodyContinuous( (physBodyHandle_t)luaL_checkinteger( L, 1 ), lua_toboolean( L, 2 ) ? qtrue : qfalse );
	return 0;
}
static int l_phys_setSleepEnabled(lua_State *L) {
	Phys_SetBodySleepEnabled( (physBodyHandle_t)luaL_checkinteger( L, 1 ), lua_toboolean( L, 2 ) ? qtrue : qfalse );
	return 0;
}
static int l_phys_setSleepThreshold(lua_State *L) {
	Phys_SetBodySleepThreshold( (physBodyHandle_t)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_phys_setHingeTarget(lua_State *L) {
	Phys_SetHingeTargetAngle( (physConstraintHandle_t)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_phys_setSliderTarget(lua_State *L) {
	Phys_SetSliderTarget( (physConstraintHandle_t)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_phys_setDistanceLength(lua_State *L) {
	Phys_SetDistanceLength( (physConstraintHandle_t)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_phys_rebuildTree(lua_State *L) {
	(void)L;
	Phys_RebuildStaticTree();
	return 0;
}
static int l_phys_setContactTuning(lua_State *L) {
	Phys_SetContactTuning( (float)luaL_checknumber( L, 1 ), (float)luaL_optnumber( L, 2, 0.7f ),
		(float)luaL_optnumber( L, 3, 0.0f ) );
	return 0;
}
static int l_phys_setMaxLinearSpeed(lua_State *L) {
	Phys_SetMaxLinearSpeed( (float)luaL_checknumber( L, 1 ) );
	return 0;
}
static int l_phys_enableSpeculative(lua_State *L) {
	Phys_EnableSpeculative( lua_toboolean( L, 1 ) ? qtrue : qfalse );
	return 0;
}
static int l_phys_setDebugDrawFlags(lua_State *L) {
	Phys_SetDebugDrawFlags( (unsigned)luaL_checkinteger( L, 1 ) );
	return 0;
}
static int l_phys_stats(lua_State *L) {
	lua_createtable( L, 0, 4 );
	lua_pushstring( L, Phys_GetBackendName() ); lua_setfield( L, -2, "backend" );
	lua_pushinteger( L, Phys_GetWorkerCount() ); lua_setfield( L, -2, "workers" );
	lua_pushinteger( L, Phys_GetBodyCount() ); lua_setfield( L, -2, "bodies" );
	lua_pushinteger( L, Phys_GetConstraintCount() ); lua_setfield( L, -2, "constraints" );
	return 1;
}
static int l_phys_replayOpen(lua_State *L) {
	lua_pushboolean( L, Phys_ReplayOpen( luaL_optstring( L, 1, "phys_recording.bin" ) ) );
	return 1;
}
static int l_phys_replayStep(lua_State *L) {
	lua_pushboolean( L, Phys_ReplayStep() );
	return 1;
}
static int l_phys_replaySeek(lua_State *L) {
	Phys_ReplaySeek( (int)luaL_checkinteger( L, 1 ) );
	return 0;
}
static int l_phys_replayClose(lua_State *L) {
	(void)L;
	Phys_ReplayClose();
	return 0;
}
static int l_phys_replayStatus(lua_State *L) {
	lua_pushboolean( L, Phys_ReplayIsOpen() );
	lua_pushinteger( L, Phys_ReplayGetFrame() );
	lua_pushinteger( L, Phys_ReplayGetFrameCount() );
	lua_pushboolean( L, Phys_ReplayHasDiverged() );
	return 4;
}

/* ========== Particles bindings ========== */

static int l_particles_init(lua_State *L) { Particles_Init(); return 0; }
static int l_particles_clear(lua_State *L) { Particles_Clear(); return 0; }
static int l_particles_emitSmoke(lua_State *L) {
	vec3_t org, dir;
	org[0]=(float)luaL_checknumber(L,1); org[1]=(float)luaL_checknumber(L,2); org[2]=(float)luaL_checknumber(L,3);
	VectorSet(dir,0,0,5);
	Particles_EmitSmoke(0, org, dir, (float)luaL_optnumber(L,4,2000), (float)luaL_optnumber(L,5,8),
		(float)luaL_optnumber(L,6,24), (float)luaL_optnumber(L,7,0.8), PC_SMOKE_GREY);
	return 0;
}
static int l_particles_emitSparks(lua_State *L) {
	vec3_t org; org[0]=(float)luaL_checknumber(L,1); org[1]=(float)luaL_checknumber(L,2); org[2]=(float)luaL_checknumber(L,3);
	Particles_EmitSparks(org, NULL, (int)luaL_optinteger(L,4,8), (float)luaL_optnumber(L,5,1), (float)luaL_optnumber(L,6,1000));
	return 0;
}
static int l_particles_count(lua_State *L) { lua_pushinteger(L, Particles_ActiveCount()); return 1; }

/* ========== Music bindings ========== */

static int l_music_init(lua_State *L) { Music_Init(); return 0; }
static int l_music_addLayer(lua_State *L) {
	lua_pushinteger(L, Music_AddLayer(luaL_checkstring(L,1), (musicLayerType_t)luaL_checkinteger(L,2),
		(float)luaL_checknumber(L,3), (float)luaL_checknumber(L,4), (float)luaL_optnumber(L,5,0.5)));
	return 1;
}
static int l_music_setIntensity(lua_State *L) { Music_SetIntensity((float)luaL_checknumber(L,1)); return 0; }
static int l_music_addStinger(lua_State *L) {
	lua_pushinteger(L, Music_AddStinger(luaL_checkstring(L,1),(float)luaL_checknumber(L,2),(float)luaL_checknumber(L,3),lua_toboolean(L,4)));
	return 1;
}
static int l_music_fadeToSilence(lua_State *L) { Music_FadeToSilence((float)luaL_checknumber(L,1)); return 0; }

/* ========== Face bindings ========== */

static int l_face_create(lua_State *L) { lua_pushinteger(L, Face_Create((int)luaL_checkinteger(L,1))); return 1; }
static int l_face_destroy(lua_State *L) { Face_Destroy((int)luaL_checkinteger(L,1)); return 0; }
static int l_face_setExpression(lua_State *L) {
	Face_SetExpression((int)luaL_checkinteger(L,1),(expressionId_t)luaL_checkinteger(L,2),(float)luaL_checknumber(L,3),(float)luaL_optnumber(L,4,0.3));
	return 0;
}
static int l_face_setFlex(lua_State *L) { Face_SetFlex((int)luaL_checkinteger(L,1),(flexControllerId_t)luaL_checkinteger(L,2),(float)luaL_checknumber(L,3)); return 0; }
static int l_face_setPhoneme(lua_State *L) { Face_SetPhoneme((int)luaL_checkinteger(L,1),(phonemeId_t)luaL_checkinteger(L,2),(float)luaL_optnumber(L,3,1.0)); return 0; }
static int l_face_setBlinkRate(lua_State *L) { Face_SetBlinkRate((int)luaL_checkinteger(L,1),(float)luaL_checknumber(L,2)); return 0; }
static int l_face_setAU(lua_State *L) {
	facsActionUnit_t au;
	if (lua_type(L, 2) == LUA_TSTRING) {
		au = Face_AUFromName(luaL_checkstring(L, 2));
	} else {
		au = (facsActionUnit_t)luaL_checkinteger(L, 2);
	}
	if (au >= FACS_AU_COUNT) {
		return luaL_error(L, "unknown FACS AU");
	}
	Face_SetAU((int)luaL_checkinteger(L, 1), au, (float)luaL_checknumber(L, 3));
	return 0;
}
static int l_face_setAUSide(lua_State *L) {
	facsActionUnit_t au;
	facsSide_t side;
	if (lua_type(L, 2) == LUA_TSTRING) {
		au = Face_AUFromName(luaL_checkstring(L, 2));
	} else {
		au = (facsActionUnit_t)luaL_checkinteger(L, 2);
	}
	if (au >= FACS_AU_COUNT) {
		return luaL_error(L, "unknown FACS AU");
	}
	side = (facsSide_t)luaL_checkinteger(L, 3);
	Face_SetAUSide((int)luaL_checkinteger(L, 1), au, side, (float)luaL_checknumber(L, 4));
	return 0;
}
static int l_face_getAU(lua_State *L) {
	facsActionUnit_t au;
	if (lua_type(L, 2) == LUA_TSTRING) {
		au = Face_AUFromName(luaL_checkstring(L, 2));
	} else {
		au = (facsActionUnit_t)luaL_checkinteger(L, 2);
	}
	lua_pushnumber(L, Face_GetAU((int)luaL_checkinteger(L, 1), au));
	return 1;
}
static int l_face_clearAUs(lua_State *L) { Face_ClearAUs((int)luaL_checkinteger(L,1)); return 0; }
static int l_face_auName(lua_State *L) {
	facsActionUnit_t au = (facsActionUnit_t)luaL_checkinteger(L, 1);
	lua_pushstring(L, Face_AUName(au));
	return 1;
}

/* ========== Horde bindings ========== */

static int l_horde_init(lua_State *L) { Horde_Init(); return 0; }
static int l_horde_spawn(lua_State *L) {
	vec3_t pos; pos[0]=(float)luaL_checknumber(L,1); pos[1]=(float)luaL_checknumber(L,2); pos[2]=(float)luaL_checknumber(L,3);
	lua_pushinteger(L, Horde_SpawnAgent(pos,(float)luaL_optnumber(L,4,100),(float)luaL_optnumber(L,5,150),(int)luaL_optinteger(L,6,-1)));
	return 1;
}
static int l_horde_kill(lua_State *L) { Horde_KillAgent((int)luaL_checkinteger(L,1)); return 0; }
static int l_horde_setTarget(lua_State *L) {
	vec3_t t; t[0]=(float)luaL_checknumber(L,2); t[1]=(float)luaL_checknumber(L,3); t[2]=(float)luaL_checknumber(L,4);
	Horde_SetTarget((int)luaL_checkinteger(L,1), t, (int)luaL_optinteger(L,5,-1));
	return 0;
}
static int l_horde_getState(lua_State *L) { lua_pushinteger(L, Horde_GetAgentState((int)luaL_checkinteger(L,1))); return 1; }
static int l_horde_getCount(lua_State *L) { lua_pushinteger(L, Horde_GetActiveCount()); return 1; }
static int l_horde_createGroup(lua_State *L) {
	vec3_t c; c[0]=(float)luaL_checknumber(L,1); c[1]=(float)luaL_checknumber(L,2); c[2]=(float)luaL_checknumber(L,3);
	lua_pushinteger(L, Horde_CreateGroup(c, (float)luaL_optnumber(L,4,200)));
	return 1;
}

/* ========== Dismember bindings ========== */

static int l_dismember_create(lua_State *L) { lua_pushinteger(L, Dismember_CreateInstance((int)luaL_checkinteger(L,1))); return 1; }
static int l_dismember_damage(lua_State *L) {
	vec3_t hp={0}, hd={0};
	Dismember_ApplyDamage((int)luaL_checkinteger(L,1),(limbId_t)luaL_checkinteger(L,2),(float)luaL_checknumber(L,3),(woundType_t)luaL_checkinteger(L,4),hp,hd);
	return 0;
}
static int l_dismember_sever(lua_State *L) {
	vec3_t f; f[0]=(float)luaL_optnumber(L,3,0); f[1]=(float)luaL_optnumber(L,4,100); f[2]=(float)luaL_optnumber(L,5,0);
	lua_pushboolean(L, Dismember_SeverLimb((int)luaL_checkinteger(L,1),(limbId_t)luaL_checkinteger(L,2),f));
	return 1;
}
static int l_dismember_explode(lua_State *L) {
	vec3_t o; o[0]=(float)luaL_checknumber(L,2); o[1]=(float)luaL_checknumber(L,3); o[2]=(float)luaL_checknumber(L,4);
	Dismember_Explode((int)luaL_checkinteger(L,1), o, (float)luaL_checknumber(L,5), (float)luaL_checknumber(L,6));
	return 0;
}
static int l_dismember_isAttached(lua_State *L) { lua_pushboolean(L, Dismember_IsLimbAttached((int)luaL_checkinteger(L,1),(limbId_t)luaL_checkinteger(L,2))); return 1; }

/* ========== Choreography bindings ========== */

static int l_choreo_create(lua_State *L) { lua_pushinteger(L, Choreo_CreateScene(luaL_checkstring(L,1))); return 1; }
static int l_choreo_addActor(lua_State *L) { lua_pushinteger(L, Choreo_AddActor((int)luaL_checkinteger(L,1),(int)luaL_checkinteger(L,2),luaL_checkstring(L,3))); return 1; }
static int l_choreo_addEvent(lua_State *L) {
	vec3_t pos={0};
	lua_pushinteger(L, Choreo_AddEvent((int)luaL_checkinteger(L,1),(choreoEventType_t)luaL_checkinteger(L,2),
		(float)luaL_checknumber(L,3),(float)luaL_checknumber(L,4),(int)luaL_checkinteger(L,5),luaL_optstring(L,6,""),pos));
	return 1;
}
static int l_choreo_play(lua_State *L) { Choreo_Play((int)luaL_checkinteger(L,1)); return 0; }
static int l_choreo_stop(lua_State *L) { Choreo_Stop((int)luaL_checkinteger(L,1)); return 0; }
static int l_choreo_isPlaying(lua_State *L) { lua_pushboolean(L, Choreo_IsPlaying((int)luaL_checkinteger(L,1))); return 1; }

/* ========== Response bindings ========== */

static int l_response_addRule(lua_State *L) { lua_pushinteger(L, Response_AddRule(luaL_checkstring(L,1),luaL_checkstring(L,2))); return 1; }
static int l_response_addCriteria(lua_State *L) {
	Response_AddCriteria((int)luaL_checkinteger(L,1),(responseCriteriaType_t)luaL_checkinteger(L,2),(float)luaL_optnumber(L,3,0),luaL_optstring(L,4,""));
	return 0;
}
static int l_response_addResponse(lua_State *L) {
	Response_AddResponse((int)luaL_checkinteger(L,1),luaL_checkstring(L,2),luaL_optstring(L,3,""),
		(float)luaL_optnumber(L,4,0),(float)luaL_optnumber(L,5,1));
	return 0;
}
static int l_response_trigger(lua_State *L) {
	responseContext_t ctx; Com_Memset(&ctx,0,sizeof(ctx));
	ctx.currentTime = (float)Sys_Milliseconds() * 0.001f;
	Response_TriggerConcept(luaL_checkstring(L,1), &ctx);
	return 0;
}

#endif /* USE_GAME_AI_MIDDLEWARE */

/* ========== VDB bindings ========== */

#include "../renderers/vulkan/vk_vdb.h"

static int l_vdb_load(lua_State *L) {
	lua_pushinteger(L, VDB_Load(luaL_checkstring(L, 1), luaL_optstring(L, 2, "density")));
	return 1;
}
static int l_vdb_free(lua_State *L) { VDB_Free((int)luaL_checkinteger(L, 1)); return 0; }
static int l_vdb_sample(lua_State *L) {
	lua_pushnumber(L, VDB_SampleFloat((int)luaL_checkinteger(L, 1),
		(float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4)));
	return 1;
}
static int l_vdb_getInfo(lua_State *L) {
	vdbGridInfo_t info;
	if (VDB_GetInfo((int)luaL_checkinteger(L, 1), &info)) {
		lua_newtable(L);
		lua_pushstring(L, info.name); lua_setfield(L, -2, "name");
		lua_pushinteger(L, info.dimX); lua_setfield(L, -2, "dimX");
		lua_pushinteger(L, info.dimY); lua_setfield(L, -2, "dimY");
		lua_pushinteger(L, info.dimZ); lua_setfield(L, -2, "dimZ");
		lua_pushnumber(L, info.voxelSize); lua_setfield(L, -2, "voxelSize");
		lua_pushinteger(L, info.activeVoxels); lua_setfield(L, -2, "activeVoxels");
		return 1;
	}
	lua_pushnil(L);
	return 1;
}
static int l_vdb_upload(lua_State *L) {
	lua_pushboolean(L, VDB_UploadToGPU((int)luaL_checkinteger(L, 1)));
	return 1;
}
static int l_vdb_bindAsFog(lua_State *L) {
	lua_pushboolean(L, VDB_BindAsFogDensity((int)luaL_checkinteger(L, 1)));
	return 1;
}
static int l_vdb_getGridCount(lua_State *L) {
	lua_pushinteger(L, VDB_GetGridCount());
	return 1;
}

/* ========== Arc Blanc ocean bindings (Algis et al. 2025) ========== */

#ifdef USE_ARC_BLANC
static int l_arcBlanc_enabled( lua_State *L )
{
	(void)L;
	lua_pushboolean( L, ArcBlanc_Enabled() );
	return 1;
}

static int l_arcBlanc_sampleHeight( lua_State *L )
{
	const float x = (float)luaL_checknumber( L, 1 );
	const float z = (float)luaL_checknumber( L, 2 );
	lua_pushnumber( L, ArcBlanc_SampleHeight( x, z ) );
	return 1;
}

static int l_arcBlanc_sampleVelocity( lua_State *L )
{
	const float x = (float)luaL_checknumber( L, 1 );
	const float y = (float)luaL_checknumber( L, 2 );
	const float z = (float)luaL_checknumber( L, 3 );
	vec3_t vel;
	ArcBlanc_SampleVelocity( x, y, z, vel );
	lua_pushnumber( L, vel[0] );
	lua_pushnumber( L, vel[1] );
	lua_pushnumber( L, vel[2] );
	return 3;
}

static int l_arcBlanc_registerHull( lua_State *L )
{
	vec3_t origin, mins, maxs;
	const int physBody = (int)luaL_checkinteger( L, 1 );
	origin[0] = (float)luaL_checknumber( L, 2 );
	origin[1] = (float)luaL_checknumber( L, 3 );
	origin[2] = (float)luaL_checknumber( L, 4 );
	mins[0] = (float)luaL_checknumber( L, 5 );
	mins[1] = (float)luaL_checknumber( L, 6 );
	mins[2] = (float)luaL_checknumber( L, 7 );
	maxs[0] = (float)luaL_checknumber( L, 8 );
	maxs[1] = (float)luaL_checknumber( L, 9 );
	maxs[2] = (float)luaL_checknumber( L, 10 );
	lua_pushinteger( L, ArcBlanc_RegisterBoxHull( physBody, origin, mins, maxs ) );
	return 1;
}

static int l_arcBlanc_unregisterHull( lua_State *L )
{
	ArcBlanc_UnregisterHull( (int)luaL_checkinteger( L, 1 ) );
	return 0;
}

static int l_arcBlanc_reseed( lua_State *L )
{
	(void)L;
	ArcBlanc_Reseed_f();
	return 0;
}
#endif

/* ========== Fog bioaerosol ecology bindings (Evans et al. 2019) ========== */

static void l_fogBio_pushCommunity( lua_State *L, const fogBioCommunity_t *c )
{
	int i;

	lua_newtable( L );
	lua_pushnumber( L, c->shannonDiversity );
	lua_setfield( L, -2, "shannon" );
	lua_pushnumber( L, c->marineFraction );
	lua_setfield( L, -2, "marine" );
	lua_pushnumber( L, c->oceanOtuFraction );
	lua_setfield( L, -2, "oceanOtu" );
	lua_pushnumber( L, c->depositionMultiplier );
	lua_setfield( L, -2, "deposition" );
	lua_pushnumber( L, c->culturableRichness );
	lua_setfield( L, -2, "richness" );
	lua_pushnumber( L, c->gramNegativeFraction );
	lua_setfield( L, -2, "gramNegative" );
	lua_pushnumber( L, c->rhodospirillalesFraction );
	lua_setfield( L, -2, "rhodospirillales" );
	lua_pushnumber( L, c->pathogenTaxaScore );
	lua_setfield( L, -2, "pathogenTaxa" );

	lua_newtable( L );
	for ( i = 0; i < FOG_BIO_PHYLUM_COUNT; i++ ) {
		lua_pushnumber( L, c->phylum[i] );
		lua_setfield( L, -2, FogBiology_PhylumName( (fogBioPhylum_t)i ) );
	}
	lua_setfield( L, -2, "phyla" );
}

static int l_fogBio_enabled( lua_State *L )
{
	(void)L;
	lua_pushboolean( L, FogBiology_Enabled() );
	return 1;
}

static int l_fogBio_getPhase( lua_State *L )
{
	(void)L;
	lua_pushstring( L, FogBiology_PhaseName( FogBiology_GetPhase() ) );
	return 1;
}

static int l_fogBio_getMarineInfluence( lua_State *L )
{
	(void)L;
	lua_pushnumber( L, FogBiology_GetMarineInfluence() );
	return 1;
}

static int l_fogBio_getPathogenRisk( lua_State *L )
{
	(void)L;
	lua_pushnumber( L, FogBiology_GetPathogenDepositionRisk() );
	return 1;
}

static int l_fogBio_getCommunity( lua_State *L )
{
	fogBioCommunity_t comm;
	const char *phaseArg;

	if ( lua_gettop( L ) >= 1 && lua_isstring( L, 1 ) ) {
		phaseArg = luaL_checkstring( L, 1 );
		if ( !Q_stricmp( phaseArg, "fog" ) ) {
			FogBiology_GetCommunity( FOG_BIO_PHASE_FOG, &comm );
		} else if ( !Q_stricmp( phaseArg, "post_fog" ) ) {
			FogBiology_GetCommunity( FOG_BIO_PHASE_POST_FOG, &comm );
		} else {
			FogBiology_GetCommunity( FOG_BIO_PHASE_CLEAR, &comm );
		}
	} else {
		FogBiology_GetCurrentCommunity( &comm );
	}
	l_fogBio_pushCommunity( L, &comm );
	return 1;
}

static int l_fogBio_setSite( lua_State *L )
{
	if ( lua_isstring( L, 1 ) ) {
		const char *site = luaL_checkstring( L, 1 );
		if ( !Q_stricmp( site, "namib" ) ) {
			FogBiology_SetSite( FOG_BIO_SITE_NAMIB );
		} else {
			FogBiology_SetSite( FOG_BIO_SITE_MAINE );
		}
	} else {
		FogBiology_SetSite( (int)luaL_checkinteger( L, 1 ) ? FOG_BIO_SITE_NAMIB : FOG_BIO_SITE_MAINE );
	}
	return 0;
}

static int l_fogBio_setCoastKm( lua_State *L )
{
	FogBiology_SetCoastDistanceKm( (float)luaL_checknumber( L, 1 ) );
	return 0;
}

static int l_fogBio_setMarineWind( lua_State *L )
{
	FogBiology_SetMarineWind( (float)luaL_checknumber( L, 1 ) );
	return 0;
}

static int l_fogBio_setFogActive( lua_State *L )
{
	FogBiology_SetFogActive( lua_toboolean( L, 1 ) );
	return 0;
}

static int l_fogBio_getCoastKm( lua_State *L )
{
	(void)L;
	lua_pushnumber( L, FogBiology_GetCoastDistanceKm() );
	return 1;
}

static int l_fogBio_poll( lua_State *L )
{
	fogBioCommunity_t comm;

	(void)L;
	lua_newtable( L );
	lua_pushstring( L, FogBiology_PhaseName( FogBiology_GetPhase() ) );
	lua_setfield( L, -2, "phase" );
	lua_pushnumber( L, FogBiology_GetMarineInfluence() );
	lua_setfield( L, -2, "marine" );
	lua_pushnumber( L, FogBiology_GetCoastDistanceKm() );
	lua_setfield( L, -2, "coastKm" );
	FogBiology_GetCurrentCommunity( &comm );
	lua_pushnumber( L, comm.shannonDiversity );
	lua_setfield( L, -2, "shannon" );
	lua_pushnumber( L, comm.depositionMultiplier );
	lua_setfield( L, -2, "deposition" );
	lua_pushnumber( L, FogBiology_GetPathogenDepositionRisk() );
	lua_setfield( L, -2, "pathogen" );
	lua_pushnumber( L, comm.pathogenTaxaScore );
	lua_setfield( L, -2, "pathogenTaxa" );
	lua_pushnumber( L, comm.gramNegativeFraction );
	lua_setfield( L, -2, "gramNegative" );
	lua_pushnumber( L, comm.oceanOtuFraction );
	lua_setfield( L, -2, "oceanOtu" );
	return 1;
}

/* ========== Engine.Genome — genetic + GAN procedural body evolution ========== */

static int l_genome_enabled( lua_State *L )
{
	(void)L;
	lua_pushboolean( L, GeneticGan_Enabled() );
	return 1;
}

static int l_genome_getDim( lua_State *L )
{
	(void)L;
	lua_pushinteger( L, GeneticGan_GetDim() );
	return 1;
}

static int l_genome_create( lua_State *L )
{
	const char *label;
	int slot;

	label = lua_isstring( L, 1 ) ? luaL_checkstring( L, 1 ) : NULL;
	slot = GeneticGan_CreateRandom( label );
	if ( slot < 0 ) {
		lua_pushnil( L );
		return 1;
	}
	lua_pushinteger( L, slot );
	return 1;
}

static int l_genome_breed( lua_State *L )
{
	int a = (int)luaL_checkinteger( L, 1 );
	int b = (int)luaL_checkinteger( L, 2 );
	float rate = lua_isnumber( L, 3 ) ? (float)lua_tonumber( L, 3 ) : -1.0f;
	const char *label = lua_isstring( L, 4 ) ? luaL_checkstring( L, 4 ) : NULL;
	int child = GeneticGan_Breed( a, b, rate, label );
	if ( child < 0 ) {
		lua_pushnil( L );
		return 1;
	}
	lua_pushinteger( L, child );
	return 1;
}

static int l_genome_mutate( lua_State *L )
{
	int slot = (int)luaL_checkinteger( L, 1 );
	float rate = lua_isnumber( L, 2 ) ? (float)lua_tonumber( L, 2 ) : -1.0f;
	float strength = lua_isnumber( L, 3 ) ? (float)lua_tonumber( L, 3 ) : 0.15f;
	slot = GeneticGan_Mutate( slot, rate, strength );
	if ( slot < 0 ) {
		lua_pushnil( L );
		return 1;
	}
	lua_pushinteger( L, slot );
	return 1;
}

static int l_genome_setFitness( lua_State *L )
{
	GeneticGan_SetFitness( (int)luaL_checkinteger( L, 1 ), (float)luaL_checknumber( L, 2 ) );
	return 0;
}

static int l_genome_getFitness( lua_State *L )
{
	lua_pushnumber( L, GeneticGan_GetFitness( (int)luaL_checkinteger( L, 1 ) ) );
	return 1;
}

static int l_genome_getGene( lua_State *L )
{
	lua_pushnumber( L, GeneticGan_GetGene( (int)luaL_checkinteger( L, 1 ), (int)luaL_checkinteger( L, 2 ) ) );
	return 1;
}

static int l_genome_setGene( lua_State *L )
{
	qboolean ok = GeneticGan_SetGene( (int)luaL_checkinteger( L, 1 ), (int)luaL_checkinteger( L, 2 ),
		(float)luaL_checknumber( L, 3 ) );
	lua_pushboolean( L, ok );
	return 1;
}

static int l_genome_selectBest( lua_State *L )
{
	int best = GeneticGan_SelectBest();
	if ( best < 0 ) {
		lua_pushnil( L );
		return 1;
	}
	lua_pushinteger( L, best );
	return 1;
}

static int l_genome_selectTournament( lua_State *L )
{
	int k = lua_isnumber( L, 1 ) ? (int)lua_tonumber( L, 1 ) : 3;
	int pick = GeneticGan_SelectTournament( k );
	if ( pick < 0 ) {
		lua_pushnil( L );
		return 1;
	}
	lua_pushinteger( L, pick );
	return 1;
}

static int l_genome_count( lua_State *L )
{
	(void)L;
	lua_pushinteger( L, GeneticGan_Count() );
	return 1;
}

static int l_genome_getPhenotype( lua_State *L )
{
	geneticGanPhenotype_t pheno;
	int slot = (int)luaL_checkinteger( L, 1 );
	int i;

	GeneticGan_GetPhenotype( slot, &pheno );
	if ( !GeneticGan_IsActive( slot ) ) {
		lua_pushnil( L );
		return 1;
	}
	lua_newtable( L );
	lua_pushnumber( L, pheno.bodyScale );
	lua_setfield( L, -2, "bodyScale" );
	lua_pushnumber( L, pheno.limbLength );
	lua_setfield( L, -2, "limbLength" );
	lua_pushnumber( L, pheno.headSize );
	lua_setfield( L, -2, "headSize" );
	lua_pushnumber( L, pheno.torsoWidth );
	lua_setfield( L, -2, "torsoWidth" );
	lua_pushnumber( L, pheno.agility );
	lua_setfield( L, -2, "agility" );
	lua_pushnumber( L, pheno.mass );
	lua_setfield( L, -2, "mass" );
	lua_newtable( L );
	for ( i = 0; i < pheno.morphCount; i++ ) {
		lua_pushnumber( L, pheno.morphWeights[i] );
		lua_rawseti( L, -2, i + 1 );
	}
	lua_setfield( L, -2, "morphWeights" );
	return 1;
}

static int l_genome_jobStatus( lua_State *L )
{
	(void)L;
	switch ( GeneticGan_GetJobStatus() ) {
	case GENETIC_GAN_JOB_RUNNING:
		lua_pushstring( L, "running" );
		break;
	case GENETIC_GAN_JOB_COMPLETED:
		lua_pushstring( L, "completed" );
		break;
	case GENETIC_GAN_JOB_FAILED:
		lua_pushstring( L, "failed" );
		break;
	default:
		lua_pushstring( L, "idle" );
		break;
	}
	return 1;
}

static int l_genome_decode( lua_State *L )
{
	const char *cmd;
	int slot = (int)luaL_checkinteger( L, 1 );

	if ( !GeneticGan_Enabled() ) {
		lua_pushboolean( L, qfalse );
		return 1;
	}
	cmd = va( "genome_generate %d\n", slot );
	Cbuf_AddText( cmd );
	lua_pushboolean( L, qtrue );
	return 1;
}

/* ========== Generic script control bindings ========== */

static int l_cvars_getString( lua_State *L )
{
	char value[MAX_CVAR_VALUE_STRING];

	Cvar_VariableStringBuffer( luaL_checkstring( L, 1 ), value, sizeof( value ) );
	lua_pushstring( L, value );
	return 1;
}

static int l_cvars_getNumber( lua_State *L )
{
	lua_pushnumber( L, Cvar_VariableValue( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int l_cvars_getInteger( lua_State *L )
{
	lua_pushinteger( L, Cvar_VariableIntegerValue( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int l_cvars_getBoolean( lua_State *L )
{
	lua_pushboolean( L, Cvar_VariableIntegerValue( luaL_checkstring( L, 1 ) ) ? qtrue : qfalse );
	return 1;
}

static int l_cvars_exists( lua_State *L )
{
	lua_pushboolean( L, Cvar_Flags( luaL_checkstring( L, 1 ) ) != CVAR_NONEXISTENT );
	return 1;
}

static int l_cvars_flags( lua_State *L )
{
	lua_pushinteger( L, (lua_Integer)Cvar_Flags( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int l_cvars_set( lua_State *L )
{
	Cvar_Set( luaL_checkstring( L, 1 ), luaL_checkstring( L, 2 ) );
	return 0;
}

static int l_cvars_setNumber( lua_State *L )
{
	char value[64];

	Com_sprintf( value, sizeof( value ), "%g", luaL_checknumber( L, 2 ) );
	Cvar_Set( luaL_checkstring( L, 1 ), value );
	return 0;
}

static int l_cvars_setInteger( lua_State *L )
{
	char value[32];

	Com_sprintf( value, sizeof( value ), "%d", (int)luaL_checkinteger( L, 2 ) );
	Cvar_Set( luaL_checkstring( L, 1 ), value );
	return 0;
}

static int l_cvars_setBoolean( lua_State *L )
{
	Cvar_Set( luaL_checkstring( L, 1 ), lua_toboolean( L, 2 ) ? "1" : "0" );
	return 0;
}

static int l_cvars_reset( lua_State *L )
{
	Cvar_Reset( luaL_checkstring( L, 1 ) );
	return 0;
}

static int l_console_exec( lua_State *L )
{
	const char *text = luaL_checkstring( L, 1 );
	const char *mode = luaL_optstring( L, 2, "append" );
	cbufExec_t when = EXEC_APPEND;

	if ( !Q_stricmp( mode, "insert" ) ) {
		when = EXEC_INSERT;
	} else if ( !Q_stricmp( mode, "now" ) ) {
		when = EXEC_NOW;
	}

	Cbuf_ExecuteText( when, text );
	return 0;
}

static int l_console_addText( lua_State *L )
{
	Cbuf_AddText( luaL_checkstring( L, 1 ) );
	return 0;
}

static int l_p2p_getSession( lua_State *L )
{
	lua_newtable( L );

	lua_pushstring( L, CL_P2P_SessionId() );
	lua_setfield( L, -2, "id" );
	lua_pushstring( L, CL_P2P_SessionAddress() );
	lua_setfield( L, -2, "address" );
	lua_pushstring( L, CL_P2P_SessionFailoverPolicy() );
	lua_setfield( L, -2, "failover" );
	lua_pushinteger( L, CL_P2P_SessionReconnectWindowSec() );
	lua_setfield( L, -2, "reconnectWindowSec" );
	lua_pushinteger( L, CL_P2P_SessionAttemptCount() );
	lua_setfield( L, -2, "attemptCount" );
	lua_pushboolean( L, CL_P2P_SessionPending() );
	lua_setfield( L, -2, "pending" );
	lua_pushboolean( L, CL_P2P_SessionMigratePending() );
	lua_setfield( L, -2, "migratePending" );
	lua_pushstring( L, CL_P2P_SessionMigrateAddress() );
	lua_setfield( L, -2, "migrateAddress" );
	lua_pushstring( L, CL_P2P_SessionCurrentTarget() );
	lua_setfield( L, -2, "currentTarget" );
	lua_pushstring( L, CL_P2P_SessionRecoveryStopReason() );
	lua_setfield( L, -2, "recoveryStopReason" );
	lua_pushboolean( L, CL_P2P_SessionIsBackupHostEligible() );
	lua_setfield( L, -2, "backupHostEligible" );

	return 1;
}

static int l_p2p_isBackupHostEligible( lua_State *L )
{
	(void)L;
	lua_pushboolean( L, CL_P2P_SessionIsBackupHostEligible() );
	return 1;
}

/* ========== Engine sprite bindings (client-local + listen-server spawn) ========== */

static engineSpriteType_t l_spriteTypeFromString( const char *typeArg ) {
	if ( typeArg && !Q_stricmp( typeArg, "flipbook" ) ) {
		return ENGINE_SPRITE_FLIPBOOK;
	}
	if ( typeArg && !Q_stricmp( typeArg, "imposter" ) ) {
		return ENGINE_SPRITE_IMPOSTER;
	}
	return ENGINE_SPRITE_BILLBOARD;
}

static int l_sprite_spawnLocal(lua_State *L) {
	engineSpriteDesc_t desc;
	const char *typeArg;
	const char *shaderArg;

	typeArg = luaL_optstring( L, 1, "billboard" );
	shaderArg = luaL_checkstring( L, 2 );

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.type = l_spriteTypeFromString( typeArg );
	desc.cols = (int)luaL_optinteger( L, 3, 2 );
	desc.rows = (int)luaL_optinteger( L, 4, 2 );
	desc.fps = (float)luaL_optnumber( L, 5, 8.0 );
	desc.origin[0] = (float)luaL_optnumber( L, 6, 0.0 );
	desc.origin[1] = (float)luaL_optnumber( L, 7, 0.0 );
	desc.origin[2] = (float)luaL_optnumber( L, 8, 64.0 );
	desc.radius = (float)luaL_optnumber( L, 9, 48.0 );
	desc.rotation = (float)luaL_optnumber( L, 10, 0.0 );

	if ( !re.RegisterShader ) {
		return luaL_error( L, "renderer not ready" );
	}
	desc.shader = re.RegisterShader( shaderArg );
	if ( !desc.shader ) {
		return luaL_error( L, "shader not found: %s", shaderArg );
	}

	CL_EngineSprite_AddLocal( &desc );
	return 0;
}

static int l_decal_spawnLocal(lua_State *L) {
	engineDecalDesc_t desc;
	const char *shaderArg;

	shaderArg = luaL_checkstring( L, 1 );
	if ( !re.RegisterShader ) {
		return luaL_error( L, "renderer not ready" );
	}
	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.shader = re.RegisterShader( shaderArg );
	if ( !desc.shader ) {
		return luaL_error( L, "shader not found: %s", shaderArg );
	}
	desc.origin[0] = (float)luaL_optnumber( L, 2, 0.0 );
	desc.origin[1] = (float)luaL_optnumber( L, 3, 0.0 );
	desc.origin[2] = (float)luaL_optnumber( L, 4, 64.0 );
	desc.radius = (float)luaL_optnumber( L, 5, 32.0 );
	desc.pitch = (float)luaL_optnumber( L, 6, 0.0 );
	desc.yaw = (float)luaL_optnumber( L, 7, 0.0 );
	CL_EngineDecal_AddLocal( &desc );
	return 0;
}

static int l_decal_spawnServer(lua_State *L) {
	char cmd[MAX_STRING_CHARS];
	const char *shaderArg = luaL_checkstring( L, 1 );

	Com_sprintf( cmd, sizeof( cmd ),
		"sv_decal_spawn %s %.0f %.0f %.0f %.0f %.0f %.0f %.0f\n",
		shaderArg,
		luaL_optnumber( L, 2, 0.0 ),
		luaL_optnumber( L, 3, 0.0 ),
		luaL_optnumber( L, 4, 64.0 ),
		luaL_optnumber( L, 5, 32.0 ),
		luaL_optnumber( L, 6, 0.0 ),
		luaL_optnumber( L, 7, 0.0 ),
		luaL_optnumber( L, 8, 0.0 ) );
	Cbuf_AddText( cmd );
	return 0;
}

static int l_char_create(lua_State *L) {
	int h = Phys_CharacterCreate(
		(float)luaL_optnumber( L, 1, 16.0 ),
		(float)luaL_optnumber( L, 2, 56.0 ),
		(float)luaL_optnumber( L, 3, 18.0 ) );
	lua_pushinteger( L, h );
	return 1;
}

static int l_char_move(lua_State *L) {
	int handle = (int)luaL_checkinteger( L, 1 );
	float dir[3];
	dir[0] = (float)luaL_optnumber( L, 2, 0.0 );
	dir[1] = (float)luaL_optnumber( L, 3, 0.0 );
	dir[2] = (float)luaL_optnumber( L, 4, 0.0 );
	Phys_CharacterMove( handle, dir, (float)luaL_optnumber( L, 5, 320.0 ),
		lua_toboolean( L, 6 ) );
	return 0;
}

static int l_animgraph_load(lua_State *L) {
	lua_pushboolean( L, G_AnimGraph_Load( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int l_animgraph_setState(lua_State *L) {
	G_AnimGraph_SetState( luaL_checkstring( L, 1 ) );
	return 0;
}

static int l_animgraph_update(lua_State *L) {
	int frame = 0, old = 0;
	float blend = 0.0f;
	G_AnimGraph_Update( (int)luaL_optinteger( L, 1, 16 ), &frame, &old, &blend );
	lua_pushinteger( L, frame );
	lua_pushnumber( L, blend );
	return 2;
}

static int l_sprite_spawnServer(lua_State *L) {
	const char *typeArg;
	const char *shaderArg;
	char cmd[MAX_STRING_CHARS];

	typeArg = luaL_optstring( L, 1, "billboard" );
	shaderArg = luaL_checkstring( L, 2 );

	Com_sprintf( cmd, sizeof( cmd ),
		"sv_sprite_spawn %s %s %d %d %.0f %.0f %.0f %.0f %.0f %.0f\n",
		typeArg, shaderArg,
		(int)luaL_optinteger( L, 3, 2 ),
		(int)luaL_optinteger( L, 4, 2 ),
		luaL_optnumber( L, 5, 8.0 ),
		luaL_optnumber( L, 6, 0.0 ),
		luaL_optnumber( L, 7, 0.0 ),
		luaL_optnumber( L, 8, 64.0 ),
		luaL_optnumber( L, 9, 48.0 ),
		luaL_optnumber( L, 10, 0.0 ) );
	Cbuf_AddText( cmd );
	return 0;
}

#ifdef USE_GAME_AI_MIDDLEWARE

/* ========== AIML bindings ========== */

#include "g_aiml.h"
#include "g_eda.h"

static int l_aiml_createBot(lua_State *L) {
	lua_pushinteger(L, AIML_CreateBot(luaL_checkstring(L, 1)));
	return 1;
}
static int l_aiml_destroyBot(lua_State *L) {
	AIML_DestroyBot((int)luaL_checkinteger(L, 1));
	return 0;
}
static int l_aiml_loadFile(lua_State *L) {
	lua_pushboolean(L, AIML_LoadFile((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2)));
	return 1;
}
static int l_aiml_setProperty(lua_State *L) {
	AIML_SetBotProperty((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2), luaL_checkstring(L, 3));
	return 0;
}
static int l_aiml_getProperty(lua_State *L) {
	lua_pushstring(L, AIML_GetBotProperty((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2)));
	return 1;
}
static int l_aiml_getResponse(lua_State *L) {
	lua_pushstring(L, AIML_GetResponse((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2), luaL_checkstring(L, 3)));
	return 1;
}
static int l_aiml_setUserVar(lua_State *L) {
	AIML_SetUserVar((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2), luaL_checkstring(L, 3), luaL_checkstring(L, 4));
	return 0;
}
static int l_aiml_getUserVar(lua_State *L) {
	lua_pushstring(L, AIML_GetUserVar((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2), luaL_checkstring(L, 3)));
	return 1;
}
static int l_aiml_resetUser(lua_State *L) {
	AIML_ResetUser((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2));
	return 0;
}
static int l_aiml_getCategoryCount(lua_State *L) {
	lua_pushinteger(L, AIML_GetCategoryCount((int)luaL_checkinteger(L, 1)));
	return 1;
}

static int l_eda_publish(lua_State *L) {
	lua_pushboolean( L, EDA_Publish( luaL_checkstring( L, 1 ), luaL_optstring( L, 2, "" ) ) );
	return 1;
}
static int l_eda_pop( lua_State *L ) {
	char ch[EDA_MAX_NAME], pay[EDA_MAX_PAYLOAD];
	if ( EDA_Pop( ch, sizeof( ch ), pay, sizeof( pay ) ) ) {
		lua_pushstring( L, ch );
		lua_pushstring( L, pay );
		return 2;
	}
	return 0;
}
static int l_eda_peek( lua_State *L ) {
	char ch[EDA_MAX_NAME], pay[EDA_MAX_PAYLOAD];
	if ( EDA_Peek( ch, sizeof( ch ), pay, sizeof( pay ) ) ) {
		lua_pushstring( L, ch );
		lua_pushstring( L, pay );
		return 2;
	}
	return 0;
}
static int l_eda_isEnabled( lua_State *L ) {
	(void)L;
	lua_pushboolean( L, EDA_IsEnabled() );
	return 1;
}
static int l_eda_queueDepth( lua_State *L ) {
	(void)L;
	lua_pushinteger( L, (lua_Integer)EDA_QueueDepth() );
	return 1;
}
static int l_eda_clear( lua_State *L ) {
	(void)L;
	EDA_Clear();
	return 0;
}
static int l_eda_drain( lua_State *L ) {
	int i, n, cap;
	edaEventRecord_t stackBuf[64];
	edaEventRecord_t *buf = stackBuf;
	cap = (int)luaL_optinteger( L, 1, 32 );
	if ( cap < 1 ) {
		cap = 1;
	}
	if ( cap > 256 ) {
		cap = 256;
	}
	if ( cap > 64 ) {
		buf = (edaEventRecord_t *)Z_TagMalloc( (int)( (size_t)cap * sizeof( *buf ) ), TAG_GENERAL );
		Com_Memset( buf, 0, (size_t)cap * sizeof( *buf ) );
	}
	n = EDA_Drain( buf, cap );
	lua_createtable( L, n, 0 );
	for ( i = 0; i < n; i++ ) {
		lua_createtable( L, 0, 2 );
		lua_pushstring( L, buf[i].channel );
		lua_setfield( L, -2, "channel" );
		lua_pushstring( L, buf[i].payload );
		lua_setfield( L, -2, "payload" );
		lua_rawseti( L, -2, i + 1 );
	}
	if ( buf != stackBuf ) {
		Z_Free( buf );
	}
	lua_pushinteger( L, (lua_Integer)n );
	return 2;
}

#endif /* USE_GAME_AI_MIDDLEWARE */

#include "g_engine_systems.h"

/* ========== Telemetry / replay / save / quest / dialogue ========== */

static int l_telem_record(lua_State *L) {
	EngineTelemetry_Record( luaL_checkstring( L, 1 ), luaL_checknumber( L, 2 ) );
	return 0;
}
static int l_telem_get(lua_State *L) {
	lua_pushnumber( L, EngineTelemetry_Get( luaL_checkstring( L, 1 ) ) );
	return 1;
}
static int l_telem_clear(lua_State *L ) {
	(void)L;
	EngineTelemetry_Clear();
	return 0;
}

static int l_replay_frame(lua_State *L ) {
	lua_pushinteger( L, EngineReplay_GetFrameIndex() );
	return 1;
}
static int l_replay_baseTime(lua_State *L ) {
	lua_pushinteger( L, EngineReplay_GetBaseTime() );
	return 1;
}

static int l_save_write(lua_State *L ) {
	lua_pushboolean( L, EngineSave_WriteSlot( (int)luaL_checkinteger( L, 1 ), luaL_checkstring( L, 2 ) ) );
	return 1;
}
static int l_save_read(lua_State *L ) {
	char buf[128];
	if ( EngineSave_ReadSlot( (int)luaL_checkinteger( L, 1 ), buf, sizeof( buf ) ) ) {
		lua_pushstring( L, buf );
	} else {
		lua_pushnil( L );
	}
	return 1;
}
static int l_save_lastSlot(lua_State *L ) {
	lua_pushinteger( L, EngineSave_LastSlot() );
	return 1;
}

static int l_db_available(lua_State *L ) {
	lua_pushboolean( L, EngineDatabase_IsAvailable() );
	return 1;
}
static int l_db_path(lua_State *L ) {
	lua_pushstring( L, EngineDatabase_GetPath() );
	return 1;
}
static int l_db_exec(lua_State *L ) {
	lua_pushboolean( L, EngineDatabase_Exec( luaL_checkstring( L, 1 ) ) );
	return 1;
}
static int l_db_queryOne(lua_State *L ) {
	char buf[1024];
	if ( EngineDatabase_QueryOne( luaL_checkstring( L, 1 ), buf, sizeof( buf ) ) ) {
		lua_pushstring( L, buf );
	} else {
		lua_pushnil( L );
	}
	return 1;
}
static int l_db_profileSet(lua_State *L ) {
	lua_pushboolean( L, EngineProfile_Set( luaL_checkstring( L, 1 ), luaL_checkstring( L, 2 ) ) );
	return 1;
}
static int l_db_profileGet(lua_State *L ) {
	char buf[1024];
	if ( EngineProfile_Get( luaL_checkstring( L, 1 ), buf, sizeof( buf ) ) ) {
		lua_pushstring( L, buf );
	} else {
		lua_pushnil( L );
	}
	return 1;
}
static int l_db_profileDelete(lua_State *L ) {
	lua_pushboolean( L, EngineProfile_Delete( luaL_checkstring( L, 1 ) ) );
	return 1;
}

static int l_quest_add(lua_State *L ) {
	lua_pushinteger( L, EngineQuest_Add( luaL_checkstring( L, 1 ), luaL_checkstring( L, 2 ), luaL_checkstring( L, 3 ) ) );
	return 1;
}
static int l_quest_setStage(lua_State *L ) {
	lua_pushboolean( L, EngineQuest_SetStage( luaL_checkstring( L, 1 ), luaL_checkstring( L, 2 ) ) );
	return 1;
}
static int l_quest_getStage(lua_State *L ) {
	lua_pushstring( L, EngineQuest_GetStage( luaL_checkstring( L, 1 ) ) );
	return 1;
}
static int l_quest_count(lua_State *L ) {
	lua_pushinteger( L, EngineQuest_Count() );
	return 1;
}

static int l_dialogue_start(lua_State *L ) {
	lua_pushinteger( L, EngineDialogue_Start( luaL_optstring( L, 1, "" ), luaL_checkstring( L, 2 ) ) );
	return 1;
}
static int l_dialogue_clear(lua_State *L ) {
	(void)L;
	EngineDialogue_Clear();
	return 0;
}
static int l_dialogue_count(lua_State *L ) {
	lua_pushinteger( L, EngineDialogue_ActiveCount() );
	return 1;
}

/* ========== ECS bindings ========== */

static int l_ecs_create(lua_State *L) {
	ecs_entity_t e = ECS_Create();
	lua_pushinteger(L, (lua_Integer)e);
	return 1;
}
static int l_ecs_destroy(lua_State *L) {
	ECS_Destroy((ecs_entity_t)luaL_checkinteger(L, 1));
	return 0;
}
static int l_ecs_valid(lua_State *L) {
	lua_pushboolean(L, ECS_Valid((ecs_entity_t)luaL_checkinteger(L, 1)));
	return 1;
}
static int l_ecs_count(lua_State *L) {
	lua_pushinteger(L, (lua_Integer)ECS_Count());
	return 1;
}
static int l_ecs_countWith( lua_State *L ) {
	ecs_component_id_t c = ECS_ComponentFromName( luaL_checkstring( L, 1 ) );
	if ( c >= ECS_COMP_COUNT ) {
		lua_pushinteger( L, 0 );
	} else {
		lua_pushinteger( L, (lua_Integer)ECS_CountWith( c ) );
	}
	return 1;
}
static int l_ecs_has(lua_State *L) {
	ecs_entity_t e = (ecs_entity_t)luaL_checkinteger(L, 1);
	ecs_component_id_t c = ECS_ComponentFromName(luaL_checkstring(L, 2));
	lua_pushboolean(L, c < ECS_COMP_COUNT && ECS_Has(e, c));
	return 1;
}
static int l_ecs_add(lua_State *L) {
	ecs_entity_t e = (ecs_entity_t)luaL_checkinteger(L, 1);
	ecs_component_id_t c = ECS_ComponentFromName(luaL_checkstring(L, 2));
	if (c < ECS_COMP_COUNT) ECS_Add(e, c);
	return 0;
}
static int l_ecs_remove(lua_State *L) {
	ecs_entity_t e = (ecs_entity_t)luaL_checkinteger(L, 1);
	ecs_component_id_t c = ECS_ComponentFromName(luaL_checkstring(L, 2));
	if (c < ECS_COMP_COUNT) ECS_Remove(e, c);
	return 0;
}
static int l_ecs_setPosition(lua_State *L) {
	ECS_SetPosition((ecs_entity_t)luaL_checkinteger(L, 1),
		(float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4));
	return 0;
}
static int l_ecs_getPosition(lua_State *L) {
	vec3_t v;
	ECS_GetPosition((ecs_entity_t)luaL_checkinteger(L, 1), v);
	lua_pushnumber(L, v[0]); lua_pushnumber(L, v[1]); lua_pushnumber(L, v[2]);
	return 3;
}
static int l_ecs_setRotation(lua_State *L) {
	ECS_SetRotation((ecs_entity_t)luaL_checkinteger(L, 1),
		(float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4));
	return 0;
}
static int l_ecs_getRotation(lua_State *L) {
	vec3_t v;
	ECS_GetRotation((ecs_entity_t)luaL_checkinteger(L, 1), v);
	lua_pushnumber(L, v[0]); lua_pushnumber(L, v[1]); lua_pushnumber(L, v[2]);
	return 3;
}
static int l_ecs_setScale(lua_State *L) {
	ECS_SetScale((ecs_entity_t)luaL_checkinteger(L, 1),
		(float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4));
	return 0;
}
static int l_ecs_getScale(lua_State *L) {
	vec3_t v;
	ECS_GetScale((ecs_entity_t)luaL_checkinteger(L, 1), v);
	lua_pushnumber(L, v[0]); lua_pushnumber(L, v[1]); lua_pushnumber(L, v[2]);
	return 3;
}
static int l_ecs_setVelocity(lua_State *L) {
	ECS_SetVelocity((ecs_entity_t)luaL_checkinteger(L, 1),
		(float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4));
	return 0;
}
static int l_ecs_getVelocity(lua_State *L) {
	vec3_t v;
	ECS_GetVelocity((ecs_entity_t)luaL_checkinteger(L, 1), v);
	lua_pushnumber(L, v[0]); lua_pushnumber(L, v[1]); lua_pushnumber(L, v[2]);
	return 3;
}
static int l_ecs_setHealth(lua_State *L) {
	ECS_SetHealth((ecs_entity_t)luaL_checkinteger(L, 1), (float)luaL_checknumber(L, 2));
	return 0;
}
static int l_ecs_getHealth(lua_State *L) {
	lua_pushnumber(L, ECS_GetHealth((ecs_entity_t)luaL_checkinteger(L, 1)));
	return 1;
}
static int l_ecs_setTag(lua_State *L) {
	ECS_SetTag((ecs_entity_t)luaL_checkinteger(L, 1), luaL_optstring(L, 2, ""));
	return 0;
}
static int l_ecs_getTag(lua_State *L) {
	lua_pushstring(L, ECS_GetTag((ecs_entity_t)luaL_checkinteger(L, 1)));
	return 1;
}
static int l_ecs_setGentityLink(lua_State *L) {
	ECS_SetGentityLink((ecs_entity_t)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
	return 0;
}
static int l_ecs_getGentityLink(lua_State *L) {
	lua_pushinteger(L, ECS_GetGentityLink((ecs_entity_t)luaL_checkinteger(L, 1)));
	return 1;
}
static int l_ecs_componentFromName(lua_State *L) {
	lua_pushinteger(L, (lua_Integer)ECS_ComponentFromName(luaL_checkstring(L, 1)));
	return 1;
}
static int l_ecs_componentName(lua_State *L) {
	lua_pushstring(L, ECS_ComponentName((ecs_component_id_t)luaL_checkinteger(L, 1)));
	return 1;
}
static int l_ecs_stepMotion( lua_State *L ) {
	ECS_StepMotion( (float)luaL_checknumber( L, 1 ) );
	return 0;
}

#ifdef USE_GAME_AI_MIDDLEWARE

/* ========== GOAP bindings ========== */

#include "g_goap.h"
#include "g_bt.h"

/* Actions */
static int l_goap_registerAction(lua_State *L) {
	lua_pushinteger(L, GOAP_RegisterAction(luaL_checkstring(L,1), (float)luaL_optnumber(L,2,1.0)));
	return 1;
}
static int l_goap_setActionPrecondition(lua_State *L) {
	GOAP_SetActionPrecondition((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2), (int)luaL_checkinteger(L,3));
	return 0;
}
static int l_goap_setActionEffect(lua_State *L) {
	GOAP_SetActionEffect((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2), (int)luaL_checkinteger(L,3));
	return 0;
}
static int l_goap_setActionDuration(lua_State *L) {
	GOAP_SetActionDuration((int)luaL_checkinteger(L,1), (float)luaL_checknumber(L,2));
	return 0;
}
static int l_goap_setActionRange(lua_State *L) {
	GOAP_SetActionRange((int)luaL_checkinteger(L,1), (float)luaL_checknumber(L,2));
	return 0;
}
static int l_goap_setActionActive(lua_State *L) {
	GOAP_SetActionActive((int)luaL_checkinteger(L,1), lua_toboolean(L,2));
	return 0;
}
static int l_goap_getActionName(lua_State *L) {
	lua_pushstring(L, GOAP_GetActionName((int)luaL_checkinteger(L,1)));
	return 1;
}

/* Goals */
static int l_goap_registerGoal(lua_State *L) {
	lua_pushinteger(L, GOAP_RegisterGoal(luaL_checkstring(L,1), (float)luaL_optnumber(L,2,1.0)));
	return 1;
}
static int l_goap_setGoalState(lua_State *L) {
	GOAP_SetGoalState((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2), (int)luaL_checkinteger(L,3));
	return 0;
}
static int l_goap_setGoalActive(lua_State *L) {
	GOAP_SetGoalActive((int)luaL_checkinteger(L,1), lua_toboolean(L,2));
	return 0;
}
static int l_goap_getGoalName(lua_State *L) {
	lua_pushstring(L, GOAP_GetGoalName((int)luaL_checkinteger(L,1)));
	return 1;
}

/* Properties */
static int l_goap_defineProperty(lua_State *L) {
	lua_pushinteger(L, GOAP_DefineProperty(luaL_checkstring(L,1)));
	return 1;
}
static int l_goap_findProperty(lua_State *L) {
	lua_pushinteger(L, GOAP_FindProperty(luaL_checkstring(L,1)));
	return 1;
}
static int l_goap_getPropertyName(lua_State *L) {
	lua_pushstring(L, GOAP_GetPropertyName((int)luaL_checkinteger(L,1)));
	return 1;
}

/* Agents */
static int l_goap_createAgent(lua_State *L) {
	lua_pushinteger(L, GOAP_CreateAgent());
	return 1;
}
static int l_goap_destroyAgent(lua_State *L) {
	GOAP_DestroyAgent((int)luaL_checkinteger(L,1));
	return 0;
}
static int l_goap_setWorldState(lua_State *L) {
	GOAP_SetAgentWorldState((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2), (int)luaL_checkinteger(L,3));
	return 0;
}
static int l_goap_getWorldState(lua_State *L) {
	lua_pushinteger(L, GOAP_GetAgentWorldState((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2)));
	return 1;
}
static int l_goap_addAgentAction(lua_State *L) {
	GOAP_AddAgentAction((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2));
	return 0;
}
static int l_goap_setReplanInterval(lua_State *L) {
	GOAP_SetAgentReplanInterval((int)luaL_checkinteger(L,1), (float)luaL_checknumber(L,2));
	return 0;
}

/* Planning */
static int l_goap_plan(lua_State *L) {
	lua_pushboolean(L, GOAP_Plan((int)luaL_checkinteger(L,1), (int)luaL_checkinteger(L,2)));
	return 1;
}
static int l_goap_autoPlan(lua_State *L) {
	lua_pushboolean(L, GOAP_AutoPlan((int)luaL_checkinteger(L,1)));
	return 1;
}
static int l_goap_getCurrentAction(lua_State *L) {
	lua_pushinteger(L, GOAP_GetCurrentAction((int)luaL_checkinteger(L,1)));
	return 1;
}
static int l_goap_advancePlan(lua_State *L) {
	GOAP_AdvancePlan((int)luaL_checkinteger(L,1));
	return 0;
}
static int l_goap_abortPlan(lua_State *L) {
	GOAP_AbortPlan((int)luaL_checkinteger(L,1));
	return 0;
}
static int l_goap_isPlanComplete(lua_State *L) {
	lua_pushboolean(L, GOAP_IsPlanComplete((int)luaL_checkinteger(L,1)));
	return 1;
}

/* Blackboard */
static int l_goap_bbSetFloat(lua_State *L) {
	GOAP_BBSetFloat((int)luaL_checkinteger(L,1), luaL_checkstring(L,2), (float)luaL_checknumber(L,3));
	return 0;
}
static int l_goap_bbGetFloat(lua_State *L) {
	lua_pushnumber(L, GOAP_BBGetFloat((int)luaL_checkinteger(L,1), luaL_checkstring(L,2)));
	return 1;
}
static int l_goap_bbSetInt(lua_State *L) {
	GOAP_BBSetInt((int)luaL_checkinteger(L,1), luaL_checkstring(L,2), (int)luaL_checkinteger(L,3));
	return 0;
}
static int l_goap_bbGetInt(lua_State *L) {
	lua_pushinteger(L, GOAP_BBGetInt((int)luaL_checkinteger(L,1), luaL_checkstring(L,2)));
	return 1;
}
static int l_goap_bbClear(lua_State *L) {
	GOAP_BBClear((int)luaL_checkinteger(L,1));
	return 0;
}

/* Defaults + debug */
static int l_goap_registerDefaults(lua_State *L) {
	(void)L;
	GOAP_RegisterDefaultActions();
	GOAP_RegisterDefaultGoals();
	return 0;
}
static int l_goap_debugAgent(lua_State *L) {
	GOAP_DebugPrintAgent((int)luaL_checkinteger(L,1));
	return 0;
}
static int l_goap_debugActions(lua_State *L) {
	(void)L;
	GOAP_DebugPrintActions();
	return 0;
}
static int l_goap_debugGoals(lua_State *L) {
	(void)L;
	GOAP_DebugPrintGoals();
	return 0;
}
static int l_goap_getActionCount(lua_State *L) {
	lua_pushinteger(L, GOAP_GetActionCount());
	return 1;
}
static int l_goap_getGoalCount(lua_State *L) {
	lua_pushinteger(L, GOAP_GetGoalCount());
	return 1;
}
static int l_goap_getAgentCount(lua_State *L) {
	lua_pushinteger(L, GOAP_GetAgentCount());
	return 1;
}
static int l_goap_setMaxPlanIterations(lua_State *L) {
	GOAP_SetMaxPlanIterations((int)luaL_checkinteger(L, 1));
	return 0;
}
static int l_goap_getMaxPlanIterations(lua_State *L) {
	lua_pushinteger(L, GOAP_GetMaxPlanIterations());
	return 1;
}
static int l_goap_getLastPlanIterations(lua_State *L) {
	lua_pushinteger(L, GOAP_GetLastPlanIterations());
	return 1;
}
static int l_goap_forceReplan(lua_State *L) {
	GOAP_ForceReplan((int)luaL_checkinteger(L, 1));
	return 0;
}

/* ========== BT bindings ========== */

static int l_bt_createTree(lua_State *L) {
	lua_pushinteger(L, BT_CreateTree());
	return 1;
}
static int l_bt_addNode(lua_State *L) {
	int tree = (int)luaL_checkinteger(L, 1);
	int type = (int)luaL_checkinteger(L, 2);
	int childIds[BT_MAX_CHILDREN];
	int n = 0, i, len;
	if (lua_istable(L, 3)) {
		lua_len(L, 3);
		len = (int)lua_tointeger(L, -1);
		lua_pop(L, 1);
		for (i = 0; i < len && i < BT_MAX_CHILDREN; i++) {
			lua_rawgeti(L, 3, i + 1);
			childIds[n++] = (int)lua_tointeger(L, -1);
			lua_pop(L, 1);
		}
	}
	lua_pushinteger(L, BT_AddNode(tree, (btNodeType_t)type, n > 0 ? childIds : NULL, n));
	return 1;
}
static int l_bt_setRoot(lua_State *L) {
	BT_SetRoot((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
	return 0;
}
static int l_bt_createAgent(lua_State *L) {
	lua_pushinteger(L, BT_CreateAgent((int)luaL_checkinteger(L, 1)));
	return 1;
}
static int l_bt_destroyAgent(lua_State *L) {
	BT_DestroyAgent((int)luaL_checkinteger(L, 1));
	return 0;
}
static int l_bt_setAgentEntity(lua_State *L) {
	BT_SetAgentEntity((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
	return 0;
}
static int l_bt_setAgentTarget(lua_State *L) {
	vec3_t v;
	v[0] = (float)luaL_checknumber(L, 2);
	v[1] = (float)luaL_checknumber(L, 3);
	v[2] = (float)luaL_checknumber(L, 4);
	BT_SetAgentTarget((int)luaL_checkinteger(L, 1), v, (int)luaL_optinteger(L, 5, -1));
	return 0;
}
static int l_bt_setAgentContext(lua_State *L) {
	BT_SetAgentContext((int)luaL_checkinteger(L, 1),
		(float)luaL_checknumber(L, 2),
		(float)luaL_checknumber(L, 3),
		(float)luaL_checknumber(L, 4));
	return 0;
}
static int l_bt_setAnimOutput(lua_State *L) {
	BT_SetAnimOutput((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2), (int)luaL_checkinteger(L, 3));
	return 0;
}
static int l_bt_bbSetFloat(lua_State *L) {
	BT_BBSetFloat((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2), (float)luaL_checknumber(L, 3));
	return 0;
}
static int l_bt_bbGetFloat(lua_State *L) {
	lua_pushnumber(L, BT_BBGetFloat((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2)));
	return 1;
}
static int l_bt_bbSetInt(lua_State *L) {
	BT_BBSetInt((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2), (int)luaL_checkinteger(L, 3));
	return 0;
}
static int l_bt_bbGetInt(lua_State *L) {
	lua_pushinteger(L, BT_BBGetInt((int)luaL_checkinteger(L, 1), luaL_checkstring(L, 2)));
	return 1;
}
static int l_bt_linkHordeAgent(lua_State *L) {
	BT_LinkHordeAgent((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
	return 0;
}
static int l_bt_linkGOAPAgent(lua_State *L) {
	BT_LinkGOAPAgent((int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
	return 0;
}
static int l_bt_getActiveCount(lua_State *L) {
	(void)L;
	lua_pushinteger(L, BT_GetActiveCount());
	return 1;
}
static int l_bt_debugAgent(lua_State *L) {
	BT_DebugPrintAgent((int)luaL_checkinteger(L, 1));
	return 0;
}

#endif /* USE_GAME_AI_MIDDLEWARE */

#include "g_lua_registration.inc"
#else /* !USE_LUA */

void LuaBindings_RegisterAll(void *luaState) {
	(void)luaState;
}

#endif /* USE_LUA */
