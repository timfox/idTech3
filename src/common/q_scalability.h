/*
===========================================================================
Scalability Configuration

Dynamic limits and configuration for modern hardware scaling
===========================================================================
*/

#ifndef __Q_SCALABILITY_H__
#define __Q_SCALABILITY_H__

#include "q_shared.h"

// Default limits (conservative for older hardware)
#define DEFAULT_MAX_MODELS			1024
#define DEFAULT_MAX_SHADERS			2048
#define DEFAULT_MAX_FONTS			16
#define DEFAULT_MAX_FONT_CACHE		64
#define DEFAULT_MAX_TEXTURES		4096
#define DEFAULT_MAX_LIGHTS			256

// Maximum allowed limits (for modern hardware)
#define MAX_MODELS_LIMIT			16384
#define MAX_SHADERS_LIMIT			32768
#define MAX_FONTS_LIMIT				128
#define MAX_FONT_CACHE_LIMIT		512
#define MAX_TEXTURES_LIMIT			65536
#define MAX_LIGHTS_LIMIT			2048

// Scalability configuration structure
typedef struct scalabilityConfig_s {
	// Model limits
	int maxModels;
	int modelsAllocated;

	// Shader limits
	int maxShaders;
	int shadersAllocated;

	// Font limits
	int maxFonts;
	int fontsAllocated;
	int maxFontCache;

	// Texture limits
	int maxTextures;
	int texturesAllocated;

	// Light limits
	int maxLights;

	// Dynamic arrays (allocated at runtime)
	void *modelArray;
	void *shaderArray;
	void *fontArray;
	void *textureArray;
} scalabilityConfig_t;

// Global scalability configuration
extern scalabilityConfig_t scalabilityConfig;

// Initialize scalability system
void Scalability_Init(void);

// Shutdown and free resources
void Scalability_Shutdown(void);

// Auto-detect hardware capabilities and set appropriate limits
void Scalability_AutoDetect(void);

// Get current limits
int Scalability_GetMaxModels(void);
int Scalability_GetMaxShaders(void);
int Scalability_GetMaxFonts(void);
int Scalability_GetMaxFontCache(void);
int Scalability_GetMaxTextures(void);
int Scalability_GetMaxLights(void);

// Set custom limits (clamped to reasonable ranges)
void Scalability_SetMaxModels(int limit);
void Scalability_SetMaxShaders(int limit);
void Scalability_SetMaxFonts(int limit);
void Scalability_SetMaxFontCache(int limit);
void Scalability_SetMaxTextures(int limit);
void Scalability_SetMaxLights(int limit);

// Check if we can allocate more of a resource type
qboolean Scalability_CanAllocateModel(void);
qboolean Scalability_CanAllocateShader(void);
qboolean Scalability_CanAllocateFont(void);
qboolean Scalability_CanAllocateTexture(void);

// Register CVars for runtime adjustment
void Scalability_RegisterCVars(void);

#endif // __Q_SCALABILITY_H__
