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
#ifndef __TR_PUBLIC_H
#define __TR_PUBLIC_H

#include "tr_types.h"
#include "vulkan/vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

// File path validation
qboolean Q_ValidateFilePath( const char *path );
const char *Sys_DefaultBasePath( void );

// Memory management
void *Z_Malloc(int size);
void Z_Free(void *ptr);

// Error handling
void NORETURN Com_Error(errorParm_t level, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
void Com_Printf(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void Com_DPrintf(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

// CVAR management
cvar_t *Cvar_Get( const char *var_name, const char *value, int flags );
void Cvar_SetDescription( cvar_t *var, const char *description );

// Additional stub functions needed by renderers
unsigned int Com_TouchMemory(void);

// Additional stub functions needed by renderers
unsigned int Com_TouchMemory(void);
qboolean Com_HasPatterns(const char *str);
int Com_Filter(const char *filter, const char *name);
int Com_FilterPath(const char *filter, const char *name);
qboolean Com_FilterExt(const char *filter, const char *name);
void Com_SortList(char **list, int n);
void Info_Print(const char *s);
int Z_FreeTags(memtag_t tag);
void Com_RandomBytes(byte *buffer, int len);
void Com_BeginRedirect(char *buffer, int buffersize, void (*flush)(const char *));
void Com_EndRedirect(void);
void *S_Malloc(int size);
struct channel_s;
void S_Spatialize(struct channel_s *ch);
qboolean FS_Initialized(void);
qboolean FS_StartupInProgress(void);
int Scalability_GetMaxFonts(void);
int Scalability_GetMaxFontCache(void);

#define	REF_API_VERSION		9
#define MAX_MOD_KNOWN		1024

struct ImDrawData;

// Forward declarations for dependency injection
struct refimport_s;
struct trGlobals_s;
struct model_s;
struct renderer_services_s;

// Type aliases for forward declarations
typedef struct model_s model_t;
typedef struct renderer_services_s renderer_services_t;

// trGlobals_t is fully defined in renderer-specific tr_local.h files
#ifndef TR_GLOBALS_DEFINED
typedef struct trGlobals_s trGlobals_t;
#endif

// Image flags for texture management
typedef enum {
	IMGFLAG_NONE           = 0x0000,
	IMGFLAG_MIPMAP         = 0x0001,
	IMGFLAG_PICMIP         = 0x0002,
	IMGFLAG_CLAMPTOEDGE    = 0x0004,
	IMGFLAG_CLAMPTOBORDER  = 0x0008,
	IMGFLAG_NO_COMPRESSION = 0x0010,
	IMGFLAG_NOLIGHTSCALE   = 0x0020,
	IMGFLAG_LIGHTMAP       = 0x0040,
	IMGFLAG_NOSCALE        = 0x0080,
	IMGFLAG_RGB            = 0x0100,
	IMGFLAG_COLORSHIFT     = 0x0200,
	IMGFLAG_CUBEMAP		   = 0x0400,
} imgFlags_t;

// Renderer context for dependency injection
typedef struct renderer_context_s {
	struct refimport_s *ri;           // Import functions
	struct trGlobals_s *globals;      // Global renderer state (can be NULL for testing)
	void *backend_data;               // Backend-specific data
	const renderer_services_t *services; // Service interface
} renderer_context_t;

// Context-aware renderer functions that don't rely on global state
typedef struct context_aware_renderer_api_s {
	// Model management
	qhandle_t (*RegisterModel)(renderer_context_t *ctx, const char *name);
	model_t *(*GetModelByHandle)(renderer_context_t *ctx, qhandle_t handle);
	void (*ModelBounds)(renderer_context_t *ctx, qhandle_t model, vec3_t mins, vec3_t maxs);

	// Shader management
	qhandle_t (*RegisterShader)(renderer_context_t *ctx, const char *name);
	qhandle_t (*RegisterShaderNoMip)(renderer_context_t *ctx, const char *name);

	// Skin management
	qhandle_t (*RegisterSkin)(renderer_context_t *ctx, const char *name);

	// Image management
	qhandle_t (*RegisterImage)(renderer_context_t *ctx, const char *name, imgFlags_t flags);

	// Font management
	qhandle_t (*RegisterFont)(renderer_context_t *ctx, const char *fontName, int pointSize, fontInfo_t *font);

	// Scene management
	void (*ClearScene)(renderer_context_t *ctx);
	void (*AddRefEntityToScene)(renderer_context_t *ctx, const refEntity_t *re);
	void (*AddPolyToScene)(renderer_context_t *ctx, qhandle_t hShader, int numVerts, const polyVert_t *verts);
	void (*AddLightToScene)(renderer_context_t *ctx, const vec3_t org, float intensity, float r, float g, float b);
	void (*RenderScene)(renderer_context_t *ctx, const refdef_t *fd);

	// World management
	void (*SetWorldVisData)(renderer_context_t *ctx, const byte *vis);
	void (*MarkLeaves)(renderer_context_t *ctx);

	// Backend operations
	void (*BeginFrame)(renderer_context_t *ctx, stereoFrame_t stereoFrame);
	void (*EndFrame)(renderer_context_t *ctx, int *frontEndMsec, int *backEndMsec);
} context_aware_renderer_api_t;

// Service interfaces to reduce coupling
typedef struct renderer_services_s {
	// File system services
	int (*FS_ReadFile)(const char *qpath, void **buffer);
	void (*FS_FreeFile)(void *buffer);
	int (*FS_WriteFile)(const char *qpath, const void *buffer, int size);

	// Memory management
	void *(*Hunk_Alloc)(int size, ha_pref pref);
	void *(*Hunk_AllocateTempMemory)(int size);
	void (*Hunk_FreeTempMemory)(void *block);
	void *(*Malloc)(int bytes);
	void (*Free)(void *buf);

	// Console and error reporting
	void (*Printf)(int level, const char *fmt, ...);
	void (*Error)(errorParm_t level, const char *fmt, ...);

	// CVars
	cvar_t *(*Cvar_Get)(const char *name, const char *value, int flags);
	float (*Cvar_VariableFloat)(const char *name);
	int (*Cvar_VariableInteger)(const char *name);
	const char *(*Cvar_VariableString)(const char *name);

	// Command system
	void (*Cmd_AddCommand)(const char *name, void (*function)(void));
	void (*Cmd_RemoveCommand)(const char *name);
	int (*Cmd_Argc)(void);
	const char *(*Cmd_Argv)(int arg);

	// Timing
	int (*Milliseconds)(void);
	int64_t (*Microseconds)(void);

	// OpenGL/Vulkan specific (backend dependent)
	void (*GLimp_Init)(qboolean fixedFunction);
	void (*GLimp_Shutdown)(qboolean unloadDLL);
	void (*GLimp_EndFrame)(void);
	void (*GLimp_LogComment)(char *comment);
} renderer_services_t;

// Function to get the default services implementation
const renderer_services_t *R_GetDefaultServices(void);

// Function to create a renderer context
renderer_context_t *R_CreateContext(const renderer_services_t *services);
void R_DestroyContext(renderer_context_t *context);

// Testable renderer functions that don't rely on global state
typedef struct testable_renderer_api_s {
	// Model registration
	qhandle_t (*RegisterModel)(const char *name);
	qhandle_t (*RegisterModel_Sync)(const char *name);
	void (*ModelBounds)(qhandle_t model, vec3_t mins, vec3_t maxs);

	// Shader registration
	qhandle_t (*RegisterShader)(const char *name);
	qhandle_t (*RegisterShaderNoMip)(const char *name);

	// Skin registration
	qhandle_t (*RegisterSkin)(const char *name);

	// Image registration
	qhandle_t (*RegisterImage)(const char *name, imgFlags_t flags);

	// Font registration
	qhandle_t (*RegisterFont)(const char *fontName, int pointSize, fontInfo_t *font);

	// Scene management
	void (*ClearScene)(void);
	void (*AddRefEntityToScene)(const refEntity_t *re);
	void (*AddPolyToScene)(qhandle_t hShader, int numVerts, const polyVert_t *verts);
	void (*AddLightToScene)(const vec3_t org, float intensity, float r, float g, float b);
	void (*AddAdditiveLightToScene)(const vec3_t org, float intensity, float r, float g, float b);

	// Rendering
	void (*RenderScene)(const refdef_t *fd);

	// Utility functions
	void (*SetColor)(const float *rgba);
	void (*DrawStretchPic)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
	void (*DrawRotatedPic)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, float angle);
	void (*DrawStretchPicGradient)(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader, const float *gradientColor, int gradientType);
	void (*DrawStretchRaw)(int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);
	void (*UploadCinematic)(int w, int h, int cols, int rows, const byte *data, int client, qboolean dirty);

	// 2D rendering
	void (*DrawString)(int x, int y, const char *str, int style, vec4_t color);
	void (*DrawStringExt)(int x, int y, const char *str, int style, vec4_t color, qboolean forceColor, qboolean shadow);
	void (*DrawChar)(int x, int y, int ch, int style, vec4_t color);

	// World interaction
	void (*RemapShader)(const char *oldShader, const char *newShader, const char *offsetTime);
	qboolean (*GetEntityToken)(char *buffer, int size);
	qboolean (*inPVS)(const vec3_t p1, const vec3_t p2);
	void (*TakeVideoFrame)(int h, int w, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg);

	// Performance monitoring
	void (*ThrottleBackend)(void);
	void (*FinishBloom)(void);
	void (*SetColorMappings)(void);
} testable_renderer_api_t;

// Get the testable renderer API (can be mocked for testing)
const testable_renderer_api_t *R_GetTestableAPI(void);

// Get the context-aware renderer API
const context_aware_renderer_api_t *R_GetContextAwareAPI(void);

// Context-aware versions of core renderer functions
// These functions work with a renderer context instead of global state
model_t *R_GetModelByHandle_Context(renderer_context_t *ctx, qhandle_t handle);
model_t *R_AllocModel_Context(renderer_context_t *ctx);
qhandle_t R_RegisterModel_Context(renderer_context_t *ctx, const char *name);

// Macros for context-aware programming
// These allow functions to work with either global state or context state

#define GET_TR_CTX(ctx) (&tr)  // Simplified to avoid type issues
#define GET_RI_CTX(ctx) (&ri)  // Simplified to avoid type issues
#define GET_SERVICES_CTX(ctx) ((ctx) && (ctx)->services ? (ctx)->services : R_GetDefaultServices())

// Context-aware logging
#define R_CTX_Printf(ctx, level, ...) \
	do { \
		const renderer_services_t *svc = GET_SERVICES_CTX(ctx); \
		if (svc && svc->Printf) { \
			svc->Printf(level, __VA_ARGS__); \
		} \
	} while(0)

#define R_CTX_Error(ctx, ...) \
	do { \
		const renderer_services_t *svc = GET_SERVICES_CTX(ctx); \
		if (svc && svc->Error) { \
			svc->Error(__VA_ARGS__); \
		} \
	} while(0)

// Mock API for testing (doesn't require graphics hardware)
const testable_renderer_api_t *R_GetMockAPI(void);

// Context-aware mock API for testing
const context_aware_renderer_api_t *R_GetContextAwareMockAPI(void);

// Mock state control functions
void R_Mock_ResetState(void);
int R_Mock_GetRegisteredModelCount(void);
int R_Mock_GetRegisteredShaderCount(void);
int R_Mock_GetRegisteredSkinCount(void);
qboolean R_Mock_WasSceneCleared(void);
int R_Mock_GetEntitiesAdded(void);
int R_Mock_GetPolysAdded(void);
int R_Mock_GetLightsAdded(void);
qboolean R_Mock_WasSceneRendered(void);

// Context-aware mock state control functions
void R_Context_Mock_ResetState(void);
int R_Context_Mock_GetRegisteredModelCount(void);
int R_Context_Mock_GetRegisteredShaderCount(void);
qboolean R_Context_Mock_WasSceneCleared(void);
int R_Context_Mock_GetEntitiesAdded(void);
int R_Context_Mock_GetPolysAdded(void);
int R_Context_Mock_GetLightsAdded(void);
qboolean R_Context_Mock_WasSceneRendered(void);

//
// these are the functions exported by the refresh module
//
typedef enum {
	REF_KEEP_CONTEXT, // don't destroy window and context
	REF_KEEP_WINDOW,  // destroy context, keep window
	REF_DESTROY_WINDOW,
	REF_UNLOAD_DLL
} refShutdownCode_t;

typedef struct {
	// called before the library is unloaded
	// if the system is just reconfiguring, pass destroyWindow = qfalse,
	// which will keep the screen from flashing to the desktop.
	void	(*Shutdown)( refShutdownCode_t code );

	// All data that will be used in a level should be
	// registered before rendering any frames to prevent disk hits,
	// but they can still be registered at a later time
	// if necessary.
	//
	// BeginRegistration makes any existing media pointers invalid
	// and returns the current gl configuration, including screen width
	// and height, which can be used by the client to intelligently
	// size display elements
	void	(*BeginRegistration)( glconfig_t *config );
	qhandle_t (*RegisterModel)( const char *name );
	qhandle_t (*RegisterSkin)( const char *name );
	qhandle_t (*RegisterShader)( const char *name );
	qhandle_t (*RegisterShaderNoMip)( const char *name );
	void	(*LoadWorld)( const char *name );

	// the vis data is a large enough block of data that we go to the trouble
	// of sharing it with the clipmodel subsystem
	void	(*SetWorldVisData)( const byte *vis );

	// EndRegistration will draw a tiny polygon with each texture, forcing
	// them to be loaded into card memory
	void	(*EndRegistration)( void );

	// a scene is built up by calls to R_ClearScene and the various R_Add functions.
	// Nothing is drawn until R_RenderScene is called.
	void	(*ClearScene)( void );
	void	(*AddRefEntityToScene)( const refEntity_t *re, qboolean intShaderTime );
	void	(*AddPolyToScene)( qhandle_t hShader , int numVerts, const polyVert_t *verts, int num );
	void	(*AddParticle)( const vec3_t origin, const vec3_t velocity, const vec3_t color, float size, float life, qhandle_t shader );
	int		(*LightForPoint)( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );
	void	(*AddLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void	(*AddAdditiveLightToScene)( const vec3_t org, float intensity, float r, float g, float b );
	void	(*AddLinearLightToScene)( const vec3_t start, const vec3_t end, float intensity, float r, float g, float b );
	void	(*RenderScene)( const refdef_t *fd );

	void	(*SetColor)( const float *rgba );	// NULL = 1,1,1,1
	void	(*DrawStretchPic) ( float x, float y, float w, float h,
		float s1, float t1, float s2, float t2, qhandle_t hShader );	// 0 = white

	// Draw images for cinematic rendering, pass as 32 bit rgba
	void	(*DrawStretchRaw)( int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty );
	void	(*UploadCinematic)( int w, int h, int cols, int rows, byte *data, int client, qboolean dirty );

	void	(*BeginFrame)( stereoFrame_t stereoFrame );

	// if the pointers are not NULL, timing info will be returned
	void	(*EndFrame)( int *frontEndMsec, int *backEndMsec );


	int		(*MarkFragments)( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );

	int		(*LerpTag)( orientation_t *tag,  qhandle_t model, int startFrame, int endFrame,
					 float frac, const char *tagName );
	void	(*ModelBounds)( qhandle_t model, vec3_t mins, vec3_t maxs );

#ifdef __USEA3D
	void    (*A3D_RenderGeometry) (void *pVoidA3D, void *pVoidGeom, void *pVoidMat, void *pVoidGeomStatus);
#endif
	qboolean (*RegisterFont)(const char *fontName, int pointSize, fontInfo_t *font);
	glyphInfo_t *(*R_GetGlyphFromFont)(fontInfo_t *font, int charCode);
	void (*R_InitFonts)(void);
	void (*R_ShutdownFonts)(void);

	// Enhanced font rendering
	float (*Font_Height)(fontInfo_t *font, float scale);
	float (*Font_Width)(const char *text, float scale, fontInfo_t *font);
	void (*Font_DrawString)(float x, float y, const char *text, const vec4_t color, float scale, fontInfo_t *font, int style);

	void	(*RemapShader)(const char *oldShader, const char *newShader, const char *offsetTime);
	qboolean (*GetEntityToken)( char *buffer, int size );
	qboolean (*inPVS)( const vec3_t p1, const vec3_t p2 );

	void	(*TakeVideoFrame)( int h, int w, byte* captureBuffer, byte *encodeBuffer, qboolean motionJpeg );

	void	(*ThrottleBackend)( void );
	void	(*FinishBloom)( void );

	void	(*SetColorMappings)( void );

	qboolean (*CanMinimize)( void ); // == fbo enabled

	const glconfig_t *(*GetConfig)( void );

	void	(*VertexLighting)( qboolean allowed );
	void	(*SyncRender)( void );

	qboolean (*ImGuiBackendInit)( void );
	void	(*ImGuiBackendShutdown)( void );
	void	(*ImGuiBackendNewFrame)( void );
	void	(*ImGuiBackendRenderDrawData)( const struct ImDrawData *drawData );


} refexport_t;

//
// these are the functions imported by the refresh module
//
typedef struct {
	// print message on the local console
	void	FORMAT_PRINTF(2, 3) (QDECL *Printf)( printParm_t printLevel, const char *fmt, ... );

	// abort the game
	void	NORETURN_PTR FORMAT_PRINTF(2, 3)(QDECL *Error)( errorParm_t errorLevel, const char *fmt, ... );

	// milliseconds should only be used for profiling, never
	// for anything game related.  Get time from the refdef
	int		(*Milliseconds)( void );

	int64_t	(*Microseconds)( void );

	// stack based memory allocation for per-level things that
	// won't be freed
#ifdef HUNK_DEBUG
	void	*(*Hunk_AllocDebug)( int size, ha_pref pref, char *label, char *file, int line );
#else
	void	*(*Hunk_Alloc)( int size, ha_pref pref );
#endif
	void	*(*Hunk_AllocateTempMemory)( int size );
	void	(*Hunk_FreeTempMemory)( void *block );

	// dynamic memory allocator for things that need to be freed
	void	*(*Malloc)( int bytes );
	void	(*Free)( void *buf );
	void	(*FreeAll)( void );

	cvar_t	*(*Cvar_Get)( const char *name, const char *value, int flags );
	void	(*Cvar_Set)( const char *name, const char *value );
	void	(*Cvar_SetValue) (const char *name, float value);
	void	(*Cvar_CheckRange)( cvar_t *cv, const char *minVal, const char *maxVal, cvarValidator_t type );
	void	(*Cvar_SetDescription)( cvar_t *cv, const char *description );

	void	(*Cvar_SetGroup)( cvar_t *var, cvarGroup_t group );
	int		(*Cvar_CheckGroup)( cvarGroup_t group );
	void	(*Cvar_ResetGroup)( cvarGroup_t group, qboolean resetModifiedFlags );

	void	(*Cvar_VariableStringBuffer)( const char *var_name, char *buffer, int bufsize );
	const char *(*Cvar_VariableString)( const char *var_name );
	int		(*Cvar_VariableIntegerValue)( const char *var_name );

	void	(*Cmd_AddCommand)( const char *name, void(*cmd)(void) );
	void	(*Cmd_RemoveCommand)( const char *name );

	int		(*Cmd_Argc) (void);
	const char	*(*Cmd_Argv) (int i);

	void	(*Cmd_ExecuteText)( cbufExec_t exec_when, const char *text );

	int		(*CM_NumClusters)(void);
	int		(*CM_ClusterSize)(void);
	qboolean (*CM_ClusterVisible)(int cluster1, int cluster2);
	byte	*(*CM_ClusterPVS)(int cluster);

	// visualization for debugging collision detection
	void	(*CM_DrawDebugSurface)( void (*drawPoly)(int color, int numPoints, float *points) );

	// a qfalse return means the file does not exist
	// NULL can be passed for buf to just determine existence
	//int		(*FS_FileIsInPAK)( const char *name, int *pCheckSum );
	int		(*FS_ReadFile)( const char *name, void **buf );
	void	(*FS_FreeFile)( void *buf );
	char **	(*FS_ListFiles)( const char *name, const char *extension, int *numfilesfound );
	void	(*FS_FreeFileList)( char **filelist );
	void	(*FS_WriteFile)( const char *qpath, const void *buffer, int size );
	qboolean (*FS_FileExists)( const char *file );

	// cinematic stuff
	void	(*CIN_UploadCinematic)( int handle );
	int		(*CIN_PlayCinematic)( const char *arg0, int xpos, int ypos, int width, int height, int bits );
	e_status (*CIN_RunCinematic)( int handle );

	void	(*CL_WriteAVIVideoFrame)( const byte *buffer, int size );

	size_t	(*CL_SaveJPGToBuffer)( byte *buffer, size_t bufSize, int quality, int image_width, int image_height, byte *image_buffer, int padding );
	void	(*CL_SaveJPG)( const char *filename, int quality, int image_width, int image_height, byte *image_buffer, int padding );
	void	(*CL_LoadJPG)( const char *filename, unsigned char **pic, int *width, int *height );

	qboolean (*CL_IsMinimized)( void );
	void	(*CL_SetScaling)( float factor, int captureWidth, int captureHeight );

	void	(*Sys_SetClipboardBitmap)( const byte *bitmap, int size );
	qboolean(*Sys_LowPhysicalMemory)( void );

	int		(*Com_RealTime)( qtime_t *qtime );

	// platform-dependent functions
	void(*GLimp_InitGamma)(glconfig_t *config);
	void(*GLimp_SetGamma)(unsigned char red[256], unsigned char green[256], unsigned char blue[256]);

	// OpenGL
	void	(*GLimp_Init)( glconfig_t *config );
	void	(*GLimp_Shutdown)( qboolean unloadDLL );
	void	(*GLimp_EndFrame)( void );
	void*	(*GL_GetProcAddress)( const char *name );

	// Vulkan
	void	(*VKimp_Init)( glconfig_t *config );
	void	(*VKimp_Shutdown)( qboolean unloadDLL );
	PFN_vkVoidFunction	(*VK_GetInstanceProcAddr)( VkInstance instance, const char *name );
	qboolean (*VK_CreateSurface)( VkInstance instance, VkSurfaceKHR *pSurface );

	// Safe shader loading system
	void	(*ScanAndLoadShaderFiles_Safe)( void );

} refimport_t;

Q_EXPORT extern refimport_t ri;

// this is the only function actually exported at the linker level
// If the module can't init to a valid rendering state, NULL will be
// returned.
#ifdef USE_RENDERER_DLOPEN
typedef	refexport_t* (QDECL *GetRefAPI_t) (int apiVersion, refimport_t * rimp);
#else
refexport_t*GetRefAPI( int apiVersion, refimport_t *rimp );
#endif

// Safe shader loading functions
const char *R_GetSafeShaderText(int *size);
void R_ShutdownSafeShaderLoadContext(void);

#ifdef __cplusplus
}
#endif

// Shared SDF CVAR declarations to avoid cross-renderer symbol conflicts
// externs for SDF CVARs moved to centralized block; see extern "C" block below

// Initialize SDF CVARs (see TR_Init_FontSDF_CVARS in tr_font_sdf.c)
// TR_Init_FontSDF_CVARS declared above; avoid duplicate prototype here

// Centralized SDF CVARs
#ifdef __cplusplus
extern "C" {
#endif
extern cvar_t *r_fontSDF;
extern cvar_t *r_fontSDFSpread;
extern cvar_t *r_fontSDFSmooth;
extern cvar_t *r_fontSDFOutline;
void TR_Init_FontSDF_CVARS(void);
#ifdef __cplusplus
}
#endif

#endif	// __TR_PUBLIC_H
