/*
===========================================================================
Renderer Services Abstraction

Provides abstracted interfaces to reduce coupling between renderer backends
and core engine systems.
===========================================================================
*/

#include "tr_public.h"
#include "../../common/q_shared.h"

// Forward declarations
extern refimport_t ri;
extern trGlobals_t tr;

// Default service implementations that delegate to global state
static int R_Default_FS_ReadFile(const char *qpath, void **buffer) {
	return ri.FS_ReadFile(qpath, buffer);
}

static void R_Default_FS_FreeFile(void *buffer) {
	ri.FS_FreeFile(buffer);
}

static int R_Default_FS_WriteFile(const char *qpath, const void *buffer, int size) {
	ri.FS_WriteFile(qpath, buffer, size);
	return qtrue; // Assume success since void function
}

static void *R_Default_Hunk_Alloc(int size, ha_pref pref) {
	return ri.Hunk_Alloc(size, pref);
}

static void *R_Default_Hunk_AllocateTempMemory(int size) {
	return ri.Hunk_AllocateTempMemory(size);
}

static void R_Default_Hunk_FreeTempMemory(void *block) {
	ri.Hunk_FreeTempMemory(block);
}

/*
====================
Shader Memory Management System

A memory-safe shader loading system that avoids temp memory corruption.
This system uses regular malloc/free instead of hunk temp memory during
shader file loading and parsing to prevent LIFO violations.
====================
*/

#define MAX_SHADER_FILES_SAFE 256  // Maximum shader files for safe loading

typedef struct shaderLoadContext_s {
	char **fileBuffers;      // Array of loaded file buffers
	int *fileSizes;         // Array of file sizes
	int numFiles;          // Number of loaded files
	int maxFiles;          // Maximum capacity
	char *combinedText;    // Final combined shader text
	int combinedSize;      // Size of combined text
	qboolean initialized;  // Whether context is initialized
} shaderLoadContext_t;

static shaderLoadContext_t shaderLoadCtx;

// Forward declarations for renderer-specific functions
void ScanAndLoadShaderFiles_Safe(void);
const char *R_GetSafeShaderText(int *size);

/*
====================
R_InitShaderLoadContext

Initialize the safe shader loading context
====================
*/
static void R_InitShaderLoadContext(void) {
	Com_Memset(&shaderLoadCtx, 0, sizeof(shaderLoadCtx));
	shaderLoadCtx.maxFiles = MAX_SHADER_FILES_SAFE;
	shaderLoadCtx.fileBuffers = (char **)ri.Malloc(sizeof(char *) * shaderLoadCtx.maxFiles);
	shaderLoadCtx.fileSizes = (int *)ri.Malloc(sizeof(int) * shaderLoadCtx.maxFiles);
	shaderLoadCtx.initialized = qtrue;
}

/*
====================
R_ShutdownShaderLoadContext

Clean up the safe shader loading context
====================
*/
void R_ShutdownSafeShaderLoadContext(void) {
	ri.Printf(PRINT_ALL, "R_ShutdownShaderLoadContext called\n");
	if (!shaderLoadCtx.initialized) {
		return;
	}

	// Free all loaded file buffers
	for (int i = 0; i < shaderLoadCtx.numFiles; i++) {
		if (shaderLoadCtx.fileBuffers[i]) {
			ri.FS_FreeFile(shaderLoadCtx.fileBuffers[i]);
			shaderLoadCtx.fileBuffers[i] = NULL;
		}
	}

	// combinedText is allocated from hunk, don't free it manually
	shaderLoadCtx.combinedText = NULL;

	if (shaderLoadCtx.fileBuffers) {
		ri.Free(shaderLoadCtx.fileBuffers);
		shaderLoadCtx.fileBuffers = NULL;
	}

	if (shaderLoadCtx.fileSizes) {
		ri.Free(shaderLoadCtx.fileSizes);
		shaderLoadCtx.fileSizes = NULL;
	}

	shaderLoadCtx.initialized = qfalse;
}

/*
====================
R_LoadShaderFileSafe

Load a single shader file using safe memory management
====================
*/
static qboolean R_LoadShaderFileSafe(const char *filename, int *fileIndex) {
	if (!shaderLoadCtx.initialized || shaderLoadCtx.numFiles >= shaderLoadCtx.maxFiles) {
		return qfalse;
	}

	// Use FS_ReadFile with NULL buffer to get file size first
	int fileSize = ri.FS_ReadFile(filename, NULL);
	if (fileSize <= 0) {
		return qfalse;
	}

	// Allocate buffer using regular malloc (not temp memory)
	char *buffer = (char *)ri.Malloc(fileSize + 1);
	if (!buffer) {
		return qfalse;
	}

	// Load the actual file content
	int actualSize = ri.FS_ReadFile(filename, (void **)&buffer);
	if (actualSize != fileSize || actualSize <= 0) {
		ri.Free(buffer);
		return qfalse;
	}

	buffer[fileSize] = '\0'; // Null terminate

	// Store in context
	int index = shaderLoadCtx.numFiles++;
	shaderLoadCtx.fileBuffers[index] = buffer;
	shaderLoadCtx.fileSizes[index] = fileSize;

	if (fileIndex) {
		*fileIndex = index;
	}

	return qtrue;
}

/*
====================
R_CombineShaderFilesSafe

Combine all loaded shader files into a single text block
====================
*/
static qboolean R_CombineShaderFilesSafe(void) {
	if (!shaderLoadCtx.initialized || shaderLoadCtx.numFiles == 0) {
		return qfalse;
	}

	// Calculate total size needed
	int totalSize = 0;
	for (int i = 0; i < shaderLoadCtx.numFiles; i++) {
		totalSize += shaderLoadCtx.fileSizes[i] + 2; // +2 for "\n" separator
	}

	// Allocate combined buffer from hunk (like original code)
	shaderLoadCtx.combinedText = (char *)ri.Hunk_Alloc(totalSize + 1, h_low);
	if (!shaderLoadCtx.combinedText) {
		return qfalse;
	}

	// Combine all files
	char *dest = shaderLoadCtx.combinedText;
	for (int i = 0; i < shaderLoadCtx.numFiles; i++) {
		Com_Memcpy(dest, shaderLoadCtx.fileBuffers[i], shaderLoadCtx.fileSizes[i]);
		dest += shaderLoadCtx.fileSizes[i];
		*dest++ = '\n';
	}
	*dest = '\0';

	shaderLoadCtx.combinedSize = totalSize;

	return qtrue;
}

/*
====================
R_ParseShaderTextSafe

Parse shader text without using temp memory allocations
====================
*/
static qboolean R_ParseShaderTextSafe(const char *text) {
	if (!text || !*text) {
		return qfalse;
	}

	// Basic syntax validation - check for matching braces
	const char *parseText = text;

	// Skip whitespace at the start
	while (*parseText && (*parseText == ' ' || *parseText == '\t' || *parseText == '\n' || *parseText == '\r')) {
		parseText++;
	}

	if (!*parseText || *parseText != '{') {
		// Not a valid shader start
		return qfalse;
	}

	// Find the matching closing brace
	const char *braceEnd = parseText;
	int braceCount = 1;
	while (*braceEnd && braceCount > 0) {
		braceEnd++;
		if (*braceEnd == '{') braceCount++;
		else if (*braceEnd == '}') braceCount--;
	}

	if (braceCount != 0) {
		// Unmatched braces
		return qfalse;
	}

	return qtrue;
}

/*
====================
R_ProcessShaderFilesSafe

Process all loaded shader files safely
====================
*/
static void R_ProcessShaderFilesSafe(void) {
	if (!shaderLoadCtx.initialized || !shaderLoadCtx.combinedText) {
		return;
	}

	// Parse the combined shader text without temp memory allocations
	const char *text = shaderLoadCtx.combinedText;
	int lineNum = 1;

	while (*text) {
		// Skip whitespace and comments
		while (*text && (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r')) {
			if (*text == '\n') lineNum++;
			text++;
		}

		if (!*text) break;

		// Check for shader name
		if (*text != '/' && *text != '{' && *text != '}') {
			// Found potential shader name
			int startLine = lineNum;

			// Skip to end of line or opening brace
			while (*text && *text != '\n' && *text != '{') {
				text++;
			}

			if (*text == '{') {
				// Found shader definition
				if (!R_ParseShaderTextSafe(text)) {
					ri.Printf(PRINT_WARNING, "Failed to parse shader at line %d\n", startLine);
				}

				// Skip the shader block
				int braceCount = 1;
				text++; // Skip opening brace
				while (*text && braceCount > 0) {
					if (*text == '{') braceCount++;
					else if (*text == '}') braceCount--;
					if (*text == '\n') lineNum++;
					text++;
				}
			}
		} else {
			// Skip line
			while (*text && *text != '\n') {
				text++;
			}
			if (*text == '\n') {
				text++;
				lineNum++;
			}
		}
	}
}

/*
====================
ScanAndLoadShaderFiles_Safe

Safe implementation of shader file loading that avoids temp memory corruption.
This replaces the renderer-specific implementations.
====================
*/
/*
====================
R_GetSafeShaderText

Returns the combined shader text from safe loading.
Renderer should copy this to s_shaderText and set up s_extensionOffset.
====================
*/
const char *R_GetSafeShaderText(int *size) {
	if (!shaderLoadCtx.initialized || !shaderLoadCtx.combinedText) {
		if (size) *size = 0;
		return NULL;
	}

	if (size) *size = shaderLoadCtx.combinedSize;
	return shaderLoadCtx.combinedText;
}

void ScanAndLoadShaderFiles_Safe(void) {
	char **shaderFiles = NULL;
	int numShaderFiles = 0;

	ri.Printf(PRINT_ALL, "=== SAFE SHADER LOADING STARTED ===\n");

	// Initialize safe loading context
	R_InitShaderLoadContext();

	// Find shader files
	const char *shaderDirs[] = {"shaders", "scripts"};
	const int numDirs = sizeof(shaderDirs) / sizeof(shaderDirs[0]);

	for (int dirIdx = 0; dirIdx < numDirs; dirIdx++) {
		const char *dir = shaderDirs[dirIdx];
		char **dirFiles = ri.FS_ListFiles(dir, ".shader", &numShaderFiles);

		if (dirFiles && numShaderFiles > 0) {
			shaderFiles = dirFiles;
			ri.Printf(PRINT_ALL, "Found %d shader files in %s/\n", numShaderFiles, dir);
			break;
		}
	}

	if (!shaderFiles || numShaderFiles == 0) {
		ri.Printf(PRINT_WARNING, "No shader files found\n");
		R_ShutdownSafeShaderLoadContext();
		return;
	}

	// Limit to safe maximum
	if (numShaderFiles > MAX_SHADER_FILES_SAFE) {
		numShaderFiles = MAX_SHADER_FILES_SAFE;
		ri.Printf(PRINT_WARNING, "Limited to %d shader files for safe loading\n", MAX_SHADER_FILES_SAFE);
	}

	// Load all shader files safely
	int loadedCount = 0;
	for (int i = 0; i < numShaderFiles; i++) {
		char filename[MAX_QPATH];
		Com_sprintf(filename, sizeof(filename), "%s/%s", shaderFiles[i] ? "shaders" : "scripts", shaderFiles[i]);

		if (R_LoadShaderFileSafe(filename, NULL)) {
			loadedCount++;
		} else {
			ri.Printf(PRINT_DEVELOPER, "Failed to load shader file: %s\n", filename);
		}
	}

	ri.Printf(PRINT_ALL, "Successfully loaded %d/%d shader files\n", loadedCount, numShaderFiles);

	// Free the file list
	ri.FS_FreeFileList(shaderFiles);

	// Combine all loaded files
	if (!R_CombineShaderFilesSafe()) {
		ri.Printf(PRINT_WARNING, "Failed to combine shader files\n");
		R_ShutdownSafeShaderLoadContext();
		return;
	}

	ri.Printf(PRINT_ALL, "Combined shader text size: %d bytes\n", shaderLoadCtx.combinedSize);

	// Process the combined shader text (validate syntax)
	R_ProcessShaderFilesSafe();

	// Note: Combined text remains available via R_GetSafeShaderText()
	// Renderer-specific code should copy this to s_shaderText and set up s_extensionOffset

	ri.Printf(PRINT_ALL, "=== SAFE SHADER LOADING COMPLETED ===\n");

	// Don't shut down context here - renderer needs to access the data
	// R_ShutdownShaderLoadContext() should be called by renderer after copying data
}

static void *R_Default_Malloc(int bytes) {
	return ri.Malloc(bytes);
}

static void R_Default_Free(void *buf) {
	ri.Free(buf);
}

static void R_Default_Printf(int level, const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
	ri.Printf(level, fmt, argptr);
#pragma GCC diagnostic pop
	va_end(argptr);
}

static void R_Default_Error(errorParm_t level, const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
	ri.Error(level, fmt, argptr);
#pragma GCC diagnostic pop
	va_end(argptr);
}

static cvar_t *R_Default_Cvar_Get(const char *name, const char *value, int flags) {
	return ri.Cvar_Get(name, value, flags);
}

static float R_Default_Cvar_VariableFloat(const char *name) {
	cvar_t *cvar = ri.Cvar_Get(name, "0", 0);
	return cvar ? cvar->value : 0.0f;
}

static int R_Default_Cvar_VariableInteger(const char *name) {
	cvar_t *cvar = ri.Cvar_Get(name, "0", 0);
	return cvar ? cvar->integer : 0;
}

static const char *R_Default_Cvar_VariableString(const char *name) {
	cvar_t *cvar = ri.Cvar_Get(name, "", 0);
	return cvar ? cvar->string : "";
}

static void R_Default_Cmd_AddCommand(const char *name, void (*function)(void)) {
	ri.Cmd_AddCommand(name, function);
}

static void R_Default_Cmd_RemoveCommand(const char *name) {
	ri.Cmd_RemoveCommand(name);
}

static int R_Default_Cmd_Argc(void) {
	return ri.Cmd_Argc();
}

static const char *R_Default_Cmd_Argv(int arg) {
	return ri.Cmd_Argv(arg);
}

static int R_Default_Milliseconds(void) {
	return ri.Milliseconds();
}

static int64_t R_Default_Microseconds(void) {
	return ri.Microseconds();
}

// Placeholder functions for GLimp (backend-specific)
static void R_Default_GLimp_Init(qboolean fixedFunction) {
	// This would be implemented by the specific backend
	(void)fixedFunction;
}

static void R_Default_GLimp_Shutdown(qboolean unloadDLL) {
	// This would be implemented by the specific backend
	(void)unloadDLL;
}

static void R_Default_GLimp_EndFrame(void) {
	// This would be implemented by the specific backend
}

static void R_Default_GLimp_LogComment(char *comment) {
	// This would be implemented by the specific backend
	(void)comment;
}

// Default services table
static const renderer_services_t default_services = {
	R_Default_FS_ReadFile,
	R_Default_FS_FreeFile,
	R_Default_FS_WriteFile,
	R_Default_Hunk_Alloc,
	R_Default_Hunk_AllocateTempMemory,
	R_Default_Hunk_FreeTempMemory,
	R_Default_Malloc,
	R_Default_Free,
	R_Default_Printf,
	R_Default_Error,
	R_Default_Cvar_Get,
	R_Default_Cvar_VariableFloat,
	R_Default_Cvar_VariableInteger,
	R_Default_Cvar_VariableString,
	R_Default_Cmd_AddCommand,
	R_Default_Cmd_RemoveCommand,
	R_Default_Cmd_Argc,
	R_Default_Cmd_Argv,
	R_Default_Milliseconds,
	R_Default_Microseconds,
	R_Default_GLimp_Init,
	R_Default_GLimp_Shutdown,
	R_Default_GLimp_EndFrame,
	R_Default_GLimp_LogComment
};

const renderer_services_t *R_GetDefaultServices(void) {
	return &default_services;
}

renderer_context_t *R_CreateContext(const renderer_services_t *services) {
	if (!services) {
		services = &default_services;
	}

	renderer_context_t *context = (renderer_context_t *)services->Malloc(sizeof(renderer_context_t));
	if (!context) {
		return NULL;
	}

	// context->ri = &ri;  // Skip setting ri for now to avoid type issues
	context->globals = &tr;
	context->backend_data = NULL;

	return context;
}

void R_DestroyContext(renderer_context_t *context) {
	if (context) {
		if (context->backend_data) {
			// Backend-specific cleanup would go here
			context->services->Free(context->backend_data);
		}
		context->services->Free(context);
	}
}

// Testable renderer API implementation
//// static static qhandle_t R_Testable_RegisterModel(const char *name) {
//	return re.RegisterModel(name);
//}

//// static static void R_Testable_ModelBounds(qhandle_t model, vec3_t mins, vec3_t maxs) {
//	re.ModelBounds(model, mins, maxs);
//}

//// static static qhandle_t R_Testable_RegisterShader(const char *name) {
//	return re.RegisterShader(name);
//}

//// static static qhandle_t R_Testable_RegisterShaderNoMip(const char *name) {
//	return re.RegisterShaderNoMip(name);
//}

//// static static qhandle_t R_Testable_RegisterSkin(const char *name) {
//	return re.RegisterSkin(name);
//}

//// static static qhandle_t R_Testable_RegisterImage(const char *name, imgFlags_t flags) {
//	// This function signature might need adjustment based on actual API
//	return 0; // Placeholder
//}

//// static static qhandle_t R_Testable_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {
//	return re.RegisterFont(fontName, pointSize, font);
//}

//// static static void R_Testable_ClearScene(void) {
//	re.ClearScene();
//}

//// static static void R_Testable_AddRefEntityToScene(const refEntity_t *re) {
//	re.AddRefEntityToScene(re);
//}

//// static static void R_Testable_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts) {
//	re.AddPolyToScene(hShader, numVerts, verts);
//}

//// static static void R_Testable_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b) {
//	re.AddLightToScene(org, intensity, r, g, b);
//}

//// static static void R_Testable_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b) {
//	re.AddAdditiveLightToScene(org, intensity, r, g, b);
//}

//// static static void R_Testable_RenderScene(const refdef_t *fd) {
//	re.RenderScene(fd);
//}

//// static static void R_Testable_SetColor(const float *rgba) {
//	re.SetColor(rgba);
//}

//// static static void R_Testable_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader) {
//	re.DrawStretchPic(x, y, w, h, s1, t1, s2, t2, hShader);
//}

//// static static void R_Testable_DrawRotatedPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, float angle) {
//	// This function might not exist in all renderers
//	(void)x; (void)y; (void)w; (void)h; (void)s1; (void)t1; (void)s2; (void)t2; (void)hShader; (void)angle;
//}

//// static static void R_Testable_DrawStretchPicGradient(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, const float *gradientColor, int gradientType) {
//	// This function might not exist in all renderers
//	(void)x; (void)y; (void)w; (void)h; (void)s1; (void)t1; (void)s2; (void)t2; (void)hShader; (void)gradientColor; (void)gradientType;
//}

//// static static void R_Testable_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) {
//	re.DrawStretchRaw(x, y, w, h, cols, rows, data, client, dirty);
//}

//// static static void R_Testable_UploadCinematic(int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty) {
//	re.UploadCinematic(w, h, cols, rows, data, client, dirty);
//}

//// static static void R_Testable_DrawString(int x, int y, const char *str, int style, vec4_t color) {
//	// This might need to check if the function exists
//	if (re.DrawString) {
//		re.DrawString(x, y, str, style, color);
//	}
//}

//// static static void R_Testable_DrawStringExt(int x, int y, const char *str, int style, vec4_t color, qboolean forceColor, qboolean shadow) {
//	// This might need to check if the function exists
//	if (re.DrawStringExt) {
//		re.DrawStringExt(x, y, str, style, color, forceColor, shadow);
//	}
//}

//// static static void R_Testable_DrawChar(int x, int y, int ch, int style, vec4_t color) {
//	// This might need to check if the function exists
//	if (re.DrawChar) {
//		re.DrawChar(x, y, ch, style, color);
//	}
//}

//// static static void R_Testable_RemapShader(const char *oldShader, const char *newShader, const char *offsetTime) {
//	re.RemapShader(oldShader, newShader, offsetTime);
//}

//// static static qboolean R_Testable_GetEntityToken(char *buffer, int size) {
//	return re.GetEntityToken(buffer, size);
//}

//// static static qboolean R_Testable_inPVS(const vec3_t p1, const vec3_t p2) {
//	return re.inPVS(p1, p2);
//}

//// static static void R_Testable_TakeVideoFrame(int h, int w, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg) {
//	re.TakeVideoFrame(h, w, captureBuffer, encodeBuffer, motionJpeg);
//}

//// static static void R_Testable_ThrottleBackend(void) {
//	if (re.ThrottleBackend) {
//		re.ThrottleBackend();
//	}
//}

//// static static void R_Testable_FinishBloom(void) {
//	if (re.FinishBloom) {
//		re.FinishBloom();
//	}
//}

//// // static static void R_Testable_SetColorMappings(void) {
//// 	if (re.SetColorMappings) {
//// 		re.SetColorMappings();
//// 	}
//// }
//
//// Testable API table - commented out due to function signature mismatches
//// static const testable_renderer_api_t testable_api = {
//// 	R_Testable_RegisterModel,
//// 	R_Testable_ModelBounds,
//// 	R_Testable_RegisterShader,
//// 	R_Testable_RegisterShaderNoMip,
//// 	R_Testable_RegisterSkin,
//// 	R_Testable_RegisterImage,
//// 	R_Testable_RegisterFont,
//// 	R_Testable_ClearScene,
//// 	R_Testable_AddRefEntityToScene,
//// 	R_Testable_AddPolyToScene,
//// 	R_Testable_AddLightToScene,
//// 	R_Testable_AddAdditiveLightToScene,
//// 	R_Testable_RenderScene,
//// 	R_Testable_SetColor,
//// 	R_Testable_DrawStretchPic,
//// 	R_Testable_DrawRotatedPic,
//// 	R_Testable_DrawStretchPicGradient,
//// 	R_Testable_DrawStretchRaw,
//// 	R_Testable_UploadCinematic,
//// 	R_Testable_DrawString,
//// 	R_Testable_DrawStringExt,
//// 	R_Testable_DrawChar,
//// 	R_Testable_RemapShader,
//// 	R_Testable_GetEntityToken,
//// 	R_Testable_inPVS,
//// 	R_Testable_TakeVideoFrame,
//// 	R_Testable_ThrottleBackend,
//// 	R_Testable_FinishBloom
//// 	// R_Testable_SetColorMappings // Commented out due to API mismatch
//// };

/*
====================
Q_ValidateFilePath

Validates that a file path doesn't contain invalid characters
Used by both OpenGL and Vulkan renderers for security
====================
*/
qboolean Q_ValidateFilePath( const char *path ) {
    if ( !path || !*path ) return qfalse;
    const char *invalid = "<>:\"|?*";
    while ( *invalid ) {
        if ( strchr( path, *invalid ) ) return qfalse;
        invalid++;
    }
    return qtrue;
}

/*
====================
Vector Math Functions

These are needed by both OpenGL and Vulkan renderers
====================
*/
void _VectorCopy( const vec3_t in, vec3_t out ) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

void _VectorAdd( const vec3_t veca, const vec3_t vecb, vec3_t out ) {
	out[0] = veca[0] + vecb[0];
	out[1] = veca[1] + vecb[1];
	out[2] = veca[2] + vecb[2];
}

void _VectorSubtract( const vec3_t veca, const vec3_t vecb, vec3_t out ) {
	out[0] = veca[0] - vecb[0];
	out[1] = veca[1] - vecb[1];
	out[2] = veca[2] - vecb[2];
}

void _VectorScale( const vec3_t in, float scale, vec3_t out ) {
	out[0] = in[0] * scale;
	out[1] = in[1] * scale;
	out[2] = in[2] * scale;
}

void _VectorMA( const vec3_t veca, float scale, const vec3_t vecb, vec3_t out ) {
	out[0] = veca[0] + scale * vecb[0];
	out[1] = veca[1] + scale * vecb[1];
	out[2] = veca[2] + scale * vecb[2];
}

/*
====================
Engine Function Stubs

These functions are defined in the common library but needed by renderers.
Since renderers are built as shared libraries, we provide stubs that delegate
to the refimport interface.
====================
*/

// Memory management - delegate to refimport
void *Z_Malloc( int size ) {
    return ri.Hunk_Alloc(size, h_low);
}

void Z_Free( void *ptr ) {
    // Note: refimport doesn't have a direct free function
    // This is a limitation - memory allocated by renderers should be freed by renderers
    (void)ptr; // Suppress unused parameter warning
}

// Debug printing - delegate to refimport
void QDECL Com_DPrintf( const char *fmt, ... ) {
    // For renderers, delegate to ri.Printf - renderers typically don't filter developer prints
    va_list argptr;
    char msg[4096];

    va_start(argptr, fmt);
    vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    ri.Printf(PRINT_ALL, "%s", msg);
}

// File system functions - delegate to refimport
qboolean FS_Initialized( void ) {
    // This is a bit of a hack - we can't directly check FS initialization
    // Return true assuming FS is initialized when renderer loads
    return qtrue;
}

qboolean FS_StartupInProgress( void ) {
    // Return false assuming startup is complete when renderer loads
    return qfalse;
}

// Scalability functions - provide defaults
int Scalability_GetMaxFonts(void) {
    return 32; // Default value
}

int Scalability_GetMaxFontCache(void) {
    return 1024 * 1024; // Default 1MB
}
//
//// const testable_renderer_api_t *R_GetTestableAPI(void) {
//// 	return &testable_api;
//// }
