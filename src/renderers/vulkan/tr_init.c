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
#ifdef USE_CIMGUI
qboolean RE_ImGuiBackend_Init( void );
void RE_ImGuiBackend_Shutdown( void );
void RE_ImGuiBackend_NewFrame( void );
void RE_ImGuiBackend_RenderDrawData( const struct ImDrawData *drawData );
#endif
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
extern cvar_t	*r_subdivisions;
extern cvar_t	*r_lodCurveError;
extern cvar_t	*r_mapGreyScale;
extern cvar_t	*r_mergeLightmaps;
extern cvar_t	*r_vbo;
extern cvar_t	*r_vertexLight;
extern cvar_t	*r_vrs;
extern cvar_t	*r_vrs_mode;
extern cvar_t	*r_vrs_center_radius;
extern cvar_t	*r_vrs_falloff_start;
extern cvar_t	*r_vrs_min_rate;
extern cvar_t	*r_vrs_max_rate;
extern cvar_t	*r_vk_profiling;
extern cvar_t	*r_vk_debug_overlay;
extern cvar_t	*r_vk_disableScreenMap;
extern cvar_t	*r_vk_icd;
extern cvar_t	*r_device;
extern cvar_t	*r_vulkan_validation;
extern cvar_t	*r_vk_renderdoc;
extern cvar_t	*r_vk_dynamicRendering;
extern cvar_t	*r_vk_asyncShaderCompile;
extern cvar_t	*r_vk_hotReload;
extern cvar_t	*r_vk_bindlessTextures;

extern cvar_t	*r_procDressing;
extern cvar_t	*r_procDressingDensity;
extern cvar_t	*r_foliageWindFrequency;
extern cvar_t	*r_foliageWindStrength;
extern cvar_t	*r_procDressingDebug;

extern cvar_t	*r_materialSystem;
extern cvar_t	*r_materialDamage;
extern cvar_t	*r_materialWetness;
extern cvar_t	*r_materialMagic;

extern cvar_t	*r_dynamicResolution;
extern cvar_t	*r_frameTelemetry;
extern cvar_t	*r_bloom;
extern cvar_t	*r_bloom_intensity;
extern cvar_t	*r_bloom_threshold;

extern cvar_t	*r_volumetricFog;
extern cvar_t	*r_volumetricFogSamples;
extern cvar_t	*r_volumetricFogScattering;
extern cvar_t	*r_volumetricFogAbsorption;

extern cvar_t	*r_dlss;
extern cvar_t	*r_dlss_quality;
extern cvar_t	*r_dlss_sharpening;

extern cvar_t	*r_fsr_enable;
extern cvar_t	*r_fsr_easu;
extern cvar_t	*r_fsr_rcas;
extern cvar_t	*r_fsr_sharpness;

extern cvar_t	*r_styleTransfer;

extern cvar_t	*r_virtualTextures;
extern cvar_t	*r_vt_pageSize;
extern cvar_t	*r_vt_cacheSize;

extern cvar_t	*r_gpuSceneGraph;
extern cvar_t	*r_gpuSceneDebug;

extern cvar_t	*r_particles_gpu;
extern cvar_t	*r_particles_max;

extern cvar_t	*r_meshShaders;
extern cvar_t	*r_meshletSize;

extern cvar_t	*r_layeredMaterials;
extern cvar_t	*r_layeredMaterialProfile;
extern cvar_t	*r_layeredMaterialMaxLayers;
extern cvar_t	*r_layeredMaterialSimple;

extern cvar_t	*r_cellLoadRadius;
extern cvar_t	*r_cellUnloadDistance;

extern cvar_t	*r_saveFontData;
extern cvar_t	*r_fullbright;
extern cvar_t	*r_singleShader;
extern cvar_t	*r_baseNormalX;
extern cvar_t	*r_baseNormalY;
extern cvar_t	*r_baseParallax;
extern cvar_t	*r_noportals;
extern cvar_t	*r_fastsky;
extern cvar_t	*r_norefresh;
extern cvar_t	*r_dlightMode;
extern cvar_t	*r_nocull;
extern cvar_t	*r_drawentities;
extern cvar_t	*r_shadows;
extern cvar_t	*r_portalOnly;
extern cvar_t	*r_raytracing;
extern cvar_t	*r_rt_samples;
extern cvar_t	*r_rt_maxDepth;
extern cvar_t	*r_rt_debugMagenta;
extern cvar_t	*r_rt_tlasUpdateMode;
extern cvar_t	*r_gamma;
extern cvar_t	*r_intensity;
extern cvar_t	*r_lightmap;
extern cvar_t	*r_showsky;
extern cvar_t	*r_detailTextures;
extern cvar_t	*r_ext_multitexture;
extern cvar_t	*r_ext_texture_filter_anisotropic;
extern cvar_t	*r_ext_max_anisotropy;
extern cvar_t	*r_railCoreWidth;
extern cvar_t	*r_railSegmentLength;
extern cvar_t	*r_railWidth;
extern cvar_t	*r_wireframe;
extern cvar_t	*r_shownormals;
extern cvar_t	*r_speeds;
extern cvar_t	*r_textureMode;
extern cvar_t	*r_marksOnTriangleMeshes;
extern cvar_t	*r_dlightBacks;
extern cvar_t	*r_debugSort;
extern cvar_t	*r_showtris;

// Additional legacy/shared cvars referenced by common renderer code paths
extern cvar_t	*r_znear;
extern cvar_t	*r_zproj;
extern cvar_t	*r_stereoSeparation;
extern cvar_t	*r_skipBackEnd;
extern cvar_t	*r_showImages;
extern cvar_t	*r_clear;
extern cvar_t	*r_finish;
extern cvar_t	*r_dynamiclight;
extern cvar_t	*r_drawworld;
extern cvar_t	*r_lockpvs;
extern cvar_t	*r_showcluster;
extern cvar_t	*r_novis;
extern cvar_t	*r_vk_debug2D;
extern cvar_t	*r_vk_debugClearColor;
extern cvar_t	*r_vk_debugUiOnly;
extern cvar_t	*r_debugSurface;
extern cvar_t	*r_teleporterFlash;
extern cvar_t	*r_drawSun;
extern cvar_t	*r_flares;
extern cvar_t	*r_flareFade;
extern cvar_t	*r_flareSize;
extern cvar_t	*r_flareCoeff;

// Shared/legacy renderer globals expected by the Vulkan renderer code
extern cvar_t	*r_nobind;
extern cvar_t	*r_roundImagesDown;
extern cvar_t	*r_colorMipLevels;
extern cvar_t	*r_picmip;
extern cvar_t	*r_nomip;
extern cvar_t	*r_simpleMipMaps;
extern cvar_t	*r_overBrightBits;
extern cvar_t	*r_mapOverBrightBits;
extern cvar_t	*r_dither;
extern cvar_t	*r_pbr;
extern cvar_t	*r_offsetUnits;
extern cvar_t	*r_offsetFactor;
extern cvar_t	*r_directedScale;
extern cvar_t	*r_debugLight;
extern cvar_t	*r_lodbias;
extern cvar_t	*r_lodscale;
extern cvar_t	*r_fbo;
extern cvar_t	*r_hdr;
extern cvar_t	*r_presentBits;
extern cvar_t	*r_texturebits;
extern cvar_t	*r_ambientScale;
extern cvar_t	*r_defaultImage;
extern cvar_t	*r_facePlaneCull;
extern cvar_t	*r_dlightSaturation;
extern cvar_t	*r_dlightIntensity;
extern cvar_t	*r_dlightScale;

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
    r_vk_icd = ri.Cvar_Get( "r_vk_icd", "", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_device = ri.Cvar_Get( "r_vkDevice", "-1", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_vulkan_validation = ri.Cvar_Get( "r_vkValidation", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_vk_renderdoc = ri.Cvar_Get( "r_vk_renderdoc", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_vk_dynamicRendering = ri.Cvar_Get( "r_vk_dynamicRendering", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_vk_asyncShaderCompile = ri.Cvar_Get( "r_vk_asyncShaderCompile", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_vk_hotReload = ri.Cvar_Get( "r_vk_hotReload", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_vk_bindlessTextures = ri.Cvar_Get( "r_vk_bindlessTextures", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );

    r_procDressing = ri.Cvar_Get( "r_procDressing", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_procDressingDensity = ri.Cvar_Get( "r_procDressingDensity", "1.0", CVAR_ARCHIVE_ND );
    r_foliageWindFrequency = ri.Cvar_Get( "r_foliageWindFrequency", "1.0", CVAR_ARCHIVE_ND );
    r_foliageWindStrength = ri.Cvar_Get( "r_foliageWindStrength", "1.0", CVAR_ARCHIVE_ND );
    r_procDressingDebug = ri.Cvar_Get( "r_procDressingDebug", "0", CVAR_ARCHIVE_ND );
    
    r_materialSystem = ri.Cvar_Get( "r_materialSystem", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_materialDamage = ri.Cvar_Get( "r_materialDamage", "1", CVAR_ARCHIVE_ND );
    r_materialWetness = ri.Cvar_Get( "r_materialWetness", "1", CVAR_ARCHIVE_ND );
    r_materialMagic = ri.Cvar_Get( "r_materialMagic", "1", CVAR_ARCHIVE_ND );
    
    r_dynamicResolution = ri.Cvar_Get( "r_dynamicResolution", "0", CVAR_ARCHIVE_ND );
    r_frameTelemetry = ri.Cvar_Get( "r_frameTelemetry", "0", CVAR_ARCHIVE_ND );
    r_bloom = ri.Cvar_Get( "r_bloom", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_bloom_intensity = ri.Cvar_Get( "r_bloom_intensity", "1.0", CVAR_ARCHIVE_ND );
    r_bloom_threshold = ri.Cvar_Get( "r_bloom_threshold", "0.8", CVAR_ARCHIVE_ND );
    
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
    
    r_fsr_enable = ri.Cvar_Get( "r_fsr_enable", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_fsr_easu = ri.Cvar_Get( "r_fsr_easu", "1", CVAR_ARCHIVE_ND );
    r_fsr_rcas = ri.Cvar_Get( "r_fsr_rcas", "1", CVAR_ARCHIVE_ND );
    r_fsr_sharpness = ri.Cvar_Get( "r_fsr_sharpness", "0.5", CVAR_ARCHIVE_ND );

    r_styleTransfer = ri.Cvar_Get( "r_styleTransfer", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );

    r_virtualTextures = ri.Cvar_Get( "r_virtualTextures", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_vt_pageSize = ri.Cvar_Get( "r_vt_pageSize", "128", CVAR_ARCHIVE_ND );
    r_vt_cacheSize = ri.Cvar_Get( "r_vt_cacheSize", "1024", CVAR_ARCHIVE_ND );

    r_gpuSceneGraph = ri.Cvar_Get( "r_gpuSceneGraph", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_gpuSceneDebug = ri.Cvar_Get( "r_gpuSceneDebug", "0", CVAR_ARCHIVE_ND );

    r_particles_gpu = ri.Cvar_Get( "r_particles_gpu", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_particles_max = ri.Cvar_Get( "r_particles_max", "2048", CVAR_ARCHIVE_ND );

    r_meshShaders = ri.Cvar_Get( "r_vkMeshShaders", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_meshletSize = ri.Cvar_Get( "r_meshletSize", "64", CVAR_ARCHIVE_ND );

    r_layeredMaterials = ri.Cvar_Get( "r_layeredMaterials", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_layeredMaterialProfile = ri.Cvar_Get( "r_layeredMaterialProfile", "0", CVAR_ARCHIVE_ND );
    r_layeredMaterialMaxLayers = ri.Cvar_Get( "r_layeredMaterialMaxLayers", "4", CVAR_ARCHIVE_ND );
    r_layeredMaterialSimple = ri.Cvar_Get( "r_layeredMaterialSimple", "0", CVAR_ARCHIVE_ND );

    r_cellLoadRadius = ri.Cvar_Get( "r_cellLoadRadius", "2000", CVAR_ARCHIVE_ND );
    r_cellUnloadDistance = ri.Cvar_Get( "r_cellUnloadDistance", "3000", CVAR_ARCHIVE_ND );
    
    r_saveFontData = ri.Cvar_Get( "r_saveFontData", "0", CVAR_CHEAT );
    r_fullbright = ri.Cvar_Get( "r_fullbright", "0", CVAR_CHEAT );
    r_singleShader = ri.Cvar_Get( "r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH );

    r_baseNormalX = ri.Cvar_Get( "r_baseNormalX", "1.0", CVAR_CHEAT );
    r_baseNormalY = ri.Cvar_Get( "r_baseNormalY", "1.0", CVAR_CHEAT );
    r_baseParallax = ri.Cvar_Get( "r_baseParallax", "0.0", CVAR_CHEAT );

    r_noportals = ri.Cvar_Get( "r_noportals", "0", CVAR_CHEAT );
    r_fastsky = ri.Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE );
    r_neatsky = ri.Cvar_Get( "r_neatsky", "0", CVAR_ARCHIVE | CVAR_LATCH );
    r_norefresh = ri.Cvar_Get( "r_norefresh", "0", CVAR_CHEAT );
    r_dlightMode = ri.Cvar_Get( "r_dlightMode", "0", CVAR_ARCHIVE );
    r_nocull = ri.Cvar_Get( "r_nocull", "0", CVAR_CHEAT );
    r_drawentities = ri.Cvar_Get( "r_drawentities", "1", CVAR_CHEAT );
    r_shadows = ri.Cvar_Get( "r_shadows", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_portalOnly = ri.Cvar_Get( "r_portalOnly", "0", CVAR_CHEAT );

    r_raytracing = ri.Cvar_Get( "r_vkRayTracing", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
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

    r_ext_texture_filter_anisotropic = ri.Cvar_Get( "r_ext_texture_filter_anisotropic", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_CheckRange( r_ext_texture_filter_anisotropic, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_ext_texture_filter_anisotropic, "Allow anisotropic filtering." );

    r_ext_max_anisotropy = ri.Cvar_Get( "r_ext_max_anisotropy", "8", CVAR_ARCHIVE_ND | CVAR_LATCH );
    ri.Cvar_CheckRange( r_ext_max_anisotropy, "1", NULL, CV_INTEGER );
    ri.Cvar_SetDescription( r_ext_max_anisotropy, "Sets maximum anisotropic level for your graphics driver. Requires \\r_ext_texture_filter_anisotropic." );

    r_marksOnTriangleMeshes = ri.Cvar_Get( "r_marksOnTriangleMeshes", "0", CVAR_ARCHIVE );
    r_dlightBacks = ri.Cvar_Get( "r_dlightBacks", "1", CVAR_CHEAT );
    r_debugSort = ri.Cvar_Get( "r_debugSort", "0", CVAR_CHEAT );
    r_printShaders = ri.Cvar_Get( "r_printShaders", "0", 0 );
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

    // Register missing cvars to avoid NULL dereference in vk.c
    r_fbo = ri.Cvar_Get( "r_fbo", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_hdr = ri.Cvar_Get( "r_hdr", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_presentBits = ri.Cvar_Get( "r_presentBits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_texturebits = ri.Cvar_Get( "r_texturebits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
    r_lodscale = ri.Cvar_Get( "r_lodscale", "5", CVAR_CHEAT );
    r_ambientScale = ri.Cvar_Get( "r_ambientScale", "0.6", CVAR_CHEAT );
    r_defaultImage = ri.Cvar_Get( "r_defaultImage", "", CVAR_ARCHIVE_ND | CVAR_LATCH );

    r_fontSDF = ri.Cvar_Get( "r_fontSDF", "0", CVAR_ARCHIVE | CVAR_LATCH );
}

// Cvar definitions
cvar_t *r_neatsky;
cvar_t *r_printShaders;
cvar_t *r_fontSDF;

void R_Init( void ) {
    static qboolean initialized = qfalse;
    if ( initialized ) {
        return;
    }
    initialized = qtrue;

    R_Register();

    // Allocate backend data buffers before any shaders are sorted.
    {
        byte *ptr;
        size_t backEndSize;

        if ( max_polys < MAX_POLYS ) {
            max_polys = MAX_POLYS;
        }
        if ( max_polyverts < MAX_POLYVERTS ) {
            max_polyverts = MAX_POLYVERTS;
        }

        backEndSize = sizeof( *backEndData )
            + sizeof( srfPoly_t ) * (size_t)max_polys
            + sizeof( polyVert_t ) * (size_t)max_polyverts;

        ptr = ri.Hunk_Alloc( backEndSize, h_low );
        backEndData = (backEndData_t *)ptr;
        backEndData->polys = (srfPoly_t *)( ptr + sizeof( *backEndData ) );
        backEndData->polyVerts = (polyVert_t *)( ptr + sizeof( *backEndData )
            + sizeof( srfPoly_t ) * (size_t)max_polys );

        R_InitNextFrame();
    }

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

    if (!rimp) {
        return NULL;
    }

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

#ifdef USE_CIMGUI
    re.ImGuiBackendInit = RE_ImGuiBackend_Init;
    re.ImGuiBackendShutdown = RE_ImGuiBackend_Shutdown;
    re.ImGuiBackendNewFrame = RE_ImGuiBackend_NewFrame;
    re.ImGuiBackendRenderDrawData = RE_ImGuiBackend_RenderDrawData;
#endif

    fprintf(stderr, "DEBUG: Renderer sizeof(refexport_t) = %zu\n", sizeof(refexport_t));
    return &re;
}
