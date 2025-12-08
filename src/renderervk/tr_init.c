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
// tr_init.c -- functions that are not called every frame

#include "tr_local.h"
#include "../renderercommon/tr_backend_iface.h"
#ifdef USE_VULKAN
#include "vk_layered_materials.h"
#endif

glconfig_t	glConfig;

qboolean	textureFilterAnisotropic;
int			maxAnisotropy;
int			gl_version;
int			gl_clamp_mode;	// GL_CLAMP or GL_CLAMP_TO_EGGE

glstate_t	glState;

glstatic_t	gls;

#ifdef USE_VULKAN
static void VkInfo_f( void );
#endif
static void GfxInfo( void );
static void VarInfo( void );
static void GL_SetDefaultState( void );

cvar_t	*r_flareSize;
cvar_t	*r_flareFade;
cvar_t	*r_flareCoeff;

cvar_t	*r_railWidth;
cvar_t	*r_railCoreWidth;
cvar_t	*r_railSegmentLength;

cvar_t	*r_detailTextures;

cvar_t	*r_znear;
cvar_t	*r_zproj;
cvar_t	*r_stereoSeparation;

cvar_t	*r_skipBackEnd;

//cvar_t	*r_anaglyphMode;

cvar_t	*r_greyscale;
cvar_t	*r_dither;
cvar_t	*r_presentBits;

static cvar_t *r_ignorehwgamma;

cvar_t  *r_teleporterFlash;

cvar_t	*r_fastsky;
cvar_t	*r_neatsky;
cvar_t	*r_drawSun;
cvar_t	*r_dynamiclight;
cvar_t  *r_mergeLightmaps;
#ifdef USE_PMLIGHT
cvar_t	*r_dlightMode;
cvar_t	*r_dlightScale;
cvar_t	*r_dlightIntensity;
#endif
cvar_t	*r_dlightSaturation;
#ifdef USE_VULKAN
cvar_t  *r_vk_icd;
cvar_t	*r_device;
#ifdef USE_VBO
cvar_t	*r_vbo;
#endif
#ifdef USE_VK_PBR
cvar_t	*r_pbr;
cvar_t	*r_glint;
cvar_t	*r_glint_intensity;
cvar_t	*r_glint_scale;
cvar_t  *r_baseNormalX;
cvar_t  *r_baseNormalY;
cvar_t  *r_baseParallax;
cvar_t  *r_baseSpecular;
#ifdef VK_CUBEMAP
cvar_t	*r_cubeMapping;
#endif
#endif

// Layered materials / material runtime params
cvar_t *r_layeredMaterials;
cvar_t *r_layeredMaterialMaxLayers;
cvar_t *r_layeredMaterialProfile;
cvar_t *r_layeredMaterialSimple;
cvar_t *r_layeredMaterialsPilot;
#ifdef USE_VULKAN_RAY_TRACING
	cvar_t	*r_raytracing;
	cvar_t	*r_rt_samples;
	cvar_t	*r_rt_maxDepth;
	cvar_t	*r_rt_debugMagenta;
	cvar_t	*r_rt_tlasUpdateMode;
	cvar_t	*r_rt_temporal;
	cvar_t	*r_rt_temporalAlpha;
	cvar_t	*r_rt_blasCompaction;
	cvar_t	*r_rt_blasReuse;
	cvar_t	*r_rt_denoise;
	cvar_t	*r_rt_denoiseMode;
	cvar_t	*r_rt_denoiseIterations;
	cvar_t	*r_rt_denoiseSpatialAlpha;
	cvar_t	*r_rt_denoiseVarianceAlpha;
	cvar_t	*r_rt_gi;
	cvar_t	*r_rt_giBounces;
	cvar_t	*r_rt_giIntensity;
	cvar_t	*r_rt_outputScale;
	cvar_t	*r_rt_shadowRays;
	cvar_t	*r_rt_adaptiveSampling;
#endif
	cvar_t	*r_pathtracing;
	cvar_t	*r_pt_samples;
	cvar_t	*r_pt_bounces;
	cvar_t	*r_pt_maxDepth;
	cvar_t	*r_pt_denoise;
	cvar_t	*r_pt_denoiseIterations;
	cvar_t	*r_pt_temporal;
	cvar_t	*r_pt_temporalAlpha;
	cvar_t	*r_pt_gi;
	cvar_t	*r_pt_giIntensity;
	cvar_t	*r_pt_outputScale;
	cvar_t	*r_postprocess_compute;
	cvar_t	*r_postprocess_workgroup;
cvar_t	*r_tonemapMode;
cvar_t	*r_tonemapExposure;
	cvar_t	*r_meshShaders;
	cvar_t	*r_meshletSize;
	cvar_t	*r_virtualTextures;
	cvar_t	*r_vt_pageSize;
	cvar_t	*r_vt_cacheSize;
	cvar_t	*r_clearcoat;
	cvar_t	*r_anisotropy;
	cvar_t	*r_subsurfaceScattering;
	cvar_t	*r_materialLOD;
	cvar_t	*r_particles_gpu;
	cvar_t	*r_particles_max;
	cvar_t	*r_particles_culling;
	cvar_t	*r_dlss;
	cvar_t	*r_dlss_quality;
	cvar_t	*r_dlss_sharpening;
	cvar_t	*r_gpuCulling;
	cvar_t	*r_gpuInstancing;
	cvar_t	*r_cullDistance;
cvar_t *r_procDressing;
cvar_t *r_procDressingDensity;
cvar_t *r_procDressingDebug;
cvar_t *r_foliageWindStrength;
cvar_t *r_foliageWindFrequency;
cvar_t *r_frameTelemetry;
cvar_t *r_gpuSceneGraph;
cvar_t *r_gpuSceneDebug;
cvar_t *r_gpuSkinning;
cvar_t *r_gpuRagdoll;
	cvar_t	*r_materialSystem;
	cvar_t	*r_materialWetness;
	cvar_t	*r_materialDamage;
	cvar_t	*r_materialMagic;
	cvar_t	*r_cellStreaming;
	cvar_t	*r_cellLoadRadius;
	cvar_t	*r_cellUnloadDistance;
	cvar_t	*r_atmosphere;
	cvar_t	*r_atmospherePreset;
	cvar_t	*r_fogDensity;
	cvar_t	*r_bloomIntensity;
cvar_t	*r_fbo;
cvar_t	*r_hdr;
cvar_t	*r_bloom;
cvar_t	*r_styleTransfer;
cvar_t	*r_styleStrength;
cvar_t	*r_styleLevels;
cvar_t	*r_styleEdge;
cvar_t	*r_bloom_threshold;
cvar_t	*r_bloom_intensity;
cvar_t	*r_bloom_threshold_mode;
cvar_t	*r_bloom_modulate;
cvar_t	*r_renderWidth;
cvar_t	*r_renderHeight;
cvar_t	*r_renderScale;
cvar_t	*r_ext_supersample;

	// Vulkan-specific debug helpers
	cvar_t	*r_vk_debug2D;
	cvar_t	*r_vk_debugClearColor;
	cvar_t	*r_vk_debugUiOnly;
	cvar_t	*r_vk_disableScreenMap;
#endif // USE_VULKAN

cvar_t	*r_dlightBacks;

cvar_t	*r_lodbias;
cvar_t	*r_lodscale;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_facePlaneCull;
cvar_t	*r_showcluster;
cvar_t	*r_nocurves;

cvar_t	*r_allowExtensions;

cvar_t	*r_ext_compressed_textures;
cvar_t	*r_ext_multitexture;
cvar_t	*r_ext_compiled_vertex_array;
cvar_t	*r_ext_texture_env_add;
cvar_t	*r_ext_texture_filter_anisotropic;
cvar_t	*r_ext_max_anisotropy;

cvar_t	*r_ignoreGLErrors;

//cvar_t	*r_stencilbits;
cvar_t	*r_texturebits;
cvar_t	*r_ext_multisample;
cvar_t	*r_ext_alpha_to_coverage;

cvar_t	*r_drawBuffer;
cvar_t	*r_lightmap;
cvar_t	*r_vertexLight;
cvar_t	*r_shadows;
cvar_t	*r_flares;
cvar_t	*r_nobind;
cvar_t	*r_singleShader;
cvar_t	*r_roundImagesDown;
cvar_t	*r_colorMipLevels;
cvar_t	*r_picmip;
cvar_t	*r_nomip;
cvar_t	*r_showtris;
cvar_t	*r_wireframe;
cvar_t	*r_showsky;
cvar_t	*r_shownormals;
cvar_t	*r_finish;
cvar_t	*r_clear;
cvar_t	*r_textureMode;
cvar_t	*r_offsetFactor;
cvar_t	*r_offsetUnits;
cvar_t	*r_gamma;
cvar_t	*r_intensity;
cvar_t	*r_lockpvs;
cvar_t	*r_noportals;
cvar_t	*r_portalOnly;

cvar_t	*r_subdivisions;
cvar_t	*r_lodCurveError;

cvar_t	*r_overBrightBits;
cvar_t	*r_mapOverBrightBits;
cvar_t	*r_mapGreyScale;

cvar_t	*r_debugSurface;
cvar_t	*r_simpleMipMaps;

cvar_t	*r_showImages;
cvar_t	*r_defaultImage;

cvar_t	*r_ambientScale;
cvar_t	*r_directedScale;
cvar_t	*r_debugLight;
cvar_t	*r_debugSort;
cvar_t	*r_printShaders;
cvar_t	*r_saveFontData;
cvar_t	*r_fontAtlasSize;
cvar_t	*r_fontDPI;
cvar_t	*r_fontHinting;
cvar_t	*r_fontAntialiasing;
cvar_t	*r_fontLCDFilter;
cvar_t	*r_fontKerning;
cvar_t	*r_fontSDF;
cvar_t	*r_fontSDFSpread;
cvar_t	*r_fontSDFSmooth;

cvar_t	*r_marksOnTriangleMeshes;

cvar_t	*r_aviMotionJpegQuality;
cvar_t	*r_screenshotJpegQuality;

static cvar_t *r_maxpolys;
static cvar_t* r_maxpolyverts;
int		max_polys;
int		max_polyverts;

#ifdef USE_VULKAN

#include "vk.h"
Vk_Instance vk;
Vk_World	vk_world;

#else

static char gl_extensions[ 32768 ];

#define GLE( ret, name, ... ) ret ( APIENTRY * q##name )( __VA_ARGS__ );
	QGL_Core_PROCS;
	QGL_Ext_PROCS;
#undef GLE

typedef struct {
	void **symbol;
	const char *name;
} sym_t;

#define GLE( ret, name, ... ) { (void**)&q##name, XSTRING(name) },
static sym_t core_procs[] = { QGL_Core_PROCS };
static sym_t ext_procs[] = { QGL_Ext_PROCS };
#undef GLE


/*
==================
R_ResolveSymbols

returns NULL on success or last failed symbol name otherwise
==================
*/
static const char *R_ResolveSymbols( sym_t *syms, int count )
{
	int i;
	for ( i = 0; i < count; i++ )
	{
		*syms[ i ].symbol = ri.GL_GetProcAddress( syms[ i ].name );
		if ( *syms[ i ].symbol == NULL )
		{
			return syms[ i ].name;
		}
	}
	return NULL;
}


static void R_ClearSymbols( sym_t *syms, int count )
{
	int i;
	for ( i = 0; i < count; i++ )
	{
		*syms[ i ].symbol = NULL;
	}
}


static void R_ClearSymTables( void )
{
	R_ClearSymbols( core_procs, ARRAY_LEN( core_procs ) );
	R_ClearSymbols( ext_procs, ARRAY_LEN( ext_procs ) );
}

#endif


// for modular renderer
#ifdef USE_RENDERER_DLOPEN
void QDECL Com_Error( errorParm_t code, const char *fmt, ... )
{
	char buf[ 4096 ];
	va_list	argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( buf, sizeof( buf ), fmt, argptr );
	va_end( argptr );
	ri.Error( code, "%s", buf );
}

void QDECL Com_Printf( const char *fmt, ... )
{
	char buf[ MAXPRINTMSG ];
	va_list	argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( buf, sizeof( buf ), fmt, argptr );
	va_end( argptr );

	ri.Printf( PRINT_ALL, "%s", buf );
}
#endif


#ifndef USE_VULKAN
/*
** R_HaveExtension
*/
static qboolean R_HaveExtension( const char *ext )
{
	const char *ptr = Q_stristr( gl_extensions, ext );
	if (ptr == NULL)
		return qfalse;
	ptr += strlen(ext);
	return ((*ptr == ' ') || (*ptr == '\0'));  // verify its complete string.
}


/*
** R_InitExtensions
*/
static void R_InitExtensions( void )
{
	GLint max_texture_size = 0;
	float version;
	size_t len;

	if ( !qglGetString( GL_EXTENSIONS ) )
	{
		ri.Error( ERR_FATAL, "OpenGL installation is broken. Please fix video drivers and/or restart your system" );
	}

	// get our config strings
	Q_strncpyz( glConfig.vendor_string, (char *)qglGetString (GL_VENDOR), sizeof( glConfig.vendor_string ) );
	Q_strncpyz( glConfig.renderer_string, (char *)qglGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );
	len = strlen( glConfig.renderer_string );
	if ( len && glConfig.renderer_string[ len - 1 ] == '\n' )
		glConfig.renderer_string[ len - 1 ] = '\0';
	Q_strncpyz( glConfig.version_string, (char *)qglGetString( GL_VERSION ), sizeof( glConfig.version_string ) );

	Q_strncpyz( gl_extensions, (char *)qglGetString( GL_EXTENSIONS ), sizeof( gl_extensions ) );
	Q_strncpyz( glConfig.extensions_string, gl_extensions, sizeof( glConfig.extensions_string ) );

	version = Q_atof( (const char *)qglGetString( GL_VERSION ) );
	gl_version = (int)(version * 10.001);

	glConfig.textureCompression = TC_NONE;

	glConfig.textureEnvAddAvailable = qfalse;

	textureFilterAnisotropic = qfalse;
	maxAnisotropy = 0;

	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;

	glConfig.numTextureUnits = 1;
	qglMultiTexCoord2fARB = NULL;
	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;

	gl_clamp_mode = GL_CLAMP; // by default

	// OpenGL driver constants
	qglGetIntegerv( GL_MAX_TEXTURE_SIZE, &max_texture_size );
	glConfig.maxTextureSize = max_texture_size;

	// stubbed or broken drivers may have reported 0...
	if ( glConfig.maxTextureSize <= 0 )
		glConfig.maxTextureSize = 0;
	else if ( glConfig.maxTextureSize > MAX_TEXTURE_SIZE )
		glConfig.maxTextureSize = MAX_TEXTURE_SIZE; // ResampleTexture() relies on that maximum

	if ( !r_allowExtensions->integer )
	{
		ri.Printf( PRINT_ALL, "*** IGNORING OPENGL EXTENSIONS ***\n" );
		return;
	}

	ri.Printf( PRINT_ALL, "Initializing OpenGL extensions\n" );

	if ( R_HaveExtension( "GL_EXT_texture_edge_clamp" ) || R_HaveExtension( "GL_SGIS_texture_edge_clamp" ) ) {
		gl_clamp_mode = GL_CLAMP_TO_EDGE;
		ri.Printf( PRINT_ALL, "...using GL_EXT_texture_edge_clamp\n" );
	} else {
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_edge_clamp not found\n" );
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW "...Degraded texture support likely!\n" );
	}

	// GL_EXT_texture_compression_s3tc
	if ( R_HaveExtension( "GL_ARB_texture_compression" ) &&
		 R_HaveExtension( "GL_EXT_texture_compression_s3tc" ) )
	{
		if ( r_ext_compressed_textures->integer ){
			glConfig.textureCompression = TC_S3TC_ARB;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_compression_s3tc\n" );
		} else {
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_compression_s3tc\n" );
		}
	} else {
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_compression_s3tc not found\n" );
	}

	// GL_S3_s3tc
	if ( glConfig.textureCompression == TC_NONE && r_ext_compressed_textures->integer ) {
		if ( R_HaveExtension( "GL_S3_s3tc" ) ) {
			if ( r_ext_compressed_textures->integer ) {
				glConfig.textureCompression = TC_S3TC;
				ri.Printf( PRINT_ALL, "...using GL_S3_s3tc\n" );
			} else {
				glConfig.textureCompression = TC_NONE;
				ri.Printf( PRINT_ALL, "...ignoring GL_S3_s3tc\n" );
			}
		} else {
			ri.Printf( PRINT_ALL, "...GL_S3_s3tc not found\n" );
		}
	}

	// GL_EXT_texture_env_add
	if ( R_HaveExtension( "EXT_texture_env_add" ) ) {
		if ( r_ext_texture_env_add->integer ) {
			glConfig.textureEnvAddAvailable = qtrue;
			ri.Printf( PRINT_ALL, "...using GL_EXT_texture_env_add\n" );
		} else {
			glConfig.textureEnvAddAvailable = qfalse;
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_env_add\n" );
		}
	} else {
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_env_add not found\n" );
	}

	// GL_ARB_multitexture
	if ( R_HaveExtension( "GL_ARB_multitexture" ) )
	{
		if ( r_ext_multitexture->integer )
		{
			qglMultiTexCoord2fARB = ri.GL_GetProcAddress( "glMultiTexCoord2fARB" );
			qglActiveTextureARB = ri.GL_GetProcAddress( "glActiveTextureARB" );
			qglClientActiveTextureARB = ri.GL_GetProcAddress( "glClientActiveTextureARB" );

			if ( qglActiveTextureARB && qglClientActiveTextureARB )
			{
				GLint textureUnits = 0;

				qglGetIntegerv( GL_MAX_ACTIVE_TEXTURES_ARB, &textureUnits );

				if ( textureUnits > 1 )
				{
					GLint max_shader_units = 0;
					GLint max_bind_units = 0;

					qglGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &max_shader_units );
					qglGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_bind_units );

					if ( max_bind_units > max_shader_units )
						max_bind_units = max_shader_units;
					if ( max_bind_units > MAX_TEXTURE_UNITS )
						max_bind_units = MAX_TEXTURE_UNITS;

					glConfig.numTextureUnits = MAX( textureUnits, max_bind_units );
					ri.Printf( PRINT_ALL, "...using GL_ARB_multitexture\n" );
				}
				else
				{
					qglMultiTexCoord2fARB = NULL;
					qglActiveTextureARB = NULL;
					qglClientActiveTextureARB = NULL;
					ri.Printf( PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n" );
				}
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_ARB_multitexture\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_ARB_multitexture not found\n" );
	}

	// GL_EXT_compiled_vertex_array
	if ( R_HaveExtension( "GL_EXT_compiled_vertex_array" ) )
	{
		if ( r_ext_compiled_vertex_array->integer )
		{
			ri.Printf( PRINT_ALL, "...using GL_EXT_compiled_vertex_array\n" );
			qglLockArraysEXT = ri.GL_GetProcAddress( "glLockArraysEXT" );
			qglUnlockArraysEXT = ri.GL_GetProcAddress( "glUnlockArraysEXT" );
			if ( !qglLockArraysEXT || !qglUnlockArraysEXT ) {
				ri.Error( ERR_FATAL, "bad getprocaddress" );
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_compiled_vertex_array\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_compiled_vertex_array not found\n" );
	}

	if ( R_HaveExtension( "GL_EXT_texture_filter_anisotropic" ) )
	{
		if ( r_ext_texture_filter_anisotropic->integer ) {
			qglGetIntegerv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy );
			if ( maxAnisotropy <= 0 ) {
				ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not properly supported!\n" );
				maxAnisotropy = 0;
			}
			else
			{
				ri.Printf( PRINT_ALL, "...using GL_EXT_texture_filter_anisotropic (max: %i)\n", maxAnisotropy );
				textureFilterAnisotropic = qtrue;
				maxAnisotropy = MIN( r_ext_texture_filter_anisotropic->integer, maxAnisotropy );
			}
		}
		else
		{
			ri.Printf( PRINT_ALL, "...ignoring GL_EXT_texture_filter_anisotropic\n" );
		}
	}
	else
	{
		ri.Printf( PRINT_ALL, "...GL_EXT_texture_filter_anisotropic not found\n" );
	}
}
#endif


/*
** InitOpenGL
**
** This function is responsible for initializing a valid OpenGL subsystem.  This
** is done by calling GLimp_Init (which gives us a working OGL subsystem) then
** setting variables, checking GL constants, and reporting the gfx system config
** to the user.
*/
static void InitOpenGL( void )
{
	//
	// initialize OS specific portions of the renderer
	//
	// GLimp_Init directly or indirectly references the following cvars:
	//		- r_fullscreen
	//		- r_mode
	//		- r_(color|depth|stencil)bits
	//		- r_ignorehwgamma
	//		- r_gamma
	//

	if ( glConfig.vidWidth == 0 )
	{
#ifdef USE_VULKAN
		if ( !ri.VKimp_Init )
		{
			ri.Error( ERR_FATAL, "Vulkan interface is not initialized" );
		}

		// This function is responsible for initializing a valid Vulkan subsystem.
		ri.VKimp_Init( &glConfig );

		gls.windowWidth = glConfig.vidWidth;
		gls.windowHeight = glConfig.vidHeight;

		gls.captureWidth = glConfig.vidWidth;
		gls.captureHeight = glConfig.vidHeight;

		ri.CL_SetScaling( 1.0, glConfig.vidWidth, glConfig.vidHeight );

		if ( r_fbo->integer )
		{
			if ( r_renderScale->integer )
			{
				glConfig.vidWidth = r_renderWidth->integer;
				glConfig.vidHeight = r_renderHeight->integer;
			}

			gls.captureWidth = glConfig.vidWidth;
			gls.captureHeight = glConfig.vidHeight;

			ri.CL_SetScaling( 1.0, gls.captureWidth, gls.captureHeight );

			if ( r_ext_supersample->integer )
			{
				glConfig.vidWidth *= 2;
				glConfig.vidHeight *= 2;
				ri.CL_SetScaling( 2.0, gls.captureWidth, gls.captureHeight );
			}
		}

		vk_initialize();
#else
		const char *err;

		ri.GLimp_Init( &glConfig );

		R_ClearSymTables();

		err = R_ResolveSymbols( core_procs, ARRAY_LEN( core_procs ) );
		if ( err )
			ri.Error( ERR_FATAL, "Error resolving core OpenGL function '%s'", err );

		R_InitExtensions();
#endif

		glConfig.deviceSupportsGamma = qfalse;

		ri.GLimp_InitGamma( &glConfig );

		gls.deviceSupportsGamma = glConfig.deviceSupportsGamma;

		if ( r_ignorehwgamma->integer )
			glConfig.deviceSupportsGamma = qfalse;

		// print info
		GfxInfo();

		gls.initTime = ri.Milliseconds();
	}

#ifdef USE_VULKAN
	if ( !vk.active ) {
		// might happen after REF_KEEP_WINDOW
		vk_initialize();
		gls.initTime = ri.Milliseconds();
	}
	if ( vk.active ) {
		vk_init_descriptors();
	} else {
		ri.Error( ERR_FATAL, "Recursive error during Vulkan initialization" );
	}
#endif

	// set default state
	GL_SetDefaultState();

	tr.inited = qtrue;
}


/*
==================
GL_CheckErrors
==================
*/
void GL_CheckErrors( void ) {
#ifdef USE_VULKAN
#else
	int		err;
    const char *s;
    char buf[32];

    err = qglGetError();
    if ( err == GL_NO_ERROR ) {
        return;
    }
    if ( r_ignoreGLErrors->integer ) {
        return;
    }
    switch( err ) {
        case GL_INVALID_ENUM:
            s = "GL_INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            s = "GL_INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            s = "GL_INVALID_OPERATION";
            break;
        case GL_STACK_OVERFLOW:
            s = "GL_STACK_OVERFLOW";
            break;
        case GL_STACK_UNDERFLOW:
            s = "GL_STACK_UNDERFLOW";
            break;
        case GL_OUT_OF_MEMORY:
            s = "GL_OUT_OF_MEMORY";
            break;
        default:
            Com_sprintf( buf, sizeof(buf), "%i", err);
            s = buf;
            break;
    }

    ri.Error( ERR_FATAL, "GL_CheckErrors: %s", s );
#endif
}


/*
==============================================================================

						SCREEN SHOTS
						
NOTE TTimo
some thoughts about the screenshots system:
screenshots get written in fs_homepath + fs_gamedir
vanilla q3 .. baseq3/screenshots/ *.tga
team arena .. missionpack/screenshots/ *.tga

two commands: "screenshot" and "screenshotJPEG"
we use statics to store a count and start writing the first screenshot/screenshot????.tga (.jpg) available
(with FS_FileExists / FS_FOpenFileWrite calls)

==============================================================================
*/

/*
==================
RB_ReadPixels

Reads an image but takes care of alignment issues for reading RGB images.

Reads a minimum offset for where the RGB data starts in the image from
integer stored at pointer offset. When the function has returned the actual
offset was written back to address offset. This address will always have an
alignment of packAlign to ensure efficient copying.

Stores the length of padding after a line of pixels to address padlen

Return value must be freed with ri.Hunk_FreeTempMemory()
==================
*/
static byte *RB_ReadPixels(int x, int y, int width, int height, size_t *offset, int *padlen, int lineAlign )
{
	(void)x;         // Suppress unused parameter warning
	(void)y;         // Suppress unused parameter warning
	(void)lineAlign;  // Suppress unused parameter warning
#ifdef USE_VULKAN
	byte *buffer, *bufstart;
	int linelen;
	int	bufAlign;
	int packAlign = 1;

	linelen = width * 3;

	bufAlign = MAX( packAlign, 16 ); // for SIMD

	// Allocate a few more bytes so that we can choose an alignment we like
	//buffer = ri.Hunk_AllocateTempMemory(padwidth * height + *offset + bufAlign - 1);
	buffer = ri.Hunk_AllocateTempMemory(width * height * 4 + *offset + bufAlign - 1);
	bufstart = PADP((intptr_t) buffer + *offset, bufAlign);

	vk_read_pixels( bufstart, width, height );

	*offset = bufstart - buffer;
	*padlen = PAD(linelen, packAlign) - linelen;

	return buffer;
#else
	byte *buffer, *bufstart;
	int padwidth, linelen;
	int	bufAlign;
	GLint packAlign;

	qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);

	linelen = width * 3;

	if ( packAlign < lineAlign )
		padwidth = PAD(linelen, lineAlign);
	else
		padwidth = PAD(linelen, packAlign);

	bufAlign = MAX( packAlign, 16 ); // for SIMD

	// Allocate a few more bytes so that we can choose an alignment we like
	buffer = ri.Hunk_AllocateTempMemory(padwidth * height + *offset + bufAlign - 1);
	bufstart = PADP((intptr_t) buffer + *offset, bufAlign);

	qglReadPixels( x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, bufstart );

	*offset = bufstart - buffer;
	*padlen = PAD(linelen, packAlign) - linelen;

	return buffer;
#endif
}


/*
==================
RB_TakeScreenshot
==================
*/
void RB_TakeScreenshot( int x, int y, int width, int height, const char *fileName )
{
	const int header_size = 18;
	byte *allbuf, *buffer;
	byte *srcptr, *destptr;
	byte *endline, *endmem;
	byte temp;
	int linelen, padlen;
	size_t offset, memcount;

	offset = header_size;
	allbuf = RB_ReadPixels( x, y, width, height, &offset, &padlen, 0 );
	buffer = allbuf + offset - header_size;

	Com_Memset( buffer, 0, header_size );
	buffer[2] = 2;		// uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24;	// pixel size

	// swap rgb to bgr and remove padding from line endings
	linelen = width * 3;

	srcptr = destptr = allbuf + offset;
	endmem = srcptr + (linelen + padlen) * height;

	while(srcptr < endmem)
	{
		endline = srcptr + linelen;

		while(srcptr < endline)
		{
			temp = srcptr[0];
			*destptr++ = srcptr[2];
			*destptr++ = srcptr[1];
			*destptr++ = temp;

			srcptr += 3;
		}

		// Skip the pad
		srcptr += padlen;
	}

	memcount = linelen * height;

	// gamma correction
	R_GammaCorrect( allbuf + offset, memcount );

	ri.FS_WriteFile( fileName, buffer, memcount + header_size );

	ri.Hunk_FreeTempMemory( allbuf );
}


/*
==================
RB_TakeScreenshotJPEG
==================
*/
void RB_TakeScreenshotJPEG( int x, int y, int width, int height, const char *fileName )
{
	byte *buffer;
	size_t offset = 0, memcount;
	int padlen;

	buffer = RB_ReadPixels(x, y, width, height, &offset, &padlen, 0);
	memcount = (width * 3 + padlen) * height;

	// gamma correction
	R_GammaCorrect( buffer + offset, memcount );

	ri.CL_SaveJPG( fileName, r_screenshotJpegQuality->integer, width, height, buffer + offset, padlen );
	ri.Hunk_FreeTempMemory( buffer );
}


static void FillBMPHeader( byte *buffer, int width, int height, int memcount, int header_size )
{
	int filesize;
	Com_Memset( buffer, 0, header_size );

	// bitmap file header
	buffer[0] = 'B';
	buffer[1] = 'M';
	filesize = memcount + header_size;
	buffer[2] = (filesize >> 0) & 255;
	buffer[3] = (filesize >> 8) & 255;
	buffer[4] = (filesize >> 16) & 255;
	buffer[5] = (filesize >> 24) & 255;
	buffer[10] = header_size; // data offset

	// bitmap info header
	buffer[14] = 40; // size of this header
	buffer[18] = (width >> 0) & 255;
	buffer[19] = (width >> 8) & 255;
	buffer[20] = (width >> 16) & 255;
	buffer[21] = (width >> 24) & 255;

	buffer[22] = (height >> 0) & 255;
	buffer[23] = (height >> 8) & 255;
	buffer[24] = (height >> 16) & 255;
	buffer[25] = (height >> 24) & 255;
	buffer[26] = 1; // number of color planes
	buffer[28] = 24; // bpp

	buffer[34] = (memcount >> 0) & 255;
	buffer[35] = (memcount >> 8) & 255;
	buffer[36] = (memcount >> 16) & 255;
	buffer[37] = (memcount >> 24) & 255;
	buffer[38] = 0xC4; // horizontal dpi
	buffer[39] = 0x0E; // horizontal dpi
	buffer[42] = 0xC4; // vertical dpi
	buffer[43] = 0x0E; // vertical dpi
}


/*
==================
RB_TakeScreenshotBMP
==================
*/
void RB_TakeScreenshotBMP( int x, int y, int width, int height, const char *fileName, int clipboardOnly )
{
	byte *allbuf;
	byte *buffer; // destination buffer
	byte *srcptr, *srcline;
	byte *destptr, *dstline;
	byte *endmem;
	byte temp[4];
	size_t memcount, offset;
	const int header_size = 54; // bitmapfileheader(14) + bitmapinfoheader(40)
	int scanlen, padlen;
	int scanpad, len;

	offset = header_size;

	allbuf = RB_ReadPixels( x, y, width, height, &offset, &padlen, 4 );
	buffer = allbuf + offset;

	// scanline length
	scanlen = PAD( width*3, 4 );
	scanpad = scanlen - width*3;
	memcount = scanlen * height;

	// swap rgb to bgr and add line padding
	if ( scanpad == 0 && padlen == 0 ) {
		// fastest case
		srcptr = destptr = allbuf + offset;
		endmem = srcptr + scanlen * height;
		while ( srcptr < endmem ) {
			temp[0] = srcptr[0];
			destptr[0] = srcptr[2];
			destptr[2] = temp[0];
			destptr += 3;
			srcptr += 3;
		}
	} else {
		// move destination buffer forward if source padding is greater than for BMP
		if ( padlen > scanpad )
			buffer += (width * 3 + padlen - scanlen ) * height;
		// point on last line
		srcptr = allbuf + offset + (height-1) * (width * 3 + padlen);
		destptr = buffer + (height-1) * scanlen;
		len = (width * 3 - 3);
		while ( destptr >= buffer ) {
			srcline = srcptr + len;
			dstline = destptr + len;
			while ( srcline >= srcptr ) {
				temp[2] = srcline[0];
				temp[1] = srcline[1];
				temp[0] = srcline[2];
				dstline[0] = temp[0];
				dstline[1] = temp[1];
				dstline[2] = temp[2];
				dstline-=3;
				srcline-=3;
			}
			srcptr -= (width * 3 + padlen);
			destptr -= scanlen;
		}
	}

	// fill this last to avoid data overwrite in case when we're moving destination buffer forward
	FillBMPHeader( buffer - header_size, width, height, memcount, header_size );

	// gamma correction
	R_GammaCorrect( buffer, memcount );

	if ( clipboardOnly ) {
		// copy starting from bitmapinfoheader
		ri.Sys_SetClipboardBitmap( buffer - 40, memcount + 40 );
	} else {
		ri.FS_WriteFile( fileName, buffer - header_size, memcount + header_size );
	}

	ri.Hunk_FreeTempMemory( allbuf );
}


/*
==================
R_ScreenshotFilename
==================
*/
static void R_ScreenshotFilename( char *fileName, const char *fileExt ) {
	qtime_t t;
	int count;

	count = 0;
	ri.Com_RealTime( &t );

	Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot-%04d%02d%02d-%02d%02d%02d.%s",
			1900 + t.tm_year, 1 + t.tm_mon,	t.tm_mday,
			t.tm_hour, t.tm_min, t.tm_sec, fileExt );

	while (	ri.FS_FileExists( fileName ) && ++count < 1000 ) {
		Com_sprintf( fileName, MAX_OSPATH, "screenshots/shot-%04d%02d%02d-%02d%02d%02d-%d.%s",
				1900 + t.tm_year, 1 + t.tm_mon,	t.tm_mday,
				t.tm_hour, t.tm_min, t.tm_sec, count, fileExt );
	}
}


/*
====================
R_LevelShot

levelshots are specialized 128*128 thumbnails for
the menu system, sampled down from full screen distorted images
====================
*/
static void R_LevelShot( void ) {
	char		checkname[MAX_OSPATH];
	byte		*buffer;
	byte		*source, *allsource;
	byte		*src, *dst;
	size_t		offset = 0;
	int			padlen;
	int			x, y;
	int			r, g, b;
	float		xScale, yScale;
	int			xx, yy;

	Com_sprintf(checkname, sizeof(checkname), "levelshots/%s.tga", tr.world->baseName);

	allsource = RB_ReadPixels(0, 0, gls.captureWidth, gls.captureHeight, &offset, &padlen, 0 );
	source = allsource + offset;

	buffer = ri.Hunk_AllocateTempMemory(128 * 128*3 + 18);
	Com_Memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = 128;
	buffer[14] = 128;
	buffer[16] = 24;	// pixel size

	// resample from source
	xScale = glConfig.vidWidth / 512.0f;
	yScale = glConfig.vidHeight / 384.0f;
	for ( y = 0 ; y < 128 ; y++ ) {
		for ( x = 0 ; x < 128 ; x++ ) {
			r = g = b = 0;
			for ( yy = 0 ; yy < 3 ; yy++ ) {
				for ( xx = 0 ; xx < 4 ; xx++ ) {
					src = source + (3 * glConfig.vidWidth + padlen) * (int)((y*3 + yy) * yScale) +
						3 * (int) ((x*4 + xx) * xScale);
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * ( y * 128 + x );
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	// gamma correction
	R_GammaCorrect( buffer + 18, 128 * 128 * 3 );

	ri.FS_WriteFile( checkname, buffer, 128 * 128*3 + 18 );

	ri.Hunk_FreeTempMemory(buffer);
	ri.Hunk_FreeTempMemory(allsource);

	ri.Printf( PRINT_ALL, "Wrote %s\n", checkname );
}


/*
==================
R_ScreenShot_f

screenshot
screenshot [silent]
screenshot [levelshot]
screenshot [filename]

Doesn't print the pacifier message if there is a second arg
==================
*/
static void R_ScreenShot_f( void ) {
	char		checkname[MAX_OSPATH];
	qboolean	silent;
	int			typeMask;
	const char	*ext;

	if ( ri.CL_IsMinimized() && !RE_CanMinimize() ) {
		ri.Printf( PRINT_WARNING, "WARNING: unable to take screenshot when minimized because FBO is not available/enabled.\n" );
		return;
	}

	if ( !strcmp( ri.Cmd_Argv(1), "levelshot" ) ) {
		R_LevelShot();
		return;
	}

	if ( Q_stricmp( ri.Cmd_Argv(0), "screenshotJPEG" ) == 0 ) {
		typeMask = SCREENSHOT_JPG;
		ext = "jpg";
	} else if ( Q_stricmp( ri.Cmd_Argv(0), "screenshotBMP" ) == 0 ) {
		typeMask = SCREENSHOT_BMP;
		ext = "bmp";
	} else {
		typeMask = SCREENSHOT_TGA;
		ext = "tga";
	}

	// check if already scheduled
	if ( backEnd.screenshotMask & typeMask )
		return;

	if ( !strcmp( ri.Cmd_Argv(1), "silent" ) ) {
		silent = qtrue;
	} else if ( typeMask == SCREENSHOT_BMP && !strcmp( ri.Cmd_Argv(1), "clipboard" ) ) {
		backEnd.screenshotMask |= SCREENSHOT_BMP_CLIPBOARD;
		silent = qtrue;
	} else {
		silent = qfalse;
	}

	if ( ri.Cmd_Argc() == 2 && !silent ) {
		// explicit filename
		Com_sprintf( checkname, MAX_OSPATH, "screenshots/%s.%s", ri.Cmd_Argv( 1 ), ext );
	} else {
		if ( backEnd.screenshotMask & SCREENSHOT_BMP_CLIPBOARD ) {
			// no need for filename, copy to system buffer
			checkname[0] = '\0';
		} else {
			// scan for a free filename
			R_ScreenshotFilename( checkname, ext );
		}
	}

	// we will make screenshot right at the end of RE_EndFrame()
	backEnd.screenshotMask |= typeMask;
	if ( typeMask == SCREENSHOT_JPG ) {
		backEnd.screenShotJPGsilent = silent;
		Q_strncpyz( backEnd.screenshotJPG, checkname, sizeof( backEnd.screenshotJPG ) );
	} else if ( typeMask == SCREENSHOT_BMP ) {
		backEnd.screenShotBMPsilent = silent;
		Q_strncpyz( backEnd.screenshotBMP, checkname, sizeof( backEnd.screenshotBMP ) );
	} else {
		backEnd.screenShotTGAsilent = silent;
		Q_strncpyz( backEnd.screenshotTGA, checkname, sizeof( backEnd.screenshotTGA ) );
	}
}


//============================================================================

/*
==================
RB_TakeVideoFrameCmd
==================
*/
const void *RB_TakeVideoFrameCmd( const void *data )
{
	const videoFrameCommand_t *cmd;
	byte		*cBuf;
	size_t		memcount, linelen;
	int			padwidth, avipadwidth, padlen, avipadlen;
	int			packAlign;

	cmd = (const videoFrameCommand_t *)data;

#ifdef USE_VULKAN
	packAlign = 1;
#else
	qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);
#endif

	linelen = cmd->width * 3;

	// Alignment stuff for glReadPixels
	padwidth = PAD(linelen, packAlign);
	padlen = padwidth - linelen;
	// AVI line padding
	avipadwidth = PAD(linelen, AVI_LINE_PADDING);
	avipadlen = avipadwidth - linelen;

	cBuf = PADP(cmd->captureBuffer, packAlign);

#ifdef USE_VULKAN
	vk_read_pixels(cBuf, cmd->width, cmd->height);
#else
	qglReadPixels(0, 0, cmd->width, cmd->height, GL_RGB, GL_UNSIGNED_BYTE, cBuf);
#endif

	memcount = padwidth * cmd->height;

	// gamma correction
	R_GammaCorrect( cBuf, memcount );

	if ( cmd->motionJpeg )
	{
		memcount = ri.CL_SaveJPGToBuffer( cmd->encodeBuffer, linelen * cmd->height,
			r_aviMotionJpegQuality->integer,
			cmd->width, cmd->height, cBuf, padlen );
		ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, memcount);
	}
	else
	{
		byte *lineend, *memend;
		byte *srcptr, *destptr;

		srcptr = cBuf;
		destptr = cmd->encodeBuffer;
		memend = srcptr + memcount;

		// swap R and B and remove line paddings
		while(srcptr < memend)
		{
			lineend = srcptr + linelen;
			while(srcptr < lineend)
			{
				*destptr++ = srcptr[2];
				*destptr++ = srcptr[1];
				*destptr++ = srcptr[0];
				srcptr += 3;
			}

			Com_Memset(destptr, '\0', avipadlen);
			destptr += avipadlen;

			srcptr += padlen;
		}

		ri.CL_WriteAVIVideoFrame(cmd->encodeBuffer, avipadwidth * cmd->height);
	}

	return (const void *)(cmd + 1);
}


//============================================================================

/*
** GL_SetDefaultState
*/
static void GL_SetDefaultState( void )
{
#ifdef USE_VULKAN
	GL_TextureMode( r_textureMode->string );

	glState.glStateBits = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE;
#else
	int i;

	glState.currenttmu = 0;
	glState.currentArray = 0;

	for ( i = 0; i < MAX_TEXTURE_UNITS; i++ )
	{
		glState.currenttextures[ i ] = 0;
		glState.glClientStateBits[ i ] = 0;
	}

	qglClearDepth( 1.0f );

	qglCullFace( GL_FRONT );
	glState.faceCulling = -1;

	qglColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	// initialize downstream texture unit if we're running
	// in a multitexture environment
	if ( qglActiveTextureARB )
	{
		qglActiveTextureARB( GL_TEXTURE1_ARB );
		GL_TextureMode( r_textureMode->string );
		GL_TexEnv( GL_MODULATE );
		qglDisable( GL_TEXTURE_2D );
		qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
		qglActiveTextureARB( GL_TEXTURE0_ARB );
	}

	qglEnable( GL_TEXTURE_2D );
	GL_TextureMode( r_textureMode->string );
	GL_TexEnv( GL_MODULATE );

	qglShadeModel( GL_SMOOTH );
	qglDepthFunc( GL_LEQUAL );

	// the vertex array is always enabled, but the color and texture
	// arrays are enabled and disabled around the compiled vertex array call
	qglEnableClientState( GL_VERTEX_ARRAY );

	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	qglDisableClientState( GL_COLOR_ARRAY );
	qglDisableClientState( GL_NORMAL_ARRAY );

	//
	// make sure our GL state vector is set correctly
	//
	glState.glStateBits = GLS_DEPTHTEST_DISABLE | GLS_DEPTHMASK_TRUE;

	qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	qglDepthMask( GL_TRUE );
	qglDisable( GL_DEPTH_TEST );
	qglEnable( GL_SCISSOR_TEST );
	qglDisable( GL_CULL_FACE );
	qglDisable( GL_BLEND );
#endif
}


/*
================
R_PrintLongString

Workaround for ri.Printf's 1024 characters buffer limit.
================
*/
static void R_PrintLongString(const char *string) {
	char buffer[1024];
	const char *p;
	int size = strlen(string);

	p = string;
	while(size > 0)
	{
		Q_strncpyz(buffer, p, sizeof (buffer) );
		ri.Printf( PRINT_DEVELOPER, "%s", buffer );
		p += 1023;
		size -= 1023;
	}
}


/*
================
GfxInfo

Prints persistent rendering configuration
================
*/
static void GfxInfo( void )
{
	const char *fsstrings[] = { "windowed", "fullscreen" };
	const char *fs;
	int mode;
#ifdef USE_VULKAN
	ri.Printf( PRINT_ALL, "\nVK_VENDOR: %s\n", glConfig.vendor_string );
	ri.Printf( PRINT_ALL, "VK_RENDERER: %s\n", glConfig.renderer_string );
	ri.Printf( PRINT_ALL, "VK_VERSION: %s\n", glConfig.version_string );

	ri.Printf( PRINT_DEVELOPER, "VK_EXTENSIONS: " );
	R_PrintLongString( glConfig.extensions_string );

	ri.Printf( PRINT_ALL, "\nVK_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize );
	ri.Printf( PRINT_ALL, "VK_MAX_TEXTURE_UNITS: %d\n", glConfig.numTextureUnits );
#else
	const char *enablestrings[] = { "disabled", "enabled" };

	ri.Printf( PRINT_ALL, "\nGL_VENDOR: %s\n", glConfig.vendor_string );
	ri.Printf( PRINT_ALL, "GL_RENDERER: %s\n", glConfig.renderer_string );
	ri.Printf( PRINT_ALL, "GL_VERSION: %s\n", glConfig.version_string );
	ri.Printf( PRINT_DEVELOPER, "GL_EXTENSIONS: " );
	R_PrintLongString( glConfig.extensions_string );
	ri.Printf( PRINT_ALL, "\n" );
	ri.Printf( PRINT_ALL, "GL_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize );
	ri.Printf( PRINT_ALL, "GL_MAX_TEXTURE_UNITS_ARB: %d\n", glConfig.numTextureUnits );
#endif

	ri.Printf( PRINT_ALL, "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits );
#ifdef USE_VULKAN
	ri.Printf( PRINT_ALL, " presentation: %s\n", vk_format_string( vk.present_format.format ) );
	if ( vk.color_format != vk.present_format.format ) {
		ri.Printf( PRINT_ALL, " color: %s\n", vk_format_string( vk.color_format ) );
	}
	if ( vk.capture_format != vk.present_format.format || vk.capture_format != vk.color_format ) {
		ri.Printf( PRINT_ALL, " capture: %s\n", vk_format_string( vk.capture_format ) );
	}
	ri.Printf( PRINT_ALL, " depth: %s\n", vk_format_string( vk.depth_format ) );
#endif
	if ( glConfig.isFullscreen )
	{
		const char *modefs = ri.Cvar_VariableString( "r_modeFullscreen" );
		if ( *modefs )
			mode = atoi( modefs );
		else
			mode = ri.Cvar_VariableIntegerValue( "r_mode" );
		fs = fsstrings[1];
	}
	else
	{
		mode = ri.Cvar_VariableIntegerValue( "r_mode" );
		fs = fsstrings[0];
	}

	if ( glConfig.vidWidth != gls.windowWidth || glConfig.vidHeight != gls.windowHeight )
	{
		ri.Printf( PRINT_ALL, "RENDER: %d x %d, MODE: %d, %d x %d %s hz:", glConfig.vidWidth, glConfig.vidHeight, mode, gls.windowWidth, gls.windowHeight, fs );
	}
	else
	{
		ri.Printf( PRINT_ALL, "MODE: %d, %d x %d %s hz:", mode, gls.windowWidth, gls.windowHeight, fs );
	}

	if ( glConfig.displayFrequency )
	{
		ri.Printf( PRINT_ALL, "%d\n", glConfig.displayFrequency );
	}
	else
	{
		ri.Printf( PRINT_ALL, "N/A\n" );
	}

#ifndef USE_VULKAN
	ri.Printf( PRINT_ALL, "multitexture: %s\n", enablestrings[qglActiveTextureARB != 0] );
	ri.Printf( PRINT_ALL, "compiled vertex arrays: %s\n", enablestrings[qglLockArraysEXT != 0 ] );
	ri.Printf( PRINT_ALL, "texenv add: %s\n", enablestrings[glConfig.textureEnvAddAvailable != 0] );
	ri.Printf( PRINT_ALL, "compressed textures: %s\n", enablestrings[glConfig.textureCompression!=TC_NONE] );
#endif
}


/*
================
VarInfo

Prints info that may change every R_Init() call
================
*/
static void VarInfo( void )
{
	if ( glConfig.deviceSupportsGamma ) {
		ri.Printf( PRINT_ALL, "GAMMA: hardware w/ %d overbright bits\n", tr.overbrightBits );
	} else {
		ri.Printf( PRINT_ALL, "GAMMA: software w/ %d overbright bits\n", tr.overbrightBits );
	}

	ri.Printf( PRINT_ALL, "texturemode: %s\n", r_textureMode->string );
	ri.Printf( PRINT_ALL, "texture bits: %d\n", r_texturebits->integer ? r_texturebits->integer : 32 );
	ri.Printf( PRINT_ALL, "picmip: %d%s\n", r_picmip->integer, r_nomip->integer ? ", worldspawn only" : "" );

#ifdef USE_VULKAN
	if ( r_vertexLight->integer ) {
		ri.Printf( PRINT_ALL, "using vertex lightmap approximation\n" );
	}
#else
	if ( r_vertexLight->integer || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		ri.Printf( PRINT_ALL, "using vertex lightmap approximation\n" );
	} else if ( glConfig.hardwareType == GLHW_RAGEPRO ) {
		ri.Printf( PRINT_ALL, "ragePro approximations enabled\n" );
	} else if ( glConfig.hardwareType == GLHW_RIVA128 ) {
		ri.Printf( PRINT_ALL, "riva128 approximations enabled\n" );
	}
#endif
	if ( r_finish->integer ) {
		ri.Printf( PRINT_ALL, "Forcing glFinish\n" );
	}
}


/*
===============
GfxInfo_f
===============
*/
static void GfxInfo_f( void )
{
	GfxInfo();
	VarInfo();
}


#ifdef USE_VULKAN
static void VkInfo_f( void )
{
	ri.Printf(PRINT_ALL, "max_vertex_usage: %iKb\n", (int)((vk.stats.vertex_buffer_max + 1023) / 1024) );
	ri.Printf(PRINT_ALL, "max_push_size: %ib\n", vk.stats.push_size_max );

	ri.Printf(PRINT_ALL, "pipeline handles: %i\n", vk.pipeline_create_count );
	ri.Printf(PRINT_ALL, "pipeline descriptors: %i, base: %i\n", vk.pipelines_count, vk.pipelines_world_base );
	ri.Printf(PRINT_ALL, "image chunks: %i\n", vk_world.num_image_chunks );
}
#endif


/*
===============
RE_SyncRender
===============
*/
static void RE_SyncRender( void )
{
#ifdef USE_VULKAN
	if ( vk.device )
		vk_wait_idle();
#else
	if ( qglFinish && backEnd.doneSurfaces )
		qglFinish();
#endif
}


/*
===============
R_Register
===============
*/
static void R_Register( void )
{
	// make sure all the commands added here are also removed in R_Shutdown
	ri.Cmd_AddCommand( "imagelist", R_ImageList_f );
	ri.Cmd_AddCommand( "shaderlist", R_ShaderList_f );
	ri.Cmd_AddCommand( "skinlist", R_SkinList_f );
	ri.Cmd_AddCommand( "modellist", R_Modellist_f );
	ri.Cmd_AddCommand( "screenshot", R_ScreenShot_f );
	ri.Cmd_AddCommand( "screenshotJPEG", R_ScreenShot_f );
	ri.Cmd_AddCommand( "screenshotBMP", R_ScreenShot_f );
	ri.Cmd_AddCommand( "gfxinfo", GfxInfo_f );
#ifdef USE_VULKAN
	ri.Cmd_AddCommand( "vkinfo", VkInfo_f );
#endif

	//
	// temporary latched variables that can only change over a restart
	//
	r_fullbright = ri.Cvar_Get( "r_fullbright", "0", CVAR_LATCH );
	ri.Cvar_SetDescription( r_fullbright, "Debugging tool to render the entire level without lighting." );
	r_overBrightBits = ri.Cvar_Get( "r_overBrightBits", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_overBrightBits, "Sets the intensity of overall brightness of texture pixels." );
	r_mapOverBrightBits = ri.Cvar_Get( "r_mapOverBrightBits", "2", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_mapOverBrightBits, "Sets the number of overbright bits baked into all lightmaps and map data." );
	r_intensity = ri.Cvar_Get( "r_intensity", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_intensity, "1", "255", CV_FLOAT );
	ri.Cvar_SetDescription( r_intensity, "Global texture lighting scale." );
	r_singleShader = ri.Cvar_Get( "r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_singleShader, "Debugging tool that only uses the default shader for all rendering." );
	r_defaultImage = ri.Cvar_Get( "r_defaultImage", "", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_defaultImage, "Replace default (missing) image texture by either exact file or solid #rgb|#rrggbb background color." );

	r_simpleMipMaps = ri.Cvar_Get( "r_simpleMipMaps", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_simpleMipMaps, "Whether or not to use a simple mipmapping algorithm or a more correct one:\n 0: off (proper linear filter)\n 1: on (for slower machines)" );
	r_vertexLight = ri.Cvar_Get( "r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_vertexLight, "Set to 1 to use vertex light instead of lightmaps, collapse all multi-stage shaders into single-stage ones, might cause rendering artifacts." );

	r_picmip = ri.Cvar_Get( "r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_picmip, "0", "16", CV_INTEGER );
	ri.Cvar_SetDescription( r_picmip, "Set texture quality, lower is better." );

	r_nomip = ri.Cvar_Get( "r_nomip", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_nomip, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_nomip, "Apply picmip only on worldspawn textures." );

	r_neatsky = ri.Cvar_Get( "r_neatsky", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_neatsky, "Disables texture mipping for skies." );
	r_roundImagesDown = ri.Cvar_Get ("r_roundImagesDown", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_roundImagesDown, "When images are scaled, round images down instead of up." );
	r_colorMipLevels = ri.Cvar_Get ("r_colorMipLevels", "0", CVAR_LATCH );
	ri.Cvar_SetDescription( r_colorMipLevels, "Debugging tool to artificially color different mipmap levels so that they are more apparent." );
	r_detailTextures = ri.Cvar_Get( "r_detailtextures", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_detailTextures, "Enables usage of shader stages flagged as detail." );
	r_texturebits = ri.Cvar_Get( "r_texturebits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_texturebits, "Number of texture bits per texture." );

	r_mergeLightmaps = ri.Cvar_Get( "r_mergeLightmaps", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_mergeLightmaps, "Merge built-in small lightmaps into bigger lightmaps (atlases)." );
#if defined (USE_VULKAN) && defined (USE_VBO)
	r_vbo = ri.Cvar_Get( "r_vbo", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_vbo, "Use Vertex Buffer Objects to cache static map geometry, may improve FPS on modern GPUs, increases hunk memory usage by 15-30MB (map-dependent)." );
#endif
#if defined (USE_VULKAN) && defined (USE_VK_PBR)
	r_pbr = ri.Cvar_Get("r_pbr", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_pbr, "Enables Physically Based Rendering. \nRequires " S_COLOR_CYAN "\\r_fbo 1 \n" S_COLOR_GREEN "Advised " S_COLOR_CYAN "\\r_vbo 1 " S_COLOR_GREEN "for static world geometry " S_COLOR_WHITE "*optional" );

	r_glint = ri.Cvar_Get( "r_glint", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_glint, "Enables glint (sparkle) highlights on PBR materials. 0 = off, 1 = on." );
	r_glint_intensity = ri.Cvar_Get( "r_glint_intensity", "0.35", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_glint_intensity, "Glint brightness multiplier. Higher values increase sparkle strength." );
	r_glint_scale = ri.Cvar_Get( "r_glint_scale", "140.0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_glint_scale, "Glint noise scale. Higher values produce finer, denser sparkles." );

	r_baseNormalX	= ri.Cvar_Get("r_baseNormalX",		"1.0",	CVAR_ARCHIVE | CVAR_LATCH );
	r_baseNormalY	= ri.Cvar_Get("r_baseNormalY",		"1.0",	CVAR_ARCHIVE | CVAR_LATCH );
	r_baseParallax	= ri.Cvar_Get("r_baseParallax",		"0.05",	CVAR_ARCHIVE | CVAR_LATCH );
	r_baseSpecular	= ri.Cvar_Get( "r_baseSpecular",	"0.04",	CVAR_ARCHIVE | CVAR_LATCH );
#ifdef VK_CUBEMAP
	r_cubeMapping = ri.Cvar_Get( "r_cubeMapping", "0", CVAR_ARCHIVE | CVAR_LATCH );
#endif
#endif
#ifdef USE_VULKAN_RAY_TRACING
	r_raytracing = ri.Cvar_Get( "r_raytracing", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_raytracing, "Enables Vulkan ray tracing. Requires ray tracing capable GPU and " S_COLOR_CYAN "\\r_fbo 1" );
	cvar_t *r_rt_pathtracing = ri.Cvar_Get( "r_rt_pathtracing", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_rt_pathtracing, "Enable path tracing mode (replaces hybrid RT lighting with multi-bounce GI). Requires r_raytracing 1." );
	r_rt_samples = ri.Cvar_Get( "r_rt_samples", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_samples, "Number of ray tracing samples per pixel (for denoising)." );
	r_rt_maxDepth = ri.Cvar_Get( "r_rt_maxDepth", "2", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_maxDepth, "Maximum ray tracing recursion depth." );
	r_rt_debugMagenta = ri.Cvar_Get( "r_rt_debugMagenta", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_debugMagenta, "Debug mode: 0=normal, 1=magenta clear + gradient test (for diagnosing pixel corruption)." );
	r_rt_tlasUpdateMode = ri.Cvar_Get( "r_rt_tlasUpdateMode", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_tlasUpdateMode, "TLAS update mode: 0=always rebuild, 1=auto (use UPDATE when only transforms change), 2=force update mode." );
	r_rt_temporal = ri.Cvar_Get( "r_rt_temporal", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_temporal, "Enable temporal accumulation for ray tracing (reduces noise by blending with previous frame)." );
	r_rt_temporalAlpha = ri.Cvar_Get( "r_rt_temporalAlpha", "0.9", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_temporalAlpha, "Temporal accumulation blend factor (0.0-1.0, higher = more history, default 0.9)." );
	r_rt_blasCompaction = ri.Cvar_Get( "r_rt_blasCompaction", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_blasCompaction, "Enable BLAS compaction to reduce memory usage (0=disabled, 1=enabled)." );
	r_rt_blasReuse = ri.Cvar_Get( "r_rt_blasReuse", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_blasReuse, "Enable BLAS reuse based on geometry hash (0=disabled, 1=enabled)." );
	r_rt_denoise = ri.Cvar_Get( "r_rt_denoise", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_denoise, "Enable ray tracing denoising (0=disabled, 1=ReLAX)." );
	r_rt_denoiseMode = ri.Cvar_Get( "r_rt_denoiseMode", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_denoiseMode, "Denoising mode: 0=SVGF, 1=ReLAX." );
	r_rt_denoiseIterations = ri.Cvar_Get( "r_rt_denoiseIterations", "3", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_denoiseIterations, "Number of spatial filter iterations (default 3)." );
	r_rt_denoiseSpatialAlpha = ri.Cvar_Get( "r_rt_denoiseSpatialAlpha", "0.5", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_denoiseSpatialAlpha, "ReLAX spatial filter blend factor (0.0-1.0, higher = more aggressive filtering, default 0.5)." );
	r_rt_denoiseVarianceAlpha = ri.Cvar_Get( "r_rt_denoiseVarianceAlpha", "0.5", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_denoiseVarianceAlpha, "ReLAX variance blend factor (0.0-1.0, controls variance estimation, default 0.5)." );
	r_rt_outputScale = ri.Cvar_Get( "r_rt_outputScale", "1.0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_outputScale, "Ray tracing output resolution scale (0.25-2.0, lower = better performance, default 1.0 = full resolution)." );
	r_rt_shadowRays = ri.Cvar_Get( "r_rt_shadowRays", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_shadowRays, "Number of shadow rays per pixel for soft shadows (1-16, higher = softer shadows but slower, default 1)." );
	r_rt_adaptiveSampling = ri.Cvar_Get( "r_rt_adaptiveSampling", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_adaptiveSampling, "Enable adaptive sampling (more samples in noisy areas, 0=disabled, 1=enabled)." );
	r_rt_gi = ri.Cvar_Get( "r_rt_gi", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_gi, "Enable multi-bounce global illumination (0=disabled, 1=enabled)." );
	r_rt_giBounces = ri.Cvar_Get( "r_rt_giBounces", "2", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_giBounces, "Maximum number of GI bounces (0-8, higher = more realistic but slower, default 2)." );
	r_rt_giIntensity = ri.Cvar_Get( "r_rt_giIntensity", "1.0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_rt_giIntensity, "Global illumination intensity multiplier (0.0-2.0, default 1.0)." );
	
	// GIBS (Global Illumination Based on Surfels) CVars
	r_gibs = ri.Cvar_Get( "r_gibs", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_gibs, "Enable GIBS (Global Illumination Based on Surfels) - efficient surfel-based GI (0=disabled, 1=enabled)." );
	r_gibs_surfelRadius = ri.Cvar_Get( "r_gibs_surfelRadius", "0.1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_gibs_surfelRadius, "GIBS surfel radius in world units (default 0.1)." );
	r_gibs_maxSurfels = ri.Cvar_Get( "r_gibs_maxSurfels", "1048576", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_gibs_maxSurfels, "Maximum number of surfels (default 1048576 = 1M)." );
	r_gibs_updateRate = ri.Cvar_Get( "r_gibs_updateRate", "4", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_gibs_updateRate, "GIBS update rate - update every N frames (default 4, lower = more accurate but slower)." );
	r_gibs_intensity = ri.Cvar_Get( "r_gibs_intensity", "1.0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_gibs_intensity, "GIBS indirect lighting intensity multiplier (0.0-2.0, default 1.0)." );
	r_gibs_samples = ri.Cvar_Get( "r_gibs_samples", "16", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_gibs_samples, "Number of ray samples per surfel update (1-64, higher = more accurate but slower, default 16)." );
#endif
	// Mesh shaders (VK_EXT_mesh_shader)
	r_meshShaders = ri.Cvar_Get( "r_meshShaders", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_meshShaders, "Enable Vulkan mesh shaders (VK_EXT_mesh_shader). Requires a supported GPU/driver and vid_restart." );
	r_meshletSize = ri.Cvar_Get( "r_meshletSize", "128", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_meshletSize, "32", "256", CV_INTEGER );
	ri.Cvar_SetDescription( r_meshletSize, "Target meshlet size used when generating meshlets (32-256)." );
	// GPU-driven culling and instancing
	r_gpuCulling = ri.Cvar_Get( "r_gpuCulling", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_gpuCulling, "Enable GPU-driven frustum culling (0=disabled, 1=enabled)." );
	r_gpuInstancing = ri.Cvar_Get( "r_gpuInstancing", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_gpuInstancing, "Enable GPU-driven instancing (0=disabled, 1=enabled)." );
	r_cullDistance = ri.Cvar_Get( "r_cullDistance", "5000", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_cullDistance, "Maximum distance for GPU culling (world units, default 5000)." );
	r_gpuSceneGraph = ri.Cvar_Get( "r_gpuSceneGraph", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_gpuSceneGraph, "Enable GPU-resident scene graph buffer (0=CPU only, 1=GPU)." );
	r_gpuSceneDebug = ri.Cvar_Get( "r_gpuSceneDebug", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_gpuSceneDebug, "Debug/visualize GPU scene graph content." );
	r_gpuSkinning = ri.Cvar_Get( "r_gpuSkinning", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_gpuSkinning, "Enable GPU skinning path for crowds (0=off, 1=on)." );
	r_gpuRagdoll = ri.Cvar_Get( "r_gpuRagdoll", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_gpuRagdoll, "Enable GPU-updated ragdoll pose buffers (0=off, 1=on)." );
	r_procDressing = ri.Cvar_Get( "r_procDressing", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_procDressing, "Enable procedural dressing rules (0=disabled, 1=enabled)." );
	r_procDressingDensity = ri.Cvar_Get( "r_procDressingDensity", "1.0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_procDressingDensity, "Global density multiplier for procedural dressing (0..2)." );
	r_procDressingDebug = ri.Cvar_Get( "r_procDressingDebug", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_procDressingDebug, "Log procedural dressing instance counts each frame." );
	r_foliageWindStrength = ri.Cvar_Get( "r_foliageWindStrength", "0.35", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_foliageWindStrength, "Wind sway strength for procedural foliage instances." );
	r_foliageWindFrequency = ri.Cvar_Get( "r_foliageWindFrequency", "0.6", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_foliageWindFrequency, "Wind sway frequency (Hz) for procedural foliage instances." );
	r_frameTelemetry = ri.Cvar_Get( "r_frameTelemetry", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_frameTelemetry, "When enabled, logs per-second GPU timing stats for frame pacing." );
	// Material system
	r_materialSystem = ri.Cvar_Get( "r_materialSystem", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_materialSystem, "Enable material system with runtime parameters (0=disabled, 1=enabled)." );
	r_materialWetness = ri.Cvar_Get( "r_materialWetness", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_materialWetness, "Global material wetness override (0.0-1.0, 0=disabled)." );
	r_materialDamage = ri.Cvar_Get( "r_materialDamage", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_materialDamage, "Global material damage override (0.0-1.0, 0=disabled)." );
	r_materialMagic = ri.Cvar_Get( "r_materialMagic", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_materialMagic, "Global material magic glow override (0.0-1.0, 0=disabled)." );
	r_layeredMaterials = ri.Cvar_Get( "r_layeredMaterials", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_layeredMaterials, "Enable layered material flattening (0=off, 1=on)." );
	r_layeredMaterialMaxLayers = ri.Cvar_Get( "r_layeredMaterialMaxLayers", XSTRING( VK_MAX_LAYERS_PER_MATERIAL ), CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_layeredMaterialMaxLayers, "Clamp maximum contributing layers per material (1-8)." );
	r_layeredMaterialSimple = ri.Cvar_Get( "r_layeredMaterialSimple", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_layeredMaterialSimple, "Force simple flatten path (single layer only)." );
	r_layeredMaterialProfile = ri.Cvar_Get( "r_layeredMaterialProfile", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_layeredMaterialProfile, "Collect layered material metrics each frame (0=off,1=on)." );
	r_layeredMaterialsPilot = ri.Cvar_Get( "r_layeredMaterialsPilot", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_layeredMaterialsPilot, "Seed demo layered materials for hero assets on startup." );
	// Cell streaming
	r_cellStreaming = ri.Cvar_Get( "r_cellStreaming", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_cellStreaming, "Enable cell-based world streaming (0=disabled, 1=enabled)." );
	r_cellLoadRadius = ri.Cvar_Get( "r_cellLoadRadius", "2", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_cellLoadRadius, "Number of cells to load around player (default 2)." );
	r_cellUnloadDistance = ri.Cvar_Get( "r_cellUnloadDistance", "4", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_cellUnloadDistance, "Distance in cells before unloading (default 4)." );
	// Atmosphere system
	r_atmosphere = ri.Cvar_Get( "r_atmosphere", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_atmosphere, "Enable atmosphere/mood system (0=disabled, 1=enabled)." );
	r_atmospherePreset = ri.Cvar_Get( "r_atmospherePreset", "3", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_atmospherePreset, "Atmosphere preset: 0=Brutal, 1=Mysterious, 2=Combat, 3=Calm." );
	r_fogDensity = ri.Cvar_Get( "r_fogDensity", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_fogDensity, "Fog density override (0=use preset, >0=override)." );
	r_bloomIntensity = ri.Cvar_Get( "r_bloomIntensity", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_bloomIntensity, "Bloom intensity override (0=use preset, >0=override)." );
	r_dlss = ri.Cvar_Get( "r_dlss", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_dlss, "Enable NVIDIA DLSS Super Resolution (requires NVIDIA RTX GPU and DLSS SDK)." );
	r_dlss_quality = ri.Cvar_Get( "r_dlss_quality", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_dlss_quality, "DLSS quality mode: 0=Performance, 1=Balanced, 2=Quality, 3=Ultra Quality." );
	r_dlss_sharpening = ri.Cvar_Get( "r_dlss_sharpening", "0.0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_dlss_sharpening, "DLSS sharpening amount (0.0-1.0, default 0.0)." );

	// Vulkan-specific debug helpers
	r_vk_debug2D = ri.Cvar_Get( "r_vk_debug2D", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_vk_debug2D, "When non-zero, logs per-frame 2D/UI quad counts and tess usage for debugging menus/console." );

	r_vk_debugClearColor = ri.Cvar_Get( "r_vk_debugClearColor", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_vk_debugClearColor, "When non-zero, clears the final swapchain image to a solid color each frame to reveal uncleared regions." );

	r_vk_debugUiOnly = ri.Cvar_Get( "r_vk_debugUiOnly", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_vk_debugUiOnly, "When non-zero, skips 3D world rendering and draws only UI/console/HUD overlays." );

	r_vk_disableScreenMap = ri.Cvar_Get( "r_vk_disableScreenMap", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_vk_disableScreenMap, "Set to 1 to hard-disable the Vulkan screenMap/$currentRender path (forces UI to use blackImage). Useful for isolating device-lost/menu corruption issues." );
	r_mapGreyScale = ri.Cvar_Get( "r_mapGreyScale", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_mapGreyScale, "-1", "1", CV_FLOAT );
	ri.Cvar_SetDescription(r_mapGreyScale, "Desaturate world map textures only, works independently from \\r_greyscale, negative values only desaturate lightmaps.");

	r_subdivisions = ri.Cvar_Get( "r_subdivisions", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription(r_subdivisions, "Distance to subdivide bezier curved surfaces. Higher values mean less subdivision and less geometric complexity.");

	r_maxpolys = ri.Cvar_Get( "r_maxpolys", XSTRING( MAX_POLYS ), CVAR_LATCH );
	ri.Cvar_SetDescription( r_maxpolys, "Maximum number of polygons to draw in a scene." );
	r_maxpolyverts = ri.Cvar_Get( "r_maxpolyverts", XSTRING( MAX_POLYVERTS ), CVAR_LATCH );
	ri.Cvar_SetDescription( r_maxpolyverts, "Maximum number of polygon vertices to draw in a scene." );

	//
	// archived variables that can change at any time
	//
	r_lodCurveError = ri.Cvar_Get( "r_lodCurveError", "250", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_lodCurveError, "-1", "8192", CV_FLOAT );
	ri.Cvar_SetDescription( r_lodCurveError, "Level of detail error on curved surface grids. Higher values result in better quality at a distance." );
	r_lodbias = ri.Cvar_Get( "r_lodbias", "-2", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_lodbias, "Sets the level of detail of in-game models:\n -2: Ultra (further delays LOD transition in the distance)\n -1: Very High (delays LOD transition in the distance)\n 0: High\n 1: Medium\n 2: Low" );
	r_flares = ri.Cvar_Get ("r_flares", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_flares, "Enables corona effects on light sources." );
	r_znear = ri.Cvar_Get( "r_znear", "4", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_znear, "0.001", "200", CV_FLOAT );
	ri.Cvar_SetDescription( r_znear, "Viewport distance from view origin (how close objects can be to the player before they're clipped out of the scene)." );
	r_zproj = ri.Cvar_Get( "r_zproj", "64", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_zproj, "Projected viewport frustum." );
	r_stereoSeparation = ri.Cvar_Get( "r_stereoSeparation", "64", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_stereoSeparation, "Control eye separation. Resulting separation is \\r_zproj divided by this value in standard units." );
	r_ignoreGLErrors = ri.Cvar_Get( "r_ignoreGLErrors", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_ignoreGLErrors, "Ignore OpenGL errors." );
	r_teleporterFlash = ri.Cvar_Get( "r_teleporterFlash", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_teleporterFlash, "Show a white screen instead of a black screen when being teleported in hyperspace." );
	r_fastsky = ri.Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_fastsky, "Draw flat colored skies." );
	r_drawSun = ri.Cvar_Get( "r_drawSun", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_drawSun, "Draw sun shader in skies." );
	r_dynamiclight = ri.Cvar_Get( "r_dynamiclight", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_dynamiclight, "Enables dynamic lighting." );
#ifdef USE_PMLIGHT
#if arm32 || arm64 // RPi4 Vulkan driver have very poor GLSL shaders performance...
	r_dlightMode = ri.Cvar_Get( "r_dlightMode", "0", CVAR_ARCHIVE );
#else
	r_dlightMode = ri.Cvar_Get( "r_dlightMode", "1", CVAR_ARCHIVE );
#endif
	ri.Cvar_CheckRange( r_dlightMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_dlightMode, "Dynamic light mode:\n 0: VQ3 'fake' dynamic lights\n 1: High-quality per-pixel dynamic lights, slightly faster than VQ3's on modern hardware\n 2: Same as 1 but applies to all MD3 models too" );
	r_dlightScale = ri.Cvar_Get( "r_dlightScale", "0.5", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dlightScale, "0.1", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_dlightScale, "Scales dynamic light radius." );
	r_dlightIntensity = ri.Cvar_Get( "r_dlightIntensity", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dlightIntensity, "0.1", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_dlightIntensity, "Adjusts dynamic light intensity but not radius." );
#endif // USE_PMLIGHT

	r_dlightSaturation = ri.Cvar_Get( "r_dlightSaturation", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dlightSaturation, "0", "1", CV_FLOAT );

	r_dlightBacks = ri.Cvar_Get( "r_dlightBacks", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_dlightBacks, "Whether or not dynamic lights should light up back-face culled geometry, affects only VQ3 dynamic lights." );
	r_finish = ri.Cvar_Get( "r_finish", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_finish, "Force a glFinish call after rendering a scene." );
	r_textureMode = ri.Cvar_Get( "r_textureMode", "GL_LINEAR_MIPMAP_NEAREST", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_textureMode, "Texture interpolation mode:\n GL_NEAREST: Nearest neighbor interpolation and will therefore appear similar to Quake II except with the added colored lighting\n GL_LINEAR: Linear interpolation and will appear to blend in objects that are closer than the resolution that the textures are set as\n GL_NEAREST_MIPMAP_NEAREST: Nearest neighbor interpolation with mipmapping for bilinear hardware, mipmapping will blend objects that are farther away than the resolution that they are set as\n GL_LINEAR_MIPMAP_NEAREST: Linear interpolation with mipmapping for bilinear hardware\n GL_NEAREST_MIPMAP_LINEAR: Nearest neighbor interpolation with mipmapping for trilinear hardware\n GL_LINEAR_MIPMAP_LINEAR: Linear interpolation with mipmapping for trilinear hardware" );
	ri.Cvar_SetGroup( r_textureMode, CVG_RENDERER );
	r_gamma = ri.Cvar_Get( "r_gamma", "1", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_gamma, "0.5", "3", CV_FLOAT );
	ri.Cvar_SetDescription( r_gamma, "Gamma correction factor." );
	ri.Cvar_SetGroup( r_gamma, CVG_RENDERER );
	r_facePlaneCull = ri.Cvar_Get ("r_facePlaneCull", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_facePlaneCull, "Enables culling of planar surfaces with back side test." );

	r_railWidth = ri.Cvar_Get( "r_railWidth", "16", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_railWidth, "Radius of railgun trails." );
	r_railCoreWidth = ri.Cvar_Get( "r_railCoreWidth", "6", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_railCoreWidth, "Size of railgun trail rings when enabled in game code (normally \\cg_oldRail 0)." );
	r_railSegmentLength = ri.Cvar_Get( "r_railSegmentLength", "32", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_railSegmentLength, "Length of segments in railgun trails." );

	r_ambientScale = ri.Cvar_Get( "r_ambientScale", "0.6", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_ambientScale, "Light grid ambient light scaling on entity models." );
	r_directedScale = ri.Cvar_Get( "r_directedScale", "1", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_directedScale, "Light grid direct light scaling on entity models." );

	//r_anaglyphMode = ri.Cvar_Get( "r_anaglyphMode", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	//ri.Cvar_SetDescription( r_anaglyphMode, "Enable rendering of anaglyph images. Valid options for 3D glasses types:\n 0: Disabled\n 1: Red-cyan\n 2: Red-blue\n 3: Red-green\n 4: Green-magenta" );

	r_greyscale = ri.Cvar_Get( "r_greyscale", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_greyscale, "-1", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_greyscale, "Desaturate rendered frame, requires \\r_fbo 1." );
	ri.Cvar_SetGroup( r_greyscale, CVG_RENDERER );

	r_dither = ri.Cvar_Get( "r_dither", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dither, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription(r_dither, "Set dithering mode:\n 0 - disabled\n 1 - ordered\nRequires " S_COLOR_CYAN "\\r_fbo 1." );
	ri.Cvar_SetGroup( r_dither, CVG_RENDERER );

	r_presentBits = ri.Cvar_Get( "r_presentBits", "24", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_presentBits, "16", "30", CV_INTEGER );
	ri.Cvar_SetDescription( r_presentBits, "Select color bits used for presentation surfaces\nRequires " S_COLOR_CYAN "\\r_fbo 1." );

	//
	// temporary variables that can change at any time
	//
	r_showImages = ri.Cvar_Get( "r_showImages", "0", CVAR_TEMP );
	ri.Cvar_SetDescription( r_showImages, "Draw all images currently loaded into memory:\n 0: Disabled\n 1: Show images set to uniform size\n 2: Show images with scaled relative to largest image" );

	r_debugLight = ri.Cvar_Get( "r_debuglight", "0", CVAR_TEMP );
	ri.Cvar_SetDescription( r_debugLight, "Debugging tool to print ambient and directed lighting information." );
	r_debugSort = ri.Cvar_Get( "r_debugSort", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_debugSort, "Debugging tool to filter out shaders with depth sorting order values higher than the set value." );
	r_printShaders = ri.Cvar_Get( "r_printShaders", "0", 0 );
	ri.Cvar_SetDescription( r_printShaders, "Debugging tool to print on console of the number of shaders used." );
	r_saveFontData = ri.Cvar_Get( "r_saveFontData", "0", 0 );
	
	// Font rendering quality CVars
	r_fontAtlasSize = ri.Cvar_Get( "r_fontAtlasSize", "256", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_fontAtlasSize, "256", "1024", CV_INTEGER );
	ri.Cvar_SetDescription( r_fontAtlasSize, "Font texture atlas size in pixels. Larger sizes allow more glyphs per texture but use more memory. Valid values: 256, 512, 1024" );
	
	r_fontDPI = ri.Cvar_Get( "r_fontDPI", "96", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_fontDPI, "72", "300", CV_FLOAT );
	ri.Cvar_SetDescription( r_fontDPI, "DPI (dots per inch) for font rendering. Higher values produce sharper text but larger textures. Typical values: 72 (standard), 96 (Windows), 144 (retina)" );
	
	r_fontHinting = ri.Cvar_Get( "r_fontHinting", "2", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_fontHinting, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_fontHinting, "Font hinting mode: 0 = None, 1 = Light, 2 = Normal (default), 3 = Strong. Hinting improves text clarity at small sizes." );
	
	r_fontAntialiasing = ri.Cvar_Get( "r_fontAntialiasing", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_fontAntialiasing, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_fontAntialiasing, "Enable font antialiasing: 0 = Disabled (monochrome), 1 = Enabled (smooth)" );
	
	r_fontLCDFilter = ri.Cvar_Get( "r_fontLCDFilter", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_fontLCDFilter, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_fontLCDFilter, "Enable LCD subpixel filtering for improved text rendering on LCD displays. Requires antialiasing enabled." );
	
	r_fontKerning = ri.Cvar_Get( "r_fontKerning", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_fontKerning, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_fontKerning, "Enable font kerning for improved text spacing. Kerning adjusts spacing between character pairs (e.g., 'AV', 'To') for better readability." );

	r_fontSDF = ri.Cvar_Get( "r_fontSDF", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_fontSDF, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_fontSDF, "Use signed distance field (SDF) font atlases when available. Requires atlas rebuild." );

	r_fontSDFSpread = ri.Cvar_Get( "r_fontSDFSpread", "6", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_fontSDFSpread, "4", "16", CV_INTEGER );
	ri.Cvar_SetDescription( r_fontSDFSpread, "SDF spread (pixels) for SDF font baking." );

	r_fontSDFSmooth = ri.Cvar_Get( "r_fontSDFSmooth", "0.25", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_fontSDFSmooth, "0.05", "0.5", CV_FLOAT );
	ri.Cvar_SetDescription( r_fontSDFSmooth, "SDF smoothstep width (normalized distance). Higher = softer edges." );

	r_nocurves = ri.Cvar_Get ("r_nocurves", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_nocurves, "Set to 1 to disable drawing world bezier curves. Set to 0 to enable." );
	r_drawworld = ri.Cvar_Get ("r_drawworld", "1", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_drawworld, "Set to 0 to disable drawing the world. Set to 1 to enable." );
	r_lightmap = ri.Cvar_Get ("r_lightmap", "0", 0 );
	ri.Cvar_SetDescription( r_lightmap, "Show only lightmaps on all world surfaces." );
	r_portalOnly = ri.Cvar_Get ("r_portalOnly", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_portalOnly, "Set to 1 to render only first portal view if it is present on the scene." );

	r_flareSize = ri.Cvar_Get( "r_flareSize", "40", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_flareSize, "Radius of light flares. Requires \\r_flares 1." );
	r_flareFade = ri.Cvar_Get( "r_flareFade", "10", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_flareFade, "Distance to fade out light flares. Requires \\r_flares 1." );
	r_flareCoeff = ri.Cvar_Get( "r_flareCoeff", "150", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_flareCoeff, "0.1", NULL, CV_FLOAT );
	ri.Cvar_SetDescription( r_flareCoeff, "Coefficient for the light flare intensity falloff function. Requires \\r_flares 1." );

	r_skipBackEnd = ri.Cvar_Get ("r_skipBackEnd", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_skipBackEnd, "Skips loading rendering backend." );

	r_lodscale = ri.Cvar_Get( "r_lodscale", "5", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_lodscale, "Set scale for level of detail adjustment." );
	r_norefresh = ri.Cvar_Get ("r_norefresh", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_norefresh, "Bypasses refreshing of the rendered scene." );
	r_drawentities = ri.Cvar_Get ("r_drawentities", "1", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_drawentities, "Draw all world entities." );
	r_nocull = ri.Cvar_Get ("r_nocull", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_nocull, "Draw all culled objects." );
	r_novis = ri.Cvar_Get ("r_novis", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_novis, "Disables usage of PVS." );
	r_showcluster = ri.Cvar_Get ("r_showcluster", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_showcluster, "Shows current cluster index." );
	r_speeds = ri.Cvar_Get ("r_speeds", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_speeds, "Prints out various debugging stats from PVS:\n 0: Disabled\n 1: Backend BSP\n 2: Frontend grid culling\n 3: Current view cluster index\n 4: Dynamic lighting\n 5: zFar clipping\n 6: Flares" );
	r_debugSurface = ri.Cvar_Get ("r_debugSurface", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_debugSurface, "Backend visual debugging tool for bezier mesh surfaces." );
	r_nobind = ri.Cvar_Get ("r_nobind", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_nobind, "Backend debugging tool: Disables texture binding." );
	r_showtris = ri.Cvar_Get ("r_showtris", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_showtris, "Debugging tool: Wireframe rendering of polygon triangles in the world." );
	r_wireframe = ri.Cvar_Get ("r_wireframe", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_wireframe, "Wireframe rendering mode (alias for r_showtris)." );
	r_shownormals = ri.Cvar_Get( "r_shownormals", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_shownormals, "Debugging tool: Show wireframe surface normals." );
	r_clear = ri.Cvar_Get( "r_clear", "0", 0 );
	ri.Cvar_SetDescription( r_clear, "Forces screen buffer clearing every frame, removing any hall of mirrors effect in void.\n Use \\r_clearColor to set color." );
	r_offsetFactor = ri.Cvar_Get( "r_offsetFactor", "-2", CVAR_CHEAT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_offsetFactor, "Offset factor for shaders with polygonOffset stages." );
	r_offsetUnits = ri.Cvar_Get( "r_offsetunits", "-1", CVAR_CHEAT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_offsetUnits, "Offset units for shaders with polygonOffset stages." );
	r_drawBuffer = ri.Cvar_Get( "r_drawBuffer", "GL_BACK", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_drawBuffer, "Sets which frame buffer to draw into." );
	r_lockpvs = ri.Cvar_Get ("r_lockpvs", "0", CVAR_CHEAT);
	ri.Cvar_SetDescription( r_lockpvs, "Debugging tool: Locks to current potentially visible set. Useful for testing vis-culling in maps." );
	r_noportals = ri.Cvar_Get( "r_noportals", "0", 0 );
	ri.Cvar_SetDescription(r_noportals, "Disables in-game portals, valid values: 0: Portals enabled\n 1: Portals disabled\n 2: Portals and mirrors disabled" );
	r_shadows = ri.Cvar_Get( "cg_shadows", "1", 0 );

	r_marksOnTriangleMeshes = ri.Cvar_Get("r_marksOnTriangleMeshes", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_marksOnTriangleMeshes, "Enables impact marks on triangle mesh surfaces (ie: MD3 models.) Requires impact marks to be enabled in the game code." );

	r_aviMotionJpegQuality = ri.Cvar_Get( "r_aviMotionJpegQuality", "90", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_aviMotionJpegQuality, "Controls quality of Jpeg video capture when \\cl_aviMotionJpeg 1." );
	r_screenshotJpegQuality = ri.Cvar_Get( "r_screenshotJpegQuality", "90", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_screenshotJpegQuality, "Controls quality of Jpeg screenshots when using screenshotJpeg." );

	r_bloom_threshold = ri.Cvar_Get( "r_bloom_threshold", "0.6", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_bloom_threshold, "Color level to extract to bloom texture, default is 0.6." );
	ri.Cvar_SetGroup( r_bloom_threshold, CVG_RENDERER );

	r_bloom_threshold_mode = ri.Cvar_Get( "r_bloom_threshold_mode", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_bloom_threshold_mode, "Color extraction mode:\n 0: (r|g|b) >= threshold\n 1: (r + g + b ) / 3 >= threshold\n 2: luma(r, g, b) >= threshold" );
	ri.Cvar_SetGroup( r_bloom_threshold_mode, CVG_RENDERER );

	r_bloom_intensity = ri.Cvar_Get( "r_bloom_intensity", "0.5", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_bloom_intensity, "Final bloom blend factor, default is 0.5." );
	ri.Cvar_SetGroup( r_bloom_intensity, CVG_RENDERER );

	r_bloom_modulate = ri.Cvar_Get( "r_bloom_modulate", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_bloom_modulate, "Modulate extracted color:\n 0: off (color = color, i.e. no changes)\n 1: by itself (color = color * color)\n 2: by intensity (color = color * luma(color))" );
	ri.Cvar_SetGroup( r_bloom_modulate, CVG_RENDERER );

	if ( glConfig.vidWidth )
		return;

	//
	// latched and archived variables that can only change over a vid_restart
	//
	r_allowExtensions = ri.Cvar_Get( "r_allowExtensions", "1", CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_DEVELOPER );
	ri.Cvar_SetDescription( r_allowExtensions, "Use all of the OpenGL extensions your card is capable of." );
	r_ext_compressed_textures = ri.Cvar_Get( "r_ext_compressed_textures", "0", CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_DEVELOPER );
	ri.Cvar_SetDescription( r_ext_compressed_textures, "Enables texture compression." );
	r_ext_multitexture = ri.Cvar_Get( "r_ext_multitexture", "1", CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_DEVELOPER );
	ri.Cvar_SetDescription( r_ext_multitexture, "Enables hardware multi-texturing (0: off, 1: on)." );
	r_ext_compiled_vertex_array = ri.Cvar_Get( "r_ext_compiled_vertex_array", "1", CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_DEVELOPER );
	ri.Cvar_SetDescription( r_ext_compiled_vertex_array, "Enables hardware-compiled vertex array rendering method." );
	r_ext_texture_env_add = ri.Cvar_Get( "r_ext_texture_env_add", "1", CVAR_ARCHIVE_ND | CVAR_LATCH | CVAR_DEVELOPER );
	ri.Cvar_SetDescription( r_ext_texture_env_add, "Enables additive blending in multitexturing. Requires \\r_ext_multitexture 1." );

	r_ext_texture_filter_anisotropic = ri.Cvar_Get( "r_ext_texture_filter_anisotropic",	"1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_texture_filter_anisotropic, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_texture_filter_anisotropic, "Allow anisotropic filtering." );

	r_ext_max_anisotropy = ri.Cvar_Get( "r_ext_max_anisotropy", "8", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_max_anisotropy, "1", NULL, CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_max_anisotropy, "Sets maximum anisotropic level for your graphics driver. Requires \\r_ext_texture_filter_anisotropic." );

	//r_stencilbits = ri.Cvar_Get( "r_stencilbits", "8", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_ignorehwgamma = ri.Cvar_Get( "r_ignorehwgamma", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ignorehwgamma, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ignorehwgamma, "Overrides hardware gamma capabilities." );

	r_showsky = ri.Cvar_Get( "r_showsky", "0", CVAR_LATCH );
	ri.Cvar_SetDescription( r_showsky, "Forces sky in front of all surfaces." );
#ifdef USE_VULKAN
	r_device = ri.Cvar_Get( "r_device", "-1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_device, "-2", NULL, CV_INTEGER );
	ri.Cvar_SetDescription( r_device, "Select physical device to render:\n" \
		" 0+ - use explicit device index\n" \
		" -1 - first discrete GPU\n" \
		" -2 - first integrated GPU" );
	r_device->modified = qfalse;
	r_vk_icd = ri.Cvar_Get( "r_vk_icd", "", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_vk_icd, "Optional override for VK_ICD_FILENAMES. Set to a driver manifest path to force a specific ICD." );

	r_fbo = ri.Cvar_Get( "r_fbo", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_fbo, "Use framebuffer objects, enables gamma correction in windowed mode and allows arbitrary video size and screenshot/video capture.\n Required for bloom, HDR rendering, anti-aliasing and greyscale effects." );
	r_hdr = ri.Cvar_Get( "r_hdr", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription(r_hdr, "Enables high dynamic range frame buffer texture format. Requires \\r_fbo 1.\n -1: 4-bit, for testing purposes, heavy color banding, might not work on all systems\n  0: 8 bit, default, moderate color banding with multi-stage shaders\n  1: 16 bit, enhanced blending precision, no color banding, might decrease performance on AMD / Intel GPUs\n" );
	r_bloom = ri.Cvar_Get( "r_bloom", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_bloom, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription(r_bloom, "Enables bloom post-processing effect. Requires \\r_fbo 1.");
	r_styleTransfer = ri.Cvar_Get( "r_styleTransfer", "0", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_styleTransfer, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_styleTransfer, "Experimental Vulkan-only style-transfer post-process. 0=off, 1=on. Requires \\r_fbo 1. Currently a no-op placeholder for integrating neural models." );
	ri.Cvar_SetGroup( r_styleTransfer, CVG_RENDERER );
	r_styleStrength = ri.Cvar_Get( "r_styleStrength", "0.35", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_styleStrength, "0.0", "2.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_styleStrength, "Style intensity for the experimental Vulkan style-transfer pass (0 = no effect, higher = stronger stylization)." );
	ri.Cvar_SetGroup( r_styleStrength, CVG_RENDERER );
	r_styleLevels = ri.Cvar_Get( "r_styleLevels", "8", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_styleLevels, "2", "32", CV_INTEGER );
	ri.Cvar_SetDescription( r_styleLevels, "Posterization levels for style-transfer pass (2-32)." );
	ri.Cvar_SetGroup( r_styleLevels, CVG_RENDERER );
	r_styleEdge = ri.Cvar_Get( "r_styleEdge", "2.0", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_styleEdge, "0.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_styleEdge, "Edge contrast scale for style-transfer pass." );
	ri.Cvar_SetGroup( r_styleEdge, CVG_RENDERER );
	r_postprocess_compute = ri.Cvar_Get( "r_postprocess_compute", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_postprocess_compute, "Use compute shader post-processing (gamma/tonemap) when available." );
	r_postprocess_workgroup = ri.Cvar_Get( "r_postprocess_workgroup", "8", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_postprocess_workgroup, "Workgroup size (X/Y) for compute post-process dispatch. 8 matches the compiled shaders." );
	r_tonemapMode = ri.Cvar_Get( "r_tonemapMode", "1", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_tonemapMode, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_tonemapMode, "Tonemap operator for compute path: 0=Reinhard, 1=ACES, 2=Filmic, 3=Uncharted2." );
	r_tonemapExposure = ri.Cvar_Get( "r_tonemapExposure", "1.0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_tonemapExposure, "Exposure multiplier used by compute tonemap (1.0 = neutral)." );

	r_ext_multisample = ri.Cvar_Get( "r_ext_multisample", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_multisample, "0", "64", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_multisample, "For anti-aliasing geometry edges, valid values: 0|2|4|6|8. Requires \\r_fbo 1." );

	r_ext_supersample = ri.Cvar_Get( "r_ext_supersample", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_supersample, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_supersample, "Super-sample anti-aliasing, requires \\r_fbo 1." );
#if 0
	r_ext_alpha_to_coverage = ri.Cvar_Get( "r_ext_alpha_to_coverage", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_alpha_to_coverage, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_alpha_to_coverage, "Enables alpha-to-coverage multisampling, requires \\r_fbo 1." );
#endif

	r_renderWidth = ri.Cvar_Get( "r_renderWidth", "800", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_renderWidth, "96", NULL, CV_INTEGER );
	ri.Cvar_SetDescription( r_renderWidth, "Video width to render to when \\r_renderScale > 0." );
	r_renderHeight = ri.Cvar_Get( "r_renderHeight", "600", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_renderHeight, "72", NULL, CV_INTEGER );
	ri.Cvar_SetDescription( r_renderHeight, "Video height to render to when \\r_renderScale > 0." );

	r_renderScale = ri.Cvar_Get( "r_renderScale", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_renderScale, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_renderScale, "Scaling mode to be used with custom render resolution:\n"
		" 0 - disabled\n"
		" 1 - nearest filtering, stretch to full size\n"
		" 2 - nearest filtering, preserve aspect ratio (black bars on sides)\n"
		" 3 - linear filtering, stretch to full size\n"
		" 4 - linear filtering, preserve aspect ratio (black bars on sides)\n" );
#endif // USE_VULKAN
}

#define EPSILON 1e-6f

/*
===============
R_Init
===============
*/
void R_Init( void ) {
#ifndef USE_VULKAN
	int	err;
#endif
	int i;
	byte *ptr;

	ri.Printf( PRINT_ALL, "----- R_Init -----\n" );

	// clear all our internal state
	Com_Memset( &tr, 0, sizeof( tr ) );
	Com_Memset( &backEnd, 0, sizeof( backEnd ) );
	Com_Memset( &tess, 0, sizeof( tess ) );
	Com_Memset( &glState, 0, sizeof( glState ) );

	if ( sizeof( glconfig_t ) != 11332 )
		ri.Error( ERR_FATAL, "Mod ABI incompatible: sizeof(glconfig_t) == %u != 11332", (unsigned int) sizeof( glconfig_t ) );

	if ( (intptr_t)tess.xyz & 15 ) {
		ri.Printf( PRINT_WARNING, "tess.xyz not 16 byte aligned\n" );
	}
	Com_Memset( tess.constantColor255, 255, sizeof( tess.constantColor255 ) );

	//
	// init function tables
	//
	for ( i = 0; i < FUNCTABLE_SIZE; i++ ) {
		tr.sinTable[i] = sin( DEG2RAD( i * 360.0f / FUNCTABLE_SIZE ) + 0.0001f );
		tr.squareTable[i] = (i < FUNCTABLE_SIZE / 2) ? 1.0f : -1.0f;
		if ( i == 0 ) {
			tr.sawToothTable[i] = EPSILON;
		} else {
			tr.sawToothTable[i] = (float)i / FUNCTABLE_SIZE;
		}
		tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];
		if ( i < FUNCTABLE_SIZE / 2 ) {
			if ( i < FUNCTABLE_SIZE / 4 ) {
				if ( i == 0 ) {
					tr.triangleTable[i] = EPSILON;
				} else {
					tr.triangleTable[i] = (float)i / (FUNCTABLE_SIZE / 4);
				}
			} else {
				tr.triangleTable[i] = 1.0f - tr.triangleTable[i - FUNCTABLE_SIZE / 4];
			}
		} else {
			tr.triangleTable[i] = -tr.triangleTable[i - FUNCTABLE_SIZE / 2];
		}
	}

	R_InitFogTable();

	R_NoiseInit();

	R_Register();

	max_polys = r_maxpolys->integer;
	max_polyverts = r_maxpolyverts->integer;

	ptr = ri.Hunk_Alloc( sizeof( *backEndData ) + sizeof(srfPoly_t) * max_polys + sizeof(polyVert_t) * max_polyverts, h_low);
	backEndData = (backEndData_t *) ptr;
	backEndData->polys = (srfPoly_t *) ((char *) ptr + sizeof( *backEndData ));
	backEndData->polyVerts = (polyVert_t *) ((char *) ptr + sizeof( *backEndData ) + sizeof(srfPoly_t) * max_polys);

	R_InitNextFrame();

	InitOpenGL();

	R_InitImages();

	VarInfo();

#ifdef USE_VULKAN
	vk_create_pipelines();
#ifdef VK_PBR_BRDFLUT
	vk_create_brfdlut();
#endif
#endif

	R_InitShaders();
	
#ifdef USE_VULKAN
	// Update font textures to use nearest filtering (fixes blurry fonts)
	extern void vk_update_font_textures( void );
	vk_update_font_textures();
#endif

	R_InitSkins();

	R_ModelInit();

	R_InitFreeType();

#ifndef USE_VULKAN
	err = qglGetError();
	if ( err != GL_NO_ERROR )
		ri.Printf( PRINT_WARNING, "glGetError() = 0x%x\n", err );
#endif

	// Install the Vulkan backend interface for graph/driver-agnostic callers.
	RB_SetBackendInterface( RB_VK_GetBackendInterface() );

	ri.Printf( PRINT_ALL, "----- finished R_Init -----\n" );
}


/*
===============
RE_Shutdown
===============
*/
static void RE_Shutdown( refShutdownCode_t code ) {
#ifdef USE_VULKAN
	//if ( code == REF_KEEP_CONTEXT ) {
	//	if ( ( ri.Milliseconds() - gls.initTime ) > 48 * 3600 * 1000 ) {
	//		code = REF_KEEP_WINDOW; // destroy context
	//	}
	//}
#endif
	ri.Printf( PRINT_ALL, "RE_Shutdown( %i )\n", code );

	ri.Cmd_RemoveCommand( "modellist" );
	ri.Cmd_RemoveCommand( "screenshotBMP" );
	ri.Cmd_RemoveCommand( "screenshotJPEG" );
	ri.Cmd_RemoveCommand( "screenshot" );
	ri.Cmd_RemoveCommand( "imagelist" );
	ri.Cmd_RemoveCommand( "shaderlist" );
	ri.Cmd_RemoveCommand( "skinlist" );
	ri.Cmd_RemoveCommand( "gfxinfo" );
	ri.Cmd_RemoveCommand( "shaderstate" );
#ifdef USE_VULKAN
	ri.Cmd_RemoveCommand( "vkinfo" );
#endif

	//if ( tr.registered ) {
		//R_IssuePendingRenderCommands();
		R_DeleteTextures();
	//}

#ifdef USE_VULKAN
	vk_release_resources();
#endif

	R_DoneFreeType();

#ifdef USE_VULKAN
	if ( r_device->modified ) {
		code = REF_UNLOAD_DLL;
	}
#endif

	// shut down platform specific OpenGL/Vulkan stuff
	if ( code != REF_KEEP_CONTEXT ) {
#ifdef USE_VULKAN
		vk_shutdown( code );

		Com_Memset( &glState, 0, sizeof( glState ) );

		if ( code != REF_KEEP_WINDOW ) {
			ri.VKimp_Shutdown( code == REF_UNLOAD_DLL ? qtrue : qfalse );
			Com_Memset( &glConfig, 0, sizeof( glConfig ) );
		}
#else
		R_ClearSymTables();
		Com_Memset( &glState, 0, sizeof( glState ) );

		if ( code != REF_KEEP_WINDOW ) {
			ri.GLimp_Shutdown( code == REF_UNLOAD_DLL ? qtrue : qfalse );
			Com_Memset( &glConfig, 0, sizeof( glConfig ) );
		}
#endif
	}

	ri.FreeAll();

	tr.registered = qfalse;
	tr.inited = qfalse;

	// Restore null backend interface for safety.
	RB_ResetBackendInterface();
}


/*
=============
RE_EndRegistration

Touch all images to make sure they are resident
=============
*/
static void RE_EndRegistration( void ) {
#ifdef USE_VULKAN
	vk_wait_idle();
	// command buffer is not in recording state at this stage
	// so we can't issue RB_ShowImages() there
#else
	R_IssuePendingRenderCommands();
	if ( !ri.Sys_LowPhysicalMemory() ) {
		RB_ShowImages();
	}
#endif
}


/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI
@@@@@@@@@@@@@@@@@@@@@
*/
#ifdef USE_RENDERER_DLOPEN
Q_EXPORT refexport_t* QDECL GetRefAPI ( int apiVersion, refimport_t *rimp );
#endif
#ifdef USE_RENDERER_DLOPEN
Q_EXPORT refexport_t* QDECL GetRefAPI ( int apiVersion, refimport_t *rimp ) {
#else
refexport_t *GetRefAPI ( int apiVersion, refimport_t *rimp ) {
#endif

	static refexport_t	re;

	ri = *rimp;

	Com_Memset( &re, 0, sizeof( re ) );

	if ( apiVersion != REF_API_VERSION ) {
		ri.Printf(PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n",
			REF_API_VERSION, apiVersion );
		return NULL;
	}

	// the RE_ functions are Renderer Entry points

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
	re.LightForPoint = R_LightForPoint;
	re.AddLightToScene = RE_AddLightToScene;
	re.AddAdditiveLightToScene = RE_AddAdditiveLightToScene;
	re.AddLinearLightToScene = RE_AddLinearLightToScene;

	re.RenderScene = RE_RenderScene;

	re.SetColor = RE_SetColor;
	re.DrawStretchPic = RE_StretchPic;
	re.DrawStretchRaw = RE_StretchRaw;
	re.UploadCinematic = RE_UploadCinematic;

	re.RegisterFont = RE_RegisterFont;
	re.RemapShader = RE_RemapShader;
	re.GetEntityToken = RE_GetEntityToken;
	re.inPVS = R_inPVS;

	re.TakeVideoFrame = RE_TakeVideoFrame;
	re.SetColorMappings = R_SetColorMappings;

	re.ThrottleBackend = RE_ThrottleBackend;
	re.FinishBloom = RE_FinishBloom;
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
