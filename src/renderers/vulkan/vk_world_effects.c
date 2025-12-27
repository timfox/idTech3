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

	// Placeholder: rendering paths (rain/snow/dust) not implemented yet.
	// This is intentionally a no-op to keep the renderer stable while the
	// system is being brought up incrementally.
}

#endif // USE_VULKAN

