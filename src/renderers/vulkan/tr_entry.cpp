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
// tr_entry.cpp -- Vulkan renderer entry point

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
extern void RE_AddRefEntityToScene(const refEntity_t *ent);
extern void RE_AddPolyToScene(poly_t *poly);
extern void RE_AddParticle(const vec3_t org, const vec3_t velocity, const vec4_t color, float size, int life);
extern void RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);
extern void RE_AddAdditiveLightToScene(const vec3_t origin, float radius, float intensity, float r, float g, float b);
extern void RE_RenderScene(const refdef_t *fd);
extern void RE_SetColor(const vec4_t color);
extern void RE_StretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
extern void RE_StretchRaw(int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);
extern void RE_UploadCinematic(int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);
extern qhandle_t RE_RegisterFont(const char *fontName);
extern void RE_RemapShader(const char *oldShader, const char *newShader, const char *timeOffset);
extern const char *RE_GetEntityToken(void);
extern qboolean RE_TakeVideoFrame(int x, int y, int width, int height, byte *buffer);
extern void RE_BeginFrame(stereoFrame_t stereoFrame);
extern void RE_EndFrame(int *frontEndMsec, int *backEndMsec);
extern int R_MarkFragments(int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t *pointBuffer, int maxFragments, markFragment_t *fragmentBuffer);
extern int R_LerpTag(orientation_t *tag, const refEntity_t *refent, const char *tagName, int startIndex);
extern void R_ModelBounds(refEntity_t *refent, vec3_t mins, vec3_t maxs);
extern qboolean RE_InPVS(const vec3_t p1, const vec3_t p2);
extern void R_LightForPoint(vec3_t point, vec3_t color);

// Entry point for dlopen
extern "C" Q_EXPORT refexport_t* QDECL GetRefAPI(int apiVersion, refimport_t *rimp) {
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
    re.BeginFrame = RE_BeginFrame;
    re.EndFrame = RE_EndFrame;
    re.MarkFragments = R_MarkFragments;
    re.LerpTag = R_LerpTag;
    re.ModelBounds = R_ModelBounds;
    re.ClearScene = RE_ClearScene;
    re.AddRefEntityToScene = RE_AddRefEntityToScene;
    re.AddPolyToScene = RE_AddPolyToScene;
    re.AddParticle = RE_AddParticle;
    re.AddLightToScene = RE_AddLightToScene;
    re.RenderScene = RE_RenderScene;
    re.SetColor = RE_SetColor;
    re.DrawStretchPic = RE_StretchPic;
    re.DrawStretchRaw = RE_StretchRaw;
    re.UploadCinematic = RE_UploadCinematic;
    re.RegisterFont = RE_RegisterFont;
    re.RemapShader = RE_RemapShader;
    re.GetEntityToken = RE_GetEntityToken;
    re.TakeVideoFrame = RE_TakeVideoFrame;
    re.inPVS = RE_InPVS;

#ifdef USE_CIMGUI
    re.ImGuiBackendInit = RE_ImGuiBackend_Init;
    re.ImGuiBackendShutdown = RE_ImGuiBackend_Shutdown;
    re.ImGuiBackendNewFrame = RE_ImGuiBackend_NewFrame;
    re.ImGuiBackendRenderDrawData = RE_ImGuiBackend_RenderDrawData;
#endif

    return &re;
}