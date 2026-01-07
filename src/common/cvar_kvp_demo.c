/*
===========================================================================
Demonstration of JSON-enhanced cvar system with backward compatibility.
Shows how existing string-based cvars continue to work alongside new JSON cvars.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

/*
================
CVAR_KVP_Demo

Demonstrates the JSON-enhanced cvar system and backward compatibility
================
*/
void CVAR_KVP_Demo(void);

/*
================
CVAR_KVP_TestValidation

Test JSON validation functionality
================
*/
void CVAR_KVP_TestValidation(void);

void CVAR_KVP_Demo(void) {
	Com_Printf("=== CVAR KVP Demo Starting ===\n");
	return; // Temporarily disabled for testing
	Com_Printf("=== CVAR KVP System Demonstration ===\n");

	// Demonstrate backward compatibility - existing cvars still work
	Com_Printf("\n--- Backward Compatibility ---\n");
	cvar_t *existing_cvar = Cvar_Get("developer", "0", CVAR_ARCHIVE);
	Com_Printf("Existing cvar 'developer': %s (integer: %d)\n", existing_cvar->string, existing_cvar->integer);

	// Set it the old way
	Cvar_Set("developer", "1");
	Com_Printf("After Cvar_Set('developer', '1'): %s (integer: %d)\n", existing_cvar->string, existing_cvar->integer);

	// Demonstrate new JSON cvars
	Com_Printf("\n--- JSON CVAR System ---\n");

	// Create a JSON cvar for renderer settings
	const char *renderer_config = "{"
		"\"multisampling\": {"
			"\"enabled\": true,"
			"\"samples\": 4,"
			"\"quality\": \"high\""
		"},"
		"\"texture\": {"
			"\"anisotropy\": 16,"
			"\"filtering\": \"trilinear\""
		"}"
	"}";

	cvar_t *json_cvar = Cvar_GetJSON("demo_renderer_config", renderer_config, CVAR_ARCHIVE);
	Com_Printf("JSON cvar created: %s\n", json_cvar->name);
	Com_Printf("JSON cvar isJSON flag: %s\n", json_cvar->isJSON ? "true" : "false");
	Com_Printf("JSON string length: %d\n", (int)strlen(json_cvar->jsonString));

	// Demonstrate JSON value retrieval
	Com_Printf("\n--- JSON Value Retrieval ---\n");
	qboolean ms_enabled = Cvar_GetJSONBoolean("demo_renderer_config", "multisampling.enabled", qfalse);
	int ms_samples = (int)Cvar_GetJSONNumber("demo_renderer_config", "multisampling.samples", 0.0);
	const char *ms_quality = Cvar_GetJSONString("demo_renderer_config", "multisampling.quality", "low");
	int anisotropy = (int)Cvar_GetJSONNumber("demo_renderer_config", "texture.anisotropy", 1.0);
	const char *filtering = Cvar_GetJSONString("demo_renderer_config", "texture.filtering", "bilinear");

	Com_Printf("Multisampling - enabled: %s, samples: %d, quality: %s\n",
			   ms_enabled ? "true" : "false", ms_samples, ms_quality);
	Com_Printf("Texture - anisotropy: %d, filtering: %s\n", anisotropy, filtering);

	// Demonstrate JSON value updates
	Com_Printf("\n--- JSON Value Updates ---\n");
	const char *updated_config = "{"
		"\"multisampling\": {"
			"\"enabled\": false,"
			"\"samples\": 8,"
			"\"quality\": \"ultra\""
		"},"
		"\"texture\": {"
			"\"anisotropy\": 32,"
			"\"filtering\": \"anisotropic\""
		"}"
	"}";

	Cvar_SetJSON("demo_renderer_config", updated_config);

	ms_enabled = Cvar_GetJSONBoolean("demo_renderer_config", "multisampling.enabled", qfalse);
	ms_samples = (int)Cvar_GetJSONNumber("demo_renderer_config", "multisampling.samples", 0.0);
	ms_quality = Cvar_GetJSONString("demo_renderer_config", "multisampling.quality", "low");
	anisotropy = (int)Cvar_GetJSONNumber("demo_renderer_config", "texture.anisotropy", 1.0);
	filtering = Cvar_GetJSONString("demo_renderer_config", "texture.filtering", "bilinear");

	Com_Printf("After update - Multisampling - enabled: %s, samples: %d, quality: %s\n",
			   ms_enabled ? "true" : "false", ms_samples, ms_quality);
	Com_Printf("After update - Texture - anisotropy: %d, filtering: %s\n", anisotropy, filtering);

	// Demonstrate complex nested access
	Com_Printf("\n--- Complex JSON Access ---\n");
	const char *complex_config = "{"
		"\"renderer\": {"
			"\"vulkan\": {"
				"\"features\": {"
					"\"raytracing\": true,"
					"\"mesh_shaders\": false"
				"},"
				"\"performance\": {"
					"\"fps_limit\": 144,"
					"\"vsync\": true"
				"}"
			"},"
			"\"opengl\": {"
				"\"version\": \"4.6\","
				"\"extensions\": [\"ARB_bindless_texture\", \"ARB_sparse_texture\"]"
			"}"
		"},"
		"\"audio\": {"
			"\"spatial\": {"
				"\"hrtf\": \"auto\","
				"\"max_sources\": 64"
			"}"
		"}"
	"}";

	Cvar_GetJSON("demo_complex_config", complex_config, 0);

	qboolean raytracing = Cvar_GetJSONBoolean("demo_complex_config", "renderer.vulkan.features.raytracing", qfalse);
	int fps_limit = (int)Cvar_GetJSONNumber("demo_complex_config", "renderer.vulkan.performance.fps_limit", 60.0);
	const char *opengl_version = Cvar_GetJSONString("demo_complex_config", "renderer.opengl.version", "3.3");
	int max_sources = (int)Cvar_GetJSONNumber("demo_complex_config", "audio.spatial.max_sources", 32.0);

	Com_Printf("Complex config - Ray tracing: %s, FPS limit: %d, OpenGL version: %s, Max sources: %d\n",
			   raytracing ? "enabled" : "disabled", fps_limit, opengl_version, max_sources);

	// Demonstrate that both systems coexist
	Com_Printf("\n--- Coexistence Test ---\n");
	Com_Printf("JSON cvars can coexist with traditional cvars\n");
	Com_Printf("Use 'cvarlist json' to see JSON-based cvars\n");
	Com_Printf("JSON cvars provide structured configuration while maintaining backward compatibility\n");

	Com_Printf("\n=== CVAR KVP Demonstration Complete ===\n");
}

/*
================
CVAR_KVP_TestValidation

Test JSON validation functionality
================
*/
void CVAR_KVP_TestValidation(void) {
	Com_Printf("=== CVAR JSON Validation Test ===\n");

	// Test valid JSON
	cvar_t *valid_cvar = Cvar_GetJSON("test_valid_json", "{\"test\": \"value\", \"number\": 42}", 0);
	if (valid_cvar) {
		Com_Printf("✓ Valid JSON accepted\n");
	} else {
		Com_Printf("✗ Valid JSON rejected\n");
	}

	// Test invalid JSON (this should still work but log a warning)
	cvar_t *invalid_cvar = Cvar_GetJSON("test_invalid_json", "{\"test\": \"value\", \"incomplete\"", 0);
	if (invalid_cvar) {
		Com_Printf("✓ Invalid JSON handled gracefully (fallback object created)\n");
	} else {
		Com_Printf("✗ Invalid JSON caused failure\n");
	}

	// Test JSON validation
	qboolean valid = Cvar_ValidateJSON(valid_cvar, "{\"validation\": \"test\"}");
	Com_Printf("JSON validation result: %s\n", valid ? "valid" : "invalid");

	Com_Printf("=== CVAR JSON Validation Test Complete ===\n");
}