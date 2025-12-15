#if defined(__APPLE__) && !defined(__ANDROID__)

#include "tr_local.h"
#include "../qcommon/qcommon.h"

/*
================
RE_Shutdown
================
*/
void RE_Shutdown(qboolean destroyWindow) {
	Metal_Shutdown();
	if (destroyWindow) {
		Metal_ShutdownWindow();
	}
}

/*
================
RE_BeginRegistration
================
*/
void RE_BeginRegistration(const char *mapName) {
	// Initialize rendering for new map
}

/*
================
RE_EndRegistration
================
*/
void RE_EndRegistration(void) {
	// Finalize map loading
}

/*
================
RE_RegisterModel
================
*/
qhandle_t RE_RegisterModel(const char *name) {
	// Register and load model
	return 0;
}

/*
================
RE_RegisterSkin
================
*/
qhandle_t RE_RegisterSkin(const char *name) {
	// Register and load skin
	return 0;
}

/*
================
RE_RegisterShader
================
*/
qhandle_t RE_RegisterShader(const char *name) {
	// Register and load shader
	return 0;
}

/*
================
RE_RegisterShaderNoMip
================
*/
qhandle_t RE_RegisterShaderNoMip(const char *name) {
	// Register shader without mipmaps
	return 0;
}

/*
================
RE_LoadWorldMap
================
*/
void RE_LoadWorldMap(const char *name) {
	// Load BSP map
}

/*
================
RE_SetWorldVisData
================
*/
void RE_SetWorldVisData(const byte *vis) {
	// Set visibility data
}

/*
================
RE_ClearScene
================
*/
void RE_ClearScene(void) {
	// Clear scene
}

/*
================
RE_AddRefEntityToScene
================
*/
void RE_AddRefEntityToScene(const refEntity_t *re, qboolean intShaderTime) {
	// Add entity to scene
}

/*
================
RE_AddPolyToScene
================
*/
void RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num) {
	// Add polygon to scene
}

/*
================
RE_AddLightToScene
================
*/
void RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b) {
	// Add light to scene
}

/*
================
RE_AddAdditiveLightToScene
================
*/
void RE_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b) {
	// Add additive light to scene
}

/*
================
RE_AddLinearLightToScene
================
*/
void RE_AddLinearLightToScene(const vec3_t start, const vec3_t end, float intensity, float r, float g, float b) {
	// Add linear light to scene
}

/*
================
RE_RenderScene
================
*/
void RE_RenderScene(const refdef_t *fd) {
	// Render scene
}

/*
================
RE_SetColor
================
Set current color for 2D rendering
*/
void RE_SetColor(const float *rgba) {
	if (!rgba) {
		// NULL means white
		tr.currentColor[0] = 1.0f;
		tr.currentColor[1] = 1.0f;
		tr.currentColor[2] = 1.0f;
		tr.currentColor[3] = 1.0f;
	} else {
		tr.currentColor[0] = rgba[0];
		tr.currentColor[1] = rgba[1];
		tr.currentColor[2] = rgba[2];
		tr.currentColor[3] = rgba[3];
	}
}

/*
================
RE_StretchPic
================
Draw a stretched picture (2D UI element)
*/
void RE_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader) {
	if (!tr.active || !metal.currentRenderEncoder) {
		return;
	}
	
	// TODO: Implement shader lookup by handle
	// For now, we'll skip shader binding and use a default white texture
	// This will be implemented when shader system is complete
	
	// Setup 2D projection
	extern void Metal_Setup2DProjection(void);
	Metal_Setup2DProjection();
	
	// Draw quad
	extern void Metal_AddQuad2D(float x, float y, float w, float h, 
								float s1, float t1, float s2, float t2,
								const float *color);
	Metal_AddQuad2D(x, y, w, h, s1, t1, s2, t2, tr.currentColor);
}

/*
================
RE_StretchRaw
================
*/
void RE_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty) {
	// Draw raw stretched image
}

/*
================
RE_UploadCinematic
================
*/
void RE_UploadCinematic(int w, int h, int cols, int rows, byte *data, int client, qboolean dirty) {
	// Upload cinematic frame
}

/*
================
RE_BeginFrame
================
*/
void RE_BeginFrame(stereoFrame_t stereoFrame) {
	Metal_BeginFrame();
}

/*
================
RE_EndFrame
================
*/
void RE_EndFrame(int *frontEndMsec, int *backEndMsec) {
	Metal_EndFrame();
	Metal_Present();
	
	if (frontEndMsec) {
		*frontEndMsec = 0;
	}
	if (backEndMsec) {
		*backEndMsec = tr.gpuTime;
	}
}

/*
================
RE_TakeVideoFrame
================
*/
void RE_TakeVideoFrame(int width, int height, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg) {
	// Capture video frame
}

/*
================
RE_RegisterFont
================
*/
void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {
	// Register font
}

/*
================
RE_RemapShader
================
*/
void RE_RemapShader(const char *oldShader, const char *newShader, const char *offsetTime) {
	// Remap shader
}

/*
================
RE_GetEntityToken
================
*/
qboolean RE_GetEntityToken(char *buffer, int size) {
	// Get entity token
	return qfalse;
}

/*
================
R_inPVS
================
*/
qboolean R_inPVS(const vec3_t p1, const vec3_t p2) {
	// Check if points are in same PVS
	return qtrue;
}

/*
================
R_MarkFragments
================
*/
int R_MarkFragments(int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer) {
	// Mark fragments
	return 0;
}

/*
================
R_LerpTag
================
*/
int R_LerpTag(orientation_t *tag, qhandle_t model, int startFrame, int endFrame, float frac, const char *tagName) {
	// Lerp tag
	return 0;
}

/*
================
R_ModelBounds
================
*/
void R_ModelBounds(qhandle_t model, vec3_t mins, vec3_t maxs) {
	// Get model bounds
}

/*
================
R_LightForPoint
================
*/
int R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir) {
	// Calculate light for point
	return 0;
}

/*
================
RE_ThrottleBackend
================
*/
void RE_ThrottleBackend(void) {
	// Throttle backend
}

/*
================
RE_FinishBloom
================
*/
void RE_FinishBloom(void) {
	// Finish bloom effect
}

/*
================
RE_CanMinimize
================
*/
qboolean RE_CanMinimize(void) {
	// Check if can minimize
	return qfalse;
}

/*
================
RE_GetConfig
================
*/
void RE_GetConfig(glconfig_t *config) {
	// Get renderer config
	if (config) {
		Com_Memset(config, 0, sizeof(*config));
		config->vidWidth = tr.width;
		config->vidHeight = tr.height;
		config->isFullscreen = qfalse;
		config->stereoEnabled = qfalse;
		config->colorBits = 32;
		config->depthBits = 32;
		config->stencilBits = 0;
		Q_strncpyz(config->renderer_string, "Metal Renderer", sizeof(config->renderer_string));
		Q_strncpyz(config->vendor_string, "Apple", sizeof(config->vendor_string));
		Q_strncpyz(config->version_string, "Metal 2.0", sizeof(config->version_string));
		Q_strncpyz(config->extensions_string, "", sizeof(config->extensions_string));
	}
}

/*
================
RE_VertexLighting
================
*/
void RE_VertexLighting(void) {
	// Vertex lighting
}

/*
================
RE_SyncRender
================
*/
void RE_SyncRender(void) {
	// Sync render
}

/*
================
GetRefAPI
================
*/
#ifdef USE_RENDERER_DLOPEN
Q_EXPORT refexport_t* QDECL GetRefAPI(int apiVersion, refimport_t *rimp);
refexport_t* QDECL GetRefAPI(int apiVersion, refimport_t *rimp) {
#else
refexport_t *GetRefAPI(int apiVersion, refimport_t *rimp);
refexport_t *GetRefAPI(int apiVersion, refimport_t *rimp) {
#endif
	static refexport_t re;
	
	ri = *rimp;
	
	Com_Memset(&re, 0, sizeof(re));
	
	if (apiVersion != REF_API_VERSION) {
		ri.Printf(PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n",
			REF_API_VERSION, apiVersion);
		return NULL;
	}
	
	// Initialize renderer
	Metal_Init();
	
	// Set up renderer interface
	re.Shutdown = RE_Shutdown;
	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.LoadWorld = RE_LoadWorldMap;
	re.SetWorldVisData = RE_SetWorldVisData;
	re.EndRegistration = RE_EndRegistration;
	re.BeginFrame = RE_BeginFrame;
	re.EndFrame = RE_EndFrame;
	re.MarkFragments = R_MarkFragments;
	re.LerpTag = R_LerpTag;
	re.ModelBounds = R_ModelBounds;
	re.ClearScene = RE_ClearScene;
	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.LightForPoint = R_LightForPoint;
	re.AddLightToScene = RE_AddLightToScene;
	re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;
	re.AddLinearLightToScene = RE_AddLinearLightToScene;
	re.RenderScene = RE_RenderScene;
	re.SetColor = RE_SetColor;
	re.DrawStretchPic = RE_StretchPic;
	re.DrawStretchRaw = RE_StretchRaw;
	re.UploadCinematic = RE_UploadCinematic;
	re.RegisterFont = RE_RegisterFont;
	re.RemapShader = RE_RemapShader;
	re.GetEntityToken = RE_GetEntityToken;
	re.inPVS = R_inPVS;
	re.TakeVideoFrame = RE_TakeVideoFrame;
	re.SetColorMappings = NULL; // TODO
	re.ThrottleBackend = RE_ThrottleBackend;
	re.FinishBloom = RE_FinishBloom;
	re.CanMinimize = RE_CanMinimize;
	re.GetConfig = RE_GetConfig;
	re.VertexLighting = RE_VertexLighting;
	re.SyncRender = RE_SyncRender;
	
	return &re;
}

#endif // __APPLE__ && !__ANDROID__

