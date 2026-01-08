/*
===========================================================================
JSON-based audio configuration system using enhanced cvar KVP functionality.
Provides structured configuration for multiple audio sources and spatial settings.
===========================================================================
*/

#include "client.h"
#include "snd_public.h"
#include "qcommon.h"

/*
================
Audio Configuration JSON Schema

This defines the structure for audio configuration using JSON cvars.
Example configuration:

{
  "general": {
    "master_volume": 1.0,
    "music_volume": 0.8,
    "sfx_volume": 1.0,
    "voice_volume": 1.0,
    "mute_when_minimized": true,
    "mute_when_unfocused": false
  },
  "spatial": {
    "enabled": true,
    "model": "hrtf",
    "distance_model": "inverse_clamped",
    "max_distance": 1000.0,
    "rolloff_factor": 1.0,
    "reference_distance": 100.0,
    "doppler_factor": 1.0,
    "doppler_velocity": 343.0
  },
  "sources": {
    "max_sources": 32,
    "reserved_sources": 4,
    "priority_system": {
      "enabled": true,
      "distance_bias": 0.7,
      "volume_bias": 0.3
    }
  },
  "effects": {
    "reverb": {
      "enabled": true,
      "preset": "generic",
      "wet_mix": 0.3,
      "dry_mix": 0.7,
      "decay_time": 1.5,
      "pre_delay": 0.1
    },
    "occlusion": {
      "enabled": true,
      "factor": 0.5,
      "lowpass_cutoff": 5000.0
    },
    "echo": {
      "enabled": false,
      "delay": 0.3,
      "feedback": 0.2,
      "wet_mix": 0.4
    }
  },
  "performance": {
    "hrtf": "auto",
    "sample_rate": 44100,
    "buffer_size": 1024,
    "update_frequency": 30
  }
}
================
*/

static cvar_t *snd_config_json = NULL;

// Function prototypes
void SND_InitConfigurationJSON(void);
void SND_GetGeneralConfig(float *master_volume, float *music_volume, float *sfx_volume, float *voice_volume,
    qboolean *mute_minimized, qboolean *mute_unfocused);
void SND_GetSpatialConfig(qboolean *enabled, const char **model, float *max_distance,
    float *rolloff_factor, float *reference_distance,
    float *doppler_factor, float *doppler_velocity);
void SND_GetSourcesConfig(int *max_sources, int *reserved_sources,
    qboolean *priority_enabled, float *distance_bias, float *volume_bias);
void SND_GetEffectsConfig(qboolean *reverb_enabled, const char **reverb_preset, float *reverb_wet_mix,
    qboolean *occlusion_enabled, float *occlusion_factor,
    qboolean *echo_enabled, float *echo_delay);
void SND_GetPerformanceConfig(const char **hrtf_mode, int *sample_rate, int *buffer_size, int *update_frequency);
void SND_UpdateGeneralConfig(float master_volume, float music_volume, float sfx_volume, float voice_volume,
    qboolean mute_minimized, qboolean mute_unfocused);
void SND_UpdateSpatialConfig(qboolean enabled, const char *model, float max_distance,
    float rolloff_factor, float reference_distance);
void SND_UpdateEffectsConfig(qboolean reverb_enabled, const char *reverb_preset, float reverb_wet_mix,
    qboolean occlusion_enabled, float occlusion_factor,
    qboolean echo_enabled, float echo_delay);
void SND_ShutdownConfigurationJSON(void);

/*
================
SND_InitConfigurationJSON

Initialize JSON-based audio configuration
================
*/
void SND_InitConfigurationJSON(void) {
	const char *default_config = "{"
		"\"general\": {"
			"\"master_volume\": 1.0,"
			"\"music_volume\": 0.8,"
			"\"sfx_volume\": 1.0,"
			"\"voice_volume\": 1.0,"
			"\"mute_when_minimized\": true,"
			"\"mute_when_unfocused\": false"
		"},"
		"\"spatial\": {"
			"\"enabled\": true,"
			"\"model\": \"inverse_clamped\","
			"\"max_distance\": 1000.0,"
			"\"rolloff_factor\": 1.0,"
			"\"reference_distance\": 100.0,"
			"\"doppler_factor\": 1.0,"
			"\"doppler_velocity\": 343.0"
		"},"
		"\"sources\": {"
			"\"max_sources\": 32,"
			"\"reserved_sources\": 4,"
			"\"priority_system\": {"
				"\"enabled\": true,"
				"\"distance_bias\": 0.7,"
				"\"volume_bias\": 0.3"
			"}"
		"},"
		"\"effects\": {"
			"\"reverb\": {"
				"\"enabled\": true,"
				"\"preset\": \"generic\","
				"\"wet_mix\": 0.3,"
				"\"dry_mix\": 0.7,"
				"\"decay_time\": 1.5,"
				"\"pre_delay\": 0.1"
			"},"
			"\"occlusion\": {"
				"\"enabled\": true,"
				"\"factor\": 0.5,"
				"\"lowpass_cutoff\": 5000.0"
			"},"
			"\"echo\": {"
				"\"enabled\": false,"
				"\"delay\": 0.3,"
				"\"feedback\": 0.2,"
				"\"wet_mix\": 0.4"
			"}"
		"},"
		"\"performance\": {"
			"\"hrtf\": \"auto\","
			"\"sample_rate\": 44100,"
			"\"buffer_size\": 1024,"
			"\"update_frequency\": 30"
		"}"
	"}";

	snd_config_json = Cvar_GetJSON("snd_config_json", default_config, CVAR_ARCHIVE);
	Cvar_SetDescription(snd_config_json, "JSON configuration for audio settings (volumes, spatial audio, effects)");
	Cvar_SetJSONValidator(snd_config_json, CVJ_TYPE_CHECK, NULL);
}

/*
================
SND_GetGeneralConfig

Retrieve general audio configuration from JSON
================
*/
void SND_GetGeneralConfig(float *master_volume, float *music_volume, float *sfx_volume, float *voice_volume,
						 qboolean *mute_minimized, qboolean *mute_unfocused) {
	if (!snd_config_json) {
		*master_volume = 1.0f;
		*music_volume = 0.8f;
		*sfx_volume = 1.0f;
		*voice_volume = 1.0f;
		*mute_minimized = qtrue;
		*mute_unfocused = qfalse;
		return;
	}

	*master_volume = (float)Cvar_GetJSONNumber("snd_config_json", "general.master_volume", 1.0);
	*music_volume = (float)Cvar_GetJSONNumber("snd_config_json", "general.music_volume", 0.8);
	*sfx_volume = (float)Cvar_GetJSONNumber("snd_config_json", "general.sfx_volume", 1.0);
	*voice_volume = (float)Cvar_GetJSONNumber("snd_config_json", "general.voice_volume", 1.0);
	*mute_minimized = Cvar_GetJSONBoolean("snd_config_json", "general.mute_when_minimized", qtrue);
	*mute_unfocused = Cvar_GetJSONBoolean("snd_config_json", "general.mute_when_unfocused", qfalse);
}

/*
================
SND_GetSpatialConfig

Retrieve spatial audio configuration from JSON
================
*/
void SND_GetSpatialConfig(qboolean *enabled, const char **model, float *max_distance,
						 float *rolloff_factor, float *reference_distance,
						 float *doppler_factor, float *doppler_velocity) {
	if (!snd_config_json) {
		*enabled = qtrue;
		*model = "inverse_clamped";
		*max_distance = 1000.0f;
		*rolloff_factor = 1.0f;
		*reference_distance = 100.0f;
		*doppler_factor = 1.0f;
		*doppler_velocity = 343.0f;
		return;
	}

	*enabled = Cvar_GetJSONBoolean("snd_config_json", "spatial.enabled", qtrue);
	*model = Cvar_GetJSONString("snd_config_json", "spatial.model", "inverse_clamped");
	*max_distance = (float)Cvar_GetJSONNumber("snd_config_json", "spatial.max_distance", 1000.0);
	*rolloff_factor = (float)Cvar_GetJSONNumber("snd_config_json", "spatial.rolloff_factor", 1.0);
	*reference_distance = (float)Cvar_GetJSONNumber("snd_config_json", "spatial.reference_distance", 100.0);
	*doppler_factor = (float)Cvar_GetJSONNumber("snd_config_json", "spatial.doppler_factor", 1.0);
	*doppler_velocity = (float)Cvar_GetJSONNumber("snd_config_json", "spatial.doppler_velocity", 343.0);
}

/*
================
SND_GetSourcesConfig

Retrieve audio sources configuration from JSON
================
*/
void SND_GetSourcesConfig(int *max_sources, int *reserved_sources,
						 qboolean *priority_enabled, float *distance_bias, float *volume_bias) {
	if (!snd_config_json) {
		*max_sources = 32;
		*reserved_sources = 4;
		*priority_enabled = qtrue;
		*distance_bias = 0.7f;
		*volume_bias = 0.3f;
		return;
	}

	*max_sources = (int)Cvar_GetJSONNumber("snd_config_json", "sources.max_sources", 32.0);
	*reserved_sources = (int)Cvar_GetJSONNumber("snd_config_json", "sources.reserved_sources", 4.0);
	*priority_enabled = Cvar_GetJSONBoolean("snd_config_json", "sources.priority_system.enabled", qtrue);
	*distance_bias = (float)Cvar_GetJSONNumber("snd_config_json", "sources.priority_system.distance_bias", 0.7);
	*volume_bias = (float)Cvar_GetJSONNumber("snd_config_json", "sources.priority_system.volume_bias", 0.3);
}

/*
================
SND_GetEffectsConfig

Retrieve audio effects configuration from JSON
================
*/
void SND_GetEffectsConfig(qboolean *reverb_enabled, const char **reverb_preset, float *reverb_wet_mix,
						 qboolean *occlusion_enabled, float *occlusion_factor,
						 qboolean *echo_enabled, float *echo_delay) {
	if (!snd_config_json) {
		*reverb_enabled = qtrue;
		*reverb_preset = "generic";
		*reverb_wet_mix = 0.3f;
		*occlusion_enabled = qtrue;
		*occlusion_factor = 0.5f;
		*echo_enabled = qfalse;
		*echo_delay = 0.3f;
		return;
	}

	*reverb_enabled = Cvar_GetJSONBoolean("snd_config_json", "effects.reverb.enabled", qtrue);
	*reverb_preset = Cvar_GetJSONString("snd_config_json", "effects.reverb.preset", "generic");
	*reverb_wet_mix = (float)Cvar_GetJSONNumber("snd_config_json", "effects.reverb.wet_mix", 0.3);
	*occlusion_enabled = Cvar_GetJSONBoolean("snd_config_json", "effects.occlusion.enabled", qtrue);
	*occlusion_factor = (float)Cvar_GetJSONNumber("snd_config_json", "effects.occlusion.factor", 0.5);
	*echo_enabled = Cvar_GetJSONBoolean("snd_config_json", "effects.echo.enabled", qfalse);
	*echo_delay = (float)Cvar_GetJSONNumber("snd_config_json", "effects.echo.delay", 0.3);
}

/*
================
SND_GetPerformanceConfig

Retrieve audio performance configuration from JSON
================
*/
void SND_GetPerformanceConfig(const char **hrtf_mode, int *sample_rate, int *buffer_size, int *update_frequency) {
	if (!snd_config_json) {
		*hrtf_mode = "auto";
		*sample_rate = 44100;
		*buffer_size = 1024;
		*update_frequency = 30;
		return;
	}

	*hrtf_mode = Cvar_GetJSONString("snd_config_json", "performance.hrtf", "auto");
	*sample_rate = (int)Cvar_GetJSONNumber("snd_config_json", "performance.sample_rate", 44100.0);
	*buffer_size = (int)Cvar_GetJSONNumber("snd_config_json", "performance.buffer_size", 1024.0);
	*update_frequency = (int)Cvar_GetJSONNumber("snd_config_json", "performance.update_frequency", 30.0);
}

/*
================
SND_UpdateGeneralConfig

Update general audio configuration in JSON
================
*/
void SND_UpdateGeneralConfig(float master_volume, float music_volume, float sfx_volume, float voice_volume,
							qboolean mute_minimized, qboolean mute_unfocused) {
	if (!snd_config_json) return;

	char json_buffer[512];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"general\": {"
				"\"master_volume\": %.2f,"
				"\"music_volume\": %.2f,"
				"\"sfx_volume\": %.2f,"
				"\"voice_volume\": %.2f,"
				"\"mute_when_minimized\": %s,"
				"\"mute_when_unfocused\": %s"
			"}"
		"}",
		master_volume,
		music_volume,
		sfx_volume,
		voice_volume,
		mute_minimized ? "true" : "false",
		mute_unfocused ? "true" : "false");

	Cvar_SetJSON("snd_config_json", json_buffer);
}

/*
================
SND_UpdateSpatialConfig

Update spatial audio configuration in JSON
================
*/
void SND_UpdateSpatialConfig(qboolean enabled, const char *model, float max_distance,
							float rolloff_factor, float reference_distance) {
	if (!snd_config_json) return;

	char json_buffer[512];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"spatial\": {"
				"\"enabled\": %s,"
				"\"model\": \"%s\","
				"\"max_distance\": %.1f,"
				"\"rolloff_factor\": %.2f,"
				"\"reference_distance\": %.1f"
			"}"
		"}",
		enabled ? "true" : "false",
		model ? model : "inverse_clamped",
		max_distance,
		rolloff_factor,
		reference_distance);

	Cvar_SetJSON("snd_config_json", json_buffer);
}

/*
================
SND_UpdateEffectsConfig

Update audio effects configuration in JSON
================
*/
void SND_UpdateEffectsConfig(qboolean reverb_enabled, const char *reverb_preset, float reverb_wet_mix,
							qboolean occlusion_enabled, float occlusion_factor,
							qboolean echo_enabled, float echo_delay) {
	if (!snd_config_json) return;

	char json_buffer[512];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"effects\": {"
				"\"reverb\": {"
					"\"enabled\": %s,"
					"\"preset\": \"%s\","
					"\"wet_mix\": %.2f"
				"},"
				"\"occlusion\": {"
					"\"enabled\": %s,"
					"\"factor\": %.2f"
				"},"
				"\"echo\": {"
					"\"enabled\": %s,"
					"\"delay\": %.2f"
				"}"
			"}"
		"}",
		reverb_enabled ? "true" : "false",
		reverb_preset ? reverb_preset : "generic",
		reverb_wet_mix,
		occlusion_enabled ? "true" : "false",
		occlusion_factor,
		echo_enabled ? "true" : "false",
		echo_delay);

	Cvar_SetJSON("snd_config_json", json_buffer);
}

/*
================
SND_ShutdownConfigurationJSON

Shutdown JSON-based audio configuration
================
*/
void SND_ShutdownConfigurationJSON(void) {
	// JSON cvars are automatically cleaned up by the cvar system
	snd_config_json = NULL;
}