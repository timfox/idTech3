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
//
//// const testable_renderer_api_t *R_GetTestableAPI(void) {
//// 	return &testable_api;
//// }
