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
#include "../../common/q_shared.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

extern refimport_t ri;

// Forward declaration for Vulkan shutdown
void vk_shutdown( refShutdownCode_t code );

// The Vulkan renderer should use functions available through linking
// If functions are missing, they need to be implemented in the renderer or added to the interface

// Vulkan renderer cvars
cvar_t	*r_subdivisions;
cvar_t	*r_lodCurveError;
cvar_t	*r_mapGreyScale;
cvar_t	*r_mergeLightmaps;
cvar_t	*r_vbo;
cvar_t	*r_vertexLight;

// VRS cvars
cvar_t	*r_vrs;
cvar_t	*r_vrs_mode;
cvar_t	*r_vrs_center_radius;
cvar_t	*r_vrs_falloff_start;
cvar_t	*r_vrs_min_rate;
cvar_t	*r_vrs_max_rate;
cvar_t	*r_vk_profiling;
cvar_t	*r_vk_debug_overlay;

// Other Vulkan cvars
cvar_t	*r_vk_disableScreenMap;
cvar_t	*r_procDressing;
cvar_t	*r_materialSystem;
cvar_t	*r_frameTelemetry;
cvar_t	*r_bloom;
cvar_t	*r_dlss;
cvar_t	*r_dlss_quality;
cvar_t	*r_dlss_sharpening;
cvar_t	*r_styleTransfer;

// Font and shader cvars
cvar_t	*r_saveFontData;
cvar_t	*r_fullbright;
cvar_t	*r_singleShader;

// Additional renderer cvars
cvar_t	*r_baseNormalX;
cvar_t	*r_baseNormalY;
cvar_t	*r_baseParallax;
cvar_t	*r_noportals;
cvar_t	*r_fastsky;
cvar_t	*r_norefresh;
cvar_t	*r_dlightMode;
cvar_t	*r_nocull;
cvar_t	*r_drawentities;
cvar_t	*r_shadows;
cvar_t	*r_portalOnly;

// RTX (Ray Tracing) cvars
cvar_t	*r_raytracing;
cvar_t	*r_rt_samples;
cvar_t	*r_rt_maxDepth;
cvar_t	*r_rt_debugMagenta;
cvar_t	*r_rt_tlasUpdateMode;

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
static void R_Register( void ) {
    // Initialize Vulkan renderer cvars
    r_subdivisions = ri.Cvar_Get( "r_subdivisions", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription(r_subdivisions, "Distance to subdivide bezier curved surfaces. Higher values mean less subdivision and less geometric complexity.");

    r_lodCurveError = ri.Cvar_Get( "r_lodCurveError", "250", CVAR_ARCHIVE_ND );
    ri.Cvar_CheckRange( r_lodCurveError, "-1", "8192", CV_FLOAT );
    ri.Cvar_SetDescription( r_lodCurveError, "Level of detail error on curved surface grids. Higher values result in better quality at a distance." );

    r_mapGreyScale = ri.Cvar_Get( "r_mapGreyScale", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_CheckRange( r_mapGreyScale, "-1", "1", CV_FLOAT );
    ri.Cvar_SetDescription(r_mapGreyScale, "Desaturate world map textures only, works independently from \\r_greyscale, negative values only desaturate lightmaps.");

    r_mergeLightmaps = ri.Cvar_Get( "r_mergeLightmaps", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_mergeLightmaps, "Merge built-in small lightmaps into bigger lightmaps (atlases)." );

#ifdef USE_VBO
    r_vbo = ri.Cvar_Get( "r_vbo", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_vbo, "Use Vertex Buffer Objects to cache static map geometry, may improve FPS on modern GPUs, increases hunk memory usage by 15-30MB (map-dependent)." );
#endif

    r_vertexLight = ri.Cvar_Get( "r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH );
    ri.Cvar_SetDescription( r_vertexLight, "Use vertex lighting instead of dynamic lighting. Faster on older GPUs but looks worse." );

    // VRS cvars
    r_vrs = ri.Cvar_Get( "r_vrs", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_vrs, "Enable Variable Rate Shading (VRS)." );

    r_vrs_mode = ri.Cvar_Get( "r_vrs_mode", "0", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_vrs_mode, "VRS shading rate mode." );

    r_vrs_center_radius = ri.Cvar_Get( "r_vrs_center_radius", "0.5", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_vrs_center_radius, "VRS center region radius." );

    r_vrs_falloff_start = ri.Cvar_Get( "r_vrs_falloff_start", "0.7", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_vrs_falloff_start, "VRS falloff start distance." );

    r_vrs_min_rate = ri.Cvar_Get( "r_vrs_min_rate", "1", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_vrs_min_rate, "VRS minimum shading rate." );

    r_vrs_max_rate = ri.Cvar_Get( "r_vrs_max_rate", "4", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_vrs_max_rate, "VRS maximum shading rate." );

    // Vulkan-specific cvars
    r_vk_profiling = ri.Cvar_Get( "r_vk_profiling", "0", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_vk_profiling, "Enable Vulkan GPU profiling." );

    r_vk_debug_overlay = ri.Cvar_Get( "r_vk_debug_overlay", "0", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_vk_debug_overlay, "Enable Vulkan debug overlay." );

    r_vk_disableScreenMap = ri.Cvar_Get( "r_vk_disableScreenMap", "0", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_vk_disableScreenMap, "Disable Vulkan screen mapping." );

    // Additional Vulkan cvars
    r_procDressing = ri.Cvar_Get( "r_procDressing", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_procDressing, "Enable procedural dressing system." );

    r_materialSystem = ri.Cvar_Get( "r_materialSystem", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_materialSystem, "Enable material system." );

    r_frameTelemetry = ri.Cvar_Get( "r_frameTelemetry", "0", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_frameTelemetry, "Enable frame telemetry." );

    r_bloom = ri.Cvar_Get( "r_bloom", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_bloom, "Enable bloom post-processing effect." );

    r_dlss = ri.Cvar_Get( "r_dlss", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_dlss, "Enable NVIDIA DLSS upscaling." );

    r_dlss_quality = ri.Cvar_Get( "r_dlss_quality", "3", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_dlss_quality, "DLSS quality setting." );

    r_dlss_sharpening = ri.Cvar_Get( "r_dlss_sharpening", "0.5", CVAR_ARCHIVE_ND );
    ri.Cvar_CheckRange( r_dlss_sharpening, "0", "1", CV_FLOAT );
    ri.Cvar_SetDescription( r_dlss_sharpening, "DLSS sharpening amount." );

    r_styleTransfer = ri.Cvar_Get( "r_styleTransfer", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_styleTransfer, "Enable neural style transfer." );

    // Font and shader cvars
    r_saveFontData = ri.Cvar_Get( "r_saveFontData", "0", CVAR_CHEAT );
    ri.Cvar_SetDescription( r_saveFontData, "Save font data for debugging." );

    r_fullbright = ri.Cvar_Get( "r_fullbright", "0", CVAR_CHEAT );
    ri.Cvar_SetDescription( r_fullbright, "Render all surfaces fullbright." );

    r_singleShader = ri.Cvar_Get( "r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH );
    ri.Cvar_SetDescription( r_singleShader, "Use single shader for all surfaces." );

    // Additional renderer cvars
    r_baseNormalX = ri.Cvar_Get( "r_baseNormalX", "1.0", CVAR_CHEAT );
    ri.Cvar_SetDescription( r_baseNormalX, "Base normal X component." );

    r_baseNormalY = ri.Cvar_Get( "r_baseNormalY", "1.0", CVAR_CHEAT );
    ri.Cvar_SetDescription( r_baseNormalY, "Base normal Y component." );

    r_baseParallax = ri.Cvar_Get( "r_baseParallax", "0.0", CVAR_CHEAT );
    ri.Cvar_SetDescription( r_baseParallax, "Base parallax value." );

    r_noportals = ri.Cvar_Get( "r_noportals", "0", CVAR_CHEAT );
    ri.Cvar_SetDescription( r_noportals, "Disable portal rendering." );

    r_fastsky = ri.Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE );
    ri.Cvar_SetDescription( r_fastsky, "Draw flat colored skies." );

    r_norefresh = ri.Cvar_Get( "r_norefresh", "0", CVAR_CHEAT );
    ri.Cvar_SetDescription( r_norefresh, "Disable scene refresh." );

    r_dlightMode = ri.Cvar_Get( "r_dlightMode", "0", CVAR_ARCHIVE );
    ri.Cvar_SetDescription( r_dlightMode, "Dynamic light mode." );

    r_nocull = ri.Cvar_Get( "r_nocull", "0", CVAR_CHEAT );
    ri.Cvar_SetDescription( r_nocull, "Disable culling." );

    r_drawentities = ri.Cvar_Get( "r_drawentities", "1", CVAR_CHEAT );
    ri.Cvar_SetDescription( r_drawentities, "Draw entities." );

    r_shadows = ri.Cvar_Get( "r_shadows", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_shadows, "Enable shadows." );

    r_portalOnly = ri.Cvar_Get( "r_portalOnly", "0", CVAR_CHEAT );
    ri.Cvar_SetDescription( r_portalOnly, "Only draw portals." );

    // RTX (Ray Tracing) cvars
    r_raytracing = ri.Cvar_Get( "r_raytracing", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_raytracing, "Enable Vulkan ray tracing." );

    r_rt_samples = ri.Cvar_Get( "r_rt_samples", "1", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_rt_samples, "Number of ray tracing samples per pixel." );

    r_rt_maxDepth = ri.Cvar_Get( "r_rt_maxDepth", "2", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_rt_maxDepth, "Maximum ray tracing recursion depth." );

    r_rt_debugMagenta = ri.Cvar_Get( "r_rt_debugMagenta", "0", CVAR_CHEAT );
    ri.Cvar_SetDescription( r_rt_debugMagenta, "Fill ray tracing output with magenta for debugging." );

    r_rt_tlasUpdateMode = ri.Cvar_Get( "r_rt_tlasUpdateMode", "1", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_rt_tlasUpdateMode, "TLAS update mode (0=never, 1=on demand, 2=every frame)." );
}

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

    // Register Vulkan renderer cvars
    R_Register();

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

// ============================================================================
// Stub implementations for functions not available through refimport_t
// ============================================================================

// Minimal Com_Printf implementation using refimport_t
void Com_Printf(const char *fmt, ...) {
    va_list argptr;
    char msg[4096];

    va_start(argptr, fmt);
    vsnprintf(msg, sizeof(msg), fmt, argptr);
    va_end(argptr);

    ri.Printf(PRINT_ALL, "%s", msg);
}

// Minimal Q_strncpyz implementation
void Q_strncpyz(char *dest, const char *src, int destsize) {
    if (!dest || destsize < 1) return;
    if (!src) {
        *dest = 0;
        return;
    }

    strncpy(dest, src, destsize - 1);
    dest[destsize - 1] = 0;
}

// Minimal Q_ValidateFilePath implementation
qboolean Q_ValidateFilePath(const char *path) {
    if (!path || !*path) return qfalse;

    // Basic path validation
    const char *invalid = "<>:\"|?*";
    while (*invalid) {
        if (strchr(path, *invalid)) return qfalse;
        invalid++;
    }
    return qtrue;
}

// Stub implementations for missing renderer functions
qboolean RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font) {
    // Stub implementation - font registration not implemented in Vulkan renderer yet
    (void)fontName; (void)pointSize; (void)font;
    return qfalse;
}

void vk_draw_geometry(Vk_Depth_Range depth_range, qboolean indexed) {
    // Stub implementation - geometry drawing not implemented yet
    (void)depth_range; (void)indexed;
}

void vk_end_frame(void) {
    // Stub implementation - frame ending not implemented yet
}
