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

#include "q_shared.h"
#include "qcommon.h"
#include "cm_public.h"
#include "cl_gameframe.h"
#include "cl_district.h"
#include "cl_openworld.h"
#ifdef USE_OPEN_WORLD
#include "../../world/world_open.h"
#include "../../world/fog_biology.h"
#endif
#ifdef USE_ARC_BLANC
#include "../../world/arc_blanc/arc_blanc.h"
#include "../../renderers/common/tr_public.h"
extern refexport_t re;
#endif
#include "cl_particles.h"
#include "cl_map_background.h"
#include "cl_window_title.h"
#include "cl_mobilefog.h"
#include "cl_engine_sprites.h"
#include "cl_engine_decals.h"
#include "../../physics/phys_bullet.h"
#include "../../physics/phys_procedural_anim.h"
#include "../../physics/phys_middleware.h"
#include "../../physics/phys_cloth.h"
#include "../../navigation/nav_recast.h"
#ifdef USE_GAME_AI_MIDDLEWARE
#include "../../game/g_director.h"
#include "../../game/g_response.h"
#include "../../game/g_choreography.h"
#include "../../game/g_horde.h"
#include "../../game/g_dismember.h"
#include "../../game/g_goap.h"
#include "../../game/g_aiml.h"
#include "../../game/g_eda.h"
#include "../../game/g_bt.h"
#endif
#include "../../game/g_facial.h"
#include "../../game/g_engine_systems.h"

#ifdef USE_BABBLE
#include "../../../modules/dialogue/babble.h"
#include "../shell/cl_subtitles.h"

static void CL_BabbleLoad_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: babble_load <path>\n" );
		return;
	}
	Babble_LoadFile( Cmd_Argv( 1 ) );
}

static void CL_BabbleStart_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: babble_start <graph>\n" );
		return;
	}
	Babble_Start( Cmd_Argv( 1 ) );
}

static void CL_BabbleAdvance_f( void ) {
	int idx = ( Cmd_Argc() > 1 ) ? atoi( Cmd_Argv( 1 ) ) : -1;
	Babble_Advance( idx );
}
#else
#include "../shell/cl_subtitles.h"
#endif

#include "../../game/game_middleware_stubs.h"
#include "../../game/g_lua_bindings.h"
#include "client.h"
#include "../../game/ecs.h"
#include "../../audio/snd_music_adaptive.h"

static qboolean gameSystemsInitialized = qfalse;
static int activeNavMesh = -1;

static cvar_t *cl_physicsEnabled;
static cvar_t *cl_navEnabled;
static cvar_t *cl_particlesEnabled;
static cvar_t *cl_btEnabled;
static cvar_t *g_ecsMotion;
#ifdef USE_ARC_BLANC
static cvar_t *cl_arcBlancUnderwaterAuto;
static cvar_t *cl_arcBlancUnderwaterDepthBias;
static cvar_t *cl_arcBlancUnderwaterDensity;
static cvar_t *cl_arcBlancUnderwaterHeightFalloff;
static cvar_t *cl_arcBlancUnderwaterIntensity;
static cvar_t *cl_arcBlancUnderwaterAmbient;
static cvar_t *cl_arcBlancUnderwaterSun;
static cvar_t *cl_arcBlancUnderwaterNoise;
static cvar_t *cl_arcBlancUnderwaterTint;
static cvar_t *r_arcBlancUnderwater;
#endif

extern void Nav_BSP_ClearGeometry(void);
extern int  Nav_BSP_AddVertex(float x, float y, float z);
extern void Nav_BSP_AddTriangle(int v0, int v1, int v2);

#ifdef USE_ARC_BLANC
typedef struct {
	qboolean active;
	char volumetricFog[16];
	char colorMode[16];
	char tint[64];
	char density[32];
	char heightFalloff[32];
	char intensity[32];
	char ambient[32];
	char sun[32];
	char noise[32];
} arcBlancUnderwaterState_t;

static arcBlancUnderwaterState_t s_arcBlancUnderwaterState;

static void CL_ArcBlancUnderwaterRestore( void )
{
#ifdef USE_ARC_BLANC
	if ( !s_arcBlancUnderwaterState.active ) {
		return;
	}
	Cvar_Set( "r_volumetricFog", s_arcBlancUnderwaterState.volumetricFog );
	Cvar_Set( "r_volumetricFogColorMode", s_arcBlancUnderwaterState.colorMode );
	Cvar_Set( "r_volumetricFogTint", s_arcBlancUnderwaterState.tint );
	Cvar_Set( "r_volumetricFogDensity", s_arcBlancUnderwaterState.density );
	Cvar_Set( "r_volumetricFogHeightFalloff", s_arcBlancUnderwaterState.heightFalloff );
	Cvar_Set( "r_volumetricFogIntensity", s_arcBlancUnderwaterState.intensity );
	Cvar_Set( "r_volumetricFogAmbientIntensity", s_arcBlancUnderwaterState.ambient );
	Cvar_Set( "r_volumetricFogSunIntensity", s_arcBlancUnderwaterState.sun );
	Cvar_Set( "r_volumetricFogNoiseStrength", s_arcBlancUnderwaterState.noise );
	s_arcBlancUnderwaterState.active = qfalse;
	Cvar_Set( "r_arcBlancUnderwater", "0" );
#endif
}

static void CL_ArcBlancUnderwaterApply( void )
{
#ifdef USE_ARC_BLANC
	if ( !s_arcBlancUnderwaterState.active ) {
		Q_strncpyz( s_arcBlancUnderwaterState.volumetricFog, Cvar_VariableString( "r_volumetricFog" ),
			sizeof( s_arcBlancUnderwaterState.volumetricFog ) );
		Q_strncpyz( s_arcBlancUnderwaterState.colorMode, Cvar_VariableString( "r_volumetricFogColorMode" ),
			sizeof( s_arcBlancUnderwaterState.colorMode ) );
		Q_strncpyz( s_arcBlancUnderwaterState.tint, Cvar_VariableString( "r_volumetricFogTint" ),
			sizeof( s_arcBlancUnderwaterState.tint ) );
		Q_strncpyz( s_arcBlancUnderwaterState.density, Cvar_VariableString( "r_volumetricFogDensity" ),
			sizeof( s_arcBlancUnderwaterState.density ) );
		Q_strncpyz( s_arcBlancUnderwaterState.heightFalloff, Cvar_VariableString( "r_volumetricFogHeightFalloff" ),
			sizeof( s_arcBlancUnderwaterState.heightFalloff ) );
		Q_strncpyz( s_arcBlancUnderwaterState.intensity, Cvar_VariableString( "r_volumetricFogIntensity" ),
			sizeof( s_arcBlancUnderwaterState.intensity ) );
		Q_strncpyz( s_arcBlancUnderwaterState.ambient, Cvar_VariableString( "r_volumetricFogAmbientIntensity" ),
			sizeof( s_arcBlancUnderwaterState.ambient ) );
		Q_strncpyz( s_arcBlancUnderwaterState.sun, Cvar_VariableString( "r_volumetricFogSunIntensity" ),
			sizeof( s_arcBlancUnderwaterState.sun ) );
		Q_strncpyz( s_arcBlancUnderwaterState.noise, Cvar_VariableString( "r_volumetricFogNoiseStrength" ),
			sizeof( s_arcBlancUnderwaterState.noise ) );
		s_arcBlancUnderwaterState.active = qtrue;
	}
	Cvar_Set( "r_volumetricFog", "1" );
	Cvar_Set( "r_volumetricFogColorMode", "1" );
	Cvar_Set( "r_volumetricFogTint",
		cl_arcBlancUnderwaterTint ? cl_arcBlancUnderwaterTint->string : "0.12 0.32 0.42" );
	Cvar_SetValue( "r_volumetricFogDensity",
		cl_arcBlancUnderwaterDensity ? cl_arcBlancUnderwaterDensity->value : 1.6f );
	Cvar_SetValue( "r_volumetricFogHeightFalloff",
		cl_arcBlancUnderwaterHeightFalloff ? cl_arcBlancUnderwaterHeightFalloff->value : 0.02f );
	Cvar_SetValue( "r_volumetricFogIntensity",
		cl_arcBlancUnderwaterIntensity ? cl_arcBlancUnderwaterIntensity->value : 2.4f );
	Cvar_SetValue( "r_volumetricFogAmbientIntensity",
		cl_arcBlancUnderwaterAmbient ? cl_arcBlancUnderwaterAmbient->value : 0.5f );
	Cvar_SetValue( "r_volumetricFogSunIntensity",
		cl_arcBlancUnderwaterSun ? cl_arcBlancUnderwaterSun->value : 0.2f );
	Cvar_SetValue( "r_volumetricFogNoiseStrength",
		cl_arcBlancUnderwaterNoise ? cl_arcBlancUnderwaterNoise->value : 0.6f );
	Cvar_Set( "r_arcBlancUnderwater", "1" );
#endif
}
#endif

static void CL_EngineSaveWrite_f( void ) {
	int slot;
	const char *label;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "usage: engine_save_write <slot> <label>\n" );
		return;
	}
	slot = atoi( Cmd_Argv( 1 ) );
	label = Cmd_Argv( 2 );
	if ( EngineSave_WriteSlot( slot, label ) ) {
		Com_Printf( "engine_save_write: slot %d ok\n", slot );
	} else {
		Com_Printf( S_COLOR_YELLOW "engine_save_write: failed slot %d\n", slot );
	}
}

static void CL_EngineSaveRead_f( void ) {
	int slot;
	char label[128];

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: engine_save_read <slot>\n" );
		return;
	}
	slot = atoi( Cmd_Argv( 1 ) );
	if ( EngineSave_ReadSlot( slot, label, sizeof( label ) ) ) {
		Com_Printf( "engine_save_read: slot %d label \"%s\"\n", slot, label );
	} else {
		Com_Printf( S_COLOR_YELLOW "engine_save_read: empty or invalid slot %d\n", slot );
	}
}

static void CL_EngineSaveInfo_f( void ) {
	Com_Printf( "EngineSave protocol v%d (com_engine_api %d.%d)\n",
		EngineSave_ProtocolVersion(),
		IDTECH3_ENGINE_API_MAJOR, IDTECH3_ENGINE_API_MINOR );
	Com_Printf( "Paths: save/engine_slot_<N>.json (legacy .txt still reads)\n" );
}

/*
===============
CL_BuildNavMesh_f

Console command: buildnavmesh [mapname]
Extracts BSP collision geometry and builds a Recast navmesh.
Called automatically or via Lua after map load.
===============
*/
static void CL_BuildNavMesh_f(void) {
	const char *mapName;
	(void)0;

	if (!gameSystemsInitialized) {
		Com_Printf("Game systems not initialized\n");
		return;
	}

	mapName = Cmd_Argc() > 1 ? Cmd_Argv(1) : "current_map";

	Nav_BSP_ClearGeometry();

	/* Extract walkable surfaces from the collision model using raycasts.
	   Shoots a grid of downward traces and connects hits into triangles.
	   This captures actual floor geometry including stairs and ramps. */
	{
		vec3_t worldMins = {-4096, -4096, -4096};
		vec3_t worldMaxs = { 4096,  4096,  4096};
		float step = 48.0f;
		int gridW, gridH, gx, gy;
		trace_t tr;
		vec3_t start, end, mins, maxs;

		VectorSet(mins, 0, 0, 0);
		VectorSet(maxs, 0, 0, 0);

		gridW = (int)((worldMaxs[0] - worldMins[0]) / step);
		gridH = (int)((worldMaxs[1] - worldMins[1]) / step);
		if (gridW > 170) gridW = 170;
		if (gridH > 170) gridH = 170;

		for (gy = 0; gy <= gridH; gy++) {
			for (gx = 0; gx <= gridW; gx++) {
				float x = worldMins[0] + gx * step;
				float y = worldMins[1] + gy * step;

				VectorSet(start, x, y, worldMaxs[2]);
				VectorSet(end, x, y, worldMins[2]);
				CM_BoxTrace(&tr, start, end, mins, maxs, 0, CONTENTS_SOLID, qfalse);

				if (tr.fraction < 1.0f && tr.plane.normal[2] > 0.7f) {
					Nav_BSP_AddVertex(tr.endpos[0], tr.endpos[1], tr.endpos[2]);
				} else {
					Nav_BSP_AddVertex(x, y, -9999.0f);
				}
			}
		}

		for (gy = 0; gy < gridH; gy++) {
			for (gx = 0; gx < gridW; gx++) {
				int stride = gridW + 1;
				int v0 = gy * stride + gx;
				int v1 = v0 + 1;
				int v2 = v0 + stride;
				int v3 = v2 + 1;
				Nav_BSP_AddTriangle(v0, v1, v2);
				Nav_BSP_AddTriangle(v1, v3, v2);
			}
		}
	}

	activeNavMesh = Nav_BuildFromBSP(mapName, NULL);
	if (activeNavMesh >= 0) {
		Com_Printf("NavMesh built for %s (handle %d)\n", mapName, activeNavMesh);
	} else {
		Com_Printf(S_COLOR_YELLOW "NavMesh build failed for %s\n", mapName);
	}
}

void CL_InitGameSystems(void) {
	if (gameSystemsInitialized) return;

	cl_physicsEnabled  = Cvar_Get("cl_physicsEnabled",  "1", CVAR_ARCHIVE);
	cl_navEnabled      = Cvar_Get("cl_navEnabled",      "1", CVAR_ARCHIVE);
	cl_particlesEnabled = Cvar_Get("cl_particlesEnabled","1", CVAR_ARCHIVE);
	cl_btEnabled       = Cvar_Get("cl_btEnabled",       "1", CVAR_ARCHIVE);
#ifdef USE_ARC_BLANC
	cl_arcBlancUnderwaterAuto = Cvar_Get( "cl_arcBlancUnderwaterAuto", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_arcBlancUnderwaterAuto,
		"Automatically apply a submerged volumetric look when the camera goes below the Arc Blanc surface." );
	cl_arcBlancUnderwaterDepthBias = Cvar_Get( "cl_arcBlancUnderwaterDepthBias", "8", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_arcBlancUnderwaterDepthBias,
		"Extra world-space depth bias before Arc Blanc underwater auto engages." );
	cl_arcBlancUnderwaterDensity = Cvar_Get( "cl_arcBlancUnderwaterDensity", "1.6", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_arcBlancUnderwaterDensity, "Underwater auto volumetric density." );
	cl_arcBlancUnderwaterHeightFalloff = Cvar_Get( "cl_arcBlancUnderwaterHeightFalloff", "0.02", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_arcBlancUnderwaterHeightFalloff, "Underwater auto fog height falloff." );
	cl_arcBlancUnderwaterIntensity = Cvar_Get( "cl_arcBlancUnderwaterIntensity", "2.4", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_arcBlancUnderwaterIntensity, "Underwater auto fog scattering intensity." );
	cl_arcBlancUnderwaterAmbient = Cvar_Get( "cl_arcBlancUnderwaterAmbient", "0.5", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_arcBlancUnderwaterAmbient, "Underwater auto ambient scattering intensity." );
	cl_arcBlancUnderwaterSun = Cvar_Get( "cl_arcBlancUnderwaterSun", "0.2", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_arcBlancUnderwaterSun, "Underwater auto directional scattering intensity." );
	cl_arcBlancUnderwaterNoise = Cvar_Get( "cl_arcBlancUnderwaterNoise", "0.6", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_arcBlancUnderwaterNoise, "Underwater auto volumetric noise strength." );
	cl_arcBlancUnderwaterTint = Cvar_Get( "cl_arcBlancUnderwaterTint", "0.12 0.32 0.42", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_arcBlancUnderwaterTint, "Underwater auto fog tint (r g b)." );
	r_arcBlancUnderwater = Cvar_Get( "r_arcBlancUnderwater", "0", CVAR_TEMP );
	Cvar_SetDescription( r_arcBlancUnderwater, "Informational mirror for Arc Blanc underwater auto state." );
#endif

	if (!Phys_Init()) {
		Com_Printf(S_COLOR_YELLOW "Warning: Physics not available, physics-dependent systems will be limited\n");
	}
	Nav_Init();
	Particles_Init();
	CL_EngineSprites_Init();
	CL_EngineDecals_Init();
	Cloth_Init();
	Music_Init();
	Face_Init();
#ifdef USE_GAME_AI_MIDDLEWARE
	Director_Init();
	Response_Init();
	Horde_Init();
	Dismember_Init();
	GOAP_Init();
	EDA_Init();
	AIML_Init();
#else
	GameMiddleware_LogDisabled();
#endif
	EngineTelemetry_Init();
	EngineReplay_Init();
	EngineSave_Init();
	EngineQuest_Init();
	EngineDialogue_Init();
#ifdef USE_BABBLE
	Babble_Init();
	Cmd_AddCommand( "babble_load", CL_BabbleLoad_f );
	Cmd_AddCommand( "babble_start", CL_BabbleStart_f );
	Cmd_AddCommand( "babble_advance", CL_BabbleAdvance_f );
#endif
	CL_Subtitles_Init();
	{
		extern void CL_UberEffects_Init( void );
		CL_UberEffects_Init();
	}
#ifdef USE_GAME_AI_MIDDLEWARE
	BT_Init();
#endif
	ECS_Init();
	g_ecsMotion = Cvar_Get( "g_ecsMotion", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( g_ecsMotion, "When 1, integrate ECS velocity into position each client frame." );
	MobileFog_Init();
#ifdef USE_OPEN_WORLD
	FogBiology_Init();
#endif
#ifdef USE_ARC_BLANC
	ArcBlanc_Init();
#endif
	BgMap_Init();
	WinTitle_Init();

	Cmd_AddCommand("buildnavmesh", CL_BuildNavMesh_f);
	Cmd_AddCommand( "engine_save_write", CL_EngineSaveWrite_f );
	Cmd_AddCommand( "engine_save_read", CL_EngineSaveRead_f );
	Cmd_AddCommand( "engine_save_info", CL_EngineSaveInfo_f );

	gameSystemsInitialized = qtrue;
	Com_Printf("Game systems initialized (physics, navigation, particles, AI, BT, audio)\n");
}

void CL_ShutdownGameSystems(void) {
	if (!gameSystemsInitialized) return;

	Phys_Shutdown();
	Nav_Shutdown();
	Particles_Clear();
	Cloth_Shutdown();
	Music_Shutdown();
#ifdef USE_GAME_AI_MIDDLEWARE
	Director_Shutdown();
	Response_Shutdown();
	Face_Shutdown();
	Horde_Shutdown();
	Dismember_Shutdown();
	GOAP_Shutdown();
#endif
	EngineDialogue_Shutdown();
#ifdef USE_BABBLE
	Babble_Shutdown();
#endif
	EngineQuest_Shutdown();
	EngineSave_Shutdown();
	EngineReplay_Shutdown();
	EngineTelemetry_Shutdown();
#ifdef USE_GAME_AI_MIDDLEWARE
	AIML_Shutdown();
	EDA_Shutdown();
	BT_Shutdown();
#endif
	ECS_Shutdown();
#ifdef USE_OPEN_WORLD
	FogBiology_Shutdown();
#endif
#ifdef USE_ARC_BLANC
	ArcBlanc_Shutdown();
	CL_ArcBlancUnderwaterRestore();
#endif
	BgMap_Shutdown();

	activeNavMesh = -1;
	gameSystemsInitialized = qfalse;
	Com_Printf("Game systems shut down\n");
}

void CL_GameFrame(float frametime) {
	if (!gameSystemsInitialized) return;

	if ( cl.snap.valid && !CL_StockBaseq3Mode() ) {
		EngineReplay_BeginFrame( cl.snap.serverTime );
		EngineTelemetry_Record( "serverTime", (double)cl.snap.serverTime );
	}

	if (cl_physicsEnabled && cl_physicsEnabled->integer) {
		Phys_StepSimulation(frametime);
		Phys_DebugDraw();
		PhysMiddleware_Frame(frametime);
	}

#ifdef USE_ARC_BLANC
	if ( !CL_StockBaseq3Mode() ) {
		ArcBlanc_Frame( frametime );
		if ( cl_arcBlancUnderwaterAuto && cl_arcBlancUnderwaterAuto->integer && cl.snap.valid ) {
			const float waterHeight = ArcBlanc_SampleHeight( cl.snap.ps.origin[0], cl.snap.ps.origin[2] );
			const int contents = CM_PointContents( cl.snap.ps.origin, 0 );
			const float bias = cl_arcBlancUnderwaterDepthBias ? cl_arcBlancUnderwaterDepthBias->value : 8.0f;
			const qboolean underwater = ( cl.snap.ps.origin[2] < waterHeight + bias ) ||
				( contents & CONTENTS_WATER ) ? qtrue : qfalse;
			if ( underwater ) {
				CL_ArcBlancUnderwaterApply();
			} else {
				CL_ArcBlancUnderwaterRestore();
			}
		} else {
			CL_ArcBlancUnderwaterRestore();
		}
		if ( ArcBlanc_Enabled() && re.ArcBlancUploadHeightMap ) {
			static byte s_arcBlancHeightRGBA[256 * 256 * 4];
			int abW = 0, abH = 0;
			ArcBlanc_BuildHeightRGBA( s_arcBlancHeightRGBA, (int)sizeof( s_arcBlancHeightRGBA ), &abW, &abH );
			if ( abW > 0 && abH > 0 ) {
				re.ArcBlancUploadHeightMap( s_arcBlancHeightRGBA, abW, abH );
			}
		}
	} else {
		CL_ArcBlancUnderwaterRestore();
	}
#endif

	if ( !CL_StockBaseq3Mode() ) {
	CL_District_Frame();
	CL_OpenWorld_Frame();
#ifdef USE_OPEN_WORLD
	if ( FogBiology_Enabled() && cls.state == CA_ACTIVE && cl.snap.valid ) {
		FogBiology_SetPlayerOrigin( cl.snap.ps.origin );
	} else {
		FogBiology_SetPlayerOrigin( NULL );
	}
	FogBiology_Frame();
	if ( FogBiology_Enabled() ) {
		fogBioCommunity_t fogBioComm;
		FogBiology_GetCurrentCommunity( &fogBioComm );
		EngineTelemetry_Record( "fog_bio_phase", (double)FogBiology_GetPhase() );
		EngineTelemetry_Record( "fog_bio_coast_km", (double)FogBiology_GetCoastDistanceKm() );
		EngineTelemetry_Record( "fog_bio_marine", (double)FogBiology_GetMarineInfluence() );
		EngineTelemetry_Record( "fog_bio_shannon", (double)fogBioComm.shannonDiversity );
		EngineTelemetry_Record( "fog_bio_deposition", (double)fogBioComm.depositionMultiplier );
		EngineTelemetry_Record( "fog_bio_ocean_otu", (double)fogBioComm.oceanOtuFraction );
		EngineTelemetry_Record( "fog_bio_gram_neg", (double)fogBioComm.gramNegativeFraction );
		EngineTelemetry_Record( "fog_bio_pathogen_risk", (double)FogBiology_GetPathogenDepositionRisk() );
		EngineTelemetry_Record( "fog_bio_pathogen_taxa", (double)fogBioComm.pathogenTaxaScore );
	}
#endif

	if ( g_ecsMotion && g_ecsMotion->integer ) {
		ECS_StepMotion( frametime );
	}

	if ( cl_navEnabled && cl_navEnabled->integer ) {
		int navMesh = activeNavMesh;
#ifdef USE_OPEN_WORLD
		if ( navMesh < 0 && WorldOpen_IsEnabled() ) {
			navMesh = Nav_GetOpenWorldMesh();
		}
#endif
		if ( navMesh >= 0 ) {
			Nav_UpdateCrowd( navMesh, frametime );
		}
	}

	if (cl_particlesEnabled && cl_particlesEnabled->integer) {
		float timeMs = (float)Sys_Milliseconds();
		float frametimeMs = frametime * 1000.0f;
		Particles_Update(timeMs, frametimeMs);
	}

#ifdef USE_GAME_AI_MIDDLEWARE
	Director_Update(frametime);
	Music_Update(Director_GetGlobalIntensity(), frametime);

	/* Director → Horde spawning bridge */
	{
		int spawnType = Director_PickSpawnType();
		if (spawnType >= 0 && Director_ShouldSpawn(spawnType)) {
			Director_SpawnTypeActivated(spawnType);
			Director_TriggerWave(0.1f);
			Com_DPrintf("Director: spawned type %d (intensity %.2f, phase %d)\n",
				spawnType, Director_GetGlobalIntensity(), (int)Director_GetPhase());
		}
	}
#else
	Music_Update(0.0f, frametime);
#endif

#ifdef USE_GAME_AI_MIDDLEWARE
	Face_Update(frametime);
	Dismember_Update(frametime);
	GOAP_Update(frametime);
	if (cl_btEnabled && cl_btEnabled->integer)
		BT_Update(frametime);
	Choreo_Update(frametime);

	if (cl_navEnabled && cl_navEnabled->integer) {
		vec3_t hordePlayerPos = {0, 0, 0};
		Horde_Update(frametime, hordePlayerPos);

		{
			int hc = Horde_GetActiveCount();
			int hi;
			for (hi = 0; hi < hc && hi < 256; hi++) {
				vec3_t agentPos;
				hordeState_t agentState = Horde_GetAgentState(hi);
				if (agentState == HORDE_STATE_DEAD) continue;
				Horde_GetAgentPos(hi, agentPos);
				if (agentPos[0] == 0 && agentPos[1] == 0 && agentPos[2] == 0) continue;
				Particles_EmitSparks(agentPos, (vec3_t){0,0,50}, 1, 8.0f, 200.0f);
			}
		}
	}
#endif

#ifdef USE_GAME_AI_MIDDLEWARE
	EDA_Frame();
#endif

	{
		vec3_t fwd = {1,0,0}, right = {0,1,0}, up = {0,0,1}, origin = {0,0,0};
		MobileFog_Frame(origin, fwd, right, up, frametime);
	}
	} else {
		Music_Update( 0.0f, frametime );
	}

	BgMap_Frame(frametime);
	WinTitle_Update(frametime);

	if ( cl.snap.valid && !CL_StockBaseq3Mode() ) {
		EngineReplay_EndFrame();
	}
}
