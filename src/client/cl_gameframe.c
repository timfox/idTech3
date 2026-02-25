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
#include "../physics/phys_bullet.h"
#include "../physics/phys_procedural_anim.h"
#include "../navigation/nav_recast.h"
#include "../game/g_director.h"
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

	Phys_Init();
	Nav_Init();
	Particles_Init();
	Director_Init();
	Music_Init();

	gameSystemsInitialized = qtrue;
	Com_Printf("Game systems initialized (physics, navigation, particles)\n");
}

void CL_ShutdownGameSystems(void) {
	if (!gameSystemsInitialized) return;

	Phys_Shutdown();
	Nav_Shutdown();
	Particles_Clear();
	Director_Shutdown();
	Music_Shutdown();

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
}
