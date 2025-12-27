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
#include "vk.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

extern refimport_t ri;

// Forward declarations for renderer entry points
void RE_Shutdown( refShutdownCode_t code );
void RE_BeginRegistration( glconfig_t *glconfigOut );
qhandle_t RE_RegisterModel( const char *name );
qhandle_t RE_RegisterSkin( const char *name );
qhandle_t RE_RegisterShader( const char *name );
qhandle_t RE_RegisterShaderNoMip( const char *name );
void RE_LoadWorldMap( const char *name );
void RE_SetWorldVisData( const byte *vis );
void RE_EndRegistration( void );
void RE_BeginFrame( stereoFrame_t stereoFrame );
void RE_EndFrame( int *frontEndMsec, int *backEndMsec );
int R_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void RE_RenderScene( const refdef_t *fd );
void RE_ClearScene( void );
void RE_AddRefEntityToScene( const refEntity_t *re, qboolean intShaderTime );
void RE_AddPolyToScene( qhandle_t hShader, int numVerts, const polyVert_t *verts, int num );
void RE_SetColor( const float *rgba );
void RE_StretchPic( float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader );
void RE_StretchRaw( int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty );
void RE_UploadCinematic( int w, int h, int cols, int rows, byte *data, int client, qboolean dirty );
void RE_RemapShader( const char *oldShader, const char *newShader, const char *timeOffset );
qboolean RE_GetEntityToken( char *buffer, int size );
void RE_TakeVideoFrame( int width, int height, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg );
qboolean RE_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font );
void RE_AddParticle( const vec3_t origin, const vec3_t velocity, const vec3_t color, float size, float life, qhandle_t shader );
qboolean RE_InPVS( const vec3_t p1, const vec3_t p2 );

int R_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
                   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );
int R_LerpTag( orientation_t *tag, qhandle_t model, int startFrame, int endFrame,
             float frac, const char *tagName );
void R_ModelBounds( qhandle_t model, vec3_t mins, vec3_t maxs );

// Global renderer cvars
cvar_t	*r_subdivisions;
cvar_t	*r_lodCurveError;
cvar_t	*r_mapGreyScale;
cvar_t	*r_mergeLightmaps;
cvar_t	*r_vbo;
cvar_t	*r_vertexLight;
cvar_t	*r_vrs;
cvar_t	*r_vrs_mode;
cvar_t	*r_vrs_center_radius;
cvar_t	*r_vrs_falloff_start;
cvar_t	*r_vrs_min_rate;
cvar_t	*r_vrs_max_rate;
cvar_t	*r_vk_profiling;
cvar_t	*r_vk_debug_overlay;
cvar_t	*r_vk_disableScreenMap;
cvar_t	*r_procDressing;
cvar_t	*r_materialSystem;
cvar_t	*r_dynamicResolution;
cvar_t	*r_frameTelemetry;
cvar_t	*r_bloom;
cvar_t	*r_volumetricFog;
cvar_t	*r_volumetricFogSamples;
cvar_t	*r_volumetricFogScattering;
cvar_t	*r_volumetricFogAbsorption;
cvar_t	*r_dlss;
cvar_t	*r_dlss_quality;
cvar_t	*r_dlss_sharpening;
cvar_t	*r_styleTransfer;
cvar_t	*r_saveFontData;
cvar_t	*r_fullbright;
cvar_t	*r_singleShader;
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
cvar_t	*r_raytracing;
cvar_t	*r_rt_samples;
cvar_t	*r_rt_maxDepth;
cvar_t	*r_rt_debugMagenta;
cvar_t	*r_rt_tlasUpdateMode;
cvar_t	*r_gamma;
cvar_t	*r_intensity;
cvar_t	*r_lightmap;
cvar_t	*r_showsky;
cvar_t	*r_detailTextures;
cvar_t	*r_ext_multitexture;
cvar_t	*r_railCoreWidth;
cvar_t	*r_railSegmentLength;
cvar_t	*r_railWidth;
cvar_t	*r_wireframe;
cvar_t	*r_shownormals;
cvar_t	*r_speeds;
cvar_t	*r_textureMode;
cvar_t	*r_marksOnTriangleMeshes;
cvar_t	*r_dlightBacks;
cvar_t	*r_debugSort;
cvar_t	*r_showtris;

// Additional legacy/shared cvars referenced by common renderer code paths
cvar_t	*r_znear;
cvar_t	*r_zproj;
cvar_t	*r_stereoSeparation;
cvar_t	*r_skipBackEnd;
cvar_t	*r_showImages;
cvar_t	*r_clear;
cvar_t	*r_finish;
cvar_t	*r_dynamiclight;
cvar_t	*r_drawworld;
cvar_t	*r_lockpvs;
cvar_t	*r_showcluster;
cvar_t	*r_novis;
cvar_t	*r_vk_debug2D;
cvar_t	*r_vk_debugClearColor;
cvar_t	*r_vk_debugUiOnly;
cvar_t	*r_debugSurface;
cvar_t	*r_teleporterFlash;
cvar_t	*r_drawSun;
cvar_t	*r_flares;
cvar_t	*r_flareFade;
cvar_t	*r_flareSize;
cvar_t	*r_flareCoeff;

// Shared/legacy renderer globals expected by the Vulkan renderer code
cvar_t	*r_nobind;
cvar_t	*r_roundImagesDown;
cvar_t	*r_colorMipLevels;
cvar_t	*r_picmip;
cvar_t	*r_nomip;
cvar_t	*r_simpleMipMaps;
cvar_t	*r_overBrightBits;
cvar_t	*r_mapOverBrightBits;
cvar_t	*r_dither;
cvar_t	*r_pbr;
cvar_t	*r_offsetUnits;
cvar_t	*r_offsetFactor;
cvar_t	*r_directedScale;
cvar_t	*r_debugLight;
cvar_t	*r_lodbias;
cvar_t	*r_facePlaneCull;
cvar_t	*r_dlightSaturation;
cvar_t	*r_dlightIntensity;
cvar_t	*r_dlightScale;

static cvar_t *r_maxpolys;
static cvar_t *r_maxpolyverts;
int max_polys;
int max_polyverts;

/*
================
RE_EndRegistration
================
*/
void RE_EndRegistration( void ) {
    ri.Printf( PRINT_ALL, "Vulkan Renderer: EndRegistration\n" );
    tr.registered = qtrue;
}

// RE_RegisterFont and RE_AddParticle are implemented in other compilation units.

/*
================
RE_InPVS
================
*/
qboolean RE_InPVS( const vec3_t p1, const vec3_t p2 ) {
    (void)p1; (void)p2;
    return qtrue;
}

/*
================
RE_Shutdown
================
*/
void RE_Shutdown( refShutdownCode_t code ) {
    ri.Printf( PRINT_ALL, "RE_Shutdown( %i )\n", code );

    // Remove console commands
    ri.Cmd_RemoveCommand( "modellist" );
    ri.Cmd_RemoveCommand( "imagelist" );
    ri.Cmd_RemoveCommand( "shaderlist" );
    ri.Cmd_RemoveCommand( "skinlist" );
    ri.Cmd_RemoveCommand( "gfxinfo" );
    ri.Cmd_RemoveCommand( "shaderstate" );

    // Shutdown Vulkan backend
    vk_shutdown( code );
}

/*
================
R_Register
================
*/
static void R_Register( void ) {
    r_subdivisions = ri.Cvar_Get( "r_subdivisions", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_subdivisions, "Distance to subdivide bezier curved surfaces." );

    r_lodCurveError = ri.Cvar_Get( "r_lodCurveError", "250", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_lodCurveError, "Level of detail error on curved surface grids." );

    r_mapGreyScale = ri.Cvar_Get( "r_mapGreyScale", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_mapGreyScale, "Desaturate world map textures." );

    r_mergeLightmaps = ri.Cvar_Get( "r_mergeLightmaps", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_mergeLightmaps, "Merge small lightmaps into atlases." );

    r_vbo = ri.Cvar_Get( "r_vbo", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_vbo, "Use Vertex Buffer Objects." );

    r_vertexLight = ri.Cvar_Get( "r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH );
    ri.Cvar_SetDescription( r_vertexLight, "Use vertex lighting." );

    r_vrs = ri.Cvar_Get( "r_vrs", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_SetDescription( r_vrs, "Enable Variable Rate Shading." );

    r_vrs_mode = ri.Cvar_Get( "r_vrs_mode", "0", CVAR_ARCHIVE_ND );
    r_vrs_center_radius = ri.Cvar_Get( "r_vrs_center_radius", "0.5", CVAR_ARCHIVE_ND );
    r_vrs_falloff_start = ri.Cvar_Get( "r_vrs_falloff_start", "0.7", CVAR_ARCHIVE_ND );
    r_vrs_min_rate = ri.Cvar_Get( "r_vrs_min_rate", "1", CVAR_ARCHIVE_ND );
    r_vrs_max_rate = ri.Cvar_Get( "r_vrs_max_rate", "4", CVAR_ARCHIVE_ND );

    r_vk_profiling = ri.Cvar_Get( "r_vk_profiling", "0", CVAR_ARCHIVE_ND );
    r_vk_debug_overlay = ri.Cvar_Get( "r_vk_debug_overlay", "0", CVAR_ARCHIVE_ND );
    r_vk_disableScreenMap = ri.Cvar_Get( "r_vk_disableScreenMap", "0", CVAR_ARCHIVE_ND );

    r_procDressing = ri.Cvar_Get( "r_procDressing", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_materialSystem = ri.Cvar_Get( "r_materialSystem", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_dynamicResolution = ri.Cvar_Get( "r_dynamicResolution", "0", CVAR_ARCHIVE_ND );
    r_frameTelemetry = ri.Cvar_Get( "r_frameTelemetry", "0", CVAR_ARCHIVE_ND );
    r_bloom = ri.Cvar_Get( "r_bloom", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    
    r_volumetricFog = ri.Cvar_Get( "r_volumetricFog", "0", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_volumetricFog, "Enable advanced volumetric fog system." );
    
    r_volumetricFogSamples = ri.Cvar_Get( "r_volumetricFogSamples", "64", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_volumetricFogSamples, "Number of samples for volumetric fog ray marching." );
    
    r_volumetricFogScattering = ri.Cvar_Get( "r_volumetricFogScattering", "0.5", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_volumetricFogScattering, "Scattering coefficient for volumetric fog." );
    
    r_volumetricFogAbsorption = ri.Cvar_Get( "r_volumetricFogAbsorption", "0.1", CVAR_ARCHIVE_ND );
    ri.Cvar_SetDescription( r_volumetricFogAbsorption, "Absorption coefficient for volumetric fog." );

    r_dlss = ri.Cvar_Get( "r_dlss", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_dlss_quality = ri.Cvar_Get( "r_dlss_quality", "3", CVAR_ARCHIVE_ND );
    r_dlss_sharpening = ri.Cvar_Get( "r_dlss_sharpening", "0.5", CVAR_ARCHIVE_ND );
    r_styleTransfer = ri.Cvar_Get( "r_styleTransfer", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );

    r_saveFontData = ri.Cvar_Get( "r_saveFontData", "0", CVAR_CHEAT );
    r_fullbright = ri.Cvar_Get( "r_fullbright", "0", CVAR_CHEAT );
    r_singleShader = ri.Cvar_Get( "r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH );

    r_baseNormalX = ri.Cvar_Get( "r_baseNormalX", "1.0", CVAR_CHEAT );
    r_baseNormalY = ri.Cvar_Get( "r_baseNormalY", "1.0", CVAR_CHEAT );
    r_baseParallax = ri.Cvar_Get( "r_baseParallax", "0.0", CVAR_CHEAT );

    r_noportals = ri.Cvar_Get( "r_noportals", "0", CVAR_CHEAT );
    r_fastsky = ri.Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE );
    r_norefresh = ri.Cvar_Get( "r_norefresh", "0", CVAR_CHEAT );
    r_dlightMode = ri.Cvar_Get( "r_dlightMode", "0", CVAR_ARCHIVE );
    r_nocull = ri.Cvar_Get( "r_nocull", "0", CVAR_CHEAT );
    r_drawentities = ri.Cvar_Get( "r_drawentities", "1", CVAR_CHEAT );
    r_shadows = ri.Cvar_Get( "r_shadows", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_portalOnly = ri.Cvar_Get( "r_portalOnly", "0", CVAR_CHEAT );

    r_raytracing = ri.Cvar_Get( "r_raytracing", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_rt_samples = ri.Cvar_Get( "r_rt_samples", "1", CVAR_ARCHIVE_ND );
    r_rt_maxDepth = ri.Cvar_Get( "r_rt_maxDepth", "2", CVAR_ARCHIVE_ND );
    r_rt_debugMagenta = ri.Cvar_Get( "r_rt_debugMagenta", "0", CVAR_CHEAT );
    r_rt_tlasUpdateMode = ri.Cvar_Get( "r_rt_tlasUpdateMode", "1", CVAR_ARCHIVE_ND );

    r_gamma = ri.Cvar_Get( "r_gamma", "1", CVAR_ARCHIVE );
    r_intensity = ri.Cvar_Get( "r_intensity", "1", CVAR_LATCH );
    r_lightmap = ri.Cvar_Get( "r_lightmap", "0", CVAR_CHEAT );
    r_showsky = ri.Cvar_Get( "r_showsky", "0", CVAR_CHEAT );
    r_detailTextures = ri.Cvar_Get( "r_detailTextures", "1", CVAR_ARCHIVE );
    r_ext_multitexture = ri.Cvar_Get( "r_ext_multitexture", "1", CVAR_ARCHIVE | CVAR_LATCH );
    r_railCoreWidth = ri.Cvar_Get( "r_railCoreWidth", "1", CVAR_ARCHIVE );
    r_railSegmentLength = ri.Cvar_Get( "r_railSegmentLength", "32", CVAR_ARCHIVE );
    r_railWidth = ri.Cvar_Get( "r_railWidth", "128", CVAR_ARCHIVE );
    r_wireframe = ri.Cvar_Get( "r_wireframe", "0", CVAR_CHEAT );
    r_shownormals = ri.Cvar_Get( "r_shownormals", "0", CVAR_CHEAT );
    r_speeds = ri.Cvar_Get( "r_speeds", "0", CVAR_ARCHIVE );

    r_textureMode = ri.Cvar_Get( "r_textureMode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE );
    ri.Cvar_CheckRange( r_textureMode, "GL_NEAREST;GL_LINEAR;GL_NEAREST_MIPMAP_NEAREST;GL_LINEAR_MIPMAP_NEAREST;GL_NEAREST_MIPMAP_LINEAR;GL_LINEAR_MIPMAP_LINEAR", NULL, CV_STRINGLIST );

    r_marksOnTriangleMeshes = ri.Cvar_Get( "r_marksOnTriangleMeshes", "0", CVAR_ARCHIVE );
    r_dlightBacks = ri.Cvar_Get( "r_dlightBacks", "1", CVAR_CHEAT );
    r_debugSort = ri.Cvar_Get( "r_debugSort", "0", CVAR_CHEAT );
    r_showtris = ri.Cvar_Get( "r_showtris", "0", CVAR_CHEAT );

    // Texture/debug controls used by shared renderer code
    r_nobind = ri.Cvar_Get( "r_nobind", "0", CVAR_CHEAT );
    r_roundImagesDown = ri.Cvar_Get( "r_roundImagesDown", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_colorMipLevels = ri.Cvar_Get( "r_colorMipLevels", "0", CVAR_LATCH );
    r_picmip = ri.Cvar_Get( "r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_nomip = ri.Cvar_Get( "r_nomip", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_simpleMipMaps = ri.Cvar_Get( "r_simpleMipMaps", "1", CVAR_ARCHIVE | CVAR_LATCH );

    r_overBrightBits = ri.Cvar_Get( "r_overBrightBits", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_mapOverBrightBits = ri.Cvar_Get( "r_mapOverBrightBits", "2", CVAR_ARCHIVE_ND | CVAR_LATCH );

    r_dither = ri.Cvar_Get( "r_dither", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_pbr = ri.Cvar_Get( "r_pbr", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );

    r_offsetUnits = ri.Cvar_Get( "r_offsetUnits", "-1", CVAR_CHEAT );
    r_offsetFactor = ri.Cvar_Get( "r_offsetFactor", "-2", CVAR_CHEAT );

    r_directedScale = ri.Cvar_Get( "r_directedScale", "1", CVAR_CHEAT );
    r_debugLight = ri.Cvar_Get( "r_debugLight", "0", CVAR_CHEAT );
    r_lodbias = ri.Cvar_Get( "r_lodbias", "0", CVAR_ARCHIVE_ND );
    r_facePlaneCull = ri.Cvar_Get( "r_facePlaneCull", "1", CVAR_ARCHIVE_ND );

    r_dlightSaturation = ri.Cvar_Get( "r_dlightSaturation", "1", CVAR_ARCHIVE_ND );
    r_dlightIntensity = ri.Cvar_Get( "r_dlightIntensity", "1", CVAR_ARCHIVE_ND );
    r_dlightScale = ri.Cvar_Get( "r_dlightScale", "1", CVAR_ARCHIVE_ND );

    // Poly buffer limits
    r_maxpolys = ri.Cvar_Get( "r_maxpolys", "6000", CVAR_LATCH );
    r_maxpolyverts = ri.Cvar_Get( "r_maxpolyverts", "30000", CVAR_LATCH );
    max_polys = r_maxpolys->integer;
    if ( max_polys < MAX_POLYS ) {
        max_polys = MAX_POLYS;
    }
    max_polyverts = r_maxpolyverts->integer;
    if ( max_polyverts < MAX_POLYVERTS ) {
        max_polyverts = MAX_POLYVERTS;
    }

    // Additional shared/legacy controls used by common renderer code
    r_znear = ri.Cvar_Get( "r_znear", "4", CVAR_CHEAT );
    r_zproj = ri.Cvar_Get( "r_zproj", "64", CVAR_ARCHIVE );
    r_stereoSeparation = ri.Cvar_Get( "r_stereoSeparation", "64", CVAR_ARCHIVE );

    r_skipBackEnd = ri.Cvar_Get( "r_skipBackEnd", "0", CVAR_CHEAT );
    r_showImages = ri.Cvar_Get( "r_showImages", "0", CVAR_CHEAT );
    r_clear = ri.Cvar_Get( "r_clear", "0", CVAR_CHEAT );
    r_finish = ri.Cvar_Get( "r_finish", "0", CVAR_CHEAT );
    r_dynamiclight = ri.Cvar_Get( "r_dynamiclight", "1", CVAR_ARCHIVE );
    r_drawworld = ri.Cvar_Get( "r_drawworld", "1", CVAR_CHEAT );
    r_lockpvs = ri.Cvar_Get( "r_lockpvs", "0", CVAR_CHEAT );
    r_showcluster = ri.Cvar_Get( "r_showcluster", "0", CVAR_CHEAT );
    r_novis = ri.Cvar_Get( "r_novis", "0", CVAR_CHEAT );

    r_vk_debug2D = ri.Cvar_Get( "r_vk_debug2D", "0", CVAR_CHEAT );
    r_vk_debugClearColor = ri.Cvar_Get( "r_vk_debugClearColor", "0", CVAR_CHEAT );
    r_vk_debugUiOnly = ri.Cvar_Get( "r_vk_debugUiOnly", "0", CVAR_CHEAT );

    r_debugSurface = ri.Cvar_Get( "r_debugSurface", "0", CVAR_CHEAT );
    r_teleporterFlash = ri.Cvar_Get( "r_teleporterFlash", "1", CVAR_ARCHIVE );
    r_drawSun = ri.Cvar_Get( "r_drawSun", "1", CVAR_ARCHIVE );
    r_flares = ri.Cvar_Get( "r_flares", "1", CVAR_ARCHIVE );
    r_flareFade = ri.Cvar_Get( "r_flareFade", "7", CVAR_ARCHIVE );
    r_flareSize = ri.Cvar_Get( "r_flareSize", "40", CVAR_ARCHIVE );
    r_flareCoeff = ri.Cvar_Get( "r_flareCoeff", "1", CVAR_ARCHIVE );
}

void R_Init( void ) {
    static qboolean initialized = qfalse;
    if ( initialized ) {
        return;
    }
    initialized = qtrue;

    R_Register();
    vk_initialize();
}

/*
================
GetRefAPI
================
*/
#ifdef USE_RENDERER_DLOPEN
Q_EXPORT refexport_t* QDECL GetRefAPI( int apiVersion, refimport_t *rimp ) {
#else
refexport_t *GetRefAPI( int apiVersion, refimport_t *rimp ) {
#endif
    static refexport_t re;

    ri = *rimp;

    Com_Memset( &re, 0, sizeof( re ) );

    if ( apiVersion != REF_API_VERSION ) {
        ri.Printf( PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n", REF_API_VERSION, apiVersion );
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

    return &re;
}

// (Helper stubs removed: these functions are provided by other compilation units.)
