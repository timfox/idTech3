/*
===========================================================================
Asset Loader System Implementation

Pluggable asset loading system for extensible format support
===========================================================================
*/

#include "q_asset_loaders.h"
#include "qcommon.h"

// CVars for asset paths
cvar_t *asset_modelPath;
cvar_t *asset_shaderPath;
cvar_t *asset_texturePath;
cvar_t *asset_soundPath;
cvar_t *asset_fontPath;
cvar_t *asset_configPath;

// Global asset path configuration
assetPathConfig_t assetPaths;

// Linked list of registered loaders
static assetLoader_t *assetLoaders[ASSET_TYPE_MAX] = {NULL};

// Initialize asset loader system
void Asset_LoadersInit(void) {
	Com_Memset(&assetPaths, 0, sizeof(assetPaths));

	// Set default paths
	Q_strncpyz(assetPaths.modelPath, "models", sizeof(assetPaths.modelPath));
	Q_strncpyz(assetPaths.shaderPath, "scripts", sizeof(assetPaths.shaderPath));
	Q_strncpyz(assetPaths.texturePath, "textures", sizeof(assetPaths.texturePath));
	Q_strncpyz(assetPaths.soundPath, "sound", sizeof(assetPaths.soundPath));
	Q_strncpyz(assetPaths.fontPath, "fonts", sizeof(assetPaths.fontPath));
	Q_strncpyz(assetPaths.configPath, "config", sizeof(assetPaths.configPath));

	// Register CVars
	Asset_RegisterPathCVars();

	// Register built-in loaders
	Asset_RegisterBuiltInLoaders();

	Com_Printf("Asset loader system initialized\n");
}

// Shutdown asset loader system
void Asset_LoadersShutdown(void) {
	// Free all registered loaders
	for (int i = 0; i < ASSET_TYPE_MAX; i++) {
		assetLoader_t *loader = assetLoaders[i];
		while (loader) {
			assetLoader_t *next = loader->next;
			Z_Free(loader);
			loader = next;
		}
		assetLoaders[i] = NULL;
	}

	Com_Memset(&assetPaths, 0, sizeof(assetPaths));
}

// Register an asset loader
qboolean Asset_RegisterLoader(assetLoader_t *loader) {
	if (!loader || !loader->extension || loader->type >= ASSET_TYPE_MAX) {
		return qfalse;
	}

	// Create a copy of the loader
	assetLoader_t *newLoader = Z_Malloc(sizeof(assetLoader_t));
	if (!newLoader) {
		return qfalse;
	}

	Com_Memcpy(newLoader, loader, sizeof(assetLoader_t));
	newLoader->next = NULL;

	// Insert at the beginning of the list (higher priority loaders first)
	if (!assetLoaders[loader->type]) {
		assetLoaders[loader->type] = newLoader;
	} else {
		// Find insertion point based on priority
		assetLoader_t *prev = NULL;
		assetLoader_t *curr = assetLoaders[loader->type];

		while (curr && curr->priority >= loader->priority) {
			prev = curr;
			curr = curr->next;
		}

		if (prev) {
			newLoader->next = prev->next;
			prev->next = newLoader;
		} else {
			newLoader->next = assetLoaders[loader->type];
			assetLoaders[loader->type] = newLoader;
		}
	}

	Com_Printf("Registered %s loader for %s files (%s)\n",
		newLoader->description, newLoader->extension, newLoader->enabled ? "enabled" : "disabled");

	return qtrue;
}

// Unregister an asset loader
qboolean Asset_UnregisterLoader(const char *extension, assetType_t type) {
	if (!extension || type >= ASSET_TYPE_MAX) {
		return qfalse;
	}

	assetLoader_t *prev = NULL;
	assetLoader_t *curr = assetLoaders[type];

	while (curr) {
		if (Q_stricmp(curr->extension, extension) == 0) {
			if (prev) {
				prev->next = curr->next;
			} else {
				assetLoaders[type] = curr->next;
			}

			Z_Free(curr);
			return qtrue;
		}

		prev = curr;
		curr = curr->next;
	}

	return qfalse;
}

// Find loader for extension and type
assetLoader_t *Asset_FindLoader(const char *extension, assetType_t type) {
	if (!extension || type >= ASSET_TYPE_MAX) {
		return NULL;
	}

	assetLoader_t *loader = assetLoaders[type];
	while (loader) {
		if (loader->enabled && Q_stricmp(loader->extension, extension) == 0) {
			return loader;
		}
		loader = loader->next;
	}

	return NULL;
}

// Load asset using appropriate loader
qhandle_t Asset_LoadModel(const char *name) {
	char resolvedPath[MAX_OSPATH];
	const char *fullPath = Asset_ResolvePath(name, ASSET_TYPE_MODEL, resolvedPath, sizeof(resolvedPath));

	if (!fullPath) {
		return 0;
	}

	// Extract extension
	const char *ext = COM_GetExtension(fullPath);
	if (!ext) {
		return 0;
	}

	assetLoader_t *loader = Asset_FindLoader(ext, ASSET_TYPE_MODEL);
	if (loader && loader->loader.loadModel) {
		return loader->loader.loadModel(fullPath);
	}

	return 0;
}

qhandle_t Asset_LoadShader(const char *name) {
	char resolvedPath[MAX_OSPATH];
	const char *fullPath = Asset_ResolvePath(name, ASSET_TYPE_SHADER, resolvedPath, sizeof(resolvedPath));

	if (!fullPath) {
		return 0;
	}

	// Shaders might not have extensions, try default loader
	assetLoader_t *loader = Asset_FindLoader("", ASSET_TYPE_SHADER);
	if (loader && loader->loader.loadShader) {
		return loader->loader.loadShader(fullPath);
	}

	return 0;
}

qhandle_t Asset_LoadTexture(const char *name, int flags) {
	char resolvedPath[MAX_OSPATH];
	const char *fullPath = Asset_ResolvePath(name, ASSET_TYPE_TEXTURE, resolvedPath, sizeof(resolvedPath));

	if (!fullPath) {
		return 0;
	}

	const char *ext = COM_GetExtension(fullPath);
	if (!ext) {
		return 0;
	}

	assetLoader_t *loader = Asset_FindLoader(ext, ASSET_TYPE_TEXTURE);
	if (loader && loader->loader.loadTexture) {
		return loader->loader.loadTexture(fullPath, flags);
	}

	return 0;
}

qboolean Asset_LoadSound(const char *name, sfxHandle_t *handle) {
	char resolvedPath[MAX_OSPATH];
	const char *fullPath = Asset_ResolvePath(name, ASSET_TYPE_SOUND, resolvedPath, sizeof(resolvedPath));

	if (!fullPath || !handle) {
		return qfalse;
	}

	const char *ext = COM_GetExtension(fullPath);
	if (!ext) {
		return qfalse;
	}

	assetLoader_t *loader = Asset_FindLoader(ext, ASSET_TYPE_SOUND);
	if (loader && loader->loader.loadSound) {
		return loader->loader.loadSound(fullPath, handle);
	}

	return qfalse;
}

qhandle_t Asset_LoadFont(const char *fontName, int pointSize, fontInfo_t *font) {
	char resolvedPath[MAX_OSPATH];
	const char *fullPath = Asset_ResolvePath(fontName, ASSET_TYPE_FONT, resolvedPath, sizeof(resolvedPath));

	if (!fullPath) {
		return 0;
	}

	const char *ext = COM_GetExtension(fullPath);
	if (!ext) {
		return 0;
	}

	assetLoader_t *loader = Asset_FindLoader(ext, ASSET_TYPE_FONT);
	if (loader && loader->loader.loadFont) {
		return loader->loader.loadFont(fullPath, pointSize, font);
	}

	return 0;
}

qboolean Asset_LoadConfig(const char *name, void *buffer, int bufferSize) {
	char resolvedPath[MAX_OSPATH];
	const char *fullPath = Asset_ResolvePath(name, ASSET_TYPE_CONFIG, resolvedPath, sizeof(resolvedPath));

	if (!fullPath || !buffer || bufferSize <= 0) {
		return qfalse;
	}

	const char *ext = COM_GetExtension(fullPath);
	if (!ext) {
		ext = ""; // Config files might not have extensions
	}

	assetLoader_t *loader = Asset_FindLoader(ext, ASSET_TYPE_CONFIG);
	if (loader && loader->loader.loadConfig) {
		return loader->loader.loadConfig(fullPath, buffer, bufferSize);
	}

	return qfalse;
}

// Path management
void Asset_SetModelPath(const char *path) {
	if (path) {
		Q_strncpyz(assetPaths.modelPath, path, sizeof(assetPaths.modelPath));
	}
}

void Asset_SetShaderPath(const char *path) {
	if (path) {
		Q_strncpyz(assetPaths.shaderPath, path, sizeof(assetPaths.shaderPath));
	}
}

void Asset_SetTexturePath(const char *path) {
	if (path) {
		Q_strncpyz(assetPaths.texturePath, path, sizeof(assetPaths.texturePath));
	}
}

void Asset_SetSoundPath(const char *path) {
	if (path) {
		Q_strncpyz(assetPaths.soundPath, path, sizeof(assetPaths.soundPath));
	}
}

void Asset_SetFontPath(const char *path) {
	if (path) {
		Q_strncpyz(assetPaths.fontPath, path, sizeof(assetPaths.fontPath));
	}
}

void Asset_SetConfigPath(const char *path) {
	if (path) {
		Q_strncpyz(assetPaths.configPath, path, sizeof(assetPaths.configPath));
	}
}

// Add custom search path
qboolean Asset_AddSearchPath(const char *path) {
	if (!path || assetPaths.numCustomPaths >= ARRAY_LEN(assetPaths.customPaths)) {
		return qfalse;
	}

	Q_strncpyz(assetPaths.customPaths[assetPaths.numCustomPaths],
		path, sizeof(assetPaths.customPaths[0]));
	assetPaths.numCustomPaths++;

	return qtrue;
}

// Resolve full path for asset
const char *Asset_ResolvePath(const char *name, assetType_t type, char *resolvedPath, int maxLength) {
	if (!name || !resolvedPath || maxLength <= 0) {
		return NULL;
	}

	// If name already has a path separator, use as-is
	if (strchr(name, '/') || strchr(name, '\\')) {
		Q_strncpyz(resolvedPath, name, maxLength);
		return resolvedPath;
	}

	// Prepend appropriate base path
	const char *basePath = NULL;
	switch (type) {
		case ASSET_TYPE_MODEL: basePath = assetPaths.modelPath; break;
		case ASSET_TYPE_SHADER: basePath = assetPaths.shaderPath; break;
		case ASSET_TYPE_TEXTURE: basePath = assetPaths.texturePath; break;
		case ASSET_TYPE_SOUND: basePath = assetPaths.soundPath; break;
		case ASSET_TYPE_FONT: basePath = assetPaths.fontPath; break;
		case ASSET_TYPE_CONFIG: basePath = assetPaths.configPath; break;
		default: return NULL;
	}

	if (!basePath || !basePath[0]) {
		Q_strncpyz(resolvedPath, name, maxLength);
		return resolvedPath;
	}

	Com_sprintf(resolvedPath, maxLength, "%s/%s", basePath, name);
	return resolvedPath;
}

// Register CVars for asset paths
void Asset_RegisterPathCVars(void) {
	asset_modelPath = Cvar_Get("asset_modelPath", "models", CVAR_ARCHIVE);
	asset_shaderPath = Cvar_Get("asset_shaderPath", "scripts", CVAR_ARCHIVE);
	asset_texturePath = Cvar_Get("asset_texturePath", "textures", CVAR_ARCHIVE);
	asset_soundPath = Cvar_Get("asset_soundPath", "sound", CVAR_ARCHIVE);
	asset_fontPath = Cvar_Get("asset_fontPath", "fonts", CVAR_ARCHIVE);
	asset_configPath = Cvar_Get("asset_configPath", "config", CVAR_ARCHIVE);
}

// Register built-in loaders
void Asset_RegisterBuiltInLoaders(void) {
	// Model loaders
	static assetLoader_t md3Loader = {
		"md3", "Quake 3 MD3 Model", ASSET_TYPE_MODEL,
		{ .loadModel = Asset_LoadMD3 }, 100, qtrue, NULL, NULL
	};
	Asset_RegisterLoader(&md3Loader);

	static assetLoader_t mdrLoader = {
		"mdr", "Quake 4 MDR Model", ASSET_TYPE_MODEL,
		{ .loadModel = Asset_LoadMDR }, 90, qtrue, NULL, NULL
	};
	Asset_RegisterLoader(&mdrLoader);

	static assetLoader_t iqmLoader = {
		"iqm", "Inter-Quake Model", ASSET_TYPE_MODEL,
		{ .loadModel = Asset_LoadIQM }, 80, qtrue, NULL, NULL
	};
	Asset_RegisterLoader(&iqmLoader);

	static assetLoader_t objLoader = {
		"obj", "Wavefront OBJ Model", ASSET_TYPE_MODEL,
		{ .loadModel = Asset_LoadOBJ }, 70, qtrue, NULL, NULL
	};
	Asset_RegisterLoader(&objLoader);

	// Texture loaders
	static assetLoader_t tgaLoader = {
		"tga", "Targa Image", ASSET_TYPE_TEXTURE,
		{ .loadTexture = Asset_LoadTGA }, 100, qtrue, NULL, NULL
	};
	Asset_RegisterLoader(&tgaLoader);

	static assetLoader_t jpgLoader = {
		"jpg", "JPEG Image", ASSET_TYPE_TEXTURE,
		{ .loadTexture = Asset_LoadJPG }, 90, qtrue, NULL, NULL
	};
	Asset_RegisterLoader(&jpgLoader);

	static assetLoader_t pngLoader = {
		"png", "PNG Image", ASSET_TYPE_TEXTURE,
		{ .loadTexture = Asset_LoadPNG }, 90, qtrue, NULL, NULL
	};
	Asset_RegisterLoader(&pngLoader);

	// Font loaders
	static assetLoader_t ttfLoader = {
		"ttf", "TrueType Font", ASSET_TYPE_FONT,
		{ .loadFont = Asset_LoadTTF }, 100, qtrue, NULL, NULL
	};
	Asset_RegisterLoader(&ttfLoader);
}

// Stub implementations for built-in loaders (to be implemented)
qhandle_t Asset_LoadMD3(const char *name) {
	Com_Printf("Asset_LoadMD3: %s (stub implementation)\n", name);
	return re.RegisterModel(name); // Fallback to engine
}

qhandle_t Asset_LoadMDR(const char *name) {
	Com_Printf("Asset_LoadMDR: %s (stub implementation)\n", name);
	return re.RegisterModel(name); // Fallback to engine
}

qhandle_t Asset_LoadIQM(const char *name) {
	Com_Printf("Asset_LoadIQM: %s (stub implementation)\n", name);
	return re.RegisterModel(name); // Fallback to engine
}

qhandle_t Asset_LoadOBJ(const char *name) {
	Com_Printf("Asset_LoadOBJ: %s (stub implementation)\n", name);
	return re.RegisterModel(name); // Fallback to engine
}

qhandle_t Asset_LoadShaderFile(const char *name) {
	Com_Printf("Asset_LoadShaderFile: %s (stub implementation)\n", name);
	return re.RegisterShader(name); // Fallback to engine
}

qhandle_t Asset_LoadTGA(const char *name, int flags) {
	Com_Printf("Asset_LoadTGA: %s (stub implementation)\n", name);
	return re.RegisterShaderNoMip(name); // Fallback to engine
}

qhandle_t Asset_LoadJPG(const char *name, int flags) {
	Com_Printf("Asset_LoadJPG: %s (stub implementation)\n", name);
	return re.RegisterShaderNoMip(name); // Fallback to engine
}

qhandle_t Asset_LoadPNG(const char *name, int flags) {
	Com_Printf("Asset_LoadPNG: %s (stub implementation)\n", name);
	return re.RegisterShaderNoMip(name); // Fallback to engine
}

qhandle_t Asset_LoadTTF(const char *fontName, int pointSize, fontInfo_t *font) {
	Com_Printf("Asset_LoadTTF: %s (stub implementation)\n", fontName);
	return re.RegisterFont(fontName, pointSize, font); // Fallback to engine
}
