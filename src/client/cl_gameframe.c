/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Game frame integration implementation.
Ticks all gameplay subsystems each client frame:
  1. Physics (Bullet rigid bodies + constraints)
  2. Procedural animation (balance, stumble, ragdoll)
  3. Navigation (Detour crowd update)
  4. Particles (update + render submission)
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "cl_gameframe.h"
#include "cl_particles.h"
#include "cl_map_background.h"
#include "cl_window_title.h"
#include "../physics/phys_bullet.h"
#include "../physics/phys_procedural_anim.h"
#include "../physics/phys_cloth.h"
#include "../navigation/nav_recast.h"
#include "../game/g_director.h"
#include "../game/g_response.h"
#include "../game/g_choreography.h"
#include "../game/g_facial.h"
#include "../game/g_horde.h"
#include "../game/g_dismember.h"
#include "../game/g_goap.h"
#include "../game/g_lua_bindings.h"
#include "../audio/snd_music_adaptive.h"

static qboolean gameSystemsInitialized = qfalse;
static int activeNavMesh = -1;

static cvar_t *cl_physicsEnabled;
static cvar_t *cl_navEnabled;
static cvar_t *cl_particlesEnabled;

void CL_InitGameSystems(void) {
	if (gameSystemsInitialized) return;

	cl_physicsEnabled  = Cvar_Get("cl_physicsEnabled",  "1", CVAR_ARCHIVE);
	cl_navEnabled      = Cvar_Get("cl_navEnabled",      "1", CVAR_ARCHIVE);
	cl_particlesEnabled = Cvar_Get("cl_particlesEnabled","1", CVAR_ARCHIVE);

	if (!Phys_Init()) {
		Com_Printf(S_COLOR_YELLOW "Warning: Physics not available, physics-dependent systems will be limited\n");
	}
	Nav_Init();
	Particles_Init();
	Cloth_Init();
	Director_Init();
	Music_Init();
	Response_Init();
	Face_Init();
	Horde_Init();
	Dismember_Init();
	GOAP_Init();
	BgMap_Init();
	WinTitle_Init();

	gameSystemsInitialized = qtrue;
	Com_Printf("Game systems initialized (physics, navigation, particles)\n");
}

void CL_ShutdownGameSystems(void) {
	if (!gameSystemsInitialized) return;

	Phys_Shutdown();
	Nav_Shutdown();
	Particles_Clear();
	Cloth_Shutdown();
	Director_Shutdown();
	Music_Shutdown();
	Response_Shutdown();
	Face_Shutdown();
	Horde_Shutdown();
	Dismember_Shutdown();
	GOAP_Shutdown();
	BgMap_Shutdown();

	activeNavMesh = -1;
	gameSystemsInitialized = qfalse;
	Com_Printf("Game systems shut down\n");
}

void CL_GameFrame(float frametime) {
	if (!gameSystemsInitialized) return;

	if (cl_physicsEnabled && cl_physicsEnabled->integer) {
		Phys_StepSimulation(frametime);
	}

	if (cl_navEnabled && cl_navEnabled->integer && activeNavMesh >= 0) {
		Nav_UpdateCrowd(activeNavMesh, frametime);
	}

	if (cl_particlesEnabled && cl_particlesEnabled->integer) {
		float timeMs = (float)Sys_Milliseconds();
		float frametimeMs = frametime * 1000.0f;
		Particles_Update(timeMs, frametimeMs);
	}

	Director_Update(frametime);
	Music_Update(Director_GetGlobalIntensity(), frametime);

	Cloth_SimulateAll(frametime);
	Face_Update(frametime);
	Dismember_Update(frametime);
	GOAP_Update(frametime);
	Choreo_Update(frametime);

	/* Horde_Update needs actual player position -- game code should
	   call Horde_Update directly with the real player origin.
	   This fallback uses origin {0,0,0} which is overridden by
	   game-side Lua calling Engine.Horde.setTarget(). */
	if (cl_navEnabled && cl_navEnabled->integer) {
		vec3_t hordePlayerPos = {0, 0, 0};
		Horde_Update(frametime, hordePlayerPos);
	}

	BgMap_Frame(frametime);
	WinTitle_Update(frametime);
}
