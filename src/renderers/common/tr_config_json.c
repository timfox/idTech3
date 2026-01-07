/*
===========================================================================
JSON-based renderer configuration system using enhanced cvar KVP functionality.
Provides structured configuration for multisampling, texture settings, and
post-processing chains.
===========================================================================
*/

#include "tr_common.h"
#include "qcommon.h"

/*
================
Renderer Configuration JSON Schema

This defines the structure for renderer configuration using JSON cvars.
Example configuration:

{
  "multisampling": {
    "enabled": true,
    "samples": 4,
    "quality": "high"
  },
  "texture": {
    "anisotropy": 16,
    "filtering": "trilinear",
    "compression": "bc7",
    "streaming": {
      "enabled": true,
      "budget_mb": 256,
      "preload_mips": 2
    }
  },
  "postprocessing": {
    "bloom": {
      "enabled": true,
      "intensity": 0.8,
      "threshold": 0.7
    },
    "tone_mapping": {
      "operator": "filmic",
      "exposure": 1.0
    },
    "color_grading": {
      "enabled": false,
      "lut": "neutral"
    }
  }
}
================
*/

static cvar_t *r_config_json = NULL;

/*
================
R_InitConfigurationJSON

Initialize JSON-based renderer configuration
================
*/
void R_InitConfigurationJSON(void) {
	const char *default_config = "{"
		"\"multisampling\": {"
			"\"enabled\": false,"
			"\"samples\": 4,"
			"\"quality\": \"high\""
		"},"
		"\"texture\": {"
			"\"anisotropy\": 8,"
			"\"filtering\": \"trilinear\","
			"\"compression\": \"bc3\","
			"\"streaming\": {"
				"\"enabled\": true,"
				"\"budget_mb\": 128,"
				"\"preload_mips\": 1"
			"}"
		"},"
		"\"postprocessing\": {"
			"\"bloom\": {"
				"\"enabled\": true,"
				"\"intensity\": 0.5,"
				"\"threshold\": 0.8"
			"},"
			"\"tone_mapping\": {"
				"\"operator\": \"reinhard\","
				"\"exposure\": 1.0"
			"},"
			"\"color_grading\": {"
				"\"enabled\": false,"
				"\"lut\": \"neutral\""
			"}"
		"}"
	"}";

	r_config_json = Cvar_GetJSON("r_config_json", default_config, CVAR_ARCHIVE);
	Cvar_SetDescription(r_config_json, "JSON configuration for renderer settings (multisampling, textures, post-processing)");
	Cvar_SetJSONValidator(r_config_json, CVJ_TYPE_CHECK, NULL);
}

/*
================
R_GetMultisamplingConfig

Retrieve multisampling configuration from JSON
================
*/
void R_GetMultisamplingConfig(qboolean *enabled, int *samples, const char **quality) {
	if (!r_config_json) {
		*enabled = qfalse;
		*samples = 4;
		*quality = "high";
		return;
	}

	*enabled = Cvar_GetJSONBoolean("r_config_json", "multisampling.enabled", qfalse);
	*samples = (int)Cvar_GetJSONNumber("r_config_json", "multisampling.samples", 4.0);
	*quality = Cvar_GetJSONString("r_config_json", "multisampling.quality", "high");
}

/*
================
R_GetTextureConfig

Retrieve texture configuration from JSON
================
*/
void R_GetTextureConfig(int *anisotropy, const char **filtering, const char **compression,
					   qboolean *streaming_enabled, int *streaming_budget_mb, int *preload_mips) {
	if (!r_config_json) {
		*anisotropy = 8;
		*filtering = "trilinear";
		*compression = "bc3";
		*streaming_enabled = qtrue;
		*streaming_budget_mb = 128;
		*preload_mips = 1;
		return;
	}

	*anisotropy = (int)Cvar_GetJSONNumber("r_config_json", "texture.anisotropy", 8.0);
	*filtering = Cvar_GetJSONString("r_config_json", "texture.filtering", "trilinear");
	*compression = Cvar_GetJSONString("r_config_json", "texture.compression", "bc3");
	*streaming_enabled = Cvar_GetJSONBoolean("r_config_json", "texture.streaming.enabled", qtrue);
	*streaming_budget_mb = (int)Cvar_GetJSONNumber("r_config_json", "texture.streaming.budget_mb", 128.0);
	*preload_mips = (int)Cvar_GetJSONNumber("r_config_json", "texture.streaming.preload_mips", 1.0);
}

/*
================
R_GetPostProcessingConfig

Retrieve post-processing configuration from JSON
================
*/
void R_GetPostProcessingConfig(qboolean *bloom_enabled, float *bloom_intensity, float *bloom_threshold,
							  const char **tone_operator, float *tone_exposure,
							  qboolean *color_grading_enabled, const char **color_grading_lut) {
	if (!r_config_json) {
		*bloom_enabled = qtrue;
		*bloom_intensity = 0.5f;
		*bloom_threshold = 0.8f;
		*tone_operator = "reinhard";
		*tone_exposure = 1.0f;
		*color_grading_enabled = qfalse;
		*color_grading_lut = "neutral";
		return;
	}

	*bloom_enabled = Cvar_GetJSONBoolean("r_config_json", "postprocessing.bloom.enabled", qtrue);
	*bloom_intensity = (float)Cvar_GetJSONNumber("r_config_json", "postprocessing.bloom.intensity", 0.5);
	*bloom_threshold = (float)Cvar_GetJSONNumber("r_config_json", "postprocessing.bloom.threshold", 0.8);
	*tone_operator = Cvar_GetJSONString("r_config_json", "postprocessing.tone_mapping.operator", "reinhard");
	*tone_exposure = (float)Cvar_GetJSONNumber("r_config_json", "postprocessing.tone_mapping.exposure", 1.0);
	*color_grading_enabled = Cvar_GetJSONBoolean("r_config_json", "postprocessing.color_grading.enabled", qfalse);
	*color_grading_lut = Cvar_GetJSONString("r_config_json", "postprocessing.color_grading.lut", "neutral");
}

/*
================
R_UpdateMultisamplingConfig

Update multisampling configuration in JSON
================
*/
void R_UpdateMultisamplingConfig(qboolean enabled, int samples, const char *quality) {
	if (!r_config_json) return;

	char json_buffer[1024];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"multisampling\": {"
				"\"enabled\": %s,"
				"\"samples\": %d,"
				"\"quality\": \"%s\""
			"}"
		"}",
		enabled ? "true" : "false",
		samples,
		quality ? quality : "high");

	Cvar_SetJSON("r_config_json", json_buffer);
}

/*
================
R_UpdateTextureConfig

Update texture configuration in JSON
================
*/
void R_UpdateTextureConfig(int anisotropy, const char *filtering, const char *compression,
						  qboolean streaming_enabled, int streaming_budget_mb, int preload_mips) {
	if (!r_config_json) return;

	char json_buffer[1024];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"texture\": {"
				"\"anisotropy\": %d,"
				"\"filtering\": \"%s\","
				"\"compression\": \"%s\","
				"\"streaming\": {"
					"\"enabled\": %s,"
					"\"budget_mb\": %d,"
					"\"preload_mips\": %d"
				"}"
			"}"
		"}",
		anisotropy,
		filtering ? filtering : "trilinear",
		compression ? compression : "bc3",
		streaming_enabled ? "true" : "false",
		streaming_budget_mb,
		preload_mips);

	Cvar_SetJSON("r_config_json", json_buffer);
}

/*
================
R_UpdatePostProcessingConfig

Update post-processing configuration in JSON
================
*/
void R_UpdatePostProcessingConfig(qboolean bloom_enabled, float bloom_intensity, float bloom_threshold,
								 const char *tone_operator, float tone_exposure,
								 qboolean color_grading_enabled, const char *color_grading_lut) {
	if (!r_config_json) return;

	char json_buffer[1024];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"postprocessing\": {"
				"\"bloom\": {"
					"\"enabled\": %s,"
					"\"intensity\": %.2f,"
					"\"threshold\": %.2f"
				"},"
				"\"tone_mapping\": {"
					"\"operator\": \"%s\","
					"\"exposure\": %.2f"
				"},"
				"\"color_grading\": {"
					"\"enabled\": %s,"
					"\"lut\": \"%s\""
				"}"
			"}"
		"}",
		bloom_enabled ? "true" : "false",
		bloom_intensity,
		bloom_threshold,
		tone_operator ? tone_operator : "reinhard",
		tone_exposure,
		color_grading_enabled ? "true" : "false",
		color_grading_lut ? color_grading_lut : "neutral");

	Cvar_SetJSON("r_config_json", json_buffer);
}

/*
================
R_ShutdownConfigurationJSON

Shutdown JSON-based renderer configuration
================
*/
void R_ShutdownConfigurationJSON(void) {
	// JSON cvars are automatically cleaned up by the cvar system
	r_config_json = NULL;
}