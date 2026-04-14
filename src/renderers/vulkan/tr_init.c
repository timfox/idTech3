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
#include "vk_fluidsim.h"
#include "vk_terrain.h"
#include "vk_vdb.h"
#include "vk_postfx.h"
#include "vk_flashlight.h"
#include "vk_skybox_hdr.h"
#ifdef USE_IMGUI
void VkImgui_Shutdown( void );
#endif

glconfig_t	glConfig;

qboolean	textureFilterAnisotropic;
int			maxAnisotropy;
int			gl_version;
int			gl_clamp_mode;	// GL_CLAMP or GL_CLAMP_TO_EGGE

glstate_t	glState;

glstatic_t	gls;

#ifdef USE_VULKAN
#include "vk_device.h"
static void VkInfo_f( void );
static void VulkanInfo_f( void );
static void VkVolumetricValidate_f( void );
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
cvar_t	*r_detail_scale;

cvar_t	*r_znear;
cvar_t	*r_zproj;
cvar_t	*r_stereoSeparation;
cvar_t	*r_firstPersonFov;
cvar_t	*r_firstPersonScale;
cvar_t	*r_firstPersonFovEnabled;
cvar_t	*r_firstPersonScaleEnabled;
cvar_t	*r_firstPersonZNear;

cvar_t	*r_skipBackEnd;

//cvar_t	*r_anaglyphMode;

cvar_t	*r_greyscale;
cvar_t	*r_dither;
cvar_t	*r_presentBits;
cvar_t	*r_outline;
cvar_t	*r_outlineThreshold;

static cvar_t *r_ignorehwgamma;

cvar_t  *r_teleporterFlash;

cvar_t	*r_fastsky;
cvar_t	*r_neatsky;
cvar_t	*r_drawSun;
cvar_t	*r_dynamiclight;
#ifdef USE_PMLIGHT
cvar_t	*r_dlightMode;
cvar_t	*r_dlightScale;
cvar_t	*r_dlightIntensity;
#endif
cvar_t	*r_dlightSaturation;
#ifdef USE_VULKAN
cvar_t	*r_device;
#ifdef USE_VBO
cvar_t	*r_vbo;
#endif
#ifdef USE_VK_PBR
cvar_t	*r_pbr;
cvar_t	*r_pbr_shExtract;
cvar_t	*r_pbr_debug;
cvar_t	*r_pbr_packedPreferred;
cvar_t	*r_pbr_multiScatter;
cvar_t	*r_pbr_multiScatterStrength;
cvar_t	*r_pbr_fresnelRoughness;
cvar_t	*r_pbr_specularAA;
cvar_t	*r_pbr_specularAAStrength;
cvar_t	*r_pbr_anisotropicSpecular;
cvar_t	*r_pbr_iblAnisoStretch;
cvar_t	*r_pom;
cvar_t	*r_pomSteps;
cvar_t	*r_pomScale;
cvar_t	*r_pomShadow;
cvar_t	*r_pomShadowSteps;
cvar_t	*r_glint;
cvar_t	*r_glintMode;
cvar_t	*r_glintDensity;
cvar_t	*r_glintMicrofacetRoughness;
cvar_t	*r_glintPixelFilterSize;
cvar_t	*r_glintSampleBudget;
cvar_t	*r_glintMaxLodClamp;
cvar_t	*r_glintRoughnessLo;
cvar_t	*r_glintRoughnessHi;
cvar_t	*r_glintDMax;
#ifdef VK_CUBEMAP
cvar_t	*r_pbr_iblIrradianceSize;
cvar_t	*r_pbr_iblPrefilterSize;
cvar_t	*r_pbr_showCubemap;
cvar_t	*r_pbr_cubemapInfo;
#endif
cvar_t  *r_baseNormalX;
cvar_t  *r_baseNormalY;
cvar_t  *r_baseParallax;
cvar_t  *r_baseSpecular;
#ifdef VK_CUBEMAP
cvar_t	*r_cubeMapping;
#endif
#ifdef HDR_DELUXE_LIGHTMAP
cvar_t	*r_deluxeMapping;
cvar_t	*r_deluxeSpecular;
#endif
#endif
cvar_t   *r_vk_pipeline_debug;
cvar_t	*r_vk_colorWriteMaskDynamic;
cvar_t	*r_vk_meshShaderNV;
cvar_t	*r_morph;
cvar_t	*r_morphMaxActive;
cvar_t	*r_morphLodStart;
cvar_t	*r_morphLodEnd;
cvar_t	*r_morphDebug;
cvar_t	*r_morphBreath;
cvar_t	*r_morphBreathAmp;
cvar_t	*r_morphBreathFreq;
cvar_t	*r_gltfAnim;
cvar_t	*r_gltfGpu;
cvar_t	*r_gltfGpuTangentFix;
cvar_t	*r_fbo;
cvar_t	*r_renderMode;
cvar_t	*r_hdr;
cvar_t	*r_bloom;
cvar_t	*r_bloom_threshold;
cvar_t	*r_bloom_intensity;
cvar_t	*r_bloom_threshold_mode;
cvar_t	*r_bloom_modulate;
cvar_t	*r_bloomKnee;
cvar_t	*r_ssao;
cvar_t	*r_ssaoMethod;
cvar_t	*r_ssaoRadius;
cvar_t	*r_ssaoBias;
cvar_t	*r_ssaoIntensity;
cvar_t	*r_ssaoPower;
cvar_t	*r_ssaoSamples;
cvar_t	*r_ssaoMaxDepthGradient;
cvar_t	*r_ssaoBlurRadius;
cvar_t	*r_hbaoDirections;
cvar_t	*r_hbaoSteps;
cvar_t	*r_oit;
cvar_t	*r_ssaoDebugView;
cvar_t	*r_renderWidth;
cvar_t	*r_renderHeight;
cvar_t	*r_renderScale;
cvar_t	*r_temporalDebug;
cvar_t	*r_screenMapScale;
cvar_t	*r_ext_supersample;
cvar_t	*r_ext_smaa;
cvar_t	*r_smaa_preset;
cvar_t	*r_smaa_threshold;
cvar_t	*r_smaa_local_contrast;
cvar_t	*r_smaa_max_search_steps;
cvar_t	*r_smaa_corner_rounding;
cvar_t	*r_taa;
cvar_t	*r_taa_feedbackStationary;
cvar_t	*r_taa_feedbackMotion;
cvar_t	*r_taa_sharpen;
cvar_t	*r_rtx;

#endif // USE_VULKAN

cvar_t	*r_dlightBacks;

cvar_t	*r_lodbias;
cvar_t	*r_lodscale;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_occlusionCulling;
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
cvar_t	*r_msaa_sample_shading;
cvar_t	*r_msaa_sample_shading_rate;
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
cvar_t	*r_showsky;
cvar_t	*r_shownormals;
cvar_t	*r_finish;
cvar_t	*r_clear;
cvar_t	*r_textureMode;
cvar_t	*r_mipLodBias;
cvar_t	*r_offsetFactor;
cvar_t	*r_offsetUnits;
cvar_t	*r_gamma;
cvar_t	*r_panini;
cvar_t	*r_panini_d;
cvar_t	*r_panini_s;
cvar_t	*r_panini_theta;
cvar_t	*r_panini_zoom;
cvar_t	*r_panini_border;
cvar_t	*r_panini_debug;
cvar_t	*r_paniniBrightness;
cvar_t	*r_paniniLensPreset;
cvar_t	*r_paniniBarrelDistortion;
cvar_t	*r_post;
cvar_t	*r_post_debug;
cvar_t	*r_exposure;
cvar_t	*r_hdr_lightmap_scale;
cvar_t	*r_lightmap_srgb_decode;
cvar_t	*r_pre_exposure_scale;
cvar_t	*r_exposure_auto;
cvar_t	*r_tonemap;
cvar_t	*r_rpi_profile;
cvar_t	*r_volumetricFog;
cvar_t	*r_volumetricFogDensity;
cvar_t	*r_volumetricFogHeightFalloff;
cvar_t	*r_volumetricFogAlbedo;
cvar_t	*r_volumetricFogExtinctionScale;
cvar_t	*r_volumetricFogBlendDistance;
cvar_t	*r_volumetricFogSphere;
cvar_t	*r_volumetricFogSphereCenter;
cvar_t	*r_volumetricFogSphereRadius;
cvar_t	*r_volumetricFogSphereDensity;
cvar_t	*r_volumetricFogCylinder;
cvar_t	*r_volumetricFogCylinderBase;
cvar_t	*r_volumetricFogCylinderTop;
cvar_t	*r_volumetricFogCylinderRadius;
cvar_t	*r_volumetricFogCylinderDensity;
cvar_t	*r_volumetricFogCone;
cvar_t	*r_volumetricFogConeApex;
cvar_t	*r_volumetricFogConeBase;
cvar_t	*r_volumetricFogConeRadius;
cvar_t	*r_volumetricFogConeDensity;
cvar_t	*r_volumetricFogDenoise;
cvar_t	*r_volumetricFogDenoiseSigma;
cvar_t	*r_volumetricFogAniso;
cvar_t	*r_volumetricFogSteps;
cvar_t	*r_volumetricFogZExponent;
cvar_t	*r_volumetricFogSliceMode;
cvar_t	*r_volumetricFogMaxDistance;
cvar_t	*r_volumetricFogJitter;
cvar_t	*r_volumetricFogTemporalWeight;
cvar_t	*r_volumetricFogReprojectionThreshold;
cvar_t	*r_volumetricFogHistoryVelocityThreshold;
cvar_t	*r_volumetricFogFireflyClamp;
cvar_t	*r_volumetricFogColorMode;
cvar_t	*r_volumetricFogTint;
cvar_t	*r_volumetricFogIntensity;
cvar_t	*r_volumetricFogQuality;
cvar_t	*r_volumetricFogResolutionScale;
cvar_t	*r_volumetricFogTransmittanceCutoff;
cvar_t	*r_volumetricFogBaseHeight;
cvar_t	*r_volumetricFogWorldMin;
cvar_t	*r_volumetricFogWorldMax;
cvar_t	*r_volumetricFogGridDim;
cvar_t	*r_volumetricFogDepthMode;
cvar_t	*r_volumetricFogSunIntensity;
cvar_t	*r_volumetricFogAmbientIntensity;
cvar_t	*r_volumetricFogNoiseDim;
cvar_t	*r_volumetricFogNoiseScale;
cvar_t	*r_volumetricFogNoiseStrength;
cvar_t	*r_volumetricFogNoiseThreshold;
cvar_t	*r_volumetricFogNoiseScroll;
cvar_t	*r_volumetricFogWindSpeed;
cvar_t	*r_volumetricFogWindDirection;
cvar_t	*r_fogFluid;
cvar_t	*r_fogFluidQuality;
cvar_t	*r_fogFluidResolutionScale;
cvar_t	*r_fogFluidViscosity;
cvar_t	*r_fogFluidPressureIterations;
cvar_t	*r_fogFluidDissipation;
cvar_t	*r_fogFluidForceScale;
cvar_t	*r_fogFluidWrap;
cvar_t	*r_fogFluidVelocityClamp;
cvar_t	*r_fogFluidAutoScale;
cvar_t	*r_fogFluidTargetMs;
cvar_t	*r_fogFluidAutoScaleRate;
cvar_t	*r_fogFluidAutoScaleMinResolution;
cvar_t	*r_fogFluidAutoScaleMinIterations;
cvar_t	*r_fogFluidFlowFieldStrength;
cvar_t	*r_fogFluidFlowFieldScale;
cvar_t	*r_fogFluidVorticity;
cvar_t	*r_fogFluidBuoyancy;
cvar_t	*r_volumetricFogValidation;
cvar_t	*r_volumetricFogValidationPrintInterval;
cvar_t	*r_volumetricFogForceCameraCut;
cvar_t	*r_volumetricFogSkipStatic;
cvar_t	*r_volumetricFogPerfTimers;
cvar_t	*r_volumetricFogPerfPrintInterval;
cvar_t	*r_volumetricFogTemporalStability;
cvar_t	*r_volumetricFogShadowContrast;
cvar_t	*r_volumetricFogShowcase;
cvar_t	*r_fog_shadows;
cvar_t	*r_fogShadowMapSize;
cvar_t	*r_fogShadowBias;
cvar_t	*r_fogShadowPcfRadius;
cvar_t	*r_fogShadowMaxDistance;
cvar_t	*r_fogShadowPadding;
cvar_t	*r_fogDebug;
cvar_t	*r_fboDebug;
cvar_t	*r_fboCinematic;
cvar_t	*r_froxelDebug;
cvar_t	*r_vk_swapchain_srgb;
cvar_t	*r_intensity;
cvar_t	*r_lockpvs;
cvar_t	*r_noportals;
cvar_t	*r_portalOnly;

cvar_t	*r_subdivisions;
cvar_t	*r_lodCurveError;

cvar_t	*r_overBrightBits;
cvar_t	*r_mapOverBrightBits;
cvar_t	*r_mapGreyScale;
cvar_t	*r_fogTint;

cvar_t	*r_debugSurface;
cvar_t	*r_simpleMipMaps;

cvar_t	*r_showImages;
cvar_t	*r_defaultImage;

cvar_t	*r_ambientScale;
cvar_t	*r_directedScale;
cvar_t	*r_shLighting;
cvar_t	*r_shWorldLighting;
cvar_t	*r_shDebugView;
cvar_t	*r_debugLight;
cvar_t	*r_debugSort;
cvar_t	*r_printShaders;
cvar_t	*r_saveFontData;

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
	QGL_Core_PROCS
	QGL_Ext_PROCS
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
#if defined(USE_RENDERER_DLOPEN) && USE_RENDERER_DLOPEN
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

		// print info will be called after R_Register

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
	ri.Printf( PRINT_ALL, "[VK] GPU compute: enabled (volumetric fog, vegetation wind, etc.)\n" );
	ri.Printf( PRINT_ALL, "[VK] NVIDIA DLSS / NGX: not integrated in-engine; use \\r_renderScale and HDR post paths, or GPU-driver upscaling\n" );
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
	char validation_msg[512];

	if ( vk_consume_validation_error( validation_msg, sizeof( validation_msg ) ) ) {
		ri.Error( ERR_FATAL, "Vulkan validation: %s", validation_msg );
	}
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
Note: Statics are not reinitialized between fs_game changes.

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
#ifdef USE_VULKAN
	byte *buffer, *bufstart;
	int linelen;
	int	bufAlign;
	int packAlign = 1;

	(void)x;
	(void)y;
	(void)lineAlign;

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

	if ( !tr.world || !tr.world->baseName[0] ) {
		ri.Printf( PRINT_WARNING, "Levelshot requires a loaded map.\n" );
		return;
	}
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

	ri.Printf( PRINT_ALL, "texturemode: %s\n", r_textureMode ? r_textureMode->string : "GL_LINEAR_MIPMAP_NEAREST" );
	ri.Printf( PRINT_ALL, "mip LOD bias: %.2f\n", r_mipLodBias ? r_mipLodBias->value : 0.0f );
	ri.Printf( PRINT_ALL, "texture bits: %d\n", r_texturebits ? (r_texturebits->integer ? r_texturebits->integer : 32) : 32 );
	ri.Printf( PRINT_ALL, "picmip: %d%s\n", r_picmip ? r_picmip->integer : 0, (r_nomip && r_nomip->integer) ? ", worldspawn only" : "" );

#ifdef USE_VULKAN
	if ( r_vertexLight && r_vertexLight->integer ) {
		ri.Printf( PRINT_ALL, "Note: using vertex lightmap approximation\n" );
	}
#if defined (USE_VK_PBR)
	ri.Printf( PRINT_ALL, "PBR SH extraction: %s\n", (r_pbr_shExtract && r_pbr_shExtract->integer) ? "enabled" : "disabled" );
	if ( r_glint && r_glint->integer ) {
		ri.Printf( PRINT_ALL, "PBR glint NDF: enabled (r_glint 1)\n" );
	}
	if ( r_pbr_iblAnisoStretch && Q_fabs( r_pbr_iblAnisoStretch->value - 1.0f ) > 0.001f ) {
		ri.Printf( PRINT_ALL, "PBR IBL anisotropy stretch: %.2f (r_pbr_iblAnisoStretch, default 1)\n", r_pbr_iblAnisoStretch->value );
	}
	if ( r_pbr_debug && r_pbr_debug->integer ) {
		ri.Printf( PRINT_ALL, "PBR debug view: mode %d (1=direct,2=ibl spec,3=irradiance,4=env samples,5-8=glint)\n", r_pbr_debug->integer );
	}
#endif
#else
	if ( (r_vertexLight && r_vertexLight->integer) || glConfig.hardwareType == GLHW_PERMEDIA2 ) {
		ri.Printf( PRINT_ALL, "Note: using vertex lightmap approximation\n" );
	} else if ( glConfig.hardwareType == GLHW_RAGEPRO ) {
		ri.Printf( PRINT_ALL, "Note: ragePro approximations\n" );
	} else if ( glConfig.hardwareType == GLHW_RIVA128 ) {
		ri.Printf( PRINT_ALL, "Note: riva128 approximations\n" );
	}
#endif
	if ( r_finish && r_finish->integer ) {
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

static void VulkanInfo_f( void )
{
	VkPhysicalDeviceProperties props;
	char driver_version[64];

	if ( !vk.physical_device ) {
		ri.Printf( PRINT_ALL, "Vulkan not initialized.\n" );
		return;
	}

	qvkGetPhysicalDeviceProperties( vk.physical_device, &props );

	/* Decode driver version (vendor-specific encoding) */
	switch ( props.vendorID ) {
		case 0x10DE: /* NVIDIA */
			Com_sprintf( driver_version, sizeof( driver_version ), "%u.%u.%u.%u",
				(props.driverVersion >> 22) & 0x3FF,
				(props.driverVersion >> 14) & 0x0FF,
				(props.driverVersion >> 6) & 0x0FF,
				(props.driverVersion >> 0) & 0x03F );
			break;
		default:
			Com_sprintf( driver_version, sizeof( driver_version ), "%u.%u.%u",
				(props.driverVersion >> 22),
				(props.driverVersion >> 12) & 0x3FF,
				(props.driverVersion >> 0) & 0xFFF );
			break;
	}

	ri.Printf( PRINT_ALL, "======== Vulkan Info ========\n" );
	ri.Printf( PRINT_ALL, "Device    : %s\n", vk_device_renderer_name( &props ) );
	ri.Printf( PRINT_ALL, "API       : %u.%u.%u\n",
		VK_VERSION_MAJOR( props.apiVersion ),
		VK_VERSION_MINOR( props.apiVersion ),
		VK_VERSION_PATCH( props.apiVersion ) );
	ri.Printf( PRINT_ALL, "Driver    : %s\n", driver_version );
	ri.Printf( PRINT_ALL, "Vendor ID : 0x%04X\n", props.vendorID );
	ri.Printf( PRINT_ALL, "Device ID : 0x%04X\n", props.deviceID );
	if ( vk_device_is_v3dv( &props ) )
		ri.Printf( PRINT_ALL, "Platform  : Raspberry Pi (V3DV)\n" );
	ri.Printf( PRINT_ALL, "==============================\n" );
}

/*
===============
R_Quality_f
===============
Apply AAA-style quality presets: 0=Low, 1=Medium, 2=High, 3=Ultra.
Requires vid_restart for some settings to take effect.
*/
static void R_Quality_f( void )
{
	const int argc = ri.Cmd_Argc();
	const char *arg = ( argc > 1 ) ? ri.Cmd_Argv( 1 ) : "";
	int preset = ( arg[0] >= '0' && arg[0] <= '3' ) ? ( arg[0] - '0' ) : -1;

	if ( preset < 0 ) {
		ri.Printf( PRINT_ALL,
			"usage: r_quality <0|1|2|3>\n"
			"  0 = Low    : Volumetric fog off, SSAO off, bloom off, SMAA off, SSR off\n"
			"  1 = Medium : Fog/SSAO/bloom/SMAA on (low quality), SSR off\n"
			"  2 = High   : Fog/SSAO/bloom/SMAA on (high quality), HBAO, SSR off\n"
			"  3 = Ultra  : All effects on, HBAO, SSR on, max quality\n"
			"Run vid_restart after changing for full effect.\n" );
		return;
	}

	switch ( preset ) {
		case 0: /* Low */
			ri.Cvar_Set( "r_taa", "0" );
			ri.Cvar_Set( "r_volumetricFog", "0" );
			ri.Cvar_Set( "r_ssao", "0" );
			ri.Cvar_Set( "r_bloom", "0" );
			ri.Cvar_Set( "r_ext_smaa", "0" );
			ri.Cvar_Set( "r_ext_multisample", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "0" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Cvar_Set( "r_ssr", "0" );
			ri.Cvar_Set( "r_sharpen", "0.0" );
			ri.Cvar_Set( "r_exposure_auto", "0" );
			ri.Printf( PRINT_ALL, "[VK] Quality preset: Low (performance)\n" );
			break;
		case 1: /* Medium */
			ri.Cvar_Set( "r_taa", "0" );
			ri.Cvar_Set( "r_volumetricFog", "1" );
			ri.Cvar_Set( "r_ssao", "1" );
			ri.Cvar_Set( "r_bloom", "1" );
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_ext_multisample", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "0" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Cvar_Set( "r_ssr", "0" );
			ri.Cvar_Set( "r_volumetricFogQuality", "1" );
			ri.Cvar_Set( "r_ssaoMethod", "0" );
			ri.Cvar_Set( "r_smaa_preset", "2" );
			ri.Cvar_Set( "r_ssaoSamples", "12" );
			ri.Cvar_Set( "r_sharpen", "0.0" );
			ri.Printf( PRINT_ALL, "[VK] Quality preset: Medium\n" );
			break;
		case 2: /* High */
			ri.Cvar_Set( "r_taa", "0" );
			ri.Cvar_Set( "r_volumetricFog", "1" );
			ri.Cvar_Set( "r_ssao", "1" );
			ri.Cvar_Set( "r_bloom", "1" );
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_ext_multisample", "4" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "1" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Cvar_Set( "r_ssr", "0" );
			ri.Cvar_Set( "r_volumetricFogQuality", "2" );
			ri.Cvar_Set( "r_ssaoMethod", "1" );
			ri.Cvar_Set( "r_smaa_preset", "3" );
			ri.Cvar_Set( "r_hbaoDirections", "8" );
			ri.Cvar_Set( "r_hbaoSteps", "8" );
			ri.Cvar_Set( "r_ssaoSamples", "16" );
			ri.Cvar_Set( "r_sharpen", "0.15" );
			ri.Printf( PRINT_ALL, "[VK] Quality preset: High (SMAA + 4x MSAA)\n" );
			break;
		case 3: /* Ultra */
			ri.Cvar_Set( "r_taa", "0" );
			ri.Cvar_Set( "r_volumetricFog", "1" );
			ri.Cvar_Set( "r_ssao", "1" );
			ri.Cvar_Set( "r_bloom", "1" );
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_ext_multisample", "4" );
			ri.Cvar_Set( "r_msaa_sample_shading", "1" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "1" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Cvar_Set( "r_ssr", "1" );
			ri.Cvar_Set( "r_volumetricFogQuality", "3" );
			ri.Cvar_Set( "r_ssaoMethod", "1" );
			ri.Cvar_Set( "r_smaa_preset", "4" );
			ri.Cvar_Set( "r_hbaoDirections", "16" );
			ri.Cvar_Set( "r_hbaoSteps", "16" );
			ri.Cvar_Set( "r_ssaoSamples", "24" );
			ri.Cvar_Set( "r_sharpen", "0.25" );
			ri.Printf( PRINT_ALL, "[VK] Quality preset: Ultra (SMAA + 4x MSAA + sample shading)\n" );
			break;
	}
	ri.Printf( PRINT_ALL, "Run vid_restart for full effect.\n" );
}

/*
===============
R_AAQuality_f
===============
Prefer high-quality spatial AA paths for Vulkan: SMAA, MSAA, and optional SSAA.
*/
static void R_AAQuality_f( void )
{
	const int argc = ri.Cmd_Argc();
	const char *arg = ( argc > 1 ) ? ri.Cmd_Argv( 1 ) : "";
	int preset = ( arg[0] >= '0' && arg[0] <= '4' ) ? ( arg[0] - '0' ) : -1;

	if ( preset < 0 ) {
		ri.Printf( PRINT_ALL,
			"usage: r_aaQuality <0|1|2|3|4>\n"
			"  0 = Off        : No AA\n"
			"  1 = SMAA       : SMAA High, no MSAA\n"
			"  2 = Balanced   : SMAA High + 2x MSAA\n"
			"  3 = High       : SMAA Ultra + 4x MSAA\n"
			"  4 = Extreme    : SMAA Ultra + 4x MSAA + sample shading + SSAA\n"
			"TAA is disabled in all presets. Run vid_restart for full effect.\n" );
		return;
	}

	ri.Cvar_Set( "r_taa", "0" );

	switch ( preset ) {
		case 0:
			ri.Cvar_Set( "r_ext_smaa", "0" );
			ri.Cvar_Set( "r_ext_multisample", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "0" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Printf( PRINT_ALL, "[VK] AA preset: Off\n" );
			break;
		case 1:
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_smaa_preset", "3" );
			ri.Cvar_Set( "r_ext_multisample", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "0" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Printf( PRINT_ALL, "[VK] AA preset: SMAA High\n" );
			break;
		case 2:
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_smaa_preset", "3" );
			ri.Cvar_Set( "r_ext_multisample", "2" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "1" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Printf( PRINT_ALL, "[VK] AA preset: SMAA High + 2x MSAA\n" );
			break;
		case 3:
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_smaa_preset", "4" );
			ri.Cvar_Set( "r_ext_multisample", "4" );
			ri.Cvar_Set( "r_msaa_sample_shading", "0" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "0.5" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "1" );
			ri.Cvar_Set( "r_ext_supersample", "0" );
			ri.Printf( PRINT_ALL, "[VK] AA preset: SMAA Ultra + 4x MSAA\n" );
			break;
		case 4:
			ri.Cvar_Set( "r_ext_smaa", "1" );
			ri.Cvar_Set( "r_smaa_preset", "4" );
			ri.Cvar_Set( "r_ext_multisample", "4" );
			ri.Cvar_Set( "r_msaa_sample_shading", "1" );
			ri.Cvar_Set( "r_msaa_sample_shading_rate", "1.0" );
			ri.Cvar_Set( "r_ext_alpha_to_coverage", "1" );
			ri.Cvar_Set( "r_ext_supersample", "1" );
			ri.Printf( PRINT_ALL, "[VK] AA preset: Extreme spatial AA (SMAA Ultra + 4x MSAA + SSAA)\n" );
			break;
	}

	ri.Printf( PRINT_ALL, "Run vid_restart for full effect.\n" );
}

static void VkVolumetricValidate_f( void )
{
	const int argc = ri.Cmd_Argc();
	const char *mode = ( argc > 1 ) ? ri.Cmd_Argv( 1 ) : "";

	if ( !mode[0] ) {
		ri.Printf( PRINT_ALL,
			"usage: vkVolumetricValidate <camera|ghosting|localspot|localpoint|msaa|checklist|reset>\n"
			"  camera [captureName]       : force one camera-cut reset and show reset overlay.\n"
			"  ghosting [captureName]     : show motion-vector threshold debug for fast camera tests.\n"
			"  localspot [captureName]    : show local spot-shadow visibility debug.\n"
			"  localpoint [captureName]   : show local point-shadow visibility debug.\n"
			"  msaa [capture base [msaa]] : queue off/on MSAA parity screenshots (+ optional sample count).\n"
			"  checklist                  : print exact validation commands for the final checklist.\n"
			"  reset                      : restore normal fog (r_fogDebug 0, r_volumetricFogValidation 0).\n" );
		return;
	}

	if ( !Q_stricmp( mode, "reset" ) ) {
		ri.Cvar_Set( "r_fogDebug", "0" );
		ri.Cvar_Set( "r_volumetricFogValidation", "0" );
		ri.Printf( PRINT_ALL, "[VK][fog] fog debug disabled: normal fog restored.\n" );
		return;
	}

	if ( !Q_stricmp( mode, "camera" ) ) {
		ri.Cvar_Set( "r_fogDebug", "10" );
		ri.Cvar_Set( "r_volumetricFogValidation", "1" );
		ri.Cvar_SetValue( "r_volumetricFogForceCameraCut", 1.0f );
		if ( argc > 2 && ri.Cmd_Argv( 2 )[0] ) {
			ri.Cmd_ExecuteText( EXEC_APPEND, va( "screenshotJPEG %s\n", ri.Cmd_Argv( 2 ) ) );
		}
		ri.Printf( PRINT_ALL, "[VK][fog] validation camera-cut armed: debug=10, forceCut=1. When done: vkVolumetricValidate reset\n" );
		return;
	}

	if ( !Q_stricmp( mode, "ghosting" ) ) {
		ri.Cvar_Set( "r_fogDebug", "7" );
		ri.Cvar_Set( "r_volumetricFogValidation", "1" );
		if ( argc > 2 && ri.Cmd_Argv( 2 )[0] ) {
			ri.Cmd_ExecuteText( EXEC_APPEND, va( "screenshotJPEG %s\n", ri.Cmd_Argv( 2 ) ) );
		}
		ri.Printf( PRINT_ALL, "[VK][fog] validation ghosting view enabled: debug=7 (motion magnitude / threshold). When done: vkVolumetricValidate reset\n" );
		return;
	}

	if ( !Q_stricmp( mode, "localspot" ) ) {
		ri.Cvar_Set( "r_fog_shadows", "1" );
		ri.Cvar_Set( "r_fogDebug", "8" );
		ri.Cvar_Set( "r_volumetricFogValidation", "1" );
		if ( argc > 2 && ri.Cmd_Argv( 2 )[0] ) {
			ri.Cmd_ExecuteText( EXEC_APPEND, va( "screenshotJPEG %s\n", ri.Cmd_Argv( 2 ) ) );
		}
		ri.Printf( PRINT_ALL, "[VK][fog] validation local spot shadow view enabled: debug=8. When done: vkVolumetricValidate reset\n" );
		return;
	}

	if ( !Q_stricmp( mode, "localpoint" ) ) {
		ri.Cvar_Set( "r_fog_shadows", "1" );
		ri.Cvar_Set( "r_fogDebug", "9" );
		ri.Cvar_Set( "r_volumetricFogValidation", "1" );
		if ( argc > 2 && ri.Cmd_Argv( 2 )[0] ) {
			ri.Cmd_ExecuteText( EXEC_APPEND, va( "screenshotJPEG %s\n", ri.Cmd_Argv( 2 ) ) );
		}
		ri.Printf( PRINT_ALL, "[VK][fog] validation local point shadow view enabled: debug=9. When done: vkVolumetricValidate reset\n" );
		return;
	}

	if ( !Q_stricmp( mode, "msaa" ) ) {
		const int current_msaa = ( r_ext_multisample ) ? r_ext_multisample->integer : 0;
		const char *base = ( argc > 2 && ri.Cmd_Argv( 2 )[0] ) ? ri.Cmd_Argv( 2 ) : "";
		int target_msaa = ( current_msaa > 1 ) ? current_msaa : 4;

		if ( argc > 3 && ri.Cmd_Argv( 3 )[0] ) {
			target_msaa = atoi( ri.Cmd_Argv( 3 ) );
		}
		if ( target_msaa < 2 ) {
			target_msaa = 2;
		} else if ( target_msaa > 8 ) {
			target_msaa = 8;
		}

		if ( !base[0] ) {
			ri.Printf( PRINT_ALL,
				"[VK][fog] msaa parity helper:\n"
				"  run: vkVolumetricValidate msaa <captureBase> [targetSamples]\n"
				"  example: vkVolumetricValidate msaa fog_msaa_parity 4\n"
				"  current r_ext_multisample=%d target=%d\n",
				current_msaa, target_msaa );
			return;
		}

		ri.Cvar_Set( "r_fogDebug", "0" );
		ri.Cmd_ExecuteText( EXEC_APPEND, va(
			"set r_fogDebug 0\n"
			"set r_ext_multisample 0\n"
			"vid_restart\n"
			"wait\nwait\nwait\nwait\n"
			"screenshotJPEG %s_off\n"
			"set r_ext_multisample %d\n"
			"vid_restart\n"
			"wait\nwait\nwait\nwait\n"
			"screenshotJPEG %s_on\n",
			base, target_msaa, base ) );

		if ( current_msaa != target_msaa ) {
			ri.Cmd_ExecuteText( EXEC_APPEND, va(
				"set r_ext_multisample %d\n"
				"vid_restart\n",
				current_msaa ) );
		}

		ri.Printf( PRINT_ALL, "[VK][fog] queued MSAA parity capture: base=%s (off vs %dx)\n", base, target_msaa );
		return;
	}

	if ( !Q_stricmp( mode, "checklist" ) ) {
		ri.Printf( PRINT_ALL,
			"[VK][fog] final checklist commands:\n"
			"  camera cut reset       : vkVolumetricValidate camera fog_camera_cut\n"
			"  fast camera ghosting   : vkVolumetricValidate ghosting fog_ghosting\n"
			"  local spot shadows     : vkVolumetricValidate localspot fog_local_spot\n"
			"  local point shadows    : vkVolumetricValidate localpoint fog_local_point\n"
			"  msaa parity capture    : vkVolumetricValidate msaa fog_msaa_parity 4\n" );
		return;
	}

	ri.Printf( PRINT_WARNING, "vkVolumetricValidate: unknown mode '%s'\n", mode );
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
	ri.Cmd_AddCommand( "vulkaninfo", VulkanInfo_f );
	ri.Cmd_AddCommand( "vkVolumetricValidate", VkVolumetricValidate_f );
	ri.Cmd_AddCommand( "r_quality", R_Quality_f );
	ri.Cmd_AddCommand( "r_aaQuality", R_AAQuality_f );
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
	r_intensity = ri.Cvar_Get( "r_intensity", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
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
	r_detail_scale = ri.Cvar_Get( "r_detail_scale", "4", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_detail_scale, "Tiling frequency for PBR detail maps (higher = more repetition)." );
	r_texturebits = ri.Cvar_Get( "r_texturebits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_texturebits, "Number of texture bits per texture." );

#if defined (USE_VULKAN) && defined (USE_VBO)
	r_vbo = ri.Cvar_Get( "r_vbo", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_vbo, "Use Vertex Buffer Objects to cache static map geometry, may improve FPS on modern GPUs, increases hunk memory usage by 15-30MB (map-dependent)." );
#endif
#if defined (USE_VULKAN) && defined (USE_VK_PBR)
	r_pbr = ri.Cvar_Get("r_pbr", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_pbr, "Enables Physically Based Rendering (metalness/roughness, IBL, Cook-Torrance BRDF).\n"
		"Requires " S_COLOR_CYAN "\\r_fbo 1" S_COLOR_WHITE " (vid_restart after changing).\n"
		"Advised " S_COLOR_CYAN "\\r_vbo 1" S_COLOR_GREEN " for static world geometry (optional)." );

	r_pbr_shExtract = ri.Cvar_Get( "r_pbr_shExtract", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_pbr_shExtract, "Extract SH coefficients from generated irradiance cubemaps for PBR." );

	r_pbr_debug = ri.Cvar_Get( "r_pbr_debug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pbr_debug, "0", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_pbr_debug,
		"PBR debug view override (Vulkan PBR only):\n"
		" 0 - off (standard PBR)\n"
		" 1 - show direct lighting only\n"
		" 2 - show specular environment contribution only\n"
		" 3 - show diffuse irradiance only\n"
		" 4 - show env/irradiance cubemap samples\n"
		" 5 - show glint D term (log)\n"
		" 6 - show glint lambda (LOD)\n"
		" 7 - show glint compensation\n"
		" 8 - show glint weight\n" );

	r_glint = ri.Cvar_Get( "r_glint", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_glint, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_glint, "Master toggle for the glint-based microfacet NDF (requires \\r_pbr 1)." );

	r_glintMode = ri.Cvar_Get( "r_glintMode", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_glintMode, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_glintMode, "Glint behavior mode: 0 = off, 1 = replace the GGX D term." );

	r_glintDensity = ri.Cvar_Get( "r_glintDensity", "3.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_glintDensity, "-4.0", "6.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_glintDensity, "Log10 density control for glint particles (N = 1e3 * pow(10, value))." );

	r_glintMicrofacetRoughness = ri.Cvar_Get( "r_glintMicrofacetRoughness", "0.01", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_glintMicrofacetRoughness, "0.001", "0.1", CV_FLOAT );
	ri.Cvar_SetDescription( r_glintMicrofacetRoughness, "Microfacet roughness for the glint lattice (smaller = sharper glints)." );

	r_glintPixelFilterSize = ri.Cvar_Get( "r_glintPixelFilterSize", "0.7", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_glintPixelFilterSize, "0.5", "1.2", CV_FLOAT );
	ri.Cvar_SetDescription( r_glintPixelFilterSize, "Pixel filter multiplier for the glint sampling lattice." );

	r_glintSampleBudget = ri.Cvar_Get( "r_glintSampleBudget", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_glintSampleBudget, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_glintSampleBudget, "Glint sample budget: 0=fast (1 tap), 1=medium (2 taps), 2=best (4 taps)." );

	r_glintMaxLodClamp = ri.Cvar_Get( "r_glintMaxLodClamp", "12.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_glintMaxLodClamp, "0.0", "16.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_glintMaxLodClamp, "Clamp on the computed LOD for glint sampling to avoid expensive levels." );

	r_glintRoughnessLo = ri.Cvar_Get( "r_glintRoughnessLo", "0.02", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_glintRoughnessLo, "0.0", "0.5", CV_FLOAT );
	ri.Cvar_SetDescription( r_glintRoughnessLo, "Lower roughness threshold where glints start fading out (smoother values)." );

	r_glintRoughnessHi = ri.Cvar_Get( "r_glintRoughnessHi", "0.15", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_glintRoughnessHi, "0.0", "0.6", CV_FLOAT );
	ri.Cvar_SetDescription( r_glintRoughnessHi, "Upper roughness threshold where glints are disabled (rougher values)." );

	r_glintDMax = ri.Cvar_Get( "r_glintDMax", "1000.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_glintDMax, "1.0", "1000000.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_glintDMax, "Clamp for the glint D term to avoid fireflies." );

	r_pbr_packedPreferred = ri.Cvar_Get( "r_pbr_packedPreferred", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_pbr_packedPreferred, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_pbr_packedPreferred,
		"Preferred packed PBR physical map format for auto-discovery:\n"
		" 0 - auto (legacy order)\n"
		" 1 - ORM (recommended)\n"
		" 2 - RMO\n"
		" 3 - MOXR\n"
		" 4 - ORMS\n"
		" 5 - RMOS\n"
		" 6 - MOSR\n"
		"Auto-discovery still falls back to other formats if the preferred suffix is not found." );

	r_pbr_multiScatter = ri.Cvar_Get( "r_pbr_multiScatter", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pbr_multiScatter, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_pbr_multiScatter, "Enable Kulla-Conty style specular IBL multiple-scattering compensation." );

	r_pbr_multiScatterStrength = ri.Cvar_Get( "r_pbr_multiScatterStrength", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pbr_multiScatterStrength, "0.0", "2.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_pbr_multiScatterStrength, "Scales specular IBL multiple-scattering compensation intensity." );

	r_pbr_fresnelRoughness = ri.Cvar_Get( "r_pbr_fresnelRoughness", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pbr_fresnelRoughness, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_pbr_fresnelRoughness, "Enable roughness-dependent Fresnel (2025 PBR). Attenuates grazing Fresnel on rough surfaces for better energy conservation." );

	r_pbr_specularAA = ri.Cvar_Get( "r_pbr_specularAA", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pbr_specularAA, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_pbr_specularAA, "Enable normal-variance specular anti-aliasing for Vulkan PBR materials." );

	r_pbr_specularAAStrength = ri.Cvar_Get( "r_pbr_specularAAStrength", "0.5", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pbr_specularAAStrength, "0.0", "2.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_pbr_specularAAStrength, "Scales roughness stabilization from normal-map variance for modern BRDF materials." );

	r_pbr_anisotropicSpecular = ri.Cvar_Get( "r_pbr_anisotropicSpecular", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pbr_anisotropicSpecular, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_pbr_anisotropicSpecular,
		"When 1 and an anisotropy map is bound, use anisotropic GGX for direct specular highlights (tangent-aligned). When 0, ignore anisotropy for direct lighting (IBL stays isotropic)." );

	r_pbr_iblAnisoStretch = ri.Cvar_Get( "r_pbr_iblAnisoStretch", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pbr_iblAnisoStretch, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_pbr_iblAnisoStretch,
		"When > 0 and an anisotropy map is bound, increase effective IBL roughness along the stretch direction (blurry elongated reflections). 0 = isotropic IBL sampling." );

	r_pom = ri.Cvar_Get( "r_pom", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pom, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_pom,
		"Vulkan PBR: Parallax Occlusion Mapping when normal + packed ORM (physical) maps are bound. Height from ORM .r (occlusion). Per-material: parallaxDepth / parallaxBias in shader." );

	r_pomSteps = ri.Cvar_Get( "r_pomSteps", "16", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pomSteps, "4", "64", CV_INTEGER );
	ri.Cvar_SetDescription( r_pomSteps, "POM ray-march step count (higher = sharper silhouettes, more GPU cost)." );

	r_pomScale = ri.Cvar_Get( "r_pomScale", "0.06", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pomScale, "0.0", "0.25", CV_FLOAT );
	ri.Cvar_SetDescription( r_pomScale, "Global height scale multiplier for POM (material parallaxDepth still applies)." );

	r_pomShadow = ri.Cvar_Get( "r_pomShadow", "0.35", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pomShadow, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_pomShadow, "Approximate self-shadowing strength for POM (0 = off)." );

	r_pomShadowSteps = ri.Cvar_Get( "r_pomShadowSteps", "6", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pomShadowSteps, "2", "16", CV_INTEGER );
	ri.Cvar_SetDescription( r_pomShadowSteps, "POM self-shadow march steps along light direction in tangent space." );

	ri.Printf( PRINT_ALL, "POM: r_pom %s (steps %d, scale %.3f, shadow %.2f)\n",
		( r_pom && r_pom->integer ) ? "on" : "off",
		( r_pomSteps ? r_pomSteps->integer : 16 ),
		( r_pomScale ? r_pomScale->value : 0.06f ),
		( r_pomShadow ? r_pomShadow->value : 0.0f ) );

	r_baseNormalX	= ri.Cvar_Get("r_baseNormalX",		"1.0",	CVAR_ARCHIVE | CVAR_LATCH );
	r_baseNormalY	= ri.Cvar_Get("r_baseNormalY",		"1.0",	CVAR_ARCHIVE | CVAR_LATCH );
	r_baseParallax	= ri.Cvar_Get("r_baseParallax",		"0.05",	CVAR_ARCHIVE | CVAR_LATCH );
	r_baseSpecular	= ri.Cvar_Get( "r_baseSpecular",	"0.04",	CVAR_ARCHIVE | CVAR_LATCH );
#ifdef VK_CUBEMAP
	r_pbr_iblIrradianceSize = ri.Cvar_Get( "r_pbr_iblIrradianceSize", "64", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_pbr_iblIrradianceSize, "16", "1024", CV_INTEGER );
	ri.Cvar_SetDescription( r_pbr_iblIrradianceSize, "IBL irradiance cubemap size (resolution per face). Power-of-two recommended. Requires renderer restart." );

	r_pbr_iblPrefilterSize = ri.Cvar_Get( "r_pbr_iblPrefilterSize", "256", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_pbr_iblPrefilterSize, "32", "2048", CV_INTEGER );
	ri.Cvar_SetDescription( r_pbr_iblPrefilterSize, "IBL prefiltered environment cubemap size (resolution per face). Power-of-two recommended. Requires renderer restart." );

	r_pbr_showCubemap = ri.Cvar_Get( "r_pbr_showCubemap", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_pbr_showCubemap, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_pbr_showCubemap, "Show current PBR cubemap selection overlay (updates a debug string cvar)." );

	r_pbr_cubemapInfo = ri.Cvar_Get( "r_pbr_cubemapInfo", "", CVAR_TEMP );
	ri.Cvar_SetDescription( r_pbr_cubemapInfo, "Read-only-ish debug string updated by renderer when r_pbr_showCubemap is enabled." );

	r_cubeMapping = ri.Cvar_Get( "r_cubeMapping", "0", CVAR_ARCHIVE | CVAR_LATCH );
#endif
	#ifdef HDR_DELUXE_LIGHTMAP
	r_deluxeMapping		= ri.Cvar_Get("r_deluxeMapping",	"1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_deluxeMapping, "Reading deluxemaps when compiled with q3map2:\n 0: off (approximated from lightgrid)\n 1: on (compiled deluxemaps)" );
	r_deluxeSpecular	= ri.Cvar_Get("r_deluxeSpecular",	"1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_deluxeSpecular, "Scale the specular response from deluxemaps" );
#endif
#endif
	r_mapGreyScale = ri.Cvar_Get( "r_mapGreyScale", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_mapGreyScale, "-1", "1", CV_FLOAT );
	ri.Cvar_SetDescription(r_mapGreyScale, "Desaturate world map textures only, works independently from \\r_greyscale, negative values only desaturate lightmaps.");

	r_fogTint = ri.Cvar_Get( "r_fogTint", "1.08 1.00 0.72", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_fogTint, "Legacy map fog RGB tint multiplier (3 floats). Applied at render time to non-volumetric fog." );

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

	r_morph = ri.Cvar_Get( "r_morph", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_morph, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_morph, "Enable IQM morph target evaluation in Vulkan renderer." );
	ri.Cvar_SetGroup( r_morph, CVG_RENDERER );

	r_morphMaxActive = ri.Cvar_Get( "r_morphMaxActive", "4", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_morphMaxActive, "1", XSTRING( IQM_MORPH_TOP_K ), CV_INTEGER );
	ri.Cvar_SetDescription( r_morphMaxActive, "Maximum active IQM morph channels evaluated per entity (top-K by absolute weight)." );
	ri.Cvar_SetGroup( r_morphMaxActive, CVG_RENDERER );

	r_morphLodStart = ri.Cvar_Get( "r_morphLodStart", "900", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_morphLodStart, "0", "65536", CV_FLOAT );
	ri.Cvar_SetDescription( r_morphLodStart, "Distance where IQM morph fading begins." );
	ri.Cvar_SetGroup( r_morphLodStart, CVG_RENDERER );

	r_morphLodEnd = ri.Cvar_Get( "r_morphLodEnd", "2200", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_morphLodEnd, "0", "65536", CV_FLOAT );
	ri.Cvar_SetDescription( r_morphLodEnd, "Distance where IQM morph contribution reaches zero." );
	ri.Cvar_SetGroup( r_morphLodEnd, CVG_RENDERER );

	r_morphDebug = ri.Cvar_Get( "r_morphDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_morphDebug, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_morphDebug, "Show IQM morph influence debug coloring." );
	ri.Cvar_SetGroup( r_morphDebug, CVG_RENDERER );

	r_morphBreath = ri.Cvar_Get( "r_morphBreath", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_morphBreath, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_morphBreath, "Procedural demo: drive morph target named 'breath' on IQM entities." );
	ri.Cvar_SetGroup( r_morphBreath, CVG_RENDERER );

	r_morphBreathAmp = ri.Cvar_Get( "r_morphBreathAmp", "0.25", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_morphBreathAmp, "0", "2", CV_FLOAT );
	ri.Cvar_SetDescription( r_morphBreathAmp, "Procedural breath amplitude." );
	ri.Cvar_SetGroup( r_morphBreathAmp, CVG_RENDERER );

	r_morphBreathFreq = ri.Cvar_Get( "r_morphBreathFreq", "0.33", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_morphBreathFreq, "0.01", "8", CV_FLOAT );
	ri.Cvar_SetDescription( r_morphBreathFreq, "Procedural breath frequency in Hz." );
	ri.Cvar_SetGroup( r_morphBreathFreq, CVG_RENDERER );

	r_gltfAnim = ri.Cvar_Get( "r_gltfAnim", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gltfAnim, "0", "64", CV_FLOAT );
	ri.Cvar_SetDescription( r_gltfAnim, "glTF clip playback: multiplies refEntity shaderTime for skeletal TRS and morph-weight sampling (frame/oldframe index clips, backlerp crossfades)." );
	ri.Cvar_SetGroup( r_gltfAnim, CVG_RENDERER );

	r_gltfGpu = ri.Cvar_Get( "r_gltfGpu", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gltfGpu, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_gltfGpu,
		"Vulkan PBR: GPU vertex skinning and morph for glTF (joint matrix SSBO + morph deltas; top-8 morph weights per draw, incl. RE_SetEntityMorphWeight). Falls back to CPU tess when off or constraints fail." );
	ri.Cvar_SetGroup( r_gltfGpu, CVG_RENDERER );

	r_gltfGpuTangentFix = ri.Cvar_Get( "r_gltfGpuTangentFix", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gltfGpuTangentFix, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_gltfGpuTangentFix,
		"Vulkan PBR glTF GPU path: re-orthonormalize tangent (Gram–Schmidt) after joint skin + morph so T matches deformed N (0=bind-pose qtangent only, legacy)." );
	ri.Cvar_SetGroup( r_gltfGpuTangentFix, CVG_RENDERER );

	r_flares = ri.Cvar_Get ("r_flares", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_flares, "Enables corona effects on light sources." );
	r_znear = ri.Cvar_Get( "r_znear", "8", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_znear, "0.001", "200", CV_FLOAT );
	ri.Cvar_SetDescription( r_znear, "Viewport distance from view origin (how close objects can be to the player before they're clipped out of the scene)." );
	r_zproj = ri.Cvar_Get( "r_zproj", "64", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_zproj, "Projected viewport frustum." );
	r_stereoSeparation = ri.Cvar_Get( "r_stereoSeparation", "64", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_stereoSeparation, "Control eye separation. Resulting separation is \\r_zproj divided by this value in standard units." );
	r_firstPersonFov = ri.Cvar_Get( "r_firstPersonFov", "90", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_firstPersonFov, "1", "179", CV_FLOAT );
	ri.Cvar_SetDescription( r_firstPersonFov, "Horizontal field of view (degrees) for first-person primitives (arms, weapons). 0 = use scene FOV." );
	ri.Cvar_SetGroup( r_firstPersonFov, CVG_RENDERER );
	r_firstPersonScale = ri.Cvar_Get( "r_firstPersonScale", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_firstPersonScale, "0.1", "2.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_firstPersonScale, "Scale factor for first-person primitives toward camera (anti-clipping). 1.0 = no scale." );
	ri.Cvar_SetGroup( r_firstPersonScale, CVG_RENDERER );
	r_firstPersonFovEnabled = ri.Cvar_Get( "r_firstPersonFovEnabled", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_firstPersonFovEnabled, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_firstPersonFovEnabled, "0 = use scene FOV for first-person; 1 = use r_firstPersonFov." );
	ri.Cvar_SetGroup( r_firstPersonFovEnabled, CVG_RENDERER );
	r_firstPersonScaleEnabled = ri.Cvar_Get( "r_firstPersonScaleEnabled", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_firstPersonScaleEnabled, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_firstPersonScaleEnabled, "0 = no scale; 1 = apply r_firstPersonScale for anti-clipping." );
	ri.Cvar_SetGroup( r_firstPersonScaleEnabled, CVG_RENDERER );
	r_firstPersonZNear = ri.Cvar_Get( "r_firstPersonZNear", "0.5", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_firstPersonZNear, "0.01", "8", CV_FLOAT );
	ri.Cvar_SetDescription( r_firstPersonZNear, "Near clip plane for first-person primitives (arms, weapons). Smaller values reduce clipping of close geometry." );
	ri.Cvar_SetGroup( r_firstPersonZNear, CVG_RENDERER );
	r_ignoreGLErrors = ri.Cvar_Get( "r_ignoreGLErrors", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_ignoreGLErrors, "Ignore OpenGL errors." );
	r_teleporterFlash = ri.Cvar_Get( "r_teleporterFlash", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_teleporterFlash, "Show a white screen instead of a black screen when being teleported in hyperspace." );
	r_fastsky = ri.Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE_ND );
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
	r_mipLodBias = ri.Cvar_Get( "r_mipLodBias", "-0.75", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_mipLodBias, "-2.0", "2.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_mipLodBias, "Texture mip LOD bias (Vulkan): negative keeps sharper mips farther away, positive blurs sooner." );
	ri.Cvar_SetGroup( r_mipLodBias, CVG_RENDERER );
	r_gamma = ri.Cvar_Get( "r_gamma", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gamma, "0.5", "3", CV_FLOAT );
	ri.Cvar_SetDescription( r_gamma, "Gamma correction factor." );
	ri.Cvar_SetGroup( r_gamma, CVG_RENDERER );

	r_panini = ri.Cvar_Get( "r_panini", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_panini, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_panini, "Panini blend amount (0=perspective, 1=full panini)." );
	ri.Cvar_SetGroup( r_panini, CVG_RENDERER );

	r_panini_d = ri.Cvar_Get( "r_panini_d", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_panini_d, "0.5", "3.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_panini_d, "Panini distance parameter (higher is milder)." );
	ri.Cvar_SetGroup( r_panini_d, CVG_RENDERER );

	r_panini_s = ri.Cvar_Get( "r_panini_s", "0.25", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_panini_s, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_panini_s, "Panini vertical compression amount." );
	ri.Cvar_SetGroup( r_panini_s, CVG_RENDERER );

	r_panini_theta = ri.Cvar_Get( "r_panini_theta", "80.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_panini_theta, "60.0", "85.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_panini_theta, "Panini angular gate in degrees to prevent edge blow-up." );
	ri.Cvar_SetGroup( r_panini_theta, CVG_RENDERER );

	r_panini_zoom = ri.Cvar_Get( "r_panini_zoom", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_panini_zoom, "1.0", "2.5", CV_FLOAT );
	ri.Cvar_SetDescription( r_panini_zoom, "Panini output zoom/fill (1.0=no zoom, higher reduces edge stretch/OOB)." );
	ri.Cvar_SetGroup( r_panini_zoom, CVG_RENDERER );

	r_panini_border = ri.Cvar_Get( "r_panini_border", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_panini_border, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_panini_border, "Panini border mode: 0=black outside source, 1=clamp to edge." );
	ri.Cvar_SetGroup( r_panini_border, CVG_RENDERER );

	r_panini_debug = ri.Cvar_Get( "r_panini_debug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_panini_debug, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_panini_debug, "Panini debug: color invalid/OOB pixels." );
	ri.Cvar_SetGroup( r_panini_debug, CVG_RENDERER );

	r_paniniBrightness = ri.Cvar_Get( "r_paniniBrightness", "1.25", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_paniniBrightness, "0.5", "2.5", CV_FLOAT );
	ri.Cvar_SetDescription( r_paniniBrightness, "Multiplier applied after Panini warp (allows brightening the post-pass)." );
	ri.Cvar_SetGroup( r_paniniBrightness, CVG_RENDERER );

	r_paniniLensPreset = ri.Cvar_Get( "r_paniniLensPreset", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_paniniLensPreset, "0", "7", CV_INTEGER );
	ri.Cvar_SetDescription( r_paniniLensPreset,
		"Camera lens preset (auto-sets panini params):\n"
		" 0 - Off (manual params)\n"
		" 1 - GoPro Wide (d=1.0, s=0.2, fov=120, zoom=1.15)\n"
		" 2 - GoPro SuperView (d=1.2, s=0.35, fov=150, zoom=1.25)\n"
		" 3 - GoPro Linear (d=0.0, s=0.0, fov=90, zoom=1.0)\n"
		" 4 - Cinematic 24mm (d=0.3, s=0.05, fov=84, zoom=1.0)\n"
		" 5 - Cinematic 35mm (d=0.15, s=0.02, fov=63, zoom=1.0)\n"
		" 6 - Fisheye (d=1.5, s=0.5, fov=170, zoom=1.4)\n"
		" 7 - Security Cam (d=0.8, s=0.15, fov=110, zoom=1.1)\n" );
	ri.Cvar_SetGroup( r_paniniLensPreset, CVG_RENDERER );

	r_paniniBarrelDistortion = ri.Cvar_Get( "r_paniniBarrelDistortion", "0.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_paniniBarrelDistortion, "-1.0", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_paniniBarrelDistortion, "Barrel/pincushion distortion coefficient. Positive = barrel (GoPro-like), negative = pincushion." );
	ri.Cvar_SetGroup( r_paniniBarrelDistortion, CVG_RENDERER );

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
	r_shLighting = ri.Cvar_Get( "r_shLighting", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_shLighting, "Enable spherical harmonics ambient lighting for entity models." );
	r_shWorldLighting = ri.Cvar_Get( "r_shWorldLighting", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_shWorldLighting, "Apply spherical harmonics to world geometry (lightmapped surfaces)." );
	r_shDebugView = ri.Cvar_Get( "r_shDebugView", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_shDebugView, "Spherical harmonics debug view:\n 0: off\n 1: SH ambient only\n 2: SH coeff[0] grayscale\n 3: World-only solid SH override" );

	//r_anaglyphMode = ri.Cvar_Get( "r_anaglyphMode", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	//ri.Cvar_SetDescription( r_anaglyphMode, "Enable rendering of anaglyph images. Valid options for 3D glasses types:\n 0: Disabled\n 1: Red-cyan\n 2: Red-blue\n 3: Red-green\n 4: Green-magenta" );

	r_greyscale = ri.Cvar_Get( "r_greyscale", "0.0", CVAR_ARCHIVE_ND );
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
	ri.Cvar_Get( "r_font", "fonts/Inter-Regular.ttf", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_font", "fonts/Inter-Regular.ttf", CVAR_ARCHIVE ), "Custom TrueType font for UI text (e.g. fonts/Inter-Regular.ttf). Overrides 'fonts/default' when set. Empty = legacy bitmap font." );
	ri.Cvar_Get( "r_consoleFont", "", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_consoleFont", "", CVAR_ARCHIVE ), "Custom TrueType font for the console (e.g. fonts/consolefont.ttf). Empty = default." );
	ri.Cvar_Get( "r_fontSize", "16", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_fontSize", "16", CVAR_ARCHIVE ), "Point size for custom fonts loaded via r_font / r_consoleFont." );
	ri.Cvar_Get( "r_svgRasterScale", "1.0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_svgRasterScale", "1.0", CVAR_ARCHIVE ),
		"SVG rasterization scale factor (vector assets only). 1.0 = intrinsic size." );
	ri.Cvar_Get( "r_svgMaxRasterSize", "4096", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_svgMaxRasterSize", "4096", CVAR_ARCHIVE ),
		"Maximum rasterized SVG dimension in pixels. Prevents pathological memory usage." );
	ri.Cvar_Get( "r_svgMaxFileBytes", "2097152", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_svgMaxFileBytes", "2097152", CVAR_ARCHIVE ),
		"Maximum accepted SVG source file size in bytes." );
	r_outline = ri.Cvar_Get( "r_outline", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_outline, "Edge-detection outline strength (0 = off, 0.5 = subtle, 1.0 = strong)." );
	ri.Cvar_SetGroup( r_outline, CVG_RENDERER );
	r_outlineThreshold = ri.Cvar_Get( "r_outlineThreshold", "0.15", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_outlineThreshold, "Luminance edge threshold for outline detection." );
	ri.Cvar_SetGroup( r_outlineThreshold, CVG_RENDERER );
	ri.Cvar_Get( "r_safeMode", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_safeMode", "0", CVAR_ARCHIVE | CVAR_LATCH ), "Safe mode: disables post-processing, bloom, SSAO, volumetric fog. Use if the engine crashes on startup." );

	r_nocurves = ri.Cvar_Get ("r_nocurves", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_nocurves, "Set to 1 to disable drawing world bezier curves. Set to 0 to enable." );
	r_drawworld = ri.Cvar_Get ("r_drawworld", "1", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_drawworld, "Set to 0 to disable drawing the world. Set to 1 to enable." );
	r_lightmap = ri.Cvar_Get ("r_lightmap", "0", 0 );
	ri.Cvar_SetDescription( r_lightmap, "Show only lightmaps on all world surfaces." );
	r_portalOnly = ri.Cvar_Get ("r_portalOnly", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_portalOnly, "Set to 1 to render only first portal view if it is present on the scene." );

	r_flareSize = ri.Cvar_Get( "r_flareSize", "40", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_flareSize, "Radius of light flares. Requires \\r_flares 1." );
	ri.Cvar_CheckRange( r_flareSize, "1", "40", CV_FLOAT );

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
	r_occlusionCulling = ri.Cvar_Get ("r_occlusionCulling", "0", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_occlusionCulling, "GPU occlusion culling for entities (0=off, 1=on). Uses previous-frame visibility." );
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
	r_shownormals = ri.Cvar_Get( "r_shownormals", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_shownormals, "Debugging tool: Show wireframe surface normals." );
	r_clear = ri.Cvar_Get( "r_clear", "0", 0 );
	ri.Cvar_SetDescription( r_clear, "Forces screen buffer clearing every frame, removing any hall of mirrors effect in void.\n Use \\r_clearColor to set color." );
	r_offsetFactor = ri.Cvar_Get( "r_offsetFactor", "-2", CVAR_CHEAT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_offsetFactor, "Offset factor for shaders with polygonOffset stages." );
	r_offsetUnits = ri.Cvar_Get( "r_offsetunits", "-1", CVAR_CHEAT | CVAR_LATCH );
	ri.Cvar_SetDescription( r_offsetUnits, "Offset units for shaders with polygonOffset stages." );
	{
		cvar_t *sv = ri.Cvar_Get( "r_shadowVolumeOffsetFactor", "1", CVAR_ARCHIVE_ND );
		ri.Cvar_SetDescription( sv, "Depth bias factor for stencil shadow volumes (reduces thin black lines; push forward)." );
	}
	{
		cvar_t *sv = ri.Cvar_Get( "r_shadowVolumeOffsetUnits", "1", CVAR_ARCHIVE_ND );
		ri.Cvar_SetDescription( sv, "Depth bias units for stencil shadow volumes." );
	}
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

	r_bloomKnee = ri.Cvar_Get( "r_bloomKnee", "0.5", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_bloomKnee, "0.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_bloomKnee, "Soft knee for the bloom extractor to control highlight rolloff." );
	ri.Cvar_SetGroup( r_bloomKnee, CVG_RENDERER );

	r_exposure = ri.Cvar_Get( "r_exposure", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_exposure, "0.01", "10.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_exposure, "Linear exposure multiplier applied before tonemapping." );
	ri.Cvar_SetGroup( r_exposure, CVG_RENDERER );

	r_hdr_lightmap_scale = ri.Cvar_Get( "r_hdr_lightmap_scale", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hdr_lightmap_scale, "0.5", "8.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_hdr_lightmap_scale, "HDR lightmap intensity scale. 8-bit lightmaps multiplied by this for HDR-like brightness (1=normal, 2+=brighter)." );
	ri.Cvar_SetGroup( r_hdr_lightmap_scale, CVG_RENDERER );

	r_lightmap_srgb_decode = ri.Cvar_Get( "r_lightmap_srgb_decode", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_lightmap_srgb_decode, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_lightmap_srgb_decode, "When r_hdr 1/2: 0=lightmaps assumed linear (default), 1=sRGB->linear decode for gamma-encoded BSP lightmaps (q3map2 -gamma)." );
	ri.Cvar_SetGroup( r_lightmap_srgb_decode, CVG_RENDERER );

	r_pre_exposure_scale = ri.Cvar_Get( "r_pre_exposure_scale", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pre_exposure_scale, "0.1", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_pre_exposure_scale, "Pre-exposure scale for bloom/tonemap pipeline. 1.0=neutral; use for HDR pipeline tweaks." );
	ri.Cvar_SetGroup( r_pre_exposure_scale, CVG_RENDERER );

	r_exposure_auto = ri.Cvar_Get( "r_exposure_auto", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_exposure_auto, "Eye adaptation: 0=manual r_exposure, 1=weighted percentile metering with temporal adaptation." );
	ri.Cvar_SetGroup( r_exposure_auto, CVG_RENDERER );
	{
		cvar_t *exp_target = ri.Cvar_Get( "r_exposure_auto_target", "1.0", CVAR_ARCHIVE_ND );
		cvar_t *exp_speed = ri.Cvar_Get( "r_exposure_auto_speed", "2.0", CVAR_ARCHIVE_ND );
		cvar_t *exp_cap_cut = ri.Cvar_Get( "r_exposure_auto_cap_on_cut", "1.35", CVAR_ARCHIVE_ND );
		cvar_t *exp_low_percent = ri.Cvar_Get( "r_autoExposure_lowPercent", "0.02", CVAR_ARCHIVE_ND );
		cvar_t *exp_high_percent = ri.Cvar_Get( "r_autoExposure_highPercent", "0.01", CVAR_ARCHIVE_ND );
		cvar_t *exp_center_weight = ri.Cvar_Get( "r_autoExposure_centerWeight", "0.60", CVAR_ARCHIVE_ND );
		cvar_t *exp_speed_up = ri.Cvar_Get( "r_autoExposure_speedUp", "1.5", CVAR_ARCHIVE_ND );
		cvar_t *exp_speed_down = ri.Cvar_Get( "r_autoExposure_speedDown", "3.0", CVAR_ARCHIVE_ND );
		cvar_t *exp_min = ri.Cvar_Get( "r_autoExposure_min", "0.5", CVAR_ARCHIVE_ND );
		cvar_t *exp_max = ri.Cvar_Get( "r_autoExposure_max", "4.0", CVAR_ARCHIVE_ND );
		ri.Cvar_CheckRange( exp_cap_cut, "0.1", "2.0", CV_FLOAT );
		ri.Cvar_CheckRange( exp_low_percent, "0.0", "0.45", CV_FLOAT );
		ri.Cvar_CheckRange( exp_high_percent, "0.0", "0.45", CV_FLOAT );
		ri.Cvar_CheckRange( exp_center_weight, "0.0", "1.5", CV_FLOAT );
		ri.Cvar_CheckRange( exp_speed_up, "0.1", "10.0", CV_FLOAT );
		ri.Cvar_CheckRange( exp_speed_down, "0.1", "10.0", CV_FLOAT );
		ri.Cvar_CheckRange( exp_min, "0.05", "8.0", CV_FLOAT );
		ri.Cvar_CheckRange( exp_max, "0.05", "16.0", CV_FLOAT );
		ri.Cvar_SetDescription( exp_target, "Target exposure for eye adaptation (r_exposure_auto 1)." );
		ri.Cvar_SetDescription( exp_speed, "Legacy eye adaptation speed control kept for compatibility." );
		ri.Cvar_SetDescription( exp_cap_cut, "Max exposure on camera cut (e.g. death). Reduces blowout when view suddenly jumps to bright sky. 0=disable cap." );
		ri.Cvar_SetDescription( exp_low_percent, "Low-end percentile discarded by auto exposure metering." );
		ri.Cvar_SetDescription( exp_high_percent, "High-end percentile discarded by auto exposure metering." );
		ri.Cvar_SetDescription( exp_center_weight, "Extra center bias for auto exposure metering." );
		ri.Cvar_SetDescription( exp_speed_up, "How quickly exposure brightens when entering dark areas." );
		ri.Cvar_SetDescription( exp_speed_down, "How quickly exposure darkens when entering bright areas." );
		ri.Cvar_SetDescription( exp_min, "Minimum auto exposure clamp." );
		ri.Cvar_SetDescription( exp_max, "Maximum auto exposure clamp." );
		ri.Cvar_SetGroup( exp_low_percent, CVG_RENDERER );
		ri.Cvar_SetGroup( exp_high_percent, CVG_RENDERER );
		ri.Cvar_SetGroup( exp_center_weight, CVG_RENDERER );
		ri.Cvar_SetGroup( exp_speed_up, CVG_RENDERER );
		ri.Cvar_SetGroup( exp_speed_down, CVG_RENDERER );
		ri.Cvar_SetGroup( exp_min, CVG_RENDERER );
		ri.Cvar_SetGroup( exp_max, CVG_RENDERER );
	}

	{
		cvar_t *local_exp = ri.Cvar_Get( "r_localExposure", "1", CVAR_ARCHIVE_ND );
		cvar_t *local_exp_strength = ri.Cvar_Get( "r_localExposure_strength", "0.35", CVAR_ARCHIVE_ND );
		cvar_t *local_exp_shadow = ri.Cvar_Get( "r_localExposure_shadowClamp", "1.5", CVAR_ARCHIVE_ND );
		cvar_t *local_exp_highlight = ri.Cvar_Get( "r_localExposure_highlightClamp", "1.5", CVAR_ARCHIVE_ND );
		ri.Cvar_CheckRange( local_exp, "0", "1", CV_INTEGER );
		ri.Cvar_CheckRange( local_exp_strength, "0.0", "1.0", CV_FLOAT );
		ri.Cvar_CheckRange( local_exp_shadow, "0.0", "3.0", CV_FLOAT );
		ri.Cvar_CheckRange( local_exp_highlight, "0.0", "3.0", CV_FLOAT );
		ri.Cvar_SetDescription( local_exp, "Local exposure compensation in the Vulkan tonemap pass." );
		ri.Cvar_SetDescription( local_exp_strength, "Strength of local exposure compensation." );
		ri.Cvar_SetDescription( local_exp_shadow, "Maximum brightening in EV for dark local regions." );
		ri.Cvar_SetDescription( local_exp_highlight, "Maximum darkening in EV for bright local regions." );
		ri.Cvar_SetGroup( local_exp, CVG_RENDERER );
		ri.Cvar_SetGroup( local_exp_strength, CVG_RENDERER );
		ri.Cvar_SetGroup( local_exp_shadow, CVG_RENDERER );
		ri.Cvar_SetGroup( local_exp_highlight, CVG_RENDERER );
	}

	{
		cvar_t *bloom_scatter = ri.Cvar_Get( "r_bloom_scatter", "0.72", CVAR_ARCHIVE_ND );
		cvar_t *bloom_energy = ri.Cvar_Get( "r_bloom_energyPreserve", "1", CVAR_ARCHIVE_ND );
		ri.Cvar_CheckRange( bloom_scatter, "0.1", "1.0", CV_FLOAT );
		ri.Cvar_CheckRange( bloom_energy, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( bloom_scatter, "Bloom falloff between mip levels. Lower values keep the glow tighter." );
		ri.Cvar_SetDescription( bloom_energy, "Normalize bloom mip weights to keep highlight energy more stable." );
		ri.Cvar_SetGroup( bloom_scatter, CVG_RENDERER );
		ri.Cvar_SetGroup( bloom_energy, CVG_RENDERER );
	}

	r_tonemap = ri.Cvar_Get( "r_tonemap", "3", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_tonemap, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_tonemap, "Tonemapping: 0=off, 1=Reinhard, 2=ACES, 3=Filmic (Hable/Uncharted2, default), 4=AgX (punchy, saturated)." );
	ri.Cvar_SetGroup( r_tonemap, CVG_RENDERER );

	r_post = ri.Cvar_Get( "r_post", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_post, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_post, "Toggle the HDR post-processing pipeline (1=tonemap + gamma pass, 0=pass-through)." );
	ri.Cvar_SetGroup( r_post, CVG_RENDERER );

	r_post_debug = ri.Cvar_Get( "r_post_debug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_post_debug, "0", "99", CV_INTEGER );
	ri.Cvar_SetDescription( r_post_debug, "Debug view for the post-process pass: 0=final, 1=pre-tonemap HDR, 2=luminance heatmap, 97=panini logical UV, 98=panini remapped source UV, 99=panini logical OOB mask." );
	ri.Cvar_SetGroup( r_post_debug, CVG_RENDERER );

	{
		cvar_t *r_post_contrast = ri.Cvar_Get( "r_post_contrast", "1.0", CVAR_ARCHIVE_ND );
		cvar_t *r_post_saturation = ri.Cvar_Get( "r_post_saturation", "1.0", CVAR_ARCHIVE_ND );
		ri.Cvar_CheckRange( r_post_contrast, "0.25", "4.0", CV_FLOAT );
		ri.Cvar_CheckRange( r_post_saturation, "0.0", "3.0", CV_FLOAT );
		ri.Cvar_SetDescription( r_post_contrast, "Post-process contrast (1.0=neutral, >1=punchier, <1=flatter)." );
		ri.Cvar_SetDescription( r_post_saturation, "Post-process saturation (1.0=neutral, >1=vivid, <1=desaturated)." );
		ri.Cvar_SetGroup( r_post_contrast, CVG_RENDERER );
		ri.Cvar_SetGroup( r_post_saturation, CVG_RENDERER );
	}

	r_rpi_profile = ri.Cvar_Get( "r_rpi_profile", "0", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_rpi_profile, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_rpi_profile, "Raspberry Pi (V3DV) performance preset. When 1, disables SSAO, volumetric fog, bloom, SMAA, SSR, fog fluid at Vulkan init. Requires vid_restart." );
	ri.Cvar_SetGroup( r_rpi_profile, CVG_RENDERER );

	r_volumetricFog = ri.Cvar_Get( "r_volumetricFog", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFog, "Enable the volumetric fog compute/composite passes before tonemapping. Requires r_fbo 1." );
	ri.Cvar_SetGroup( r_volumetricFog, CVG_RENDERER );

	r_volumetricFogDensity = ri.Cvar_Get( "r_volumetricFogDensity", "0.6", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogDensity, "0", "5", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogDensity, "Fog density multiplier for the volumetric fog pass." );
	ri.Cvar_SetGroup( r_volumetricFogDensity, CVG_RENDERER );

	r_volumetricFogHeightFalloff = ri.Cvar_Get( "r_volumetricFogHeightFalloff", "0.015", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogHeightFalloff, "0", "5", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogHeightFalloff, "Controls vertical falloff of density for the height fog component." );
	ri.Cvar_SetGroup( r_volumetricFogHeightFalloff, CVG_RENDERER );

	r_volumetricFogAlbedo = ri.Cvar_Get( "r_volumetricFogAlbedo", "0.95", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogAlbedo, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogAlbedo, "Single-scatter albedo (0=absorbing, 1=fully scattering). Controls scatter vs absorption ratio." );
	ri.Cvar_SetGroup( r_volumetricFogAlbedo, CVG_RENDERER );

	r_volumetricFogExtinctionScale = ri.Cvar_Get( "r_volumetricFogExtinctionScale", "0.65", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogExtinctionScale, "0.1", "10", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogExtinctionScale, "Scale for extinction coefficient. Multiplies density for beam attenuation." );
	ri.Cvar_SetGroup( r_volumetricFogExtinctionScale, CVG_RENDERER );

	r_volumetricFogBlendDistance = ri.Cvar_Get( "r_volumetricFogBlendDistance", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogBlendDistance, "0", "256", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogBlendDistance, "Distance from volume bounds over which density blends (0=hard edge)." );
	ri.Cvar_SetGroup( r_volumetricFogBlendDistance, CVG_RENDERER );

	r_volumetricFogSphere = ri.Cvar_Get( "r_volumetricFogSphere", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogSphere, "Enable debug sphere fog volume (1=on)." );
	ri.Cvar_SetGroup( r_volumetricFogSphere, CVG_RENDERER );

	r_volumetricFogSphereCenter = ri.Cvar_Get( "r_volumetricFogSphereCenter", "0 0 0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogSphereCenter, "Sphere fog center (x y z)." );
	ri.Cvar_SetGroup( r_volumetricFogSphereCenter, CVG_RENDERER );

	r_volumetricFogSphereRadius = ri.Cvar_Get( "r_volumetricFogSphereRadius", "128", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogSphereRadius, "1", "2048", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogSphereRadius, "Sphere fog radius." );
	ri.Cvar_SetGroup( r_volumetricFogSphereRadius, CVG_RENDERER );

	r_volumetricFogSphereDensity = ri.Cvar_Get( "r_volumetricFogSphereDensity", "0.01", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogSphereDensity, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogSphereDensity, "Sphere fog density." );
	ri.Cvar_SetGroup( r_volumetricFogSphereDensity, CVG_RENDERER );

	r_volumetricFogCylinder = ri.Cvar_Get( "r_volumetricFogCylinder", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogCylinder, "Enable debug cylinder fog volume (1=on)." );
	ri.Cvar_SetGroup( r_volumetricFogCylinder, CVG_RENDERER );

	r_volumetricFogCylinderBase = ri.Cvar_Get( "r_volumetricFogCylinderBase", "0 0 0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogCylinderBase, "Cylinder base center (x y z)." );
	ri.Cvar_SetGroup( r_volumetricFogCylinderBase, CVG_RENDERER );

	r_volumetricFogCylinderTop = ri.Cvar_Get( "r_volumetricFogCylinderTop", "0 0 128", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogCylinderTop, "Cylinder top center (x y z)." );
	ri.Cvar_SetGroup( r_volumetricFogCylinderTop, CVG_RENDERER );

	r_volumetricFogCylinderRadius = ri.Cvar_Get( "r_volumetricFogCylinderRadius", "64", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogCylinderRadius, "1", "1024", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogCylinderRadius, "Cylinder radius." );
	ri.Cvar_SetGroup( r_volumetricFogCylinderRadius, CVG_RENDERER );

	r_volumetricFogCylinderDensity = ri.Cvar_Get( "r_volumetricFogCylinderDensity", "0.01", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogCylinderDensity, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogCylinderDensity, "Cylinder fog density." );
	ri.Cvar_SetGroup( r_volumetricFogCylinderDensity, CVG_RENDERER );

	r_volumetricFogCone = ri.Cvar_Get( "r_volumetricFogCone", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogCone, "Enable debug cone fog volume (1=on)." );
	ri.Cvar_SetGroup( r_volumetricFogCone, CVG_RENDERER );

	r_volumetricFogConeApex = ri.Cvar_Get( "r_volumetricFogConeApex", "0 0 64", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogConeApex, "Cone apex (x y z)." );
	ri.Cvar_SetGroup( r_volumetricFogConeApex, CVG_RENDERER );

	r_volumetricFogConeBase = ri.Cvar_Get( "r_volumetricFogConeBase", "0 0 0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogConeBase, "Cone base center (x y z)." );
	ri.Cvar_SetGroup( r_volumetricFogConeBase, CVG_RENDERER );

	r_volumetricFogConeRadius = ri.Cvar_Get( "r_volumetricFogConeRadius", "96", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogConeRadius, "1", "1024", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogConeRadius, "Cone base radius." );
	ri.Cvar_SetGroup( r_volumetricFogConeRadius, CVG_RENDERER );

	r_volumetricFogConeDensity = ri.Cvar_Get( "r_volumetricFogConeDensity", "0.01", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogConeDensity, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogConeDensity, "Cone fog density." );
	ri.Cvar_SetGroup( r_volumetricFogConeDensity, CVG_RENDERER );

	r_volumetricFogDenoise = ri.Cvar_Get( "r_volumetricFogDenoise", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogDenoise, "Enable Gaussian spatial denoise on volumetric fog (0=off, 1=on)." );
	ri.Cvar_SetGroup( r_volumetricFogDenoise, CVG_RENDERER );

	r_volumetricFogDenoiseSigma = ri.Cvar_Get( "r_volumetricFogDenoiseSigma", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogDenoiseSigma, "0.1", "3", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogDenoiseSigma, "Gaussian sigma for volumetric denoise (spatial blur strength)." );
	ri.Cvar_SetGroup( r_volumetricFogDenoiseSigma, CVG_RENDERER );

	r_volumetricFogAniso = ri.Cvar_Get( "r_volumetricFogAniso", "0.6", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogAniso, "-0.999", "0.999", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogAniso, "Henyey-Greenstein anisotropy factor (positive = forward scattering, negative = backward)." );
	ri.Cvar_SetGroup( r_volumetricFogAniso, CVG_RENDERER );

	r_volumetricFogSteps = ri.Cvar_Get( "r_volumetricFogSteps", "32", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogSteps, "1", "256", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogSteps, "Raymarch steps per pixel when compositing volumetric fog." );
	ri.Cvar_SetGroup( r_volumetricFogSteps, CVG_RENDERER );

	r_volumetricFogZExponent = ri.Cvar_Get( "r_volumetricFogZExponent", "1.5", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogZExponent, "1.0", "8.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogZExponent, "Exponent used for volumetric depth-slice distribution (higher values allocate more slices near camera)." );
	ri.Cvar_SetGroup( r_volumetricFogZExponent, CVG_RENDERER );
	r_volumetricFogSliceMode = ri.Cvar_Get( "r_volumetricFogSliceMode", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogSliceMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogSliceMode, "Slice distribution: 0=exponential (more near camera), 1=linear (equal spacing), 2=logarithmic (more in distance)." );
	ri.Cvar_SetGroup( r_volumetricFogSliceMode, CVG_RENDERER );

	r_volumetricFogMaxDistance = ri.Cvar_Get( "r_volumetricFogMaxDistance", "4096", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogMaxDistance, "1", "65536", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogMaxDistance, "Maximum integration distance for volumetric fog in world units." );
	ri.Cvar_SetGroup( r_volumetricFogMaxDistance, CVG_RENDERER );

	r_volumetricFogJitter = ri.Cvar_Get( "r_volumetricFogJitter", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogJitter, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogJitter, "Introduces sub-pixel jitter to the fog raymarch samples." );
	ri.Cvar_SetGroup( r_volumetricFogJitter, CVG_RENDERER );

	r_volumetricFogTemporalWeight = ri.Cvar_Get( "r_volumetricFogTemporalWeight", "0.72", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogTemporalWeight, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogTemporalWeight, "History blend weight for temporal reprojection (0 = no history)." );
	ri.Cvar_SetGroup( r_volumetricFogTemporalWeight, CVG_RENDERER );

	r_volumetricFogReprojectionThreshold = ri.Cvar_Get( "r_volumetricFogReprojectionThreshold", "0.075", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogReprojectionThreshold, "0", "2", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogReprojectionThreshold, "Reject history when reprojection motion exceeds this screen-space threshold." );
	ri.Cvar_SetGroup( r_volumetricFogReprojectionThreshold, CVG_RENDERER );

	r_volumetricFogHistoryVelocityThreshold = ri.Cvar_Get( "r_volumetricFogHistoryVelocityThreshold", "0.075", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogHistoryVelocityThreshold, "0", "2", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogHistoryVelocityThreshold, "Reject history when per-pixel motion velocity magnitude exceeds this screen-space threshold." );
	ri.Cvar_SetGroup( r_volumetricFogHistoryVelocityThreshold, CVG_RENDERER );

	r_volumetricFogFireflyClamp = ri.Cvar_Get( "r_volumetricFogFireflyClamp", "8.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogFireflyClamp, "0", "128", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogFireflyClamp, "Optional luminance clamp used to suppress temporal fireflies (0 disables)." );
	ri.Cvar_SetGroup( r_volumetricFogFireflyClamp, CVG_RENDERER );

	r_volumetricFogColorMode = ri.Cvar_Get( "r_volumetricFogColorMode", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogColorMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogColorMode, "Volumetric fog color source: 0=map fog volume (fallback sun tint), 1=use r_volumetricFogTint, 2=use nearest IBL cubemap SH (fallback mode 0)." );
	ri.Cvar_SetGroup( r_volumetricFogColorMode, CVG_RENDERER );

	r_volumetricFogTint = ri.Cvar_Get( "r_volumetricFogTint", "1.08 1.00 0.72", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogTint, "Volumetric fog RGB tint (3 floats). Applied as a multiplier in modes 0 and 2, or used directly in mode 1." );
	ri.Cvar_SetGroup( r_volumetricFogTint, CVG_RENDERER );

	r_volumetricFogIntensity = ri.Cvar_Get( "r_volumetricFogIntensity", "1.5", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogIntensity, "0", "50", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogIntensity, "Scattering intensity multiplier for volumetric fog color (useful to brighten tints/IBL contribution)." );
	ri.Cvar_SetGroup( r_volumetricFogIntensity, CVG_RENDERER );

	r_volumetricFogQuality = ri.Cvar_Get( "r_volumetricFogQuality", "2", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_volumetricFogQuality, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogQuality, "Volumetric quality tier: 0=low, 1=medium, 2=high, 3=ultra. Requires vid_restart." );
	ri.Cvar_SetGroup( r_volumetricFogQuality, CVG_RENDERER );

	r_volumetricFogResolutionScale = ri.Cvar_Get( "r_volumetricFogResolutionScale", "1.0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_volumetricFogResolutionScale, "0.25", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogResolutionScale, "Scales volumetric froxel XY resolution before quality tiering (0.25-1.0). Requires vid_restart." );
	ri.Cvar_SetGroup( r_volumetricFogResolutionScale, CVG_RENDERER );

	r_volumetricFogTransmittanceCutoff = ri.Cvar_Get( "r_volumetricFogTransmittanceCutoff", "0.01", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogTransmittanceCutoff, "0.0001", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogTransmittanceCutoff, "Early-out threshold for volumetric integration (smaller values = higher quality)." );
	ri.Cvar_SetGroup( r_volumetricFogTransmittanceCutoff, CVG_RENDERER );

	r_volumetricFogBaseHeight = ri.Cvar_Get( "r_volumetricFogBaseHeight", "0.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogBaseHeight, "-8192", "8192", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogBaseHeight, "World-space Z reference height for the volumetric fog height falloff (0 = world origin)." );
	ri.Cvar_SetGroup( r_volumetricFogBaseHeight, CVG_RENDERER );

	r_volumetricFogWorldMin = ri.Cvar_Get( "r_volumetricFogWorldMin", "-2048 -2048 -256", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogWorldMin, "World-space fog AABB minimum corner (x y z)." );
	ri.Cvar_SetGroup( r_volumetricFogWorldMin, CVG_RENDERER );

	r_volumetricFogWorldMax = ri.Cvar_Get( "r_volumetricFogWorldMax", "2048 2048 1024", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogWorldMax, "World-space fog AABB maximum corner (x y z)." );
	ri.Cvar_SetGroup( r_volumetricFogWorldMax, CVG_RENDERER );

	r_volumetricFogGridDim = ri.Cvar_Get( "r_volumetricFogGridDim", "160 90 96", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_volumetricFogGridDim, "World-space froxel grid dimensions (x y z). Requires vid_restart." );
	ri.Cvar_SetGroup( r_volumetricFogGridDim, CVG_RENDERER );

	r_volumetricFogDepthMode = ri.Cvar_Get( "r_volumetricFogDepthMode", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogDepthMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogDepthMode, "Volumetric depth decode mode: 0=standard 0..1, 1=reversed-Z 0..1, 2=linear viewZ packed." );
	ri.Cvar_SetGroup( r_volumetricFogDepthMode, CVG_RENDERER );

	r_volumetricFogSunIntensity = ri.Cvar_Get( "r_volumetricFogSunIntensity", "1.25", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogSunIntensity, "0", "64", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogSunIntensity, "Directional light intensity used by world-space volumetric scattering." );
	ri.Cvar_SetGroup( r_volumetricFogSunIntensity, CVG_RENDERER );

	r_volumetricFogAmbientIntensity = ri.Cvar_Get( "r_volumetricFogAmbientIntensity", "1.25", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogAmbientIntensity, "0", "64", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogAmbientIntensity, "Ambient light intensity used by world-space volumetric scattering." );
	ri.Cvar_SetGroup( r_volumetricFogAmbientIntensity, CVG_RENDERER );

	r_volumetricFogNoiseDim = ri.Cvar_Get( "r_volumetricFogNoiseDim", "64", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_volumetricFogNoiseDim, "8", "128", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogNoiseDim, "3D noise texture dimension for volumetric fog modulation. Requires vid_restart." );
	ri.Cvar_SetGroup( r_volumetricFogNoiseDim, CVG_RENDERER );

	r_volumetricFogNoiseScale = ri.Cvar_Get( "r_volumetricFogNoiseScale", "0.0125", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogNoiseScale, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogNoiseScale, "World-to-noise UV scale for volumetric fog." );
	ri.Cvar_SetGroup( r_volumetricFogNoiseScale, CVG_RENDERER );

	r_volumetricFogNoiseStrength = ri.Cvar_Get( "r_volumetricFogNoiseStrength", "0.85", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogNoiseStrength, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogNoiseStrength, "How strongly 3D noise modulates volumetric density (0=off, 1=full)." );
	ri.Cvar_SetGroup( r_volumetricFogNoiseStrength, CVG_RENDERER );

	r_volumetricFogNoiseThreshold = ri.Cvar_Get( "r_volumetricFogNoiseThreshold", "0.2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogNoiseThreshold, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogNoiseThreshold, "Threshold applied to 3D noise before density modulation." );
	ri.Cvar_SetGroup( r_volumetricFogNoiseThreshold, CVG_RENDERER );

	r_volumetricFogNoiseScroll = ri.Cvar_Get( "r_volumetricFogNoiseScroll", "0.03 0.01 0.02", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogNoiseScroll, "3D noise scroll velocity (x y z) for volumetric fog movement." );
	ri.Cvar_SetGroup( r_volumetricFogNoiseScroll, CVG_RENDERER );

	r_volumetricFogWindSpeed = ri.Cvar_Get( "r_volumetricFogWindSpeed", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogWindSpeed, "0", "64", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogWindSpeed, "World-space wind speed multiplier for animated volumetric noise advection." );
	ri.Cvar_SetGroup( r_volumetricFogWindSpeed, CVG_RENDERER );

	r_volumetricFogWindDirection = ri.Cvar_Get( "r_volumetricFogWindDirection", "1 0 0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_volumetricFogWindDirection, "World-space wind direction for volumetric noise advection (x y z)." );
	ri.Cvar_SetGroup( r_volumetricFogWindDirection, CVG_RENDERER );

	r_fogFluid = ri.Cvar_Get( "r_fogFluid", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluid, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_fogFluid, "Enable 2D fluid-driven advection for volumetric fog extinction/scattering." );
	ri.Cvar_SetGroup( r_fogFluid, CVG_RENDERER );

	r_fogFluidQuality = ri.Cvar_Get( "r_fogFluidQuality", "2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidQuality, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_fogFluidQuality, "Fluid quality tier: 0=low, 1=medium, 2=high, 3=ultra." );
	ri.Cvar_SetGroup( r_fogFluidQuality, CVG_RENDERER );

	r_fogFluidResolutionScale = ri.Cvar_Get( "r_fogFluidResolutionScale", "0.5", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_fogFluidResolutionScale, "0.125", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogFluidResolutionScale, "Fluid XY grid scale relative to froxel XY resolution. Requires vid_restart." );
	ri.Cvar_SetGroup( r_fogFluidResolutionScale, CVG_RENDERER );

	r_fogFluidViscosity = ri.Cvar_Get( "r_fogFluidViscosity", "0.05", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidViscosity, "0.0", "10.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogFluidViscosity, "Velocity diffusion strength for the fluid solve." );
	ri.Cvar_SetGroup( r_fogFluidViscosity, CVG_RENDERER );

	r_fogFluidPressureIterations = ri.Cvar_Get( "r_fogFluidPressureIterations", "12", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidPressureIterations, "1", "64", CV_INTEGER );
	ri.Cvar_SetDescription( r_fogFluidPressureIterations, "Jacobi iterations for incompressibility projection." );
	ri.Cvar_SetGroup( r_fogFluidPressureIterations, CVG_RENDERER );

	r_fogFluidDissipation = ri.Cvar_Get( "r_fogFluidDissipation", "0.985", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidDissipation, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogFluidDissipation, "Per-frame density retention (1=no loss)." );
	ri.Cvar_SetGroup( r_fogFluidDissipation, CVG_RENDERER );

	r_fogFluidForceScale = ri.Cvar_Get( "r_fogFluidForceScale", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidForceScale, "0.0", "128.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogFluidForceScale, "External force scale applied to fluid velocity from wind/noise impulses." );
	ri.Cvar_SetGroup( r_fogFluidForceScale, CVG_RENDERER );

	r_fogFluidWrap = ri.Cvar_Get( "r_fogFluidWrap", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidWrap, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_fogFluidWrap, "Fluid boundary mode: 0=zero velocity walls, 1=wrap." );
	ri.Cvar_SetGroup( r_fogFluidWrap, CVG_RENDERER );

	r_fogFluidVelocityClamp = ri.Cvar_Get( "r_fogFluidVelocityClamp", "96.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidVelocityClamp, "1.0", "4096.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogFluidVelocityClamp, "Maximum velocity magnitude clamp to avoid simulation blow-ups." );
	ri.Cvar_SetGroup( r_fogFluidVelocityClamp, CVG_RENDERER );

	r_fogFluidAutoScale = ri.Cvar_Get( "r_fogFluidAutoScale", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidAutoScale, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_fogFluidAutoScale, "Auto-adjust fluid pressure iterations and effective simulation resolution to keep GPU time in budget." );
	ri.Cvar_SetGroup( r_fogFluidAutoScale, CVG_RENDERER );

	r_fogFluidTargetMs = ri.Cvar_Get( "r_fogFluidTargetMs", "1.2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidTargetMs, "0.1", "8.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogFluidTargetMs, "Target GPU milliseconds budget for fluid simulation auto-scaling." );
	ri.Cvar_SetGroup( r_fogFluidTargetMs, CVG_RENDERER );

	r_fogFluidAutoScaleRate = ri.Cvar_Get( "r_fogFluidAutoScaleRate", "0.08", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidAutoScaleRate, "0.01", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogFluidAutoScaleRate, "Adaptation speed for fluid auto-scaling (higher reacts faster)." );
	ri.Cvar_SetGroup( r_fogFluidAutoScaleRate, CVG_RENDERER );

	r_fogFluidAutoScaleMinResolution = ri.Cvar_Get( "r_fogFluidAutoScaleMinResolution", "0.45", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidAutoScaleMinResolution, "0.125", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogFluidAutoScaleMinResolution, "Minimum effective internal fluid resolution multiplier used by auto-scaling." );
	ri.Cvar_SetGroup( r_fogFluidAutoScaleMinResolution, CVG_RENDERER );

	r_fogFluidAutoScaleMinIterations = ri.Cvar_Get( "r_fogFluidAutoScaleMinIterations", "6", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidAutoScaleMinIterations, "1", "64", CV_INTEGER );
	ri.Cvar_SetDescription( r_fogFluidAutoScaleMinIterations, "Minimum pressure iterations allowed by fluid auto-scaling." );
	ri.Cvar_SetGroup( r_fogFluidAutoScaleMinIterations, CVG_RENDERER );

	r_fogFluidFlowFieldStrength = ri.Cvar_Get( "r_fogFluidFlowFieldStrength", "0.35", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidFlowFieldStrength, "0.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogFluidFlowFieldStrength, "Art-driven noise flow influence strength when advecting fog with the fluid field." );
	ri.Cvar_SetGroup( r_fogFluidFlowFieldStrength, CVG_RENDERER );

	r_fogFluidFlowFieldScale = ri.Cvar_Get( "r_fogFluidFlowFieldScale", "0.004", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidFlowFieldScale, "0.0001", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogFluidFlowFieldScale, "World-space scale for the art-driven flow field sampled from 3D fog noise." );
	ri.Cvar_SetGroup( r_fogFluidFlowFieldScale, CVG_RENDERER );

	r_fogFluidVorticity = ri.Cvar_Get( "r_fogFluidVorticity", "0.3", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidVorticity, "0.0", "2.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogFluidVorticity, "Vorticity confinement strength for fluid simulation." );
	ri.Cvar_SetGroup( r_fogFluidVorticity, CVG_RENDERER );

	r_fogFluidBuoyancy = ri.Cvar_Get( "r_fogFluidBuoyancy", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogFluidBuoyancy, "0.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogFluidBuoyancy, "Buoyancy force strength for fluid simulation." );
	ri.Cvar_SetGroup( r_fogFluidBuoyancy, CVG_RENDERER );

	r_volumetricFogValidation = ri.Cvar_Get( "r_volumetricFogValidation", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_volumetricFogValidation, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogValidation, "Enable periodic runtime validation logging for volumetric fog checklist checks." );
	ri.Cvar_SetGroup( r_volumetricFogValidation, CVG_RENDERER );

	r_volumetricFogValidationPrintInterval = ri.Cvar_Get( "r_volumetricFogValidationPrintInterval", "120", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogValidationPrintInterval, "1", "6000", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogValidationPrintInterval, "Frame interval between runtime volumetric validation log lines when r_volumetricFogValidation is enabled." );
	ri.Cvar_SetGroup( r_volumetricFogValidationPrintInterval, CVG_RENDERER );

	r_volumetricFogForceCameraCut = ri.Cvar_Get( "r_volumetricFogForceCameraCut", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_volumetricFogForceCameraCut, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogForceCameraCut, "One-shot debug trigger to force a volumetric camera-cut history reset on the next frame." );
	ri.Cvar_SetGroup( r_volumetricFogForceCameraCut, CVG_RENDERER );

	r_volumetricFogSkipStatic = ri.Cvar_Get( "r_volumetricFogSkipStatic", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogSkipStatic, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogSkipStatic, "Skip volumetric fog when view is nearly static (e.g. death cam). Prevents gradient/streak artifacts. 0=always run fog." );
	ri.Cvar_SetGroup( r_volumetricFogSkipStatic, CVG_RENDERER );

	r_volumetricFogPerfTimers = ri.Cvar_Get( "r_volumetricFogPerfTimers", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogPerfTimers, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogPerfTimers, "Enable Vulkan timestamp profiling for volumetric fog stages and fluid auto-scaling." );
	ri.Cvar_SetGroup( r_volumetricFogPerfTimers, CVG_RENDERER );

	r_volumetricFogPerfPrintInterval = ri.Cvar_Get( "r_volumetricFogPerfPrintInterval", "120", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogPerfPrintInterval, "1", "6000", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogPerfPrintInterval, "Frame interval between printed volumetric performance timer lines when perf timers are enabled." );
	ri.Cvar_SetGroup( r_volumetricFogPerfPrintInterval, CVG_RENDERER );

	r_volumetricFogTemporalStability = ri.Cvar_Get( "r_volumetricFogTemporalStability", "0.7", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogTemporalStability, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogTemporalStability, "Additional temporal stabilization strength for volumetric history blending." );
	ri.Cvar_SetGroup( r_volumetricFogTemporalStability, CVG_RENDERER );

	r_volumetricFogShadowContrast = ri.Cvar_Get( "r_volumetricFogShadowContrast", "1.35", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogShadowContrast, "0.5", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogShadowContrast, "Shadow contrast shaping for volumetric sun/local light scattering." );
	ri.Cvar_SetGroup( r_volumetricFogShadowContrast, CVG_RENDERER );

	r_volumetricFogShowcase = ri.Cvar_Get( "r_volumetricFogShowcase", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogShowcase, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogShowcase, "Showcase presets for visibly stronger fog in regular maps: 0=off (default), 1=cinematic haze, 2=heavy shafts, 3=full-force stress test." );
	ri.Cvar_SetGroup( r_volumetricFogShowcase, CVG_RENDERER );

	r_fog_shadows = ri.Cvar_Get( "r_fog_shadows", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fog_shadows, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_fog_shadows, "Enable directional sun-shadow visibility in volumetric froxel lighting." );
	ri.Cvar_SetGroup( r_fog_shadows, CVG_RENDERER );

	r_fogShadowMapSize = ri.Cvar_Get( "r_fogShadowMapSize", "1024", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_fogShadowMapSize, "256", "4096", CV_INTEGER );
	ri.Cvar_SetDescription( r_fogShadowMapSize, "Sun shadow map resolution used by volumetric fog (single-cascade). Requires vid_restart." );
	ri.Cvar_SetGroup( r_fogShadowMapSize, CVG_RENDERER );

	r_fogShadowBias = ri.Cvar_Get( "r_fogShadowBias", "0.001", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogShadowBias, "0", "0.05", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogShadowBias, "Depth bias for volumetric sun-shadow sampling." );
	ri.Cvar_SetGroup( r_fogShadowBias, CVG_RENDERER );

	r_fogShadowPcfRadius = ri.Cvar_Get( "r_fogShadowPcfRadius", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogShadowPcfRadius, "0", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogShadowPcfRadius, "PCF radius in texels for volumetric sun-shadow filtering." );
	ri.Cvar_SetGroup( r_fogShadowPcfRadius, CVG_RENDERER );

	r_fogShadowMaxDistance = ri.Cvar_Get( "r_fogShadowMaxDistance", "4096", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogShadowMaxDistance, "256", "32768", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogShadowMaxDistance, "Max camera depth fitted into the volumetric sun shadow cascade." );
	ri.Cvar_SetGroup( r_fogShadowMaxDistance, CVG_RENDERER );

	r_fogShadowPadding = ri.Cvar_Get( "r_fogShadowPadding", "64", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogShadowPadding, "0", "4096", CV_FLOAT );
	ri.Cvar_SetDescription( r_fogShadowPadding, "World-space XY padding added to volumetric sun shadow cascade bounds." );
	ri.Cvar_SetGroup( r_fogShadowPadding, CVG_RENDERER );

	r_fogDebug = ri.Cvar_Get( "r_fogDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_fogDebug, "0", "13", CV_INTEGER );
	ri.Cvar_SetDescription( r_fogDebug, "Volumetric fog debug view: 0=off, 1=froxel coords, 2=extinction slice, 3=scattering slice, 4=temporal validity/weight, 5=integrated transmittance, 6=sun-shadow debug slice, 7=motion magnitude/threshold, 8=local spot-shadow visibility, 9=local point-shadow visibility, 10=camera-cut/reset state, 11=GPU safety telemetry counters, 12=perf budget/autoscale state, 13=fog contribution heatmap." );
	ri.Cvar_SetGroup( r_fogDebug, CVG_RENDERER );

	/* Fog debug must not persist: reset if loaded from stale config */
	if ( r_fogDebug->integer > 0 || r_volumetricFogValidation->integer > 0 ) {
		ri.Cvar_Set( "r_fogDebug", "0" );
		ri.Cvar_Set( "r_volumetricFogValidation", "0" );
		ri.Printf( PRINT_ALL, "[VK][fog] Fog debug was enabled in config; reset to normal. Use vkVolumetricValidate for debug views.\n" );
	}

	r_fboDebug = ri.Cvar_Get( "r_fboDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fboDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_fboDebug, "FBO diagnostics (all levels throttled to 1/sec):\n 0 - off\n 1 - descriptor source changes\n 2 - gamma/layout\n 3 - pipeline state\n 4 - one-time troubleshooting tips when FBO broken (r_oit 0, r_exposure_auto 0, etc)." );
	ri.Cvar_SetGroup( r_fboDebug, CVG_RENDERER );

	r_fboCinematic = ri.Cvar_Get( "r_fboCinematic", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fboCinematic, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_fboCinematic, "Use full FBO pipeline for cinematics/menus (no world). 0=skip luminance compute (workaround for VK_ERROR_DEVICE_LOST on some drivers)." );
	ri.Cvar_SetGroup( r_fboCinematic, CVG_RENDERER );

	r_froxelDebug = ri.Cvar_Get( "r_froxelDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_froxelDebug, "0", "5", CV_INTEGER );
	ri.Cvar_SetDescription( r_froxelDebug, "Volumetric froxel debug injection: 0=normal, 1=uvw gradient, 2=constant density fill, 3=sun visibility, 4=shadow uv/depth, 5=binary sun mask." );
	ri.Cvar_SetGroup( r_froxelDebug, CVG_RENDERER );

	r_vk_swapchain_srgb = ri.Cvar_Get( "r_vk_swapchain_srgb", "0", CVAR_ROM );
	ri.Cvar_SetDescription( r_vk_swapchain_srgb, "Read-only: 1 if the selected Vulkan swapchain format is sRGB." );

	r_vk_pipeline_debug = ri.Cvar_Get( "r_vk_pipeline_debug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_vk_pipeline_debug, "Print Vulkan pipeline creation info (discard mode, shader type, fog, etc)." );
	ri.Cvar_SetGroup( r_vk_pipeline_debug, CVG_RENDERER );

	r_vk_colorWriteMaskDynamic = ri.Cvar_Get( "r_vk_colorWriteMaskDynamic", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_vk_colorWriteMaskDynamic, "Enable VK_EXT_extended_dynamic_state3 for RB_ColorMask. Requires vid_restart. Disabled by default (OIT crash on some drivers)." );
	ri.Cvar_SetGroup( r_vk_colorWriteMaskDynamic, CVG_RENDERER );

	r_vk_meshShaderNV = ri.Cvar_Get( "r_vk_meshShaderNV", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vk_meshShaderNV, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vk_meshShaderNV,
		"Request VK_NV_mesh_shader at device creation (NVIDIA). Default off: extension is enabled for future mesh pipelines only; no draw path uses mesh shaders yet. Requires vid_restart." );
	ri.Cvar_SetGroup( r_vk_meshShaderNV, CVG_RENDERER );

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

	r_fbo = ri.Cvar_Get( "r_fbo", "1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_SetDescription( r_fbo, "Framebuffer objects for offscreen rendering. Required for PBR, HDR, bloom, MSAA, SMAA, SSAO, gamma correction.\n"
		"Use vid_restart after changing. Default 1 recommended." );
	r_renderMode = ri.Cvar_Get( "r_renderMode", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_renderMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_renderMode, "Rendering path. Requires vid_restart.\n 0: Forward (default)\n 1: Deferred (placeholder)\n 2: Forward+ (placeholder)\nDeferred and forward+ would need G-buffers, light culling, and separate passes; they are not implemented yet." );
	r_hdr = ri.Cvar_Get( "r_hdr", "2", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_hdr, "-1", "3", CV_INTEGER );
	ri.Cvar_SetDescription(r_hdr, "HDR frame buffer format. Requires \\r_fbo 1.\n -1: 4-bit (B4G4R4A4), testing only\n  0: 8-bit, moderate banding\n  1: 16-bit float (RGBA16F)\n  2: 32-bit float (RGBA32F), default, fallback to 16F if unsupported\n  3: 64-bit float (RGBA64F), optional; falls back to 32F (glslang lacks dvec4 fragment output support)\n" );
	r_bloom = ri.Cvar_Get( "r_bloom", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_bloom, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription(r_bloom, "Enables bloom post-processing effect. Requires \\r_fbo 1.");

	r_ssao = ri.Cvar_Get( "r_ssao", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ssao, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ssao, "Enables screen-space ambient occlusion (SSAO). Requires \\r_fbo 1." );

	r_ssaoMethod = ri.Cvar_Get( "r_ssaoMethod", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ssaoMethod, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ssaoMethod, "AO algorithm: 0=SSAO (hemisphere), 1=HBAO (horizon-based). Requires vid_restart." );

	r_ssaoRadius = ri.Cvar_Get( "r_ssaoRadius", "8.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_ssaoRadius, "1.0", "128.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_ssaoRadius, "SSAO sample radius in view space units (higher = broader occlusion)." );

	r_ssaoBias = ri.Cvar_Get( "r_ssaoBias", "0.75", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_ssaoBias, "0.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_ssaoBias, "SSAO depth bias to reduce self-occlusion." );

	r_ssaoIntensity = ri.Cvar_Get( "r_ssaoIntensity", "0.35", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_ssaoIntensity, "0.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_ssaoIntensity, "SSAO intensity multiplier." );

	r_ssaoPower = ri.Cvar_Get( "r_ssaoPower", "1.15", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_ssaoPower, "0.5", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_ssaoPower, "SSAO contrast shaping power (higher = darker occlusion)." );

	r_ssaoSamples = ri.Cvar_Get( "r_ssaoSamples", "12", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_ssaoSamples, "4", "32", CV_INTEGER );
	ri.Cvar_SetDescription( r_ssaoSamples, "SSAO sample count (higher = smoother, slower)." );

	r_hbaoDirections = ri.Cvar_Get( "r_hbaoDirections", "8", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hbaoDirections, "4", "16", CV_INTEGER );
	ri.Cvar_SetDescription( r_hbaoDirections, "HBAO ray directions (4=fast, 8=default, 16=quality)." );

	r_hbaoSteps = ri.Cvar_Get( "r_hbaoSteps", "8", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hbaoSteps, "4", "16", CV_INTEGER );
	ri.Cvar_SetDescription( r_hbaoSteps, "HBAO steps per direction (4=fast, 8=default, 16=quality)." );

	r_ssaoMaxDepthGradient = ri.Cvar_Get( "r_ssaoMaxDepthGradient", "0.08", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_ssaoMaxDepthGradient, "0.0", "0.5", CV_FLOAT );
	ri.Cvar_SetDescription( r_ssaoMaxDepthGradient, "Skip SSAO at depth edges (object silhouettes) to reduce halos. 0=disabled, lower=stricter." );

	r_ssaoBlurRadius = ri.Cvar_Get( "r_ssaoBlurRadius", "2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_ssaoBlurRadius, "0", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_ssaoBlurRadius, "SSAO blur radius in pixels (0 disables blur)." );

	r_oit = ri.Cvar_Get( "r_oit", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_oit, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_oit, "Order-independent transparency (WBOIT). Correct blending of overlapping transparent surfaces. Requires \\r_fbo 1." );
	r_ssaoDebugView = ri.Cvar_Get( "r_ssaoDebugView", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_ssaoDebugView, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_ssaoDebugView, "SSAO debug view:\n 0: off\n 1: show AO only\n 2: show depth" );
	if ( r_ssao->integer ) {
		ri.Printf( PRINT_ALL, "%s enabled.\n", ( r_ssaoMethod && r_ssaoMethod->integer ) ? "HBAO" : "SSAO" );
	}

	r_ext_multisample = ri.Cvar_Get( "r_ext_multisample", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_multisample, "0", "16", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_multisample, "MSAA sample count for geometry edges: 0=off, 2|4|8|16. Requires \\r_fbo 1. Use with SMAA for alpha edges." );

	r_msaa_sample_shading = ri.Cvar_Get( "r_msaa_sample_shading", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_msaa_sample_shading, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_msaa_sample_shading, "Per-sample shading when MSAA on: improves alpha edges and specular, ~2x fragment cost. Requires \\r_ext_multisample 2+." );
	r_msaa_sample_shading_rate = ri.Cvar_Get( "r_msaa_sample_shading_rate", "0.5", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_msaa_sample_shading_rate, "0.25", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_msaa_sample_shading_rate, "Minimum fraction of MSAA samples shaded per fragment when \\r_msaa_sample_shading 1. 0.5 is a good quality/cost balance; 1.0 shades every sample." );

	r_ext_supersample = ri.Cvar_Get( "r_ext_supersample", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_supersample, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_supersample, "Super-sample anti-aliasing, requires \\r_fbo 1." );

	r_screenMapScale = ri.Cvar_Get( "r_screenMapScale", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_screenMapScale, "1", "16", CV_INTEGER );
	ri.Cvar_SetDescription( r_screenMapScale, "Downscale divisor for screenMap reflections/refractions. 1=full res, 2=half res, 4=quarter res, 8=eighth res, 16=legacy. Requires vid_restart." );

	r_ext_smaa = ri.Cvar_Get( "r_ext_smaa", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_smaa, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_smaa, "Enables SMAA post-processing, requires \\r_fbo 1." );

	r_smaa_preset = ri.Cvar_Get( "r_smaa_preset", "3", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_smaa_preset, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_smaa_preset, "SMAA quality preset: 0=Custom, 1=Low, 2=Medium, 3=High, 4=Ultra. Overrides threshold/localContrast/searchSteps when non-zero." );

	r_smaa_threshold = ri.Cvar_Get( "r_smaa_threshold", "0.1", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_smaa_threshold, "0.01", "0.5", CV_FLOAT );
	ri.Cvar_SetDescription( r_smaa_threshold, "SMAA edge detection threshold (lower = more edges, higher = fewer). Used when r_smaa_preset 0." );

	r_smaa_local_contrast = ri.Cvar_Get( "r_smaa_local_contrast", "2.0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_smaa_local_contrast, "1.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_smaa_local_contrast, "SMAA local contrast adaptation factor. Used when r_smaa_preset 0." );

	r_smaa_max_search_steps = ri.Cvar_Get( "r_smaa_max_search_steps", "16", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_smaa_max_search_steps, "8", "32", CV_INTEGER );
	ri.Cvar_SetDescription( r_smaa_max_search_steps, "SMAA blend search steps (higher = better quality, more cost). Used when r_smaa_preset 0." );

	r_smaa_corner_rounding = ri.Cvar_Get( "r_smaa_corner_rounding", "0.2", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_smaa_corner_rounding, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_smaa_corner_rounding, "SMAA corner rounding strength (0=off, 1=full). Attenuates edges at L-corners for smoother silhouettes." );
	r_taa = ri.Cvar_Get( "r_taa", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_taa, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_taa, "Optional temporal resolve for Vulkan HDR post-processing. Disabled by default in favor of SMAA/MSAA paths." );
	ri.Cvar_SetGroup( r_taa, CVG_RENDERER );
	r_taa_feedbackStationary = ri.Cvar_Get( "r_taa_feedbackStationary", "0.92", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_taa_feedbackStationary, "0.0", "0.99", CV_FLOAT );
	ri.Cvar_SetDescription( r_taa_feedbackStationary, "TAA history feedback for stable pixels. Higher = smoother, lower = more responsive." );
	ri.Cvar_SetGroup( r_taa_feedbackStationary, CVG_RENDERER );
	r_taa_feedbackMotion = ri.Cvar_Get( "r_taa_feedbackMotion", "0.72", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_taa_feedbackMotion, "0.0", "0.99", CV_FLOAT );
	ri.Cvar_SetDescription( r_taa_feedbackMotion, "TAA history feedback for moving pixels. Lower helps reduce ghosting." );
	ri.Cvar_SetGroup( r_taa_feedbackMotion, CVG_RENDERER );
	r_taa_sharpen = ri.Cvar_Get( "r_taa_sharpen", "0.12", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_taa_sharpen, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_taa_sharpen, "Post-resolve sharpening amount applied inside the TAA pass." );
	ri.Cvar_SetGroup( r_taa_sharpen, CVG_RENDERER );

	r_rtx = ri.Cvar_Get( "r_rtx", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_rtx, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtx, "Ray tracing (0=off, 1=shadows, 2=reflections, 3=full). Requires USE_VULKAN_RTX build and RT-capable GPU. See docs/RENDERERS_FUTURE.md." );
	r_ext_alpha_to_coverage = ri.Cvar_Get( "r_ext_alpha_to_coverage", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_alpha_to_coverage, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_alpha_to_coverage, "Alpha-to-coverage for alpha-tested surfaces (foliage, grates) when MSAA is on. Enabled by default for Vulkan MSAA paths. Requires \\r_fbo 1 and \\r_ext_multisample 2+." );

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
	r_temporalDebug = ri.Cvar_Get( "r_temporalDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalDebug, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalDebug, "Temporal diagnostics:\n 0 - off\n 1 - log temporal reset reasons\n 2 - log reset reasons plus shared camera-cut and invalidated consumers." );
	#endif // USE_VULKAN

	// Register modular subsystem cvars
	CBTerrain_RegisterCvars();
	PostFX_RegisterCvars();
	VDB_Init();
	ProjLight_Init();
	SkyboxHDR_Init();
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

	if ( sizeof( glconfig_t ) != 11324 )
		ri.Error( ERR_FATAL, "Mod ABI incompatible: sizeof(glconfig_t) == %u != 11324", (unsigned int) sizeof( glconfig_t ) );

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
	ri.Printf( PRINT_ALL, "[VK] SH lighting: %s\n", r_shLighting && r_shLighting->integer ? "enabled" : "disabled" );
	ri.Printf( PRINT_ALL, "[VK] SH world: %s\n", r_shWorldLighting && r_shWorldLighting->integer ? "enabled" : "disabled" );
	ri.Printf( PRINT_ALL, "[VK] SH debug view: %d\n", r_shDebugView ? r_shDebugView->integer : 0 );
	ri.Printf( PRINT_ALL, "[VK][morph] IQM morph: %s (max channels=%d, top-k cap=%d, active=%d)\n",
		( r_morph && r_morph->integer ) ? "enabled" : "disabled",
		IQM_MORPH_MAX_CHANNELS, IQM_MORPH_TOP_K,
		r_morphMaxActive ? r_morphMaxActive->integer : IQM_MORPH_TOP_K );
	ri.Printf( PRINT_ALL, "[VK][gltf] clip playback speed scale r_gltfAnim=%.3f\n",
		r_gltfAnim ? r_gltfAnim->value : 1.0f );
	ri.Printf( PRINT_ALL, "[VK][gltf] GPU skin/morph path: %s (r_gltfGpu)\n",
		( r_gltfGpu && r_gltfGpu->integer ) ? "on" : "off" );
	ri.Printf( PRINT_ALL, "[VK][gltf] GPU tangent Gram–Schmidt after skin+morph: %s (r_gltfGpuTangentFix)\n",
		( r_gltfGpuTangentFix && r_gltfGpuTangentFix->integer ) ? "on" : "off" );


	max_polys = r_maxpolys ? r_maxpolys->integer : MAX_POLYS;
	max_polyverts = r_maxpolyverts ? r_maxpolyverts->integer : MAX_POLYVERTS;

	ptr = ri.Hunk_Alloc( sizeof( *backEndData ) + sizeof(srfPoly_t) * max_polys + sizeof(polyVert_t) * max_polyverts, h_low);
	backEndData = (backEndData_t *) ptr;
	backEndData->polys = (srfPoly_t *) ((char *) ptr + sizeof( *backEndData ));
	backEndData->polyVerts = (polyVert_t *) ((char *) ptr + sizeof( *backEndData ) + sizeof(srfPoly_t) * max_polys);

	R_InitNextFrame();

	InitOpenGL();

	// print renderer info after Vulkan is fully initialized
	GfxInfo();

	R_InitImages();

	VarInfo();

#ifdef USE_VULKAN
	vk_create_pipelines();
#ifdef VK_PBR_BRDFLUT
	vk_create_brfdlut();
#endif
	vk_validate_pbr_ibl_resources();
#endif

	R_InitShaders();

	R_InitSkins();

	R_ModelInit();

	R_InitFreeType();

#ifndef USE_VULKAN
	err = qglGetError();
	if ( err != GL_NO_ERROR )
		ri.Printf( PRINT_WARNING, "glGetError() = 0x%x\n", err );
#endif

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
	ri.Cmd_RemoveCommand( "vulkaninfo" );
	ri.Cmd_RemoveCommand( "vkVolumetricValidate" );
	ri.Cmd_RemoveCommand( "r_aaQuality" );
#endif

	//if ( tr.registered ) {
		//R_IssuePendingRenderCommands();
		R_DeleteTextures();
	//}

#ifdef USE_VULKAN
	vk_release_resources();
#endif

#ifdef USE_IMGUI
	VkImgui_Shutdown();
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
			if ( ri.VKimp_Shutdown ) {
				ri.VKimp_Shutdown( code == REF_UNLOAD_DLL ? qtrue : qfalse );
			}
			Com_Memset( &glConfig, 0, sizeof( glConfig ) );
		}
#else
		R_ClearSymTables();
		Com_Memset( &glState, 0, sizeof( glState ) );

		if ( code != REF_KEEP_WINDOW ) {
			if ( ri.GLimp_Shutdown ) {
				ri.GLimp_Shutdown( code == REF_UNLOAD_DLL ? qtrue : qfalse );
			}
			Com_Memset( &glConfig, 0, sizeof( glConfig ) );
		}
#endif
	}

	ri.FreeAll();

	tr.registered = qfalse;
	tr.inited = qfalse;
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
	re.SetEntityMorphWeight = RE_SetEntityMorphWeight;
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
	re.ReloadTexture = R_ReloadTexture;

	return &re;
}
