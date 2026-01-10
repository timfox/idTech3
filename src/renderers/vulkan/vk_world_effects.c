/*
=============================================================================
World Effects System Implementation (Vulkan)
=============================================================================
*/

#include "tr_local.h"
#include "vk_world_effects.h"
#include "vk.h"

#include <string.h>
#include <math.h>

#ifdef USE_VULKAN

// CVars
cvar_t *r_worldEffects;
cvar_t *r_worldEffectsWind;
cvar_t *r_worldEffectsWindStrength;
cvar_t *r_worldEffectsWindFrequency;
cvar_t *r_worldEffectsWeather;

// Existing foliage wind CVars consumed by `vk_proc_dressing.c`
extern cvar_t *r_foliageWindStrength;
extern cvar_t *r_foliageWindFrequency;

typedef struct {
	qboolean initialized;
	qboolean enabled;
	vec3_t wind_dir;
	float wind_strength;
	float wind_frequency;
	int weather_mode;
} world_effects_system_t;

static world_effects_system_t we;

void vk_world_effects_init(void) {
	Com_Memset(&we, 0, sizeof(we));

	// Keep off by default until actual GPU implementations land.
	r_worldEffects = ri.Cvar_Get("r_worldEffects", "0", CVAR_ARCHIVE);
	r_worldEffectsWind = ri.Cvar_Get("r_worldEffectsWind", "1", CVAR_ARCHIVE);
	r_worldEffectsWindStrength = ri.Cvar_Get("r_worldEffectsWindStrength", "0.5", CVAR_ARCHIVE);
	r_worldEffectsWindFrequency = ri.Cvar_Get("r_worldEffectsWindFrequency", "0.5", CVAR_ARCHIVE);
	r_worldEffectsWeather = ri.Cvar_Get("r_worldEffectsWeather", "0", CVAR_ARCHIVE);

	// Ensure downstream wind CVars exist.
	r_foliageWindStrength = ri.Cvar_Get("r_foliageWindStrength", r_worldEffectsWindStrength->string, CVAR_ARCHIVE);
	r_foliageWindFrequency = ri.Cvar_Get("r_foliageWindFrequency", r_worldEffectsWindFrequency->string, CVAR_ARCHIVE);

	VectorSet(we.wind_dir, 1.0f, 0.0f, 0.0f);
	we.wind_strength = r_worldEffectsWindStrength->value;
	we.wind_frequency = r_worldEffectsWindFrequency->value;
	we.weather_mode = r_worldEffectsWeather->integer;

	we.initialized = qtrue;
	we.enabled = (r_worldEffects->integer != 0);

	if (we.enabled) {
		ri.Printf(PRINT_ALL, "Vulkan: World effects enabled (weather=%d)\n", we.weather_mode);
	}
}

void vk_world_effects_shutdown(void) {
	we.initialized = qfalse;
}

void vk_world_effects_update(void) {
	if (!we.initialized) {
		return;
	}

	we.enabled = (r_worldEffects->integer != 0);
	if (!we.enabled) {
		return;
	}

	we.wind_strength = r_worldEffectsWindStrength->value;
	we.wind_frequency = r_worldEffectsWindFrequency->value;
	we.weather_mode = r_worldEffectsWeather->integer;

	// Placeholder: slowly vary wind direction over time.
	if (r_worldEffectsWind->integer) {
		const float t = tr.refdef.floatTime * we.wind_frequency;
		we.wind_dir[0] = cosf(t);
		we.wind_dir[1] = 0.0f;
		we.wind_dir[2] = sinf(t);
		VectorNormalize(we.wind_dir);
	}

	// Drive foliage wind for procedural dressing.
	if (r_foliageWindStrength) {
		ri.Cvar_Set("r_foliageWindStrength", va("%.3f", we.wind_strength));
	}
	if (r_foliageWindFrequency) {
		ri.Cvar_Set("r_foliageWindFrequency", va("%.3f", we.wind_frequency));
	}
}

void vk_world_effects_render(void) {
	if (!we.initialized || !we.enabled) {
		return;
	}

	// Weather effect rendering using particle system
	// Weather mode: 0 = none, 1 = rain, 2 = snow, 3 = dust
	if (we.weather_mode == 0) {
		return; // No weather effects
	}

	// Use the CPU particle system to add weather particles
	// This integrates with the existing particle rendering pipeline
	extern void RE_AddParticle( const vec3_t origin, const vec3_t velocity, const vec3_t color, float size, float life, qhandle_t shader );
	extern qhandle_t RE_RegisterShaderNoMip( const char *name );
	
	// Get camera position for particle spawning
	vec3_t cameraPos;
	VectorCopy(tr.refdef.vieworg, cameraPos);
	
	// Spawn weather particles based on mode
	const int particlesPerFrame = 10; // Adjust based on performance
	const float spawnRadius = 512.0f; // Spawn particles around camera
	
	for (int i = 0; i < particlesPerFrame; i++) {
		vec3_t origin, velocity;
		vec3_t color;
		float size, life;
		qhandle_t shader;
		
		// Random position around camera
		origin[0] = cameraPos[0] + (rand() % (int)(spawnRadius * 2)) - spawnRadius;
		origin[1] = cameraPos[1] + (rand() % (int)(spawnRadius * 2)) - spawnRadius;
		origin[2] = cameraPos[2] + (rand() % (int)(spawnRadius * 2)) - spawnRadius;
		
		switch (we.weather_mode) {
			case 1: // Rain
				// Rain falls downward with wind influence
				velocity[0] = we.wind_dir[0] * we.wind_strength * 50.0f;
				velocity[1] = we.wind_dir[1] * we.wind_strength * 50.0f;
				velocity[2] = -200.0f + (we.wind_dir[2] * we.wind_strength * 20.0f);
				VectorSet(color, 0.7f, 0.7f, 0.8f); // Light gray/blue
				size = 2.0f;
				life = 2.0f;
				shader = RE_RegisterShaderNoMip("gfx/weather/rain");
				break;
				
			case 2: // Snow
				// Snow falls slower with more wind influence
				velocity[0] = we.wind_dir[0] * we.wind_strength * 30.0f;
				velocity[1] = we.wind_dir[1] * we.wind_strength * 30.0f;
				velocity[2] = -50.0f + (we.wind_dir[2] * we.wind_strength * 10.0f);
				VectorSet(color, 1.0f, 1.0f, 1.0f); // White
				size = 3.0f;
				life = 5.0f;
				shader = RE_RegisterShaderNoMip("gfx/weather/snow");
				break;
				
			case 3: // Dust
				// Dust particles are smaller and more wind-driven
				velocity[0] = we.wind_dir[0] * we.wind_strength * 40.0f;
				velocity[1] = we.wind_dir[1] * we.wind_strength * 40.0f;
				velocity[2] = -10.0f + (we.wind_dir[2] * we.wind_strength * 5.0f);
				VectorSet(color, 0.6f, 0.5f, 0.4f); // Brown/tan
				size = 1.0f;
				life = 3.0f;
				shader = RE_RegisterShaderNoMip("gfx/weather/dust");
				break;
				
			default:
				return; // Unknown weather mode
		}
		
		// Add particle to system
		RE_AddParticle(origin, velocity, color, size, life, shader);
	}
	
	// Note: Actual particle rendering is handled by R_RenderCPUParticles() in tr_scene.c
	// which is called from RE_RenderScene(). This function just spawns the particles.
}

#endif // USE_VULKAN

