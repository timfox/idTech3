/*
===========================================================================
JSON-based renderer configuration system using enhanced cvar KVP functionality.
Provides structured configuration for multisampling, texture settings, and
post-processing chains through traditional cvar interface.
===========================================================================
*/

#include "client.h"
#include "qcommon.h"

// Traditional cvars that renderers can access normally
cvar_t *r_multisample_enabled = NULL;
cvar_t *r_multisample_samples = NULL;
cvar_t *r_multisample_quality = NULL;
cvar_t *r_texture_anisotropy = NULL;
cvar_t *r_texture_filtering = NULL;
cvar_t *r_texture_compression = NULL;
cvar_t *r_texture_streaming_enabled = NULL;
cvar_t *r_texture_streaming_budget = NULL;
cvar_t *r_texture_streaming_preload = NULL;
cvar_t *r_bloom_enabled = NULL;
cvar_t *r_bloom_intensity = NULL;
cvar_t *r_bloom_threshold = NULL;
cvar_t *r_tone_mapping_operator = NULL;
cvar_t *r_tone_mapping_exposure = NULL;
cvar_t *r_color_grading_enabled = NULL;
cvar_t *r_color_grading_lut = NULL;

// JSON backing cvar
static cvar_t *r_config_json = NULL;

/*
================
R_InitConfigurationJSON

Initialize JSON-based renderer configuration and sync to traditional cvars
================
*/
void R_InitConfigurationJSON(void) {
	Com_Printf("JSON renderer config initialization temporarily disabled\n");
	return; // Temporarily disabled for testing

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

	// Create the JSON cvar with default values
	r_config_json = Cvar_Get("r_config_json", default_config, CVAR_ARCHIVE);
	Cvar_SetDescription(r_config_json, "JSON configuration for renderer settings (multisampling, textures, post-processing)");

	// Create traditional cvars with default values - these will be synced from JSON later
	r_multisample_enabled = Cvar_Get("r_multisample_enabled", "0", CVAR_ARCHIVE);
	r_multisample_samples = Cvar_Get("r_multisample_samples", "4", CVAR_ARCHIVE);
	r_multisample_quality = Cvar_Get("r_multisample_quality", "high", CVAR_ARCHIVE);

	r_texture_anisotropy = Cvar_Get("r_texture_anisotropy", "8", CVAR_ARCHIVE);
	r_texture_filtering = Cvar_Get("r_texture_filtering", "trilinear", CVAR_ARCHIVE);
	r_texture_compression = Cvar_Get("r_texture_compression", "bc3", CVAR_ARCHIVE);
	r_texture_streaming_enabled = Cvar_Get("r_texture_streaming_enabled", "1", CVAR_ARCHIVE);
	r_texture_streaming_budget = Cvar_Get("r_texture_streaming_budget", "128", CVAR_ARCHIVE);
	r_texture_streaming_preload = Cvar_Get("r_texture_streaming_preload", "1", CVAR_ARCHIVE);

	r_bloom_enabled = Cvar_Get("r_bloom_enabled", "1", CVAR_ARCHIVE);
	r_bloom_intensity = Cvar_Get("r_bloom_intensity", "0.50", CVAR_ARCHIVE);
	r_bloom_threshold = Cvar_Get("r_bloom_threshold", "0.80", CVAR_ARCHIVE);

	r_tone_mapping_operator = Cvar_Get("r_tone_mapping_operator", "reinhard", CVAR_ARCHIVE);
	r_tone_mapping_exposure = Cvar_Get("r_tone_mapping_exposure", "1.00", CVAR_ARCHIVE);

	r_color_grading_enabled = Cvar_Get("r_color_grading_enabled", "0", CVAR_ARCHIVE);
	r_color_grading_lut = Cvar_Get("r_color_grading_lut", "neutral", CVAR_ARCHIVE);
}

/*
================
R_SyncConfigurationFromJSON

Sync traditional cvars from JSON configuration
================
*/
void R_SyncConfigurationFromJSON(void) {
	if (!r_config_json) return;

	// Update traditional cvars from JSON
	Cvar_Set("r_multisample_enabled",
		Cvar_GetJSONBoolean("r_config_json", "multisampling.enabled", qfalse) ? "1" : "0");
	Cvar_Set("r_multisample_samples",
		va("%d", (int)Cvar_GetJSONNumber("r_config_json", "multisampling.samples", 4.0)));
	Cvar_Set("r_multisample_quality",
		Cvar_GetJSONString("r_config_json", "multisampling.quality", "high"));

	Cvar_Set("r_texture_anisotropy",
		va("%d", (int)Cvar_GetJSONNumber("r_config_json", "texture.anisotropy", 8.0)));
	Cvar_Set("r_texture_filtering",
		Cvar_GetJSONString("r_config_json", "texture.filtering", "trilinear"));
	Cvar_Set("r_texture_compression",
		Cvar_GetJSONString("r_config_json", "texture.compression", "bc3"));
	Cvar_Set("r_texture_streaming_enabled",
		Cvar_GetJSONBoolean("r_config_json", "texture.streaming.enabled", qtrue) ? "1" : "0");
	Cvar_Set("r_texture_streaming_budget",
		va("%d", (int)Cvar_GetJSONNumber("r_config_json", "texture.streaming.budget_mb", 128.0)));
	Cvar_Set("r_texture_streaming_preload",
		va("%d", (int)Cvar_GetJSONNumber("r_config_json", "texture.streaming.preload_mips", 1.0)));

	Cvar_Set("r_bloom_enabled",
		Cvar_GetJSONBoolean("r_config_json", "postprocessing.bloom.enabled", qtrue) ? "1" : "0");
	Cvar_Set("r_bloom_intensity",
		va("%.2f", Cvar_GetJSONNumber("r_config_json", "postprocessing.bloom.intensity", 0.5)));
	Cvar_Set("r_bloom_threshold",
		va("%.2f", Cvar_GetJSONNumber("r_config_json", "postprocessing.bloom.threshold", 0.8)));

	Cvar_Set("r_tone_mapping_operator",
		Cvar_GetJSONString("r_config_json", "postprocessing.tone_mapping.operator", "reinhard"));
	Cvar_Set("r_tone_mapping_exposure",
		va("%.2f", Cvar_GetJSONNumber("r_config_json", "postprocessing.tone_mapping.exposure", 1.0)));

	Cvar_Set("r_color_grading_enabled",
		Cvar_GetJSONBoolean("r_config_json", "postprocessing.color_grading.enabled", qfalse) ? "1" : "0");
	Cvar_Set("r_color_grading_lut",
		Cvar_GetJSONString("r_config_json", "postprocessing.color_grading.lut", "neutral"));
}

/*
================
R_UpdateConfigurationJSON

Update JSON configuration from traditional cvars
================
*/
void R_UpdateConfigurationJSON(void) {
	if (!r_config_json) return;

	char json_buffer[2048];
	Com_sprintf(json_buffer, sizeof(json_buffer),
		"{"
			"\"multisampling\": {"
				"\"enabled\": %s,"
				"\"samples\": %d,"
				"\"quality\": \"%s\""
			"},"
			"\"texture\": {"
				"\"anisotropy\": %d,"
				"\"filtering\": \"%s\","
				"\"compression\": \"%s\","
				"\"streaming\": {"
					"\"enabled\": %s,"
					"\"budget_mb\": %d,"
					"\"preload_mips\": %d"
				"}"
			"},"
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
		r_multisample_enabled->integer ? "true" : "false",
		r_multisample_samples->integer,
		r_multisample_quality->string,
		r_texture_anisotropy->integer,
		r_texture_filtering->string,
		r_texture_compression->string,
		r_texture_streaming_enabled->integer ? "true" : "false",
		r_texture_streaming_budget->integer,
		r_texture_streaming_preload->integer,
		r_bloom_enabled->integer ? "true" : "false",
		r_bloom_intensity->value,
		r_bloom_threshold->value,
		r_tone_mapping_operator->string,
		r_tone_mapping_exposure->value,
		r_color_grading_enabled->integer ? "true" : "false",
		r_color_grading_lut->string);

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
	r_multisample_enabled = NULL;
	r_multisample_samples = NULL;
	r_multisample_quality = NULL;
	r_texture_anisotropy = NULL;
	r_texture_filtering = NULL;
	r_texture_compression = NULL;
	r_texture_streaming_enabled = NULL;
	r_texture_streaming_budget = NULL;
	r_texture_streaming_preload = NULL;
	r_bloom_enabled = NULL;
	r_bloom_intensity = NULL;
	r_bloom_threshold = NULL;
	r_tone_mapping_operator = NULL;
	r_tone_mapping_exposure = NULL;
	r_color_grading_enabled = NULL;
	r_color_grading_lut = NULL;
}