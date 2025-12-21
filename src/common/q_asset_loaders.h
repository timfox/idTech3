/*
===========================================================================
Asset Loader System

Pluggable asset loading system for extensible format support
===========================================================================
*/

#ifndef __Q_ASSET_LOADERS_H__
#define __Q_ASSET_LOADERS_H__

#include "q_shared.h"

// Asset types
typedef enum {
	ASSET_TYPE_MODEL,
	ASSET_TYPE_SHADER,
	ASSET_TYPE_TEXTURE,
	ASSET_TYPE_SOUND,
	ASSET_TYPE_FONT,
	ASSET_TYPE_CONFIG,
	ASSET_TYPE_MAX
} assetType_t;

// Asset loader function signatures
typedef qhandle_t (*ModelLoaderFunc)(const char *name);
typedef qhandle_t (*ShaderLoaderFunc)(const char *name);
typedef qhandle_t (*TextureLoaderFunc)(const char *name, int flags);
typedef qboolean (*SoundLoaderFunc)(const char *name, sfxHandle_t *handle);
typedef qhandle_t (*FontLoaderFunc)(const char *fontName, int pointSize, fontInfo_t *font);
typedef qboolean (*ConfigLoaderFunc)(const char *name, void *buffer, int bufferSize);

// Asset loader registration structure
typedef struct assetLoader_s {
	const char *extension;        // File extension (without dot)
	const char *description;      // Human-readable description
	assetType_t type;            // Asset type this loader handles

	// Loader functions (only one should be set based on type)
	union {
		ModelLoaderFunc loadModel;
		ShaderLoaderFunc loadShader;
		TextureLoaderFunc loadTexture;
		SoundLoaderFunc loadSound;
		FontLoaderFunc loadFont;
		ConfigLoaderFunc loadConfig;
	} loader;

	int priority;                // Higher priority loaders are tried first
	qboolean enabled;            // Can be disabled at runtime
	void *userData;              // User data for loader-specific context

	struct assetLoader_s *next; // Linked list
} assetLoader_t;

// Asset path configuration
typedef struct assetPathConfig_s {
	char modelPath[MAX_OSPATH];     // Base path for models
	char shaderPath[MAX_OSPATH];    // Base path for shaders
	char texturePath[MAX_OSPATH];   // Base path for textures
	char soundPath[MAX_OSPATH];     // Base path for sounds
	char fontPath[MAX_OSPATH];      // Base path for fonts
	char configPath[MAX_OSPATH];    // Base path for configs

	// Custom search paths (mod support)
	char customPaths[8][MAX_OSPATH]; // Up to 8 custom search paths
	int numCustomPaths;
} assetPathConfig_t;

// Global asset system
extern assetPathConfig_t assetPaths;

// Initialize asset loader system
void Asset_LoadersInit(void);

// Shutdown asset loader system
void Asset_LoadersShutdown(void);

// Register an asset loader
qboolean Asset_RegisterLoader(assetLoader_t *loader);

// Unregister an asset loader
qboolean Asset_UnregisterLoader(const char *extension, assetType_t type);

// Find loader for extension and type
assetLoader_t *Asset_FindLoader(const char *extension, assetType_t type);

// Load asset using appropriate loader
qhandle_t Asset_LoadModel(const char *name);
qhandle_t Asset_LoadShader(const char *name);
qhandle_t Asset_LoadTexture(const char *name, int flags);
qboolean Asset_LoadSound(const char *name, sfxHandle_t *handle);
qhandle_t Asset_LoadFont(const char *fontName, int pointSize, fontInfo_t *font);
qboolean Asset_LoadConfig(const char *name, void *buffer, int bufferSize);

// Path management
void Asset_SetModelPath(const char *path);
void Asset_SetShaderPath(const char *path);
void Asset_SetTexturePath(const char *path);
void Asset_SetSoundPath(const char *path);
void Asset_SetFontPath(const char *path);
void Asset_SetConfigPath(const char *path);

// Add custom search path
qboolean Asset_AddSearchPath(const char *path);

// Resolve full path for asset
const char *Asset_ResolvePath(const char *name, assetType_t type, char *resolvedPath, int maxLength);

// Register CVars for asset paths
void Asset_RegisterPathCVars(void);

// Built-in loader registration functions
void Asset_RegisterBuiltInLoaders(void);

// Model format loaders
qhandle_t Asset_LoadMD3(const char *name);
qhandle_t Asset_LoadMDR(const char *name);
qhandle_t Asset_LoadIQM(const char *name);
qhandle_t Asset_LoadOBJ(const char *name);

// Shader loaders
qhandle_t Asset_LoadShaderFile(const char *name);

// Texture loaders
qhandle_t Asset_LoadTGA(const char *name, int flags);
qhandle_t Asset_LoadJPG(const char *name, int flags);
qhandle_t Asset_LoadPNG(const char *name, int flags);

// Font loaders
qhandle_t Asset_LoadTTF(const char *fontName, int pointSize, fontInfo_t *font);

#endif // __Q_ASSET_LOADERS_H__
