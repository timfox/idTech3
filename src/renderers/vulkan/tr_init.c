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
// tr_init.c -- Vulkan renderer initialization

#include "tr_local.h"
#include "../renderercommon/tr_backend_iface.h"

extern refimport_t ri;

// Forward declaration for Vulkan shutdown
void vk_shutdown( refShutdownCode_t code );

// Vulkan-specific globals will be defined in vk.h

static void VK_GfxInfo(void) {
    ri.Printf(PRINT_ALL, "\n----- Vulkan Renderer Info -----\n");
    ri.Printf(PRINT_ALL, "Vulkan renderer initialized\n");
    ri.Printf(PRINT_ALL, "---------------------------------\n");
}

/*
===============
RE_Shutdown
===============
*/
static void RE_Shutdown( refShutdownCode_t code ) {
    ri.Printf( PRINT_ALL, "RE_Shutdown( %i )\n", code );

    // Remove console commands
    ri.Cmd_RemoveCommand( "modellist" );
    ri.Cmd_RemoveCommand( "imagelist" );
    ri.Cmd_RemoveCommand( "shaderlist" );
    ri.Cmd_RemoveCommand( "skinlist" );
    ri.Cmd_RemoveCommand( "gfxinfo" );
    ri.Cmd_RemoveCommand( "shaderstate" );

    // Shutdown Vulkan renderer
    vk_shutdown(code);

    // Shutdown other systems
    // Note: Vulkan-specific shutdown code would go here
}

/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

Returns a valid refexport_t structure to the engine
@@@@@@@@@@@@@@@@@@@@@
*/
#ifdef USE_RENDERER_DLOPEN
Q_EXPORT refexport_t* QDECL GetRefAPI(int apiVersion, refimport_t *rimp);
refexport_t* QDECL GetRefAPI(int apiVersion, refimport_t *rimp) {
#else
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

    // Vulkan renderer will be initialized when RE_BeginRegistration is called

    // Fill in the renderer entry points
    re.Shutdown = RE_Shutdown;

    re.BeginRegistration = RE_BeginRegistration;
    re.RegisterModel = RE_RegisterModel;
    re.RegisterSkin = RE_RegisterSkin;
    re.RegisterShader = RE_RegisterShader;
    re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
    re.LoadWorld = RE_LoadWorldMap;
    re.SetWorldVisData = RE_SetWorldVisData;
    // re.EndRegistration = RE_EndRegistration;  // Function doesn't exist

    re.BeginFrame = RE_BeginFrame;
    re.EndFrame = RE_EndFrame;

    re.MarkFragments = R_MarkFragments;
    re.LerpTag = R_LerpTag;
    re.ModelBounds = R_ModelBounds;

    re.ClearScene = RE_ClearScene;
    re.AddRefEntityToScene = RE_AddRefEntityToScene;
    re.AddPolyToScene = RE_AddPolyToScene;
    // re.AddParticle = RE_AddParticle;  // Function doesn't exist
    re.AddLightToScene = RE_AddLightToScene;
    re.RenderScene = RE_RenderScene;

    re.SetColor = RE_SetColor;
    re.DrawStretchPic = RE_StretchPic;
    re.DrawStretchRaw = RE_StretchRaw;
    re.UploadCinematic = RE_UploadCinematic;

    re.RegisterFont = RE_RegisterFont;
    re.RemapShader = RE_RemapShader;
    re.GetEntityToken = RE_GetEntityToken;
    // re.InvalidateTextures = RE_InvalidateTextures;  // Not in refexport_t

    re.TakeVideoFrame = RE_TakeVideoFrame;

    return &re;
}
