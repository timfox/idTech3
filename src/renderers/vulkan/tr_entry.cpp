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