/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Roadmap-only renderer plugin: exports GetRefAPI with safe no-op RE_* entry
points. Built as idtech3_metal (Apple) or idtech3_dxr (Windows) when
USE_METAL_RENDERER / USE_DXR_RENDERER are ON. Not shippable — use Vulkan.
===========================================================================
*/

#include "q_shared.h"
#include "renderers/common/tr_public.h"
#include "renderers/common/renderer_backend.h"

#if !defined( RENDERER_PLATFORM_STUB_METAL ) && !defined( RENDERER_PLATFORM_STUB_DXR )
#error "tr_platform_renderer_stub.c requires RENDERER_PLATFORM_STUB_METAL or RENDERER_PLATFORM_STUB_DXR"
#endif

#if defined( RENDERER_PLATFORM_STUB_METAL )
#define STUB_BACKEND_ID   RENDERER_BACKEND_METAL
#define STUB_BACKEND_NAME "Metal"
#define STUB_DOC_PATH     "docs/METAL_RENDERER.md"
#define STUB_CVAR_NAME    "r_metalScaffold"
#define STUB_CMAKE_FLAG   "USE_METAL_RENDERER=ON"
#elif defined( RENDERER_PLATFORM_STUB_DXR )
#define STUB_BACKEND_ID   RENDERER_BACKEND_DXR
#define STUB_BACKEND_NAME "DXR"
#define STUB_DOC_PATH     "docs/DXR_RENDERER.md"
#define STUB_CVAR_NAME    "r_dxrScaffold"
#define STUB_CMAKE_FLAG   "USE_DXR_RENDERER=ON"
#endif

static refimport_t s_ri;
static glconfig_t s_glconfig;
static qboolean s_loggedOnce;

static void Stub_LogOnce( void )
{
	if ( s_loggedOnce ) {
		return;
	}
	s_loggedOnce = qtrue;
	s_ri.Printf( PRINT_ALL,
		"[%s] roadmap renderer scaffold (%s) — not shippable; use cl_renderer vulkan. See %s\n",
		STUB_BACKEND_NAME, STUB_CMAKE_FLAG, STUB_DOC_PATH );
	(void)s_ri.Cvar_Get( STUB_CVAR_NAME, "1", CVAR_ROM | CVAR_NORESTART );
}

static void Stub_Shutdown( refShutdownCode_t code )
{
	(void)code;
}

static void Stub_BeginRegistration( glconfig_t *config )
{
	Stub_LogOnce();
	Com_Memset( &s_glconfig, 0, sizeof( s_glconfig ) );
	s_glconfig.vidWidth = 640;
	s_glconfig.vidHeight = 480;
	s_glconfig.windowAspect = 640.0f / 480.0f;
	s_glconfig.displayAspect = s_glconfig.windowAspect;
	s_glconfig.isFullscreen = qfalse;
	s_glconfig.stereoEnabled = qfalse;
	s_glconfig.smpActive = qfalse;
	s_glconfig.hardwareType = GLHW_GENERIC;
	*config = s_glconfig;
}

static qhandle_t Stub_RegisterStub( const char *name )
{
	(void)name;
	return 0;
}

static void Stub_LoadWorld( const char *name ) { (void)name; }
static qboolean Stub_BspStreamMerge( int cellX, int cellY, float sectorSize )
{
	(void)cellX; (void)cellY; (void)sectorSize;
	return qfalse;
}
static void Stub_BspStreamUnmerge( int cellX, int cellY ) { (void)cellX; (void)cellY; }
static void Stub_SetWorldVisData( const byte *vis ) { (void)vis; }
static void Stub_EndRegistration( void ) {}
static void Stub_ClearScene( void ) {}
static void Stub_SetEntityMorphWeight( const refEntity_t *re, const char *name, float weight )
{
	(void)re; (void)name; (void)weight;
}
static void Stub_AddRefEntity( const refEntity_t *re, qboolean intShaderTime )
{
	(void)re; (void)intShaderTime;
}
static void Stub_AddPoly( qhandle_t hShader, int numVerts, const polyVert_t *verts, int num )
{
	(void)hShader; (void)numVerts; (void)verts; (void)num;
}
static int Stub_LightForPoint( vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir )
{
	(void)point;
	VectorSet( ambientLight, 0.25f, 0.25f, 0.25f );
	VectorSet( directedLight, 0.0f, 0.0f, 0.0f );
	VectorSet( lightDir, 0.0f, 0.0f, 1.0f );
	return 0;
}
static void Stub_AddLight( const vec3_t org, float intensity, float r, float g, float b )
{
	(void)org; (void)intensity; (void)r; (void)g; (void)b;
}
static void Stub_RenderScene( const refdef_t *fd ) { (void)fd; }
static void Stub_SetColor( const float *rgba ) { (void)rgba; }
static void Stub_DrawStretchPic( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, qhandle_t hShader )
{
	(void)x; (void)y; (void)w; (void)h;
	(void)s1; (void)t1; (void)s2; (void)t2; (void)hShader;
}
static void Stub_DrawStretchRaw( int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty )
{
	(void)x; (void)y; (void)w; (void)h; (void)cols; (void)rows; (void)data; (void)client; (void)dirty;
}
static void Stub_UploadCinematic( int w, int h, int cols, int rows, byte *data, int client, qboolean dirty )
{
	(void)w; (void)h; (void)cols; (void)rows; (void)data; (void)client; (void)dirty;
}
static void Stub_BeginFrame( stereoFrame_t stereoFrame ) { (void)stereoFrame; }
static void Stub_EndFrame( int *frontEndMsec, int *backEndMsec )
{
	if ( frontEndMsec ) {
		*frontEndMsec = 0;
	}
	if ( backEndMsec ) {
		*backEndMsec = 0;
	}
}
static int Stub_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
	int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer )
{
	(void)numPoints; (void)points; (void)projection;
	(void)maxPoints; (void)pointBuffer; (void)maxFragments; (void)fragmentBuffer;
	return 0;
}
static int Stub_LerpTag( orientation_t *tag, qhandle_t model, int startFrame, int endFrame,
	float frac, const char *tagName )
{
	(void)tag; (void)model; (void)startFrame; (void)endFrame; (void)frac; (void)tagName;
	return -1;
}
static void Stub_ModelBounds( qhandle_t model, vec3_t mins, vec3_t maxs )
{
	(void)model;
	VectorClear( mins );
	VectorClear( maxs );
}
static void Stub_RegisterFont( const char *fontName, int pointSize, fontInfo_t *font )
{
	(void)fontName; (void)pointSize; (void)font;
}
static void Stub_ClearTrueTypeFontCache( void ) {}
static float Stub_GetFontKerning( const fontInfo_t *font, int prevIndex, int nextIndex )
{
	(void)font; (void)prevIndex; (void)nextIndex;
	return 0.0f;
}
static void Stub_RemapShader( const char *oldShader, const char *newShader, const char *offsetTime )
{
	(void)oldShader; (void)newShader; (void)offsetTime;
}
static qboolean Stub_GetEntityToken( char *buffer, int size )
{
	(void)buffer; (void)size;
	return qfalse;
}
static qboolean Stub_inPVS( const vec3_t p1, const vec3_t p2 )
{
	(void)p1; (void)p2;
	return qtrue;
}
static void Stub_TakeVideoFrame( int h, int w, byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg )
{
	(void)h; (void)w; (void)captureBuffer; (void)encodeBuffer; (void)motionJpeg;
}
static void Stub_ThrottleBackend( void ) {}
static void Stub_FinishBloom( void ) {}
static void Stub_SetColorMappings( void ) {}
static qboolean Stub_CanMinimize( void ) { return qfalse; }
static const glconfig_t *Stub_GetConfig( void ) { return &s_glconfig; }
static void Stub_VertexLighting( qboolean allowed ) { (void)allowed; }
static void Stub_SyncRender( void ) {}
static qboolean Stub_ReloadTexture( const char *name ) { (void)name; return qfalse; }
static void Stub_DrawStretchPicEx( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, qhandle_t hShader, float sdfEdgeSoftening )
{
	(void)x; (void)y; (void)w; (void)h;
	(void)s1; (void)t1; (void)s2; (void)t2; (void)hShader; (void)sdfEdgeSoftening;
}
static void Stub_DrawStretchPicSubpixel( float x, float y, float w, float h,
	float s1, float t1, float s2, float t2, qhandle_t hShader, float subpixelShift )
{
	(void)x; (void)y; (void)w; (void)h;
	(void)s1; (void)t1; (void)s2; (void)t2; (void)hShader; (void)subpixelShift;
}
static qboolean Stub_LoadVectorFont( const char *path ) { (void)path; return qfalse; }
static qboolean Stub_VectorFontActive( void ) { return qfalse; }
static qboolean Stub_DrawVectorString( float x, float y, float scale, const char *text,
	const float *color, float shadowOff )
{
	(void)x; (void)y; (void)scale; (void)text; (void)color; (void)shadowOff;
	return qfalse;
}
static void Stub_DrawVectorGlyph( float x, float y, float w, float h,
	float emS1, float emT1, float emS2, float emT2, int curveStart, int curveCount )
{
	(void)x; (void)y; (void)w; (void)h;
	(void)emS1; (void)emT1; (void)emS2; (void)emT2; (void)curveStart; (void)curveCount;
}
static void Stub_AddEngineSprite( const engineSpriteDesc_t *desc ) { (void)desc; }
static void Stub_AddEngineSpriteAtTime( const engineSpriteDesc_t *desc, int refdefTimeMs )
{
	(void)desc; (void)refdefTimeMs;
}
static void Stub_AddEngineDecal( const engineDecalDesc_t *desc ) { (void)desc; }

#ifdef USE_RENDERER_DLOPEN
Q_EXPORT refexport_t *QDECL GetRefAPI( int apiVersion, refimport_t *rimp )
#else
refexport_t *GetRefAPI( int apiVersion, refimport_t *rimp )
#endif
{
	static refexport_t re;

	if ( !rimp ) {
		return NULL;
	}
	s_ri = *rimp;
	Com_Memset( &re, 0, sizeof( re ) );

	if ( apiVersion != REF_API_VERSION ) {
		s_ri.Printf( PRINT_ALL, "[%s] mismatched REF_API_VERSION: expected %i, got %i\n",
			STUB_BACKEND_NAME, REF_API_VERSION, apiVersion );
		return NULL;
	}

	re.Shutdown = Stub_Shutdown;
	re.BeginRegistration = Stub_BeginRegistration;
	re.RegisterModel = Stub_RegisterStub;
	re.RegisterSkin = Stub_RegisterStub;
	re.RegisterShader = Stub_RegisterStub;
	re.RegisterShaderNoMip = Stub_RegisterStub;
	re.LoadWorld = Stub_LoadWorld;
	re.BspStreamMergeSector = Stub_BspStreamMerge;
	re.BspStreamUnmergeSector = Stub_BspStreamUnmerge;
	re.SetWorldVisData = Stub_SetWorldVisData;
	re.EndRegistration = Stub_EndRegistration;
	re.BeginFrame = Stub_BeginFrame;
	re.EndFrame = Stub_EndFrame;
	re.MarkFragments = Stub_MarkFragments;
	re.LerpTag = Stub_LerpTag;
	re.ModelBounds = Stub_ModelBounds;
	re.ClearScene = Stub_ClearScene;
	re.SetEntityMorphWeight = Stub_SetEntityMorphWeight;
	re.AddRefEntityToScene = Stub_AddRefEntity;
	re.AddEngineSpriteToScene = Stub_AddEngineSprite;
	re.AddEngineSpriteToSceneAtTime = Stub_AddEngineSpriteAtTime;
	re.AddEngineDecalToScene = Stub_AddEngineDecal;
	re.AddPolyToScene = Stub_AddPoly;
	re.LightForPoint = Stub_LightForPoint;
	re.AddLightToScene = Stub_AddLight;
	re.AddAdditiveLightToScene = Stub_AddLight;
	re.AddLinearLightToScene = Stub_AddLight;
	re.RenderScene = Stub_RenderScene;
	re.SetColor = Stub_SetColor;
	re.DrawStretchPic = Stub_DrawStretchPic;
	re.DrawStretchRaw = Stub_DrawStretchRaw;
	re.UploadCinematic = Stub_UploadCinematic;
	re.RegisterFont = Stub_RegisterFont;
	re.ClearTrueTypeFontCache = Stub_ClearTrueTypeFontCache;
	re.GetFontKerning = Stub_GetFontKerning;
	re.RemapShader = Stub_RemapShader;
	re.GetEntityToken = Stub_GetEntityToken;
	re.inPVS = Stub_inPVS;
	re.TakeVideoFrame = Stub_TakeVideoFrame;
	re.SetColorMappings = Stub_SetColorMappings;
	re.ThrottleBackend = Stub_ThrottleBackend;
	re.FinishBloom = Stub_FinishBloom;
	re.CanMinimize = Stub_CanMinimize;
	re.GetConfig = Stub_GetConfig;
	re.VertexLighting = Stub_VertexLighting;
	re.SyncRender = Stub_SyncRender;
	re.ReloadTexture = Stub_ReloadTexture;
	re.DrawStretchPicEx = Stub_DrawStretchPicEx;
	re.DrawStretchPicSubpixel = Stub_DrawStretchPicSubpixel;
	re.LoadVectorFont = Stub_LoadVectorFont;
	re.VectorFontActive = Stub_VectorFontActive;
	re.DrawVectorString = Stub_DrawVectorString;
	re.DrawVectorGlyph = Stub_DrawVectorGlyph;

	(void)STUB_BACKEND_ID;
	return &re;
}
