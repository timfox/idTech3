/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_entry.c -- Vulkan renderer entry point

#include "tr_local.h"
#include "../renderercommon/tr_backend_iface.h"
#include "../../common/q_shared.h"
#include "vk.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

extern refimport_t ri;

// Forward declarations for Vulkan renderer functions
extern void R_Register(void);
extern void RE_Shutdown(refShutdownCode_t code);
extern void RE_BeginRegistration(glconfig_t *glconfigOut);
extern qhandle_t RE_RegisterModel(const char *name);
extern qhandle_t RE_RegisterSkin(const char *name);
extern qhandle_t RE_RegisterShader(const char *name);
extern qhandle_t RE_RegisterShaderNoMip(const char *name);
extern void RE_LoadWorldMap(const char *name);
extern void RE_SetWorldVisData(const byte *vis);
extern void RE_EndRegistration(void);
extern void RE_ClearScene(void);
extern void RE_AddRefEntityToScene(const refEntity_t *re, qboolean intShaderTime);
extern void RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num);
extern void RE_AddParticle(const vec3_t origin, const vec3_t velocity, const vec3_t color, float size, float life, qhandle_t shader);
extern int R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);
extern void RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);
extern void RE_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b);
extern void RE_AddLinearLightToScene(const vec3_t start, const vec3_t end, float intensity, float r, float g, float b);
extern void RE_RenderScene(const refdef_t *fd);
extern void RE_SetColor(const float *rgba);
extern void RE_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
extern void RE_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty);
extern void RE_UploadCinematic(int w, int h, int cols, int rows, byte *data, int client, qboolean dirty);
extern void RE_BeginFrame(stereoFrame_t stereoFrame);
extern void RE_EndFrame(int *frontEndMsec, int *backEndMsec);
extern int R_MarkFragments(int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec_t *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer);
extern int R_LerpTag(orientation_t *tag, qhandle_t model, int startFrame, int endFrame, float frac, const char *tagName);
extern void R_ModelBounds(qhandle_t model, vec3_t mins, vec3_t maxs);
extern qboolean RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);
extern glyphInfo_t *R_GetGlyphFromFont(fontInfo_t *font, int charCode);
extern void R_InitFonts(void);
extern void R_ShutdownFonts(void);
extern float Font_Height(fontInfo_t *font, float scale);
extern float Font_Width(const char *text, float scale, fontInfo_t *font);
extern void Font_DrawString(float x, float y, const char *text, const vec4_t color, float scale, fontInfo_t *font, int style);
extern void RE_RemapShader(const char *oldShader, const char *newShader, const char *timeOffset);
extern qboolean RE_GetEntityToken(char *buffer, int size);
extern qboolean RE_InPVS(const vec3_t p1, const vec3_t p2);
extern void RE_TakeVideoFrame(int h, int w, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg);
extern void RE_ThrottleBackend(void);
extern void RE_FinishBloom(void);
extern void RE_SetColorMappings(void);
extern qboolean RE_CanMinimize(void);
extern const glconfig_t *RE_GetConfig(void);
extern void RE_VertexLighting(qboolean allowed);
extern void RE_SyncRender(void);

// Entry point for dlopen
Q_EXPORT __attribute__((visibility("default"))) refexport_t* GetRefAPI(int apiVersion, refimport_t *rimp) {
    static refexport_t re;

    if (!rimp) {
        return NULL;
    }

    ri = *rimp;

    Com_Memset( &re, 0, sizeof( re ) );

    if ( apiVersion != REF_API_VERSION ) {
        ri.Printf( PRINT_ALL, "Vulkan Renderer: Mismatched REF_API_VERSION: expected %i, got %i\n", REF_API_VERSION, apiVersion );
        return NULL;
    }

    R_Register();

    re.Shutdown = RE_Shutdown;
    re.BeginRegistration = RE_BeginRegistration;
    re.RegisterModel = RE_RegisterModel;
    re.RegisterSkin = RE_RegisterSkin;
    re.RegisterShader = RE_RegisterShader;
    re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
    re.LoadWorld = RE_LoadWorldMap;
    re.SetWorldVisData = RE_SetWorldVisData;
    re.EndRegistration = RE_EndRegistration;
    re.ClearScene = RE_ClearScene;
    re.AddRefEntityToScene = RE_AddRefEntityToScene;
    re.AddPolyToScene = RE_AddPolyToScene;
    re.AddParticle = RE_AddParticle;
    re.LightForPoint = R_LightForPoint;
    re.AddLightToScene = RE_AddLightToScene;
    re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;
    re.AddLinearLightToScene = RE_AddLinearLightToScene;
    re.RenderScene = RE_RenderScene;
    re.SetColor = RE_SetColor;
    re.DrawStretchPic = RE_StretchPic;
    re.DrawStretchRaw = RE_StretchRaw;
    re.UploadCinematic = RE_UploadCinematic;
    re.BeginFrame = RE_BeginFrame;
    re.EndFrame = RE_EndFrame;
    re.MarkFragments = R_MarkFragments;
    re.LerpTag = R_LerpTag;
    re.ModelBounds = R_ModelBounds;

#ifdef __USEA3D
    re.A3D_RenderGeometry = NULL;
#endif
    re.RegisterFont = RE_RegisterFont;
    re.R_GetGlyphFromFont = R_GetGlyphFromFont;
    re.R_InitFonts = R_InitFonts;
    re.R_ShutdownFonts = R_ShutdownFonts;
    re.Font_Height = Font_Height;
    re.Font_Width = Font_Width;
    re.Font_DrawString = Font_DrawString;
    re.RemapShader = RE_RemapShader;
    re.GetEntityToken = RE_GetEntityToken;
    re.inPVS = RE_InPVS;
    re.TakeVideoFrame = RE_TakeVideoFrame;
    re.ThrottleBackend = RE_ThrottleBackend;
    re.FinishBloom = RE_FinishBloom;
    re.SetColorMappings = RE_SetColorMappings;
    re.CanMinimize = RE_CanMinimize;
    re.GetConfig = RE_GetConfig;
    re.VertexLighting = RE_VertexLighting;
    re.SyncRender = RE_SyncRender;

#ifdef USE_CIMGUI
    re.ImGuiBackendInit = RE_ImGuiBackend_Init;
    re.ImGuiBackendShutdown = RE_ImGuiBackend_Shutdown;
    re.ImGuiBackendNewFrame = RE_ImGuiBackend_NewFrame;
    re.ImGuiBackendRenderDrawData = RE_ImGuiBackend_RenderDrawData;
#endif

    return &re;
}