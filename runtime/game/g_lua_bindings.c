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

static int l_phys_init(lua_State *L) { lua_pushboolean(L, Phys_Init()); return 1; }
static int l_phys_step(lua_State *L) { Phys_StepSimulation((float)luaL_checknumber(L,1)); return 0; }
static int l_phys_createBody(lua_State *L) {
	physBodyDef_t def; Com_Memset(&def,0,sizeof(def));
	def.shape = PHYS_SHAPE_BOX; def.type = PHYS_BODY_DYNAMIC;
	def.position[0]=(float)luaL_checknumber(L,1); def.position[1]=(float)luaL_checknumber(L,2); def.position[2]=(float)luaL_checknumber(L,3);
	def.mass=(float)luaL_optnumber(L,4,10); def.halfExtents[0]=def.halfExtents[1]=def.halfExtents[2]=(float)luaL_optnumber(L,5,8);
	def.friction=0.5f; def.restitution=0.3f; def.collisionGroup=1; def.collisionMask=-1;
	lua_pushinteger(L, Phys_CreateBody(&def));
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

/* ========== Registration ========== */

static void registerTable(lua_State *L, const char *name, const luaL_Reg *funcs) {
	lua_newtable(L);
	const luaL_Reg *f;
	for (f = funcs; f->name; f++) {
		lua_pushcfunction(L, f->func);
		lua_setfield(L, -2, f->name);
	}
	lua_setfield(L, -2, name);
}

void LuaBindings_RegisterAll(void *luaState) {
	lua_State *L = (lua_State *)luaState;
	if (!L) return;

	lua_newtable(L);

#ifdef USE_GAME_AI_MIDDLEWARE
	static const luaL_Reg directorFuncs[] = {
		{"init", l_director_init}, {"update", l_director_update}, {"getPhase", l_director_getPhase},
		{"forcePhase", l_director_forcePhase}, {"getIntensity", l_director_getIntensity},
		{"getPlayerIntensity", l_director_getPlayerIntensity}, {"getPlayerStress", l_director_getPlayerStress},
		{"triggerWave", l_director_triggerWave}, {"updatePlayer", l_director_updatePlayer},
		{"playerKill", l_director_playerKill}, {"playerDeath", l_director_playerDeath},
		{"playerDamage", l_director_playerDamage}, {"addSpawnType", l_director_addSpawnType},
		{"shouldSpawn", l_director_shouldSpawn}, {"pickSpawnType", l_director_pickSpawnType},
		{"addZone", l_director_addZone},
		{NULL, NULL}
	};
	registerTable(L, "Director", directorFuncs);
#endif

	static const luaL_Reg navFuncs[] = {
		{"init", l_nav_init}, {"buildFromBSP", l_nav_buildFromBSP},
		{"findPath", l_nav_findPath}, {"addAgent", l_nav_addAgent},
		{NULL, NULL}
	};
	registerTable(L, "Nav", navFuncs);

	static const luaL_Reg physicsFuncs[] = {
		{"init", l_phys_init}, {"step", l_phys_step}, {"createBody", l_phys_createBody},
		{"destroyBody", l_phys_destroyBody}, {"applyImpulse", l_phys_applyImpulse},
		{"applyImpulseRadius", l_phys_applyImpulseRadius},
		{"createBox", l_phys_createBox}, {"createSphere", l_phys_createSphere},
		{"createStatic", l_phys_createStatic}, {"createShadow", l_phys_createShadow},
		{"setShadowPose", l_phys_setShadowPose}, {"createBuoyancy", l_phys_createBuoyancy},
		{"getTransform", l_phys_getTransform},
		{NULL, NULL}
	};
	registerTable(L, "Physics", physicsFuncs);

	static const luaL_Reg particleFuncs[] = {
		{"init", l_particles_init}, {"clear", l_particles_clear},
		{"emitSmoke", l_particles_emitSmoke}, {"emitSparks", l_particles_emitSparks},
		{"count", l_particles_count},
		{NULL, NULL}
	};
	registerTable(L, "Particles", particleFuncs);

	static const luaL_Reg musicFuncs[] = {
		{"init", l_music_init}, {"addLayer", l_music_addLayer},
		{"setIntensity", l_music_setIntensity}, {"addStinger", l_music_addStinger},
		{"fadeToSilence", l_music_fadeToSilence},
		{NULL, NULL}
	};
	registerTable(L, "Music", musicFuncs);

#ifdef USE_GAME_AI_MIDDLEWARE
	static const luaL_Reg faceFuncs[] = {
		{"create", l_face_create}, {"destroy", l_face_destroy},
		{"setExpression", l_face_setExpression}, {"setFlex", l_face_setFlex},
		{"setPhoneme", l_face_setPhoneme}, {"setBlinkRate", l_face_setBlinkRate},
		{"setAU", l_face_setAU}, {"setAUSide", l_face_setAUSide},
		{"getAU", l_face_getAU}, {"clearAUs", l_face_clearAUs},
		{"auName", l_face_auName},
		{NULL, NULL}
	};
	registerTable(L, "Face", faceFuncs);
	/* FACS AU enum constants on Engine.Face (AU12 = index, etc.) */
	lua_getglobal(L, "Engine");
	lua_getfield(L, -1, "Face");
	{
		int ai;
		for (ai = 0; ai < FACS_AU_COUNT; ai++) {
			lua_pushinteger(L, ai);
			lua_setfield(L, -2, Face_AUName((facsActionUnit_t)ai));
		}
		lua_pushinteger(L, FACS_SIDE_BOTH); lua_setfield(L, -2, "SIDE_BOTH");
		lua_pushinteger(L, FACS_SIDE_LEFT); lua_setfield(L, -2, "SIDE_LEFT");
		lua_pushinteger(L, FACS_SIDE_RIGHT); lua_setfield(L, -2, "SIDE_RIGHT");
	}
	lua_pop(L, 2);

	static const luaL_Reg hordeFuncs[] = {
		{"init", l_horde_init}, {"spawn", l_horde_spawn}, {"kill", l_horde_kill},
		{"setTarget", l_horde_setTarget}, {"getState", l_horde_getState},
		{"getCount", l_horde_getCount}, {"createGroup", l_horde_createGroup},
		{NULL, NULL}
	};
	registerTable(L, "Horde", hordeFuncs);

	static const luaL_Reg dismemberFuncs[] = {
		{"create", l_dismember_create}, {"damage", l_dismember_damage},
		{"sever", l_dismember_sever}, {"explode", l_dismember_explode},
		{"isAttached", l_dismember_isAttached},
		{NULL, NULL}
	};
	registerTable(L, "Dismember", dismemberFuncs);

	static const luaL_Reg choreoFuncs[] = {
		{"create", l_choreo_create}, {"addActor", l_choreo_addActor},
		{"addEvent", l_choreo_addEvent}, {"play", l_choreo_play},
		{"stop", l_choreo_stop}, {"isPlaying", l_choreo_isPlaying},
		{NULL, NULL}
	};
	registerTable(L, "Choreo", choreoFuncs);

	static const luaL_Reg responseFuncs[] = {
		{"addRule", l_response_addRule}, {"addCriteria", l_response_addCriteria},
		{"addResponse", l_response_addResponse}, {"trigger", l_response_trigger},
		{NULL, NULL}
	};
	registerTable(L, "Response", responseFuncs);
#endif

#ifdef USE_GAME_AI_MIDDLEWARE
	static const luaL_Reg goapFuncs[] = {
		/* Actions */
		{"registerAction", l_goap_registerAction},
		{"setActionPrecondition", l_goap_setActionPrecondition},
		{"setActionEffect", l_goap_setActionEffect},
		{"setActionDuration", l_goap_setActionDuration},
		{"setActionRange", l_goap_setActionRange},
		{"setActionActive", l_goap_setActionActive},
		{"getActionName", l_goap_getActionName},
		/* Goals */
		{"registerGoal", l_goap_registerGoal},
		{"setGoalState", l_goap_setGoalState},
		{"setGoalActive", l_goap_setGoalActive},
		{"getGoalName", l_goap_getGoalName},
		/* Properties */
		{"defineProperty", l_goap_defineProperty},
		{"findProperty", l_goap_findProperty},
		{"getPropertyName", l_goap_getPropertyName},
		/* Agents */
		{"createAgent", l_goap_createAgent},
		{"destroyAgent", l_goap_destroyAgent},
		{"setWorldState", l_goap_setWorldState},
		{"getWorldState", l_goap_getWorldState},
		{"addAgentAction", l_goap_addAgentAction},
		{"setReplanInterval", l_goap_setReplanInterval},
		/* Planning */
		{"plan", l_goap_plan},
		{"autoPlan", l_goap_autoPlan},
		{"getCurrentAction", l_goap_getCurrentAction},
		{"advancePlan", l_goap_advancePlan},
		{"abortPlan", l_goap_abortPlan},
		{"isPlanComplete", l_goap_isPlanComplete},
		/* Blackboard */
		{"bbSetFloat", l_goap_bbSetFloat},
		{"bbGetFloat", l_goap_bbGetFloat},
		{"bbSetInt", l_goap_bbSetInt},
		{"bbGetInt", l_goap_bbGetInt},
		{"bbClear", l_goap_bbClear},
		/* Defaults + debug */
		{"registerDefaults", l_goap_registerDefaults},
		{"debugAgent", l_goap_debugAgent},
		{"debugActions", l_goap_debugActions},
		{"debugGoals", l_goap_debugGoals},
		{"getActionCount", l_goap_getActionCount},
		{"getGoalCount", l_goap_getGoalCount},
		{"getAgentCount", l_goap_getAgentCount},
		{"setMaxPlanIterations", l_goap_setMaxPlanIterations},
		{"getMaxPlanIterations", l_goap_getMaxPlanIterations},
		{"getLastPlanIterations", l_goap_getLastPlanIterations},
		{"forceReplan", l_goap_forceReplan},
		{NULL, NULL}
	};
	registerTable(L, "GOAP", goapFuncs);

	static const luaL_Reg btFuncs[] = {
		{"createTree", l_bt_createTree}, {"addNode", l_bt_addNode}, {"setRoot", l_bt_setRoot},
		{"createAgent", l_bt_createAgent}, {"destroyAgent", l_bt_destroyAgent},
		{"setAgentEntity", l_bt_setAgentEntity}, {"setAgentTarget", l_bt_setAgentTarget},
		{"setAgentContext", l_bt_setAgentContext}, {"setAnimOutput", l_bt_setAnimOutput},
		{"bbSetFloat", l_bt_bbSetFloat}, {"bbGetFloat", l_bt_bbGetFloat},
		{"bbSetInt", l_bt_bbSetInt}, {"bbGetInt", l_bt_bbGetInt},
		{"linkHordeAgent", l_bt_linkHordeAgent}, {"linkGOAPAgent", l_bt_linkGOAPAgent},
		{"getActiveCount", l_bt_getActiveCount}, {"debugAgent", l_bt_debugAgent},
		{NULL, NULL}
	};
	registerTable(L, "BT", btFuncs);
#endif

	static const luaL_Reg ecsFuncs[] = {
		{"create", l_ecs_create}, {"destroy", l_ecs_destroy},
		{"valid", l_ecs_valid}, {"count", l_ecs_count}, {"countWith", l_ecs_countWith},
		{"has", l_ecs_has}, {"add", l_ecs_add}, {"remove", l_ecs_remove},
		{"setPosition", l_ecs_setPosition}, {"getPosition", l_ecs_getPosition},
		{"setRotation", l_ecs_setRotation}, {"getRotation", l_ecs_getRotation},
		{"setScale", l_ecs_setScale}, {"getScale", l_ecs_getScale},
		{"setVelocity", l_ecs_setVelocity}, {"getVelocity", l_ecs_getVelocity},
		{"setHealth", l_ecs_setHealth}, {"getHealth", l_ecs_getHealth},
		{"setTag", l_ecs_setTag}, {"getTag", l_ecs_getTag},
		{"setGentityLink", l_ecs_setGentityLink}, {"getGentityLink", l_ecs_getGentityLink},
		{"componentFromName", l_ecs_componentFromName}, {"componentName", l_ecs_componentName},
		{"stepMotion", l_ecs_stepMotion},
		{NULL, NULL}
	};
	registerTable(L, "ECS", ecsFuncs);

#ifdef USE_GAME_AI_MIDDLEWARE
	static const luaL_Reg aimlFuncs[] = {
		{"createBot", l_aiml_createBot},
		{"destroyBot", l_aiml_destroyBot},
		{"loadFile", l_aiml_loadFile},
		{"setProperty", l_aiml_setProperty},
		{"getProperty", l_aiml_getProperty},
		{"getResponse", l_aiml_getResponse},
		{"setUserVar", l_aiml_setUserVar},
		{"getUserVar", l_aiml_getUserVar},
		{"resetUser", l_aiml_resetUser},
		{"getCategoryCount", l_aiml_getCategoryCount},
		{NULL, NULL}
	};
	registerTable(L, "AIML", aimlFuncs);

	static const luaL_Reg eventFuncs[] = {
		{"publish", l_eda_publish},
		{"pop", l_eda_pop},
		{"peek", l_eda_peek},
		{"isEnabled", l_eda_isEnabled},
		{"queueDepth", l_eda_queueDepth},
		{"drain", l_eda_drain},
		{"clear", l_eda_clear},
		{NULL, NULL}
	};
	registerTable(L, "Events", eventFuncs);
#endif

	static const luaL_Reg telemetryFuncs[] = {
		{"record", l_telem_record},
		{"get", l_telem_get},
		{"clear", l_telem_clear},
		{NULL, NULL}
	};
	registerTable(L, "Telemetry", telemetryFuncs);

	static const luaL_Reg cvarFuncs[] = {
		{"getString", l_cvars_getString},
		{"getNumber", l_cvars_getNumber},
		{"getInteger", l_cvars_getInteger},
		{"getBoolean", l_cvars_getBoolean},
		{"exists", l_cvars_exists},
		{"flags", l_cvars_flags},
		{"set", l_cvars_set},
		{"setNumber", l_cvars_setNumber},
		{"setInteger", l_cvars_setInteger},
		{"setBoolean", l_cvars_setBoolean},
		{"reset", l_cvars_reset},
		{NULL, NULL}
	};
	registerTable(L, "Cvars", cvarFuncs);

	static const luaL_Reg consoleFuncs[] = {
		{"exec", l_console_exec},
		{"addText", l_console_addText},
		{NULL, NULL}
	};
	registerTable(L, "Console", consoleFuncs);

	static const luaL_Reg p2pFuncs[] = {
		{"getSession", l_p2p_getSession},
		{"isBackupHostEligible", l_p2p_isBackupHostEligible},
		{NULL, NULL}
	};
	registerTable(L, "P2P", p2pFuncs);

	static const luaL_Reg replayFuncs[] = {
		{"frameIndex", l_replay_frame},
		{"baseTime", l_replay_baseTime},
		{NULL, NULL}
	};
	registerTable(L, "Replay", replayFuncs);

	static const luaL_Reg saveFuncs[] = {
		{"write", l_save_write},
		{"read", l_save_read},
		{"writeSlot", l_save_write},
		{"readSlot", l_save_read},
		{"lastSlot", l_save_lastSlot},
		{NULL, NULL}
	};
	registerTable(L, "Save", saveFuncs);

	static const luaL_Reg dbFuncs[] = {
		{"available", l_db_available},
		{"path", l_db_path},
		{"exec", l_db_exec},
		{"queryOne", l_db_queryOne},
		{"profileSet", l_db_profileSet},
		{"profileGet", l_db_profileGet},
		{"profileDelete", l_db_profileDelete},
		{NULL, NULL}
	};
	registerTable(L, "DB", dbFuncs);

	static const luaL_Reg questFuncs[] = {
		{"add", l_quest_add},
		{"setStage", l_quest_setStage},
		{"getStage", l_quest_getStage},
		{"count", l_quest_count},
		{NULL, NULL}
	};
	registerTable(L, "Quest", questFuncs);

	static const luaL_Reg dialogueFuncs[] = {
		{"start", l_dialogue_start},
		{"clear", l_dialogue_clear},
		{"count", l_dialogue_count},
		{NULL, NULL}
	};
	registerTable(L, "Dialogue", dialogueFuncs);

	static const luaL_Reg vdbFuncs[] = {
		{"load", l_vdb_load},
		{"free", l_vdb_free},
		{"sample", l_vdb_sample},
		{"getInfo", l_vdb_getInfo},
		{"upload", l_vdb_upload},
		{"bindAsFog", l_vdb_bindAsFog},
		{"getGridCount", l_vdb_getGridCount},
		{NULL, NULL}
	};
	registerTable(L, "VDB", vdbFuncs);

	static const luaL_Reg fogBioFuncs[] = {
		{"enabled", l_fogBio_enabled},
		{"getPhase", l_fogBio_getPhase},
		{"getMarineInfluence", l_fogBio_getMarineInfluence},
		{"getCoastKm", l_fogBio_getCoastKm},
		{"getPathogenRisk", l_fogBio_getPathogenRisk},
		{"getCommunity", l_fogBio_getCommunity},
		{"setSite", l_fogBio_setSite},
		{"setCoastKm", l_fogBio_setCoastKm},
		{"setMarineWind", l_fogBio_setMarineWind},
		{"setFogActive", l_fogBio_setFogActive},
		{"poll", l_fogBio_poll},
		{NULL, NULL}
	};
	registerTable(L, "FogBiology", fogBioFuncs);

#ifdef USE_ARC_BLANC
	static const luaL_Reg arcBlancFuncs[] = {
		{"enabled", l_arcBlanc_enabled},
		{"sampleHeight", l_arcBlanc_sampleHeight},
		{"sampleVelocity", l_arcBlanc_sampleVelocity},
		{"registerHull", l_arcBlanc_registerHull},
		{"unregisterHull", l_arcBlanc_unregisterHull},
		{"reseed", l_arcBlanc_reseed},
		{NULL, NULL}
	};
	registerTable(L, "ArcBlanc", arcBlancFuncs);
#endif

	static const luaL_Reg genomeFuncs[] = {
		{"enabled", l_genome_enabled},
		{"getDim", l_genome_getDim},
		{"create", l_genome_create},
		{"breed", l_genome_breed},
		{"mutate", l_genome_mutate},
		{"setFitness", l_genome_setFitness},
		{"getFitness", l_genome_getFitness},
		{"getGene", l_genome_getGene},
		{"setGene", l_genome_setGene},
		{"selectBest", l_genome_selectBest},
		{"selectTournament", l_genome_selectTournament},
		{"count", l_genome_count},
		{"getPhenotype", l_genome_getPhenotype},
		{"jobStatus", l_genome_jobStatus},
		{"decode", l_genome_decode},
		{NULL, NULL}
	};
	registerTable(L, "Genome", genomeFuncs);

	static const luaL_Reg spriteFuncs[] = {
		{"spawnLocal", l_sprite_spawnLocal},
		{"spawnServer", l_sprite_spawnServer},
		{NULL, NULL}
	};
	registerTable(L, "Sprites", spriteFuncs);

	static const luaL_Reg decalFuncs[] = {
		{"spawnLocal", l_decal_spawnLocal},
		{"spawnServer", l_decal_spawnServer},
		{NULL, NULL}
	};
	registerTable(L, "Decals", decalFuncs);

	static const luaL_Reg charFuncs[] = {
		{"create", l_char_create},
		{"move", l_char_move},
		{NULL, NULL}
	};
	registerTable(L, "Character", charFuncs);

	static const luaL_Reg animFuncs[] = {
		{"load", l_animgraph_load},
		{"setState", l_animgraph_setState},
		{"update", l_animgraph_update},
		{NULL, NULL}
	};
	registerTable(L, "AnimGraph", animFuncs);

	lua_setglobal(L, "Engine");
	Com_Printf("Lua bindings registered: Engine.{...,Cvars,Console,P2P,Sprites,Decals,Character,AnimGraph}\n");
}

#else /* !USE_LUA */

void LuaBindings_RegisterAll(void *luaState) {
	(void)luaState;
}

#endif /* USE_LUA */
