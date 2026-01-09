/*
===========================================================================
id Tech 3 - RTX Renderer Header

RTX renderer header with public API declarations
===========================================================================
*/

#ifndef __VK_RTX_H__
#define __VK_RTX_H__

#include "../../renderercommon/tr_public.h"
#include "../../renderercommon/tr_types.h"

// RTX-specific CVARs (created locally in renderer functions)
// r_rtx_mode: 0=off/vulkan fallback, 1=hardware RT, 2=compute RT, 3=ray marching
// r_rtx_denoise: Enable denoising (0=off, 1=on)
// r_rtx_imgui: Show ImGui debug interface (0=off, 1=on)

// RTX renderer statistics (internal use only)

// Public RTX API functions (matching refexport_t interface)
qboolean RTX_Init(void);
void RTX_Shutdown(refShutdownCode_t code);
void RTX_BeginFrame(stereoFrame_t stereoFrame);
void RTX_RenderScene(const refdef_t *fd);
void RTX_EndFrame(int *frontEndMsec, int *backEndMsec);

qhandle_t RTX_RegisterModel(const char *name);
qhandle_t RTX_RegisterSkin(const char *name);
qhandle_t RTX_RegisterShader(const char *name);
qhandle_t RTX_RegisterShaderNoMip(const char *name);
void RTX_LoadWorld(const char *name);
void RTX_SetWorldVisData(const byte *vis);
void RTX_EndRegistration(void);
void RTX_ClearScene(void);
void RTX_AddRefEntityToScene(const refEntity_t *re, qboolean intShaderTime);
void RTX_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num);
void RTX_AddParticle(const vec3_t origin, const vec3_t velocity, const vec3_t color, float size, float life, qhandle_t shader);
int RTX_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);
void RTX_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);
void RTX_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b);
void RTX_AddLinearLightToScene(const vec3_t start, const vec3_t end, float intensity, float r, float g, float b);
void RTX_SetColor(const float *rgba);
void RTX_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
void RTX_DrawStretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty);
void RTX_UploadCinematic(int w, int h, int cols, int rows, byte *data, int client, qboolean dirty);
int RTX_MarkFragments(int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer);
int RTX_LerpTag(orientation_t *tag, qhandle_t model, int startFrame, int endFrame, float frac, const char *tagName);
void RTX_ModelBounds(qhandle_t model, vec3_t mins, vec3_t maxs);
qboolean RTX_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);
void RTX_RemapShader(const char *oldShader, const char *newShader, const char *offsetTime);
qboolean RTX_GetEntityToken(char *buffer, int size);
qboolean RTX_inPVS(const vec3_t p1, const vec3_t p2);
void RTX_TakeVideoFrame(int h, int w, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg);
void RTX_ThrottleBackend(void);
void RTX_FinishBloom(void);
void RTX_SetColorMappings(void);
qboolean RTX_CanMinimize(void);
const glconfig_t *RTX_GetConfig(void);
void RTX_VertexLighting(qboolean allowed);
void RTX_SyncRender(void);

// ImGui support (implemented in vk_rtx_main.c)
qboolean RTX_ImGuiBackendInit(void);
void RTX_ImGuiBackendShutdown(void);
void RTX_ImGuiBackendNewFrame(void);
void RTX_ImGuiBackendRenderDrawData(const struct ImDrawData *drawData);

// Entry point
refexport_t* RTX_GetRefAPI(int apiVersion, refimport_t* rimp);

#endif // __VK_RTX_H__