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


// Extern declarations for functions implemented in other files
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
extern void RE_SyncRender(void);
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
extern int R_MarkFragments(int numPoints, const vec3_t *points, const vec3_t projection, int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer);
extern int R_LerpTag(orientation_t *tag, qhandle_t model, int startFrame, int endFrame, float frac, const char *tagName);
extern void R_ModelBounds(qhandle_t model, vec3_t mins, vec3_t maxs);
extern qboolean RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);
extern glyphInfo_t *R_GetGlyphFromFont(fontInfo_t *font, int charCode);
extern void R_InitFonts(void);
extern void R_ShutdownFonts(void);
extern void RE_RemapShader(const char *oldShader, const char *newShader, const char *timeOffset);
extern qboolean RE_GetEntityToken(char *buffer, int size);
extern qboolean RE_InPVS(const vec3_t p1, const vec3_t p2);
extern void RE_TakeVideoFrame(int h, int w, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg);
extern void RE_ThrottleBackend(void);
extern void RE_FinishBloom(void);
extern qboolean RE_CanMinimize(void);
extern const glconfig_t *RE_GetConfig(void);
extern void RE_VertexLighting(qboolean allowed);

// Missing function implementations
float Font_Height(fontInfo_t *font, float scale) {
    // Use renderercommon implementation
    extern float RE_Font_Height(fontInfo_t *font, float scale);
    return RE_Font_Height(font, scale);
}

float Font_Width(const char *text, float scale, fontInfo_t *font) {
    // Use renderercommon implementation
    extern float RE_Font_Width(const char *text, float scale, fontInfo_t *font);
    return RE_Font_Width(text, scale, font);
}

void Font_DrawString(float x, float y, const char *text, const vec4_t color, float scale, fontInfo_t *font, int style) {
    // Use renderercommon implementation
    extern void RE_Font_DrawString(float x, float y, const char *text, const vec4_t color, float scale, fontInfo_t *font, int style);
    RE_Font_DrawString(x, y, text, color, scale, font, style);
}

void RE_SetColorMappings(void) {
    // Call the internal R_SetColorMappings function
    extern void R_SetColorMappings(void);
    R_SetColorMappings();
}



// All these functions are implemented in other files in this library
// No extern declarations needed

// Entry point for dlopen
Q_EXPORT __attribute__((visibility("default"))) refexport_t* GetRefAPI(int apiVersion, refimport_t *rimp) {
    // STABLE BASELINE: Return NULL to force fallback to OpenGL renderer
    // This prevents crashes while maintaining Vulkan renderer loading capability
    // TODO: Fix underlying renderer interface issues before enabling full Vulkan support
    (void)apiVersion; (void)rimp;
    return NULL;
}

void RE_SyncRender(void) {
    // Vulkan equivalent of glFinish - ensure all rendering commands are complete
    // For stub implementation, do nothing
}