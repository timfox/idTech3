/*
===========================================================================
Scalability Configuration Implementation

Dynamic limits and configuration for modern hardware scaling
===========================================================================
*/

#include "q_scalability.h"
#include "qcommon.h"

// CVars for runtime adjustment
cvar_t *r_maxModels;
cvar_t *r_maxShaders;
cvar_t *r_maxFonts;
cvar_t *r_maxFontCache;
cvar_t *r_maxTextures;
cvar_t *r_maxLights;
cvar_t *r_autoScalability;

// Global scalability configuration
scalabilityConfig_t scalabilityConfig;

// Forward declarations
extern int Sys_GetPhysicalMemoryMB(void);
extern int Sys_GetNumCPUCores(void);

// Initialize scalability system
void Scalability_Init(void) {
	Com_Memset(&scalabilityConfig, 0, sizeof(scalabilityConfig));

	// Register CVars
	Scalability_RegisterCVars();

	// Auto-detect or use defaults
	if (r_autoScalability && r_autoScalability->integer) {
		Scalability_AutoDetect();
	} else {
		// Use configured limits
		scalabilityConfig.maxModels = r_maxModels ? r_maxModels->integer : DEFAULT_MAX_MODELS;
		scalabilityConfig.maxShaders = r_maxShaders ? r_maxShaders->integer : DEFAULT_MAX_SHADERS;
		scalabilityConfig.maxFonts = r_maxFonts ? r_maxFonts->integer : DEFAULT_MAX_FONTS;
		scalabilityConfig.maxFontCache = r_maxFontCache ? r_maxFontCache->integer : DEFAULT_MAX_FONT_CACHE;
		scalabilityConfig.maxTextures = r_maxTextures ? r_maxTextures->integer : DEFAULT_MAX_TEXTURES;
		scalabilityConfig.maxLights = r_maxLights ? r_maxLights->integer : DEFAULT_MAX_LIGHTS;
	}

	// Clamp to reasonable limits
	scalabilityConfig.maxModels = Com_Clamp(64, MAX_MODELS_LIMIT, scalabilityConfig.maxModels);
	scalabilityConfig.maxShaders = Com_Clamp(128, MAX_SHADERS_LIMIT, scalabilityConfig.maxShaders);
	scalabilityConfig.maxFonts = Com_Clamp(4, MAX_FONTS_LIMIT, scalabilityConfig.maxFonts);
	scalabilityConfig.maxFontCache = Com_Clamp(8, MAX_FONT_CACHE_LIMIT, scalabilityConfig.maxFontCache);
	scalabilityConfig.maxTextures = Com_Clamp(256, MAX_TEXTURES_LIMIT, scalabilityConfig.maxTextures);
	scalabilityConfig.maxLights = Com_Clamp(32, MAX_LIGHTS_LIMIT, scalabilityConfig.maxLights);

	Com_Printf("Scalability initialized:\n");
	Com_Printf("  Max Models: %d\n", scalabilityConfig.maxModels);
	Com_Printf("  Max Shaders: %d\n", scalabilityConfig.maxShaders);
	Com_Printf("  Max Fonts: %d\n", scalabilityConfig.maxFonts);
	Com_Printf("  Max Font Cache: %d\n", scalabilityConfig.maxFontCache);
	Com_Printf("  Max Textures: %d\n", scalabilityConfig.maxTextures);
	Com_Printf("  Max Lights: %d\n", scalabilityConfig.maxLights);
}

// Shutdown and free resources
void Scalability_Shutdown(void) {
	// Free dynamically allocated arrays if any
	if (scalabilityConfig.modelArray) {
		Z_Free(scalabilityConfig.modelArray);
		scalabilityConfig.modelArray = NULL;
	}

	if (scalabilityConfig.shaderArray) {
		Z_Free(scalabilityConfig.shaderArray);
		scalabilityConfig.shaderArray = NULL;
	}

	if (scalabilityConfig.fontArray) {
		Z_Free(scalabilityConfig.fontArray);
		scalabilityConfig.fontArray = NULL;
	}

	if (scalabilityConfig.textureArray) {
		Z_Free(scalabilityConfig.textureArray);
		scalabilityConfig.textureArray = NULL;
	}

	Com_Memset(&scalabilityConfig, 0, sizeof(scalabilityConfig));
}

// Auto-detect hardware capabilities and set appropriate limits
void Scalability_AutoDetect(void) {
	int physicalMemoryMB = Sys_GetPhysicalMemoryMB();
	int numCPUCores = Sys_GetNumCPUCores();

	Com_Printf("Auto-detecting scalability limits...\n");
	Com_Printf("  Physical Memory: %d MB\n", physicalMemoryMB);
	Com_Printf("  CPU Cores: %d\n", numCPUCores);

	// Base limits for low-end hardware (512MB RAM, 2 cores)
	int baseModels = 512;
	int baseShaders = 1024;
	int baseFonts = 8;
	int baseFontCache = 32;
	int baseTextures = 2048;
	int baseLights = 128;

	// Scale based on memory (more memory = more assets)
	float memoryScale = Com_Clamp(0.5f, 4.0f, physicalMemoryMB / 2048.0f); // Normalize to 2GB baseline

	// Scale based on CPU cores (more cores = more concurrent processing)
	float cpuScale = Com_Clamp(0.5f, 2.0f, numCPUCores / 4.0f); // Normalize to 4 cores baseline

	// Combined scale factor
	float totalScale = memoryScale * cpuScale;

	scalabilityConfig.maxModels = (int)(baseModels * totalScale);
	scalabilityConfig.maxShaders = (int)(baseShaders * totalScale);
	scalabilityConfig.maxFonts = (int)(baseFonts * Com_Clamp(0.5f, 2.0f, memoryScale)); // Fonts scale with memory
	scalabilityConfig.maxFontCache = (int)(baseFontCache * Com_Clamp(0.5f, 2.0f, memoryScale));
	scalabilityConfig.maxTextures = (int)(baseTextures * totalScale);
	scalabilityConfig.maxLights = (int)(baseLights * Com_Clamp(0.5f, 1.5f, cpuScale)); // Lights scale with CPU

	// Ensure minimums
	scalabilityConfig.maxModels = (scalabilityConfig.maxModels > 256) ? scalabilityConfig.maxModels : 256;
	scalabilityConfig.maxShaders = (scalabilityConfig.maxShaders > 512) ? scalabilityConfig.maxShaders : 512;
	scalabilityConfig.maxFonts = (scalabilityConfig.maxFonts > 6) ? scalabilityConfig.maxFonts : 6;
	scalabilityConfig.maxFontCache = (scalabilityConfig.maxFontCache > 16) ? scalabilityConfig.maxFontCache : 16;
	scalabilityConfig.maxTextures = (scalabilityConfig.maxTextures > 1024) ? scalabilityConfig.maxTextures : 1024;
	scalabilityConfig.maxLights = (scalabilityConfig.maxLights > 64) ? scalabilityConfig.maxLights : 64;
}

// Get current limits
int Scalability_GetMaxModels(void) { return scalabilityConfig.maxModels; }
int Scalability_GetMaxShaders(void) { return scalabilityConfig.maxShaders; }
int Scalability_GetMaxFonts(void) { return scalabilityConfig.maxFonts; }
int Scalability_GetMaxFontCache(void) { return scalabilityConfig.maxFontCache; }
int Scalability_GetMaxTextures(void) { return scalabilityConfig.maxTextures; }
int Scalability_GetMaxLights(void) { return scalabilityConfig.maxLights; }

// Set custom limits (clamped to reasonable ranges)
void Scalability_SetMaxModels(int limit) {
	scalabilityConfig.maxModels = Com_Clamp(64, MAX_MODELS_LIMIT, limit);
}
void Scalability_SetMaxShaders(int limit) {
	scalabilityConfig.maxShaders = Com_Clamp(128, MAX_SHADERS_LIMIT, limit);
}
void Scalability_SetMaxFonts(int limit) {
	scalabilityConfig.maxFonts = Com_Clamp(4, MAX_FONTS_LIMIT, limit);
}
void Scalability_SetMaxFontCache(int limit) {
	scalabilityConfig.maxFontCache = Com_Clamp(8, MAX_FONT_CACHE_LIMIT, limit);
}
void Scalability_SetMaxTextures(int limit) {
	scalabilityConfig.maxTextures = Com_Clamp(256, MAX_TEXTURES_LIMIT, limit);
}
void Scalability_SetMaxLights(int limit) {
	scalabilityConfig.maxLights = Com_Clamp(32, MAX_LIGHTS_LIMIT, limit);
}

// Check if we can allocate more of a resource type
qboolean Scalability_CanAllocateModel(void) {
	return scalabilityConfig.modelsAllocated < scalabilityConfig.maxModels;
}
qboolean Scalability_CanAllocateShader(void) {
	return scalabilityConfig.shadersAllocated < scalabilityConfig.maxShaders;
}
qboolean Scalability_CanAllocateFont(void) {
	return scalabilityConfig.fontsAllocated < scalabilityConfig.maxFonts;
}
qboolean Scalability_CanAllocateTexture(void) {
	return scalabilityConfig.texturesAllocated < scalabilityConfig.maxTextures;
}

// Register CVars for runtime adjustment
void Scalability_RegisterCVars(void) {
	r_maxModels = Cvar_Get("r_maxModels", va("%d", DEFAULT_MAX_MODELS), CVAR_ARCHIVE | CVAR_LATCH);
	r_maxShaders = Cvar_Get("r_maxShaders", va("%d", DEFAULT_MAX_SHADERS), CVAR_ARCHIVE | CVAR_LATCH);
	r_maxFonts = Cvar_Get("r_maxFonts", va("%d", DEFAULT_MAX_FONTS), CVAR_ARCHIVE | CVAR_LATCH);
	r_maxFontCache = Cvar_Get("r_maxFontCache", va("%d", DEFAULT_MAX_FONT_CACHE), CVAR_ARCHIVE | CVAR_LATCH);
	r_maxTextures = Cvar_Get("r_maxTextures", va("%d", DEFAULT_MAX_TEXTURES), CVAR_ARCHIVE | CVAR_LATCH);
	r_maxLights = Cvar_Get("r_maxLights", va("%d", DEFAULT_MAX_LIGHTS), CVAR_ARCHIVE | CVAR_LATCH);
	r_autoScalability = Cvar_Get("r_autoScalability", "1", CVAR_ARCHIVE);
}
