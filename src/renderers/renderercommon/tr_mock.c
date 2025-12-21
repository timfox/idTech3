/*
===========================================================================
Mock Renderer for Testing

Provides mock implementations of renderer functions for unit testing
without requiring actual graphics hardware or full renderer initialization.
===========================================================================
*/

#include "tr_public.h"
#include "../../common/q_shared.h"

// imgFlags_t definition for testing
typedef int imgFlags_t;

// Forward declarations for mock functions
qhandle_t R_Mock_RegisterModel(const char *name);
qhandle_t R_Mock_RegisterModel_Sync(const char *name);
void R_Mock_ModelBounds(qhandle_t model, vec3_t mins, vec3_t maxs);
qhandle_t R_Mock_RegisterShader(const char *name);
qhandle_t R_Mock_RegisterShaderNoMip(const char *name);
qhandle_t R_Mock_RegisterSkin(const char *name);
qhandle_t R_Mock_RegisterImage(const char *name, int flags);
qhandle_t R_Mock_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);
void R_Mock_ClearScene(void);
void R_Mock_AddRefEntityToScene(const refEntity_t *re);
void R_Mock_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts);
void R_Mock_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);
void R_Mock_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b);
void R_Mock_RenderScene(const refdef_t *fd);
void R_Mock_SetColor(const float *rgba);
void R_Mock_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
void R_Mock_DrawRotatedPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, float angle);
void R_Mock_DrawStretchPicGradient(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, const float *gradientColor, int gradientType);
void R_Mock_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);
void R_Mock_UploadCinematic(int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);
void R_Mock_DrawString(int x, int y, const char *str, int style, vec4_t color);
void R_Mock_DrawStringExt(int x, int y, const char *str, int style, vec4_t color, qboolean forceColor, qboolean shadow);
void R_Mock_DrawChar(int x, int y, int ch, int style, vec4_t color);
void R_Mock_RemapShader(const char *oldShader, const char *newShader, const char *offsetTime);
qboolean R_Mock_GetEntityToken(char *buffer, int size);
qboolean R_Mock_inPVS(const vec3_t p1, const vec3_t p2);
void R_Mock_TakeVideoFrame(int h, int w, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg);
void R_Mock_ThrottleBackend(void);
void R_Mock_FinishBloom(void);
void R_Mock_SetColorMappings(void);

// Minimal definitions for testing
typedef enum {
	MOD_BAD = 0,
	MOD_BRUSH,
	MOD_MESH,
	MOD_MD4,
	MOD_MDR,
	MOD_IQM
} modtype_t;

typedef struct model_s {
	char name[MAX_QPATH];
	modtype_t type;
	int index;
	int numLods;
} model_t;

// Mock state for testing
static struct {
	int registered_models;
	int registered_shaders;
	int registered_skins;
	qboolean scene_cleared;
	int entities_added;
	int polys_added;
	int lights_added;
	qboolean scene_rendered;
} mock_state;

// Mock implementations - disabled due to API mismatch issues
// TODO: Fix mock API to match current refexport_t interface

// Context-aware mock state
static struct {
	int context_registered_models;
	int context_registered_shaders;
	qboolean context_scene_cleared;
	int context_entities_added;
	int context_polys_added;
	int context_lights_added;
	qboolean context_scene_rendered;
} context_mock_state;

// Context-aware mock implementations
static qhandle_t R_Context_Mock_RegisterModel(renderer_context_t *ctx, const char *name) {
	(void)ctx; (void)name;
	context_mock_state.context_registered_models++;
	return context_mock_state.context_registered_models;
}

static model_t *R_Context_Mock_GetModelByHandle([[maybe_unused]] renderer_context_t *ctx, qhandle_t handle) {
	// Return a mock model structure
	static model_t mock_model;
	mock_model.index = handle;
	Q_strncpyz(mock_model.name, "mock_model", sizeof(mock_model.name));
	mock_model.type = MOD_MESH;
	return &mock_model;
}

static void R_Context_Mock_ModelBounds([[maybe_unused]] renderer_context_t *ctx, [[maybe_unused]] qhandle_t model, vec3_t mins, vec3_t maxs) {
	// Return mock bounds
	VectorSet(mins, -5, -5, -5);
	VectorSet(maxs, 5, 5, 5);
}

static qhandle_t R_Context_Mock_RegisterShader([[maybe_unused]] renderer_context_t *ctx, [[maybe_unused]] const char *name) {
	context_mock_state.context_registered_shaders++;
	return context_mock_state.context_registered_shaders;
}

static qhandle_t R_Context_Mock_RegisterShaderNoMip([[maybe_unused]] renderer_context_t *ctx, [[maybe_unused]] const char *name) {
	context_mock_state.context_registered_shaders++;
	return context_mock_state.context_registered_shaders;
}

static qhandle_t R_Context_Mock_RegisterSkin([[maybe_unused]] renderer_context_t *ctx, [[maybe_unused]] const char *name) {
	return 1;
}

static qhandle_t R_Context_Mock_RegisterImage([[maybe_unused]] renderer_context_t *ctx, [[maybe_unused]] const char *name, [[maybe_unused]] imgFlags_t flags) {
	return 1;
}

static qhandle_t R_Context_Mock_RegisterFont([[maybe_unused]] renderer_context_t *ctx, const char *fontName, int pointSize, fontInfo_t *font) {
	if (font) {
		font->pointSize = pointSize;
		Q_strncpyz(font->name, fontName, sizeof(font->name));
	}
	return 1;
}

static void R_Context_Mock_ClearScene([[maybe_unused]] renderer_context_t *ctx) {
	context_mock_state.context_scene_cleared = qtrue;
	context_mock_state.context_entities_added = 0;
	context_mock_state.context_polys_added = 0;
	context_mock_state.context_lights_added = 0;
	context_mock_state.context_scene_rendered = qfalse;
}

static void R_Context_Mock_AddRefEntityToScene([[maybe_unused]] renderer_context_t *ctx, [[maybe_unused]] const refEntity_t *re) {
	context_mock_state.context_entities_added++;
}

static void R_Context_Mock_AddPolyToScene([[maybe_unused]] renderer_context_t *ctx, [[maybe_unused]] qhandle_t hShader, [[maybe_unused]] int numVerts, [[maybe_unused]] const polyVert_t *verts) {
	context_mock_state.context_polys_added++;
}

static void R_Context_Mock_AddLightToScene([[maybe_unused]] renderer_context_t *ctx, [[maybe_unused]] const vec3_t org, [[maybe_unused]] float intensity, [[maybe_unused]] float r, [[maybe_unused]] float g, [[maybe_unused]] float b) {
	context_mock_state.context_lights_added++;
}

static void R_Context_Mock_RenderScene([[maybe_unused]] renderer_context_t *ctx, [[maybe_unused]] const refdef_t *fd) {
	context_mock_state.context_scene_rendered = qtrue;
}

static void R_Context_Mock_SetWorldVisData([[maybe_unused]] renderer_context_t *ctx, [[maybe_unused]] const byte *vis) {
	// Mock implementation
}

static void R_Context_Mock_MarkLeaves([[maybe_unused]] renderer_context_t *ctx) {
	// Mock implementation
}

static void R_Context_Mock_BeginFrame([[maybe_unused]] renderer_context_t *ctx, [[maybe_unused]] stereoFrame_t stereoFrame) {
	// Mock implementation
}

static void R_Context_Mock_EndFrame([[maybe_unused]] renderer_context_t *ctx, int *frontEndMsec, int *backEndMsec) {
	// Mock implementation
	if (frontEndMsec) *frontEndMsec = 10;
	if (backEndMsec) *backEndMsec = 5;
}

// Context-aware mock API table
static const context_aware_renderer_api_t context_mock_api = {
	R_Context_Mock_RegisterModel,
	R_Context_Mock_GetModelByHandle,
	R_Context_Mock_ModelBounds,
	R_Context_Mock_RegisterShader,
	R_Context_Mock_RegisterShaderNoMip,
	R_Context_Mock_RegisterSkin,
	R_Context_Mock_RegisterImage,
	R_Context_Mock_RegisterFont,
	R_Context_Mock_ClearScene,
	R_Context_Mock_AddRefEntityToScene,
	R_Context_Mock_AddPolyToScene,
	R_Context_Mock_AddLightToScene,
	R_Context_Mock_RenderScene,
	R_Context_Mock_SetWorldVisData,
	R_Context_Mock_MarkLeaves,
	R_Context_Mock_BeginFrame,
	R_Context_Mock_EndFrame
};

// Mock API table - defined after function implementations

// Functions to control mock state for testing
void R_Mock_ResetState(void) {
	Com_Memset(&mock_state, 0, sizeof(mock_state));
}

int R_Mock_GetRegisteredModelCount(void) {
	return mock_state.registered_models;
}

int R_Mock_GetRegisteredShaderCount(void) {
	return mock_state.registered_shaders;
}

int R_Mock_GetRegisteredSkinCount(void) {
	return mock_state.registered_skins;
}

qboolean R_Mock_WasSceneCleared(void) {
	return mock_state.scene_cleared;
}

int R_Mock_GetEntitiesAdded(void) {
	return mock_state.entities_added;
}

int R_Mock_GetPolysAdded(void) {
	return mock_state.polys_added;
}

int R_Mock_GetLightsAdded(void) {
	return mock_state.lights_added;
}

qboolean R_Mock_WasSceneRendered(void) {
	return mock_state.scene_rendered;
}

// Mock renderer API implementations
qhandle_t R_Mock_RegisterModel([[maybe_unused]] const char *name) {
	mock_state.registered_models++;
	return mock_state.registered_models;
}

qhandle_t R_Mock_RegisterModel_Sync([[maybe_unused]] const char *name) {
	mock_state.registered_models++;
	return mock_state.registered_models;
}

void R_Mock_ModelBounds([[maybe_unused]] qhandle_t model, vec3_t mins, vec3_t maxs) {
	VectorSet(mins, -10, -10, -10);
	VectorSet(maxs, 10, 10, 10);
}

qhandle_t R_Mock_RegisterShader([[maybe_unused]] const char *name) {
	mock_state.registered_shaders++;
	return mock_state.registered_shaders;
}

qhandle_t R_Mock_RegisterShaderNoMip([[maybe_unused]] const char *name) {
	mock_state.registered_shaders++;
	return mock_state.registered_shaders;
}

qhandle_t R_Mock_RegisterSkin([[maybe_unused]] const char *name) {
	mock_state.registered_skins++;
	return mock_state.registered_skins;
}

qhandle_t R_Mock_RegisterImage([[maybe_unused]] const char *name, [[maybe_unused]] int flags) {
	return 1;
}

qhandle_t R_Mock_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {
	if (font) {
		font->pointSize = pointSize;
		Q_strncpyz(font->name, fontName, sizeof(font->name));
	}
	return 1;
}

void R_Mock_ClearScene(void) {
	mock_state.scene_cleared = qtrue;
}

void R_Mock_AddRefEntityToScene([[maybe_unused]] const refEntity_t *re) {
	mock_state.entities_added++;
}

void R_Mock_AddPolyToScene([[maybe_unused]] qhandle_t hShader, [[maybe_unused]] int numVerts, [[maybe_unused]] const polyVert_t *verts) {
	mock_state.polys_added++;
}

void R_Mock_AddLightToScene([[maybe_unused]] const vec3_t org, [[maybe_unused]] float intensity, [[maybe_unused]] float r, [[maybe_unused]] float g, [[maybe_unused]] float b) {
	mock_state.lights_added++;
}

void R_Mock_AddAdditiveLightToScene([[maybe_unused]] const vec3_t org, [[maybe_unused]] float intensity, [[maybe_unused]] float r, [[maybe_unused]] float g, [[maybe_unused]] float b) {
	mock_state.lights_added++;
}

void R_Mock_RenderScene([[maybe_unused]] const refdef_t *fd) {
	mock_state.scene_rendered = qtrue;
}

void R_Mock_SetColor([[maybe_unused]] const float *rgba) {
	// Stub
}

void R_Mock_DrawStretchPic([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] float w, [[maybe_unused]] float h, [[maybe_unused]] float s1, [[maybe_unused]] float t1, [[maybe_unused]] float s2, [[maybe_unused]] float t2, [[maybe_unused]] qhandle_t hShader) {
	// Stub
}

void R_Mock_DrawRotatedPic([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] float w, [[maybe_unused]] float h, [[maybe_unused]] float s1, [[maybe_unused]] float t1, [[maybe_unused]] float s2, [[maybe_unused]] float t2, [[maybe_unused]] qhandle_t hShader, [[maybe_unused]] float angle) {
	// Stub
}

void R_Mock_DrawStretchPicGradient([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] float w, [[maybe_unused]] float h, [[maybe_unused]] float s1, [[maybe_unused]] float t1, [[maybe_unused]] float s2, [[maybe_unused]] float t2, [[maybe_unused]] qhandle_t hShader, [[maybe_unused]] const float *gradientColor, [[maybe_unused]] int gradientType) {
	// Stub
}

void R_Mock_DrawStretchRaw([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int w, [[maybe_unused]] int h, [[maybe_unused]] int cols, [[maybe_unused]] int rows, [[maybe_unused]] const byte *data, [[maybe_unused]] int client, [[maybe_unused]] qboolean dirty) {
	// Stub
}

void R_Mock_UploadCinematic([[maybe_unused]] int w, [[maybe_unused]] int h, [[maybe_unused]] int cols, [[maybe_unused]] int rows, [[maybe_unused]] const byte *data, [[maybe_unused]] int client, [[maybe_unused]] qboolean dirty) {
	// Stub
}

void R_Mock_DrawString([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] const char *str, [[maybe_unused]] int style, [[maybe_unused]] vec4_t color) {
	// Stub
}

void R_Mock_DrawStringExt([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] const char *str, [[maybe_unused]] int style, [[maybe_unused]] vec4_t color, [[maybe_unused]] qboolean forceColor, [[maybe_unused]] qboolean shadow) {
	// Stub
}

void R_Mock_DrawChar([[maybe_unused]] int x, [[maybe_unused]] int y, [[maybe_unused]] int ch, [[maybe_unused]] int style, [[maybe_unused]] vec4_t color) {
	// Stub
}

void R_Mock_RemapShader([[maybe_unused]] const char *oldShader, [[maybe_unused]] const char *newShader, [[maybe_unused]] const char *offsetTime) {
	// Stub
}

qboolean R_Mock_GetEntityToken([[maybe_unused]] char *buffer, [[maybe_unused]] int size) {
	return qfalse;
}

qboolean R_Mock_inPVS([[maybe_unused]] const vec3_t p1, [[maybe_unused]] const vec3_t p2) {
	return qtrue;
}

void R_Mock_TakeVideoFrame([[maybe_unused]] int h, [[maybe_unused]] int w, [[maybe_unused]] byte *captureBuffer, [[maybe_unused]] byte *encodeBuffer, [[maybe_unused]] qboolean motionJpeg) {
	// Stub
}

void R_Mock_ThrottleBackend(void) {
	// Stub
}

void R_Mock_FinishBloom(void) {
	// Stub
}

void R_Mock_SetColorMappings(void) {
	// Stub
}

// Mock API table
static const testable_renderer_api_t mock_api = {
	R_Mock_RegisterModel,
	R_Mock_RegisterModel_Sync,
	R_Mock_ModelBounds,
	R_Mock_RegisterShader,
	R_Mock_RegisterShaderNoMip,
	R_Mock_RegisterSkin,
	R_Mock_RegisterImage,
	R_Mock_RegisterFont,
	R_Mock_ClearScene,
	R_Mock_AddRefEntityToScene,
	R_Mock_AddPolyToScene,
	R_Mock_AddLightToScene,
	R_Mock_AddAdditiveLightToScene,
	R_Mock_RenderScene,
	R_Mock_SetColor,
	R_Mock_DrawStretchPic,
	R_Mock_DrawRotatedPic,
	R_Mock_DrawStretchPicGradient,
	R_Mock_DrawStretchRaw,
	R_Mock_UploadCinematic,
	R_Mock_DrawString,
	R_Mock_DrawStringExt,
	R_Mock_DrawChar,
	R_Mock_RemapShader,
	R_Mock_GetEntityToken,
	R_Mock_inPVS,
	R_Mock_TakeVideoFrame,
	R_Mock_ThrottleBackend,
	R_Mock_FinishBloom,
	R_Mock_SetColorMappings
};

// Context-aware mock state control functions
void R_Context_Mock_ResetState(void) {
	Com_Memset(&context_mock_state, 0, sizeof(context_mock_state));
}

int R_Context_Mock_GetRegisteredModelCount(void) {
	return context_mock_state.context_registered_models;
}

int R_Context_Mock_GetRegisteredShaderCount(void) {
	return context_mock_state.context_registered_shaders;
}

qboolean R_Context_Mock_WasSceneCleared(void) {
	return context_mock_state.context_scene_cleared;
}

int R_Context_Mock_GetEntitiesAdded(void) {
	return context_mock_state.context_entities_added;
}

int R_Context_Mock_GetPolysAdded(void) {
	return context_mock_state.context_polys_added;
}

int R_Context_Mock_GetLightsAdded(void) {
	return context_mock_state.context_lights_added;
}

qboolean R_Context_Mock_WasSceneRendered(void) {
	return context_mock_state.context_scene_rendered;
}

// Get the mock API for testing
const testable_renderer_api_t *R_GetMockAPI(void) {
	return &mock_api;
}

// Get the context-aware mock API for testing
const context_aware_renderer_api_t *R_GetContextAwareMockAPI(void) {
	return &context_mock_api;
}
