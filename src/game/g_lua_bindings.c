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

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "g_lua_bindings.h"

#ifdef USE_LUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "g_director.h"
#include "g_facial.h"
#include "g_horde.h"
#include "g_dismember.h"
#include "g_choreography.h"
#include "g_response.h"
#include "../physics/phys_bullet.h"
#include "../physics/phys_procedural_anim.h"
#include "../physics/phys_ik.h"
#include "../navigation/nav_recast.h"
#include "../client/cl_particles.h"
#include "../audio/snd_music_adaptive.h"

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

/* ========== AIML bindings ========== */

#include "g_aiml.h"

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

/* ========== GOAP bindings ========== */

#include "g_goap.h"

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

	static const luaL_Reg navFuncs[] = {
		{"init", l_nav_init}, {"buildFromBSP", l_nav_buildFromBSP},
		{"findPath", l_nav_findPath}, {"addAgent", l_nav_addAgent},
		{NULL, NULL}
	};
	registerTable(L, "Nav", navFuncs);

	static const luaL_Reg physicsFuncs[] = {
		{"init", l_phys_init}, {"step", l_phys_step}, {"createBody", l_phys_createBody},
		{"destroyBody", l_phys_destroyBody}, {"applyImpulse", l_phys_applyImpulse},
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

	static const luaL_Reg faceFuncs[] = {
		{"create", l_face_create}, {"destroy", l_face_destroy},
		{"setExpression", l_face_setExpression}, {"setFlex", l_face_setFlex},
		{"setPhoneme", l_face_setPhoneme}, {"setBlinkRate", l_face_setBlinkRate},
		{NULL, NULL}
	};
	registerTable(L, "Face", faceFuncs);

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
		{NULL, NULL}
	};
	registerTable(L, "GOAP", goapFuncs);

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

	lua_setglobal(L, "Engine");
	Com_Printf("Lua bindings registered: Engine.{Director,Nav,Physics,Particles,Music,Face,Horde,Dismember,Choreo,Response,GOAP}\n");
}

#else /* !USE_LUA */

void LuaBindings_RegisterAll(void *luaState) {
	(void)luaState;
}

#endif /* USE_LUA */
