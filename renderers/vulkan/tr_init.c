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
#include "vk_authored_flares.h"
#include "tr_bsp_stream.h"
#include "../common/tr_vector_font.h"
#include "tr_sprite_props.h"
#include "tr_decal_props.h"
#include "tr_material_paint.h"
#include "vk_upscale.h"
#include "vk_ui_blur.h"
#include "vk_vt.h"
#include "vk_meshlets.h"
#include "vk_ndgi.h"
#include "vk_niv.h"
#include "vk_nslm.h"
#include "vk_nist.h"
#include "vk_nvc.h"
#include "vk_fsa.h"
#include "vk_vfgi.h"
#include "vk_renderformer.h"
#include "vk_wpt.h"
#include "vk_vuda.h"
#include "vk_vksplat.h"
#include "vk_curast.h"
#include "vk_graph_bfs.h"
#include "vk_mimir.h"
#include "vk_iris.h"
#include "vk_grtx.h"
#include "vk_mgs.h"
#include "vk_squeezeme.h"
#include "vk_wsp.h"
#include "vk_dressi.h"
#include "extensions/scaffold/vk_arc_blanc.h"
#include "extensions/scaffold/vk_emulator_screen.h"
#include "extensions/scaffold/vk_webcam_screen.h"
#include "vk_raygun.h"
#include "vk_fluidsim.h"
#include "vk_terrain.h"
#include "vk_biome.h"
#include "vk_vegetation_gpu.h"
#include "vk_vdb.h"
#include "vk_postfx.h"
#include "vk_flashlight.h"
#include "vk_util.h"
#include "vk_post_fog.h"
#include "vk_deferred_gbuffer.h"
#include "vk_visibility_buffer.h"
#include "vk_temporal.h"
#include "vk_view_state.h"
#include "vk_present_recon.h"
#include "vk_gpu_scene.h"
#include "vk_hiz.h"
#include "vk_sky_owner.h"
#include "vk_weather.h"
#include "vk_volumetric_clouds.h"
#include "vk_material_ir.h"
#include "vk_material_graph.h"
#include "vk_material_instance.h"
#include "vk_material_cache.h"
#include "vk_surface_evolution.h"
#include "vk_vshadow.h"
#include "vk_present_color.h"
#include "vk_exposure_histogram.h"
#include "vk_cinematic_camera.h"
#include "vk_capture_pipeline.h"
#include "vk_color_grade.h"
#include "vk_reference_lab.h"
#include "vk_frequency_aware.h"
#include "vk_spatial_aa.h"
#include "vk_scene_platform.h"
#include "vk_photometric.h"
#include "vk_ltc.h"
#include "vk_ht_throughput.h"
#include "vk_ht_animation.h"
#include "vk_gpu_scene.h"
#include "vk_pass_registry.h"
#include "vk_raster_gi.h"
#include "vk_selective_sun_shadow.h"
#include "vk_selective_reflection.h"
#include "tr_mesh_normal_policy.h"
#include "vk_raster_ultra.h"
#include "vk_sun_csm.h"
#include "vk_sim_render_profile.h"
#include "vk_sim_render_debug.h"
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
#include "vk_forward_plus.h"
#include "vk_render_path.h"
#include "vk_aa_policy.h"
static void VkInfo_f( void );
static void VulkanInfo_f( void );
static void VkVolumetricValidate_f( void );
#endif
static void GfxInfo( void );
static void VarInfo( void );
static void GL_SetDefaultState( void );

static qboolean R_IsSurfGame( void )
{
	const char *fsGame = ri.Cvar_VariableString( "fs_game" );
	const char *baseGame = ri.Cvar_VariableString( "fs_basegame" );

	return ( ( fsGame && !Q_stricmp( fsGame, "surf" ) ) ||
		( baseGame && !Q_stricmp( baseGame, "surf" ) ) ) ? qtrue : qfalse;
}

static void R_MigrateSurfViewmodelProjection( void )
{
	static qboolean warnedAlias;
	static qboolean warnedMigration;
	const char *obsoleteFovEnabled;
	qboolean migrated = qfalse;

	if ( !R_IsSurfGame() ) {
		return;
	}

	/* Historical local configs used this unscoped name; it has never driven
	 * the renderer projection and must not appear to override the real cvar. */
	obsoleteFovEnabled = ri.Cvar_VariableString( "FovEnabled" );
	if ( obsoleteFovEnabled && obsoleteFovEnabled[0] && !warnedAlias ) {
		ri.Printf( PRINT_WARNING,
			S_COLOR_YELLOW "[Surf] obsolete FovEnabled=\"%s\" is ignored; "
			"use r_firstPersonFovEnabled\n", obsoleteFovEnabled );
		warnedAlias = qtrue;
	}

	if ( !r_firstPersonFovEnabled->integer || r_firstPersonZNear->value <= 0.1251f ) {
		ri.Cvar_Set( "r_firstPersonFovEnabled", "1" );
		ri.Cvar_Set( "r_firstPersonFov", "65" );
		ri.Cvar_Set( "r_firstPersonZNear", "4" );
		migrated = qtrue;
	}

	if ( migrated && !warnedMigration ) {
		ri.Printf( PRINT_WARNING,
			S_COLOR_YELLOW "[Surf] migrated stale viewmodel projection: "
			"r_firstPersonFovEnabled=1 r_firstPersonFov=65 r_firstPersonZNear=4. "
			"Use an explicit debug cfg after startup for legacy comparison.\n" );
		warnedMigration = qtrue;
	}
}

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
#ifdef USE_IMGUI
cvar_t	*r_imgui;
cvar_t	*r_imguiTheme;
#endif

cvar_t	*r_greyscale;
cvar_t	*r_dither;
cvar_t	*r_presentBits;
cvar_t	*r_outline;
cvar_t	*r_outlineThreshold;
cvar_t	*r_sdfScreenAa;
cvar_t	*r_sdfOutline;
cvar_t	*r_sdfOutlineWidth;
cvar_t	*r_fontGamma;
cvar_t	*r_fontLcdWeight;

static cvar_t *r_ignorehwgamma;

cvar_t  *r_teleporterFlash;

cvar_t	*r_fastsky;
cvar_t	*r_neatsky;
cvar_t	*r_dynamiclight;
cvar_t	*r_dlightMode;
cvar_t	*r_dlightScale;
cvar_t	*r_dlightIntensity;
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
cvar_t	*r_pbr_bindlog;
#ifdef VK_CUBEMAP
cvar_t	*r_ibl_forceCapture;
#endif
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
cvar_t	*r_materialBlend;
cvar_t	*r_materialBlendSharpness;
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
cvar_t	*r_deluxeMapping;
cvar_t	*r_deluxeSpecular;
#endif
cvar_t   *r_vk_pipeline_debug;
cvar_t	*r_vk_pipelineCacheDisk;
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
cvar_t	*r_deferredGBuffer;
cvar_t	*r_deferredGBufferFill;
cvar_t	*r_deferredGBufferDebug;
cvar_t	*r_visibilityBuffer;
cvar_t	*r_visibilityBufferFill;
cvar_t	*r_visibilityBufferDebug;
cvar_t	*r_visibilityBufferLateShade;
cvar_t	*r_materialClassify;
cvar_t	*r_deferredLighting;
cvar_t	*r_deferredUnlitBase;
cvar_t	*r_deferredLightingStrength;
cvar_t	*r_deferredSpecular;
cvar_t	*r_deferredSpecularStrength;
cvar_t	*r_deferredAOCoupling;
cvar_t	*r_deferredDefaultMetalness;
cvar_t	*r_deferredDefaultRoughness;
cvar_t	*r_deferredNormalEdgeThreshold;
cvar_t	*r_deferredMaterialClassify;
cvar_t	*r_hdr;
cvar_t	*r_bloom;
cvar_t	*r_bloom_threshold;
cvar_t	*r_bloom_intensity;
cvar_t	*r_bloom_threshold_mode;
cvar_t	*r_bloom_modulate;
cvar_t	*r_bloomKnee;
cvar_t	*r_lensFlare;
cvar_t	*r_lensFlareStrength;
cvar_t	*r_lensFlareF1;
cvar_t	*r_lensFlareF2;
cvar_t	*r_lensFlareF3;
cvar_t	*r_lensFlareTintR;
cvar_t	*r_lensFlareTintG;
cvar_t	*r_lensFlareTintB;
cvar_t	*r_fp64Points;
cvar_t	*r_fp64PointsMode;
cvar_t	*r_fp64PointsSize;
cvar_t	*r_fp64PointsMaxVerts;
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
cvar_t	*r_oitForwardPlus;
cvar_t	*r_oitClassify;
cvar_t	*r_oitDebug;
cvar_t	*r_stochasticAlpha;
cvar_t	*r_ssaoDebugView;
cvar_t	*r_renderWidth;
cvar_t	*r_renderHeight;
cvar_t	*r_renderScale;
cvar_t	*r_temporalDebug;
cvar_t	*r_temporalCustomShaderMotion;
cvar_t	*r_temporalCpuSkinPrev;
cvar_t	*r_screenMapScale;
cvar_t	*r_ext_supersample;
cvar_t	*r_ext_smaa;
cvar_t	*r_ext_fxaa;
cvar_t	*r_fxaa_subpix;
cvar_t	*r_fxaa_edgeThreshold;
cvar_t	*r_simRenderProfile;
cvar_t	*r_simRenderProfileAutoApply;
cvar_t	*r_simRenderDebug;
cvar_t	*r_volumetricFogAccurate;
cvar_t	*r_postAaAfterBloom;
cvar_t	*r_smaa_preset;
cvar_t	*r_smaa_threshold;
cvar_t	*r_smaa_local_contrast;
cvar_t	*r_smaa_max_search_steps;
cvar_t	*r_smaa_corner_rounding;
cvar_t	*r_taa;
cvar_t	*r_taa_feedbackStationary;
cvar_t	*r_taa_feedbackMotion;
cvar_t	*r_taa_sharpen;
cvar_t	*r_taaMotionVectors;
cvar_t	*r_rtx;
cvar_t	*r_rtxDemo;
cvar_t	*r_rtxWorldPrimCap;
cvar_t	*r_rtxWorldMaterials;
cvar_t	*r_rtxWorldUvSample;
cvar_t	*r_rtxWorldAlbedoMode;
cvar_t	*r_rtxComposite;
cvar_t	*r_rtxSamples;
cvar_t	*r_rtxEntities;
cvar_t	*r_rtxEntityCap;
cvar_t	*r_rtxEntityTriCap;
cvar_t	*r_rtxEntityMaterials;
cvar_t	*r_rtxEntityUvSample;
cvar_t	*r_rtxEntityBlasUpdate;
cvar_t	*r_rtxTlasUpdate;
cvar_t	*r_rtxBindless;
cvar_t	*r_rtxBindlessCap;
cvar_t	*r_rtxBindlessMode;
cvar_t	*r_pathtrace;
cvar_t	*r_pathtrace_arch;
cvar_t	*r_pathtrace_bounces;
cvar_t	*r_pathtrace_samples;
cvar_t	*r_pathtrace_denoise;
cvar_t	*r_pathtrace_denoiseStrength;
cvar_t	*r_pathtrace_denoiseDepthTol;
cvar_t	*r_pathtrace_debug;
cvar_t	*r_pathtrace_composite;
cvar_t	*r_hybrid1;
cvar_t	*r_hybrid1Quality;
cvar_t	*r_hybrid1_shadow;
cvar_t	*r_hybrid1_spec;
cvar_t	*r_hybrid1_historyClamp;
cvar_t	*r_hybrid1_historyGamma;
cvar_t	*r_hybrid1_temporalAlpha;
cvar_t	*r_hybrid1_adaptiveBlur;
cvar_t	*r_hybrid1_separableBlur;
cvar_t	*r_hybrid1_reinhard;
cvar_t	*r_hybrid1_atrousIters;
cvar_t	*r_hybrid1_phiColor;
cvar_t	*r_hybrid1_rayBias;
cvar_t	*r_hybrid1_tMin;
cvar_t	*r_hybrid1_depthTol;
cvar_t	*r_hybrid1_normalDot;
cvar_t	*r_hybrid1_adaptiveAngle;
cvar_t	*r_hybrid1_adaptiveRough;
cvar_t	*r_hybrid1_specRoughMax;
cvar_t	*r_hybrid1_sunRadius;
cvar_t	*r_hybrid1_contactHarden;
cvar_t	*r_hybrid1_ggx;
cvar_t	*r_hybrid1_glint;
cvar_t	*r_hybrid1_iblMode;
cvar_t	*r_hybrid1_diffuseDirect;
cvar_t	*r_hybrid1_dlightShadows;
cvar_t	*r_hybrid1_shadowStrength;
cvar_t	*r_hybrid1_specStrength;
cvar_t	*r_hybrid1_debug;
cvar_t	*r_hybrid1_diffuse;
cvar_t	*r_hybrid1_diffuseStrength;
cvar_t	*r_hybrid1_ibl;
cvar_t	*r_hybrid1_taa;
cvar_t	*r_hybrid1_motion;
cvar_t	*r_hybrid1_restir;
cvar_t	*r_vdbFog;
cvar_t	*r_vdbFogBlend;
cvar_t	*r_forwardPlus;
cvar_t	*r_forwardPlusMaxPerTile;
cvar_t	*r_forwardPlusDebug;
cvar_t	*r_forwardPlusShade;
cvar_t	*r_forwardPlusOverflowShade;
cvar_t	*r_forwardPlusLuminanceSort;
cvar_t	*r_forwardPlusDistanceSort;
cvar_t	*r_forwardPlusDepthCull;
cvar_t	*r_forwardPlusHiZ;
cvar_t	*r_forwardPlusZSlices;
cvar_t	*r_forwardPlusZSliceMode;
cvar_t	*r_forwardPlusSpecularStrength;
cvar_t	*r_forwardPlusEnergyRenorm;

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
cvar_t	*r_panini_console;
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
cvar_t	*r_volumetricFogCompositeMode;
cvar_t	*r_volumetricFogIntegration;
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
cvar_t	*r_fogShadowSnap;
cvar_t	*r_pbrSunShadow;
cvar_t	*r_pbrSunShadowStrength;
cvar_t	*r_fogDebug;
cvar_t	*r_fboDebug;
cvar_t	*r_fboCinematic;
cvar_t	*r_froxelDebug;
cvar_t	*r_vk_swapchain_srgb;
cvar_t	*r_vk_clearhdr;
cvar_t	*r_vk_disableblend;
cvar_t	*r_vk_bindlog;
cvar_t	*r_intensity;
cvar_t	*r_dynamicLightScale;
cvar_t	*r_lightGammaLink;
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
#ifdef USE_IMGUI
cvar_t	*r_imgui;
cvar_t	*r_imguiTheme;
cvar_t	*r_studio_tools;
#endif
cvar_t	*r_defaultImage;

cvar_t	*r_ambientScale;
cvar_t	*r_directedScale;
cvar_t	*r_shLighting;
cvar_t	*r_shWorldLighting;
cvar_t	*r_shWorldStrength;
cvar_t	*r_classicLighting;
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
			R_Upscale_ApplyRenderScaleDefaults();

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
	/* Proprietary NVIDIA DLSS/NGX SDK is not vendored; in-engine temporal path is r_upscale 2. */
	ri.Printf( PRINT_ALL, "[VK] Upscale: NVIDIA DLSS/NGX SDK not shipped; use \\r_upscale 1|2, \\r_renderScale, \\r_taa, or GPU-driver scaling\n" );
	if ( r_volumetricFog && r_volumetricFog->integer ) {
		ri.Printf( PRINT_ALL, "[VK][fog] r_volumetricFogCompositeMode=%d (0=standard, 1=depth-weighted in-scatter, 2=HDR clamp)\n",
			r_volumetricFogCompositeMode ? r_volumetricFogCompositeMode->integer : 0 );
	}
	if ( r_vdbFog && r_vdbFog->integer ) {
		ri.Printf( PRINT_ALL, "[VK][fog] r_vdbFog=1 (blend bound VDB density in volumetric compute when GPU-uploaded)\n" );
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

#include "tr_init_capture.inc"

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


#include "tr_init_info.inc"
#include "tr_init_diagnostics.inc"

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
	ri.Cmd_AddCommand( "screenshotEXR", R_ScreenShot_f );
	ri.Cmd_AddCommand( "gfxinfo", GfxInfo_f );
#ifdef USE_VULKAN
	vk_ui_blur_register_cvars();
	ri.Cmd_AddCommand( "vkinfo", VkInfo_f );
	ri.Cmd_AddCommand( "vulkaninfo", VulkanInfo_f );
	ri.Cmd_AddCommand( "renderer_status", R_RendererStatus_f );
	ri.Cmd_AddCommand( "havenrp_renderer_status", R_HavenRPRendererStatus_f );
	ri.Cmd_AddCommand( "deferred_gbuffer_status", vk_deferred_gbuffer_status_f );
	ri.Cmd_AddCommand( "temporal_status", vk_temporal_status_f );
	ri.Cmd_AddCommand( "r_dumpTemporalState", vk_temporal_status_f );
	ri.Cmd_AddCommand( "r_captureTemporalDebug", vk_capture_temporal_debug_f );
	ri.Cmd_AddCommand( "r_printWeaponPresentation", vk_print_weapon_presentation_f );
	ri.Cmd_AddCommand( "temporal_ghost_status", vk_temporal_ghost_status_f );
	ri.Cmd_AddCommand( "surf_validateTemporalConfig", vk_surf_validate_temporal_config_f );
	ri.Cmd_AddCommand( "r_printViewmodelProjection", vk_print_viewmodel_projection_f );
	ri.Cmd_AddCommand( "temporal_motion_status", vk_motion_status_f );
	ri.Cmd_AddCommand( "temporal_resolution_status", vk_temporal_resolution_status_f );
	ri.Cmd_AddCommand( "present_recon_status", vk_present_recon_status_f );
	ri.Cmd_AddCommand( "motion_vector_cert", vk_motion_vector_cert_status_f );
	ri.Cmd_AddCommand( "visibility_buffer_status", vk_visibility_buffer_status_f );
	ri.Cmd_AddCommand( "renderer_profile", R_RendererProfile_f );
	ri.Cmd_AddCommand( "renderer_health", R_RendererHealth_f );
	ri.Cmd_AddCommand( "renderer_deferred_safe", R_RendererDeferredSafe_f );
	ri.Cmd_AddCommand( "renderer_modern_safe", R_RendererModernSafe_f );
	ri.Cmd_AddCommand( "renderer_clustered_safe", R_RendererClusteredSafe_f );
	ri.Cmd_AddCommand( "renderer_spine_1_1_cert", R_RendererSpine11Cert_f );
	ri.Cmd_AddCommand( "spine_1_1_stress", R_Spine11Stress_f );
	ri.Cmd_AddCommand( "spine_1_1_focus_pulse", R_Spine11FocusPulse_f );
	ri.Cmd_AddCommand( "spine_1_1_stress_report", R_Spine11StressReport_f );
	ri.Cmd_AddCommand( "renderer_subsystems", R_RendererSubsystems_f );
	ri.Cmd_AddCommand( "renderer_compat", R_RendererCompatibility_f );
	ri.Cmd_AddCommand( "renderer_compatibility", R_RendererCompatibility_f );
	ri.Cmd_AddCommand( "vkVolumetricValidate", VkVolumetricValidate_f );
	ri.Cmd_AddCommand( "r_quality", R_Quality_f );
	ri.Cmd_AddCommand( "sim_render_profile", R_SimRenderProfile_f );
	ri.Cmd_AddCommand( "sim_render_debug", R_SimRenderDebug_f );
	ri.Cmd_AddCommand( "volumetric_accurate", R_VolumetricAccurate_f );
	ri.Cmd_AddCommand( "volumetric_integration", R_VolumetricIntegration_f );
	ri.Cmd_AddCommand( "r_aaQuality", R_AAQuality_f );
	ri.Cmd_AddCommand( "fp64_points_gen", R_FP64_PointsGen_f );
	ri.Cmd_AddCommand( "fp64_points_load", R_FP64_PointsLoad_f );
	ri.Cmd_AddCommand( "fp64_points_clear", R_FP64_PointsClear_f );
	ri.Cmd_AddCommand( "fp64_points_benchmark", R_FP64_PointsBenchmark_f );
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
	r_dynamicLightScale = ri.Cvar_Get( "r_dynamicLightScale", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_dynamicLightScale, "0.25", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_dynamicLightScale,
		"Multiplier for dynamic light color on projector, Forward+, deferred, and volumetric paths. "
		"With HDR (r_fbo 1), tune brightness here or via r_exposure instead of r_gamma." );
	ri.Cvar_SetGroup( r_dynamicLightScale, CVG_RENDERER );
	if ( r_dynamicLightScale && r_dynamicLightScale->value != 1.0f ) {
		ri.Printf( PRINT_ALL, "[VK][lighting] r_dynamicLightScale=%.2f\n", r_dynamicLightScale->value );
	}
	r_lightGammaLink = ri.Cvar_Get( "r_lightGammaLink", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_lightGammaLink, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_lightGammaLink,
		"When 1 (default), dynamic light intensity uses 2*pow(r_intensity,r_gamma) on legacy gamma-off paths. "
		"When 0, uses 2*r_intensity only (decouples lights from display r_gamma; pair with r_dynamicLightScale / r_exposure)." );
	ri.Cvar_SetGroup( r_lightGammaLink, CVG_RENDERER );
	if ( r_lightGammaLink && !r_lightGammaLink->integer ) {
		ri.Printf( PRINT_ALL, "[VK][lighting] r_lightGammaLink=0 (dynamic lights decoupled from r_gamma)\n" );
	}
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
	ri.Cvar_CheckRange( r_pbr_debug, "0", "19", CV_INTEGER );
	ri.Cvar_SetDescription( r_pbr_debug,
		"PBR lighting decomposition (Vulkan PBR):\n"
		" 0 - off\n"
		" 1 - direct lighting\n"
		" 2 - IBL specular\n"
		" 3 - diffuse irradiance\n"
		" 4 - env/irradiance samples\n"
		" 5-8 - glint debug\n"
		" 9 - albedo\n"
		" 10 - world normal\n"
		" 11 - roughness\n"
		" 12 - metallic\n"
		" 13 - ambient contribution\n"
		" 14 - material AO\n"
		" 15 - emissive\n"
		" 16 - pre-tonemap energy sum\n"
		" 17-19 - IBL/glint resource health\n" );

	r_pbr_bindlog = ri.Cvar_Get( "r_pbr_bindlog", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_pbr_bindlog, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_pbr_bindlog,
		"Log PBR IBL/glint bind state once per map (env/irr VkImageView handles and descriptor writes)." );
	ri.Cvar_SetGroup( r_pbr_bindlog, CVG_RENDERER );

#ifdef VK_CUBEMAP
	r_ibl_forceCapture = ri.Cvar_Get( "r_ibl_forceCapture", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_ibl_forceCapture, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ibl_forceCapture,
		"When 1, wait-idle after cubemap convolution and emit PBR IBL post logs (diagnostics)." );
	ri.Cvar_SetGroup( r_ibl_forceCapture, CVG_RENDERER );
#endif

	r_glint = ri.Cvar_Get( "r_glint", "1", CVAR_ARCHIVE_ND );
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

	r_materialBlend = ri.Cvar_Get( "r_materialBlend", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_materialBlend, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_materialBlend,
		"Vulkan PBR multi-material blend: vertex RGBA weights + optional height-blend from normalHeightMap alpha. 0 = force layer0 only." );

	r_materialBlendSharpness = ri.Cvar_Get( "r_materialBlendSharpness", "8", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_materialBlendSharpness, "0", "64", CV_FLOAT );
	ri.Cvar_SetDescription( r_materialBlendSharpness,
		"Global height-blend sharpness override when > 0; otherwise use shader blendSharpness. Soft normalize when no layer has height." );

	ri.Printf( PRINT_ALL, "Material blend: %s (vertex + height)\n",
		( r_materialBlend && r_materialBlend->integer ) ? "ON" : "OFF" );

	R_MaterialPaint_RegisterCvars();
	R_MaterialPaint_RegisterCommands();

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
	r_deluxeMapping		= ri.Cvar_Get("r_deluxeMapping",	"1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_deluxeMapping, "Reading deluxemaps when compiled with q3map2:\n 0: off (approximated from lightgrid)\n 1: on (compiled deluxemaps)" );
	r_deluxeSpecular	= ri.Cvar_Get("r_deluxeSpecular",	"1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_deluxeSpecular, "Scale the specular response from deluxemaps" );
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

	r_gltfGpuTangentFix = ri.Cvar_Get( "r_gltfGpuTangentFix", "2", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_gltfGpuTangentFix, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_gltfGpuTangentFix,
		"Vulkan PBR glTF GPU tangent: 0=bind-pose T only, 1=Gram–Schmidt vs deformed N after skin+morph (default), 2=topology-weighted MikkTSpace-inspired average from incident triangles (needs per-primitive topo; see docs/GLTF.md). Latched: vid_restart to rebuild pipelines." );
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
	R_MigrateSurfViewmodelProjection();
	r_ignoreGLErrors = ri.Cvar_Get( "r_ignoreGLErrors", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_ignoreGLErrors, "Ignore OpenGL errors." );
	r_teleporterFlash = ri.Cvar_Get( "r_teleporterFlash", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_teleporterFlash, "Show a white screen instead of a black screen when being teleported in hyperspace." );
	r_fastsky = ri.Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_fastsky, "Draw flat colored skies." );
	r_dynamiclight = ri.Cvar_Get( "r_dynamiclight", "1", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( r_dynamiclight, "Enables dynamic lighting." );
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

	r_panini_console = ri.Cvar_Get( "r_panini_console", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_panini_console, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_panini_console,
		"Apply Panini warp to console/UI frames: 0=skip (default, keeps console edges straight), 1=warp console with the scene." );
	ri.Cvar_SetGroup( r_panini_console, CVG_RENDERER );
	ri.Printf( PRINT_ALL, "[VK] Panini console mask: r_panini_console=%d (0=skip warp while console/UI is drawn)\n",
		r_panini_console->integer );

	r_facePlaneCull = ri.Cvar_Get ("r_facePlaneCull", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_facePlaneCull, "Enables culling of planar surfaces with back side test." );

	r_railWidth = ri.Cvar_Get( "r_railWidth", "16", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_railWidth, "Radius of railgun trails." );
	r_railCoreWidth = ri.Cvar_Get( "r_railCoreWidth", "6", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_railCoreWidth, "Size of railgun trail rings when enabled in game code (normally \\cg_oldRail 0)." );
	r_railSegmentLength = ri.Cvar_Get( "r_railSegmentLength", "32", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_railSegmentLength, "Length of segments in railgun trails." );

	r_ambientScale = ri.Cvar_Get( "r_ambientScale", "1.2", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_ambientScale, "Light grid ambient light scaling on entity models." );
	r_directedScale = ri.Cvar_Get( "r_directedScale", "1.15", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_directedScale, "Light grid direct light scaling on entity models." );
	r_shLighting = ri.Cvar_Get( "r_shLighting", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_shLighting, "Enable spherical harmonics ambient lighting for entity models." );
	r_classicLighting = ri.Cvar_Get( "r_classicLighting", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_classicLighting, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_classicLighting,
		"When 1 (default), preserve retail Q3 / classic mod look: no SH world vertex tint, PBR sun shadow, "
		"or Forward+ overflow shade. Set 0 to enable those features via their cvars (modern / full conversions)." );
	ri.Cvar_SetGroup( r_classicLighting, CVG_RENDERER );
	if ( r_classicLighting && r_classicLighting->integer ) {
		ri.Printf( PRINT_ALL, "[VK][lighting] r_classicLighting=1 (retail/baseq3-compatible lighting)\n" );
	}
	r_shWorldLighting = ri.Cvar_Get( "r_shWorldLighting", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_shWorldLighting, "Apply spherical harmonics to world geometry (lightmapped and vertex-lit BSP surfaces)." );
	r_shWorldStrength = ri.Cvar_Get( "r_shWorldStrength", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_shWorldStrength, "0", "2", CV_FLOAT );
	ri.Cvar_SetDescription( r_shWorldStrength,
		"Blend light-grid SH into world vertex colors (0=identity/white, 1=full SH, >1 exaggerate). "
		"Uses r_directedScale like entity models." );
	ri.Cvar_SetGroup( r_shWorldStrength, CVG_RENDERER );
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
#ifdef USE_IMGUI
	r_imgui = ri.Cvar_Get( "r_imgui", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_imgui, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_imgui,
		"Draw Dear ImGui debug inspector overlay (Vulkan): dockspace, PostFX/physics/volumetrics panels. "
		"Requires USE_IMGUI build. Set 0 during gameplay if mouse capture conflicts with look." );
	if ( r_imgui && r_imgui->integer ) {
		ri.Printf( PRINT_ALL, "ImGui inspector overlay: r_imgui 1 (toggle with r_imgui 0)\n" );
	}
	r_imguiTheme = ri.Cvar_Get( "r_imguiTheme", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_imguiTheme, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_imguiTheme,
		"ImGui editor theme: 0=Pablo dark (VEditor-style), 1=Spectrum light." );
	ri.Cvar_SetGroup( r_imguiTheme, CVG_RENDERER );
#endif

	r_debugLight = ri.Cvar_Get( "r_debuglight", "0", CVAR_TEMP );
	ri.Cvar_SetDescription( r_debugLight, "Debugging tool to print ambient and directed lighting information." );
	r_debugSort = ri.Cvar_Get( "r_debugSort", "0", CVAR_CHEAT );
	ri.Cvar_SetDescription( r_debugSort, "Debugging tool to filter out shaders with depth sorting order values higher than the set value." );
	r_printShaders = ri.Cvar_Get( "r_printShaders", "0", 0 );
	ri.Cvar_SetDescription( r_printShaders, "Debugging tool to print on console of the number of shaders used." );
	r_saveFontData = ri.Cvar_Get( "r_saveFontData", "0", 0 );
	ri.Cvar_Get( "r_font", "fonts/Inter-Regular.ttf", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_font", "fonts/Inter-Regular.ttf", CVAR_ARCHIVE ),
		"TrueType font for UI VM text and (with cl_builtInTtf 1) engine HUD / console via FreeType glyph atlases. Empty = legacy bitmap font only." );
	ri.Cvar_Get( "r_consoleFont", "fonts/Inter-Regular.ttf", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_consoleFont", "fonts/Inter-Regular.ttf", CVAR_ARCHIVE ),
		"TrueType font for the console (FreeType). Default fonts/Inter-Regular.ttf; empty uses r_font." );
	ri.Cvar_Get( "r_fontSize", "16", CVAR_ARCHIVE );
	ri.Cvar_SetDescription( ri.Cvar_Get( "r_fontSize", "16", CVAR_ARCHIVE ), "Point size for custom fonts loaded via r_font / r_consoleFont." );
	{
		cvar_t *fa = ri.Cvar_Get( "r_fontAtlasSize", "512", CVAR_ARCHIVE );
		ri.Cvar_CheckRange( fa, "256", "2048", CV_INTEGER );
		ri.Cvar_SetDescription( fa,
			"FreeType atlas page size for TrueType glyphs. Larger pages reduce fragmentation for high-DPI and LCD glyphs. Values snap to 256, 512, 1024, or 2048. Apply with reloadTtf or vid_restart." );
	}
	{
		cvar_t *fd = ri.Cvar_Get( "r_fontDpi", "96", CVAR_ARCHIVE );
		ri.Cvar_CheckRange( fd, "72", "144", CV_INTEGER );
		ri.Cvar_SetDescription( fd,
			"FreeType device DPI for TrueType glyph rasterization (72 = legacy sizing, 96+ = denser atlas / sharper upscaled console). Apply with reloadTtf or vid_restart." );
	}
	{
		cvar_t *fh = ri.Cvar_Get( "r_fontHint", "1", CVAR_ARCHIVE );
		ri.Cvar_CheckRange( fh, "0", "2", CV_INTEGER );
		ri.Cvar_SetDescription( fh,
			"FreeType load hinting: 0 = FT_LOAD_DEFAULT, 1 = FT_LOAD_TARGET_LIGHT (default), 2 = FT_LOAD_TARGET_NORMAL. Apply with reloadTtf or vid_restart." );
	}
	{
		cvar_t *fm = ri.Cvar_Get( "r_fontMipmap", "1", CVAR_ARCHIVE );
		ri.Cvar_CheckRange( fm, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( fm,
			"Build mipmaps for FreeType TrueType atlas pages. Helps minified UI text; 0 = single mip (legacy). Apply with reloadTtf or vid_restart." );
	}
	{
		cvar_t *fv = ri.Cvar_Get( "r_fontVerticalHint", "0", CVAR_ARCHIVE );
		ri.Cvar_CheckRange( fv, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( fv,
			"Rougier HAL-00821839 vertical-only FreeType hinting (horizontal DPI x100 trick). 1 = crisp vertical stems with accurate horizontal advance. Apply with reloadTtf." );
	}
	{
		cvar_t *fl = ri.Cvar_Get( "r_fontLcd", "0", CVAR_ARCHIVE );
		ri.Cvar_CheckRange( fl, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( fl,
			"Rougier HAL-00821839 LCD/subpixel FreeType atlas (FT_RENDER_MODE_LCD). Best with r_fontSubpixelPos 1. Apply with reloadTtf." );
	}
	{
		r_fontLcdWeight = ri.Cvar_Get( "r_fontLcdWeight", "0.35", CVAR_ARCHIVE );
		ri.Cvar_CheckRange( r_fontLcdWeight, "0.0", "1.0", CV_FLOAT );
		ri.Cvar_SetDescription( r_fontLcdWeight,
			"Blend strength for RGB LCD glyph coverage in the Vulkan subpixel text shader. 0 = monochrome alpha, 1 = full per-channel LCD coverage. Best when r_fontLcd 1 and r_fontSubpixelPos 1." );
	}
	{
		cvar_t *fp = ri.Cvar_Get( "r_fontSubpixelPos", "0", CVAR_ARCHIVE );
		ri.Cvar_CheckRange( fp, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( fp,
			"Rougier HAL-00821839 subpixel glyph positioning (Vulkan uiSubpixelText shader). 1 = fractional horizontal placement. Disable r_fontSubpixel when using this." );
	}
	r_sdfScreenAa = ri.Cvar_Get( "r_sdfScreenAa", "1", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_sdfScreenAa, "0", "8", CV_FLOAT );
	ri.Cvar_SetDescription( r_sdfScreenAa,
		"Vulkan uiSdfText: scales fwidth(distance) for screen-space edge AA (resolution-independent; Green/Alvin-style). 0 = use push r_sdfSmoothing band only." );
	r_sdfOutline = ri.Cvar_Get( "r_sdfOutline", "0", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_sdfOutline, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_sdfOutline,
		"Vulkan uiSdfText: Green (2007) outline ring around SDF glyphs (single-channel distance field)." );
	r_sdfOutlineWidth = ri.Cvar_Get( "r_sdfOutlineWidth", "0.06", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_sdfOutlineWidth, "0.01", "0.25", CV_FLOAT );
	ri.Cvar_SetDescription( r_sdfOutlineWidth,
		"Vulkan uiSdfText: outline width in SDF distance units when r_sdfOutline is 1." );
	r_fontGamma = ri.Cvar_Get( "r_fontGamma", "1.0", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_fontGamma, "0.5", "3.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fontGamma,
		"Linearize coverage before display gamma (Rougier HAL-05430837: apply AA before gamma). 1.0 = off; uiSdfText and uiSubpixelText." );
	ri.Printf( PRINT_ALL, "SDF UI text: r_sdfScreenAa=%.2f (fwidth edge AA; 0 disables)\n", r_sdfScreenAa->value );
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

#ifdef USE_IMGUI
	r_imgui = ri.Cvar_Get( "r_imgui", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_imgui,
		"Vulkan ImGui debug inspector overlay (0=off, 1=on). Toggle at runtime with F11 when enabled." );
	ri.Cvar_CheckRange( r_imgui, "0", "1", CV_INTEGER );
	ri.Printf( PRINT_ALL, "[VK][imgui] debug inspector r_imgui=%d (F11 toggles when enabled)\n",
		r_imgui->integer );
	r_studio_tools = ri.Cvar_Get( "r_studio_tools", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_studio_tools, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_studio_tools,
		"When 1 (with r_imgui 1): id Studio-style panels — Session, Console, Entities, Paint, Animation (see docs/IN_ENGINE_STUDIO_TOOLS.md)." );
	ri.Printf( PRINT_ALL, "[VK][studio] r_studio_tools=%d (0=inspector only, 1=Studio Session/Console/Entities/Paint/Animation)\n",
		r_studio_tools->integer );
#endif

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
	ri.Cvar_SetDescription( r_speeds, "Prints out various debugging stats from PVS:\n 0: Disabled\n 1: Backend BSP\n 2: Frontend grid culling\n 3: Current view cluster index\n 4: Dynamic lighting\n 5: zFar clipping\n 6: Flares\n 7: Sim render profile + volumetric GPU ms" );
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
	ri.Cvar_SetDescription( r_post, "HDR grading stack after world resolve (exposure + tonemap always run for world; 1=full grade/bloom knee/local exposure/LUT, 0=minimal resolve only)." );
	ri.Cvar_SetGroup( r_post, CVG_RENDERER );

	r_post_debug = ri.Cvar_Get( "r_post_debug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_post_debug, "0", "99", CV_INTEGER );
	ri.Cvar_SetDescription( r_post_debug, "Debug view for the post-process pass: 0=final, 1=pre-tonemap linear HDR (clamp/10), 2=luminance heatmap. White-floor diagnosis: if mode 1 is already white, lighting energy is too high before tonemap; if mode 1 looks sane but mode 0 is blown, tonemap/gamma is wrong. Also try r_vk_clearhdr 0 (accumulation) and r_vk_disableblend 1 (additive blend)." );
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
	ri.Cvar_SetDescription( r_volumetricFog,
		"Enable froxel volumetric fog (compute fill + HDR composite before tonemap). "
		"Requires r_fbo 1. Half-res RGBA16F grid (vidW/2,vidH/2,64) with height fog, HG phase, and sun lighting." );
	ri.Cvar_SetGroup( r_volumetricFog, CVG_RENDERER );

	r_volumetricFogDensity = ri.Cvar_Get( "r_volumetricFogDensity", "0.6", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogDensity, "0", "5", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogDensity, "Fog density multiplier for the volumetric fog pass (height-fog extinction base)." );
	ri.Cvar_SetGroup( r_volumetricFogDensity, CVG_RENDERER );

	r_volumetricFogHeightFalloff = ri.Cvar_Get( "r_volumetricFogHeightFalloff", "0.015", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogHeightFalloff, "0", "5", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogHeightFalloff, "Vertical density falloff for height fog (higher = thinner aloft)." );
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
	ri.Cvar_SetDescription( r_volumetricFogAniso, "Henyey-Greenstein anisotropy (positive = forward scatter, negative = backscatter)." );
	ri.Cvar_SetGroup( r_volumetricFogAniso, CVG_RENDERER );

	r_volumetricFogSteps = ri.Cvar_Get( "r_volumetricFogSteps", "48", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogSteps, "1", "256", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogSteps, "Depth-limited raymarch steps when compositing froxel fog into HDR (before tonemap)." );
	ri.Cvar_SetGroup( r_volumetricFogSteps, CVG_RENDERER );

	r_volumetricFogZExponent = ri.Cvar_Get( "r_volumetricFogZExponent", "1.5", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogZExponent, "1.0", "8.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogZExponent, "Exponent for volumetric depth-slice distribution (paired with r_volumetricFogSliceMode)." );
	ri.Cvar_SetGroup( r_volumetricFogZExponent, CVG_RENDERER );
	r_volumetricFogSliceMode = ri.Cvar_Get( "r_volumetricFogSliceMode", "2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogSliceMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogSliceMode,
		"Froxel Z slicing: 0=exponential (more near camera), 1=linear, 2=logarithmic (default; denser far slices)." );
	ri.Cvar_SetGroup( r_volumetricFogSliceMode, CVG_RENDERER );

	r_volumetricFogMaxDistance = ri.Cvar_Get( "r_volumetricFogMaxDistance", "4096", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogMaxDistance, "1", "65536", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogMaxDistance, "Maximum integration distance for volumetric fog in world units." );
	ri.Cvar_SetGroup( r_volumetricFogMaxDistance, CVG_RENDERER );

	r_volumetricFogJitter = ri.Cvar_Get( "r_volumetricFogJitter", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogJitter, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogJitter, "Sub-pixel jitter for froxel density / composite raymarch (stabilized by temporal)." );
	ri.Cvar_SetGroup( r_volumetricFogJitter, CVG_RENDERER );

	r_volumetricFogTemporalWeight = ri.Cvar_Get( "r_volumetricFogTemporalWeight", "0.72", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogTemporalWeight, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_volumetricFogTemporalWeight,
		"History blend weight for froxel temporal reprojection with neighborhood clamping (0 = no history)." );
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

	r_volumetricFogCompositeMode = ri.Cvar_Get( "r_volumetricFogCompositeMode", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogCompositeMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogCompositeMode,
		"Volumetric fog composite: 0=physical (scene*T + in-scatter integral, default), "
		"1=artistic depth-weighted in-scatter (reduces near-camera glow), "
		"2=HDR clamp (clamp final RGB to \\r_volumetricFogFireflyClamp after composite)." );
	ri.Cvar_SetGroup( r_volumetricFogCompositeMode, CVG_RENDERER );

	r_volumetricFogIntegration = ri.Cvar_Get( "r_volumetricFogIntegration", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogIntegration, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogIntegration,
		"Volumetric composite integration: 0=froxel grid march (default), 1=screen analytical approx, "
		"2=screen ray march + sun shadow map, 3=OpenVDB Woodcock/delta tracking (majorant grid; skips froxel compute)." );
	ri.Cvar_SetGroup( r_volumetricFogIntegration, CVG_RENDERER );
	if ( r_volumetricFogIntegration && r_volumetricFogIntegration->integer > 0 ) {
		ri.Printf( PRINT_ALL,
			"...volumetric fog integration mode %d (screen-space; froxel compute skipped when > 0)\n",
			r_volumetricFogIntegration->integer );
	}

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

	r_volumetricFogTransmittanceCutoff = ri.Cvar_Get( "r_volumetricFogTransmittanceCutoff", "0.002", CVAR_ARCHIVE_ND );
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

	r_volumetricFogGridDim = ri.Cvar_Get( "r_volumetricFogGridDim", "0 0 64", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_volumetricFogGridDim,
		"Froxel grid (x y z). Default \"0 0 64\" = half-res (vidWidth/2, vidHeight/2, clamped to 640x360) with 64 slices. "
		"Explicit sizes e.g. \"160 90 96\" override the auto clamp. Requires vid_restart." );
	ri.Cvar_SetGroup( r_volumetricFogGridDim, CVG_RENDERER );

	ri.Printf( PRINT_ALL,
		"[VK][fog] froxel path: density=%.3f heightFalloff=%.3f aniso=%.2f steps=%d jitter=%.2f temporalWeight=%.2f sliceMode=%d grid=\"%s\"\n",
		r_volumetricFogDensity->value,
		r_volumetricFogHeightFalloff->value,
		r_volumetricFogAniso->value,
		r_volumetricFogSteps->integer,
		r_volumetricFogJitter->value,
		r_volumetricFogTemporalWeight->value,
		r_volumetricFogSliceMode->integer,
		r_volumetricFogGridDim->string );

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
	ri.Cvar_SetDescription( r_volumetricFogSkipStatic, "Skip volumetric fog and disable TAA when view is nearly static for ~0.5s (e.g. death cam). Prevents gradient/streak artifacts. 0=always keep fog/TAA history." );
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
	r_fogShadowSnap = ri.Cvar_Get( "r_fogShadowSnap", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fogShadowSnap, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_fogShadowSnap,
		"When 1, snap volumetric sun shadow ortho bounds to shadow-map texels (reduces camera-move shimmer)." );
	ri.Cvar_SetGroup( r_fogShadowSnap, CVG_RENDERER );
	r_pbrSunShadow = ri.Cvar_Get( "r_pbrSunShadow", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pbrSunShadow, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_pbrSunShadow,
		"PBR direct sun shadows on opaque surfaces (deluxe/lightmap directional). Uses the same sun shadow map as "
		"volumetric fog; requires r_fbo 1. vid_restart after first enable if Forward+ layout was created before this build." );
	ri.Cvar_SetGroup( r_pbrSunShadow, CVG_RENDERER );
	if ( r_pbrSunShadow && r_pbrSunShadow->integer && r_classicLighting && !r_classicLighting->integer ) {
		ri.Printf( PRINT_ALL, "[VK][lighting] r_pbrSunShadow=1 (PBR deluxe direct x sun shadow map)\n" );
	}
	r_pbrSunShadowStrength = ri.Cvar_Get( "r_pbrSunShadowStrength", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pbrSunShadowStrength, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_pbrSunShadowStrength, "Blend PBR sun shadow into deluxe direct lighting (0=off, 1=full)." );
	ri.Cvar_SetGroup( r_pbrSunShadowStrength, CVG_RENDERER );

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

	r_vk_clearhdr = ri.Cvar_Get( "r_vk_clearhdr", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vk_clearhdr, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vk_clearhdr,
		"Clear the HDR color target at the start of the main pass (1=clear to black, 0=preserve for accumulation diagnosis). Requires vid_restart." );
	ri.Cvar_SetGroup( r_vk_clearhdr, CVG_RENDERER );

	r_vk_disableblend = ri.Cvar_Get( "r_vk_disableblend", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vk_disableblend, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vk_disableblend,
		"Force blendEnable=OFF for opaque main-pass pipelines (depth-write). Use to diagnose additive white glow; set 0 only if needed." );
	ri.Cvar_SetGroup( r_vk_disableblend, CVG_RENDERER );

	r_vk_bindlog = ri.Cvar_Get( "r_vk_bindlog", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_vk_bindlog, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vk_bindlog,
		"Per-frame log of HDR clear status and active post/debug mode. White-floor diagnosis: r_vk_clearhdr 0, r_vk_disableblend 1, r_post_debug 1|2." );
	ri.Cvar_SetGroup( r_vk_bindlog, CVG_RENDERER );

	r_vk_pipeline_debug = ri.Cvar_Get( "r_vk_pipeline_debug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_vk_pipeline_debug, "Print Vulkan pipeline creation info (discard mode, shader type, fog, etc)." );
	ri.Cvar_SetGroup( r_vk_pipeline_debug, CVG_RENDERER );

	r_vk_pipelineCacheDisk = ri.Cvar_Get( "r_vk_pipelineCacheDisk", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vk_pipelineCacheDisk, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vk_pipelineCacheDisk,
		"When 1 (latched): load/save VkPipelineCache under fs_homepath as vk/pcache_<UUIDhex>_<schema>.bin (legacy vk/pcache_<UUIDhex> still loads once). Schema bumps invalidate incompatible caches. Saves on shutdown or vid_restart." );
	ri.Cvar_SetGroup( r_vk_pipelineCacheDisk, CVG_RENDERER );

	r_vk_colorWriteMaskDynamic = ri.Cvar_Get( "r_vk_colorWriteMaskDynamic", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_vk_colorWriteMaskDynamic, "Enable VK_EXT_extended_dynamic_state3 for RB_ColorMask. Requires vid_restart. Disabled by default (OIT crash on some drivers)." );
	ri.Cvar_SetGroup( r_vk_colorWriteMaskDynamic, CVG_RENDERER );

	r_vk_meshShaderNV = ri.Cvar_Get( "r_vk_meshShaderNV", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vk_meshShaderNV, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vk_meshShaderNV,
		"Request VK_NV_mesh_shader at device creation (NVIDIA). Default off: extension is enabled for future mesh pipelines only; no draw path uses mesh shaders yet. "
		"Planned use: Loop&Blinn glyphlet string dispatch (r_vectorFontMode 2, AMD GPUOpen). Requires vid_restart." );
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
	ri.Cvar_SetGroup( r_fbo, CVG_RENDERER );
	r_renderMode = ri.Cvar_Get( "r_renderMode", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_renderMode, "0", "5", CV_INTEGER );
	ri.Cvar_SetDescription( r_renderMode,
		"Vulkan lighting path (latched, vid_restart).\n"
		" 0: Forward (classic projector; r_forwardPlus may still be 1)\n"
		" 1: Deferred opaque + Forward+ transparent (r_deferredLighting 1)\n"
		" 2: Tier A Certified Raster — Forward+ primary (Spine 1.0 boot via modern_vulkan.cfg)\n"
		" 3: Unified Clustered — deferred opaque + Forward+ transparent (opt-in)\n"
		" 4: Tier B Selective Hybrid — clustered raster + exclusive RT signal owners (opt-in; RTX)\n"
		" 5: Tier C Path-Traced Reference — exclusive PT lighting (opt-in; not gameplay default)\n"
		"See docs/RENDERER_SPINE_1.2.md. Recovery: exec modern_vulkan.cfg / gfx_safe.cfg." );
	r_deferredGBuffer = ri.Cvar_Get( "r_deferredGBuffer", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_deferredGBuffer, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredGBuffer,
		"With r_renderMode 1/2/3: allocate full-res G-buffer images (albedo=color format, normal RGBA16F, material RGBA16F, lighting RGBA16F). "
		"Mode 2 uses this as a sidecar for temporal/advanced consumers; mode 1/3 use it for deferred lighting. Requires r_fbo 1 and vid_restart." );
	ri.Cvar_SetGroup( r_deferredGBuffer, CVG_RENDERER );
	if ( r_deferredGBuffer && r_deferredGBuffer->integer ) {
		ri.Printf( PRINT_ALL, "[VK][deferred] r_deferredGBuffer=1 (G-buffer RTs when r_renderMode 1/2/3)\n" );
	}
	r_deferredGBufferFill = ri.Cvar_Get( "r_deferredGBufferFill", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_deferredGBufferFill, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredGBufferFill,
		"With r_renderMode 1/2/3 and r_deferredGBuffer 1: after opaque (mode 3) or main geometry, copy scene color to G-buffer albedo "
		"and export normal/material from opaque PBR shaders when possible; MSAA/legacy paths fill normal/material from depth (compute). "
		"Mode 3 Unified Clustered captures after opaque only." );
	ri.Cvar_SetGroup( r_deferredGBufferFill, CVG_RENDERER );
	if ( r_deferredGBufferFill && r_deferredGBufferFill->integer ) {
		ri.Printf( PRINT_ALL, "[VK][deferred] r_deferredGBufferFill=1 (capture after geometry each frame)\n" );
	}
	r_deferredGBufferDebug = ri.Cvar_Get( "r_deferredGBufferDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_deferredGBufferDebug, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredGBufferDebug,
		"Visualize deferred buffers on scene color before bloom: 0=off, 1=albedo, 2=normal (view XYZ), 3=material, 4=lighting (requires r_deferredLighting 1), 5=normal confidence, 6=motion vectors. "
		"Requires r_renderMode 1/2/3, r_deferredGBuffer 1, r_deferredGBufferFill 1." );
	ri.Cvar_SetGroup( r_deferredGBufferDebug, CVG_RENDERER );
	r_visibilityBuffer = ri.Cvar_Get( "r_visibilityBuffer", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_visibilityBuffer, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_visibilityBuffer,
		"With r_renderMode 1/2/3: allocate compact visibility-buffer RTs (IDs R32G32_UINT, bary R16G16_UNORM, class R8_UINT). "
		"2027 Phase 1 foundation; coexists with r_deferredGBuffer. Requires r_fbo 1 and vid_restart. See docs/RENDERER_2027.md." );
	ri.Cvar_SetGroup( r_visibilityBuffer, CVG_RENDERER );
	if ( r_visibilityBuffer && r_visibilityBuffer->integer ) {
		ri.Printf( PRINT_ALL, "[VK][visbuf] r_visibilityBuffer=1 (visibility RTs when r_renderMode 1/2/3)\n" );
	}
	r_visibilityBufferFill = ri.Cvar_Get( "r_visibilityBufferFill", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_visibilityBufferFill, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_visibilityBufferFill,
		"With r_visibilityBuffer 1: after opaque (mode 3) or main geometry:\n"
		" 0 - off\n"
		" 1 - depth-proxy compute fill (Morton/depth draw/prim ids)\n"
		" 2 - prefer true PrimID/drawId MRT when available (non-MSAA deferred export); else depth proxy\n"
		"Material classify still runs when r_materialClassify 1." );
	ri.Cvar_SetGroup( r_visibilityBufferFill, CVG_RENDERER );
	if ( r_visibilityBufferFill && r_visibilityBufferFill->integer ) {
		ri.Printf( PRINT_ALL, "[VK][visbuf] r_visibilityBufferFill=%d\n", r_visibilityBufferFill->integer );
	}
	r_visibilityBufferDebug = ri.Cvar_Get( "r_visibilityBufferDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_visibilityBufferDebug, "0", "5", CV_INTEGER );
	ri.Cvar_SetDescription( r_visibilityBufferDebug,
		"Visualize visibility buffer on scene color before bloom: 0=off, 1=drawId, 2=primId, 3=bary, 4=material class, "
		"5=late-shade preview (albedo*class; pairs with r_visibilityBufferLateShade). Requires r_visibilityBuffer 1." );
	ri.Cvar_SetGroup( r_visibilityBufferDebug, CVG_RENDERER );
	r_visibilityBufferLateShade = ri.Cvar_Get( "r_visibilityBufferLateShade", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_visibilityBufferLateShade, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_visibilityBufferLateShade,
		"Exclusive late-shade opaque lighting (PrimID fill=2, non-MSAA): skips classic deferred lighting and shades once from G-buffer MRTs + Forward+ tiles. Default 0. Requires r_visibilityBuffer 1, r_visibilityBufferFill 2, r_deferredLighting 1." );
	ri.Cvar_SetGroup( r_visibilityBufferLateShade, CVG_RENDERER );
	if ( r_visibilityBufferLateShade && r_visibilityBufferLateShade->integer ) {
		ri.Printf( PRINT_ALL, "[VK][visbuf] r_visibilityBufferLateShade=1 (exclusive late-shade; no dual deferred lighting)\n" );
	}
	r_materialClassify = ri.Cvar_Get( "r_materialClassify", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_materialClassify, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_materialClassify,
		"With r_visibilityBufferFill 1: compute material class map (simple/layered/transmission/emissive/alpha_test) "
		"from G-buffer material + depth. Phase 1 stub for specialized shade dispatch." );
	ri.Cvar_SetGroup( r_materialClassify, CVG_RENDERER );
	if ( r_materialClassify && r_materialClassify->integer ) {
		ri.Printf( PRINT_ALL, "[VK][visbuf] r_materialClassify=1 (class map after visibility fill)\n" );
	}
	r_deferredLighting = ri.Cvar_Get( "r_deferredLighting", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_deferredLighting, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredLighting,
		"Deferred diffuse (point+spot lights via Forward+ tile lists). Requires r_renderMode 1, 3, or 4, "
		"r_deferredGBuffer 1, r_deferredGBufferFill 1, r_forwardPlus 1. "
		"Mode 1/3: opaque deferred handoff + Forward+ transparent (r_forwardPlusShade kept on for filter 2). "
		"Fails open to Forward+ if G-buffer/lighting/composite is not path-ready. "
		"Ignored in r_renderMode 2 modern Forward+ default. See docs/RENDERER_PATH_OWNERSHIP.md." );
	ri.Cvar_SetGroup( r_deferredLighting, CVG_RENDERER );
	if ( r_deferredLighting && r_deferredLighting->integer ) {
		ri.Printf( PRINT_ALL, "[VK][deferred] r_deferredLighting=1 (G-buffer diffuse + Forward+ tiles; point+spot)\n" );
	}
	r_deferredUnlitBase = ri.Cvar_Get( "r_deferredUnlitBase", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_deferredUnlitBase, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredUnlitBase,
		"With r_deferredLighting 1: additive dynamic lights on static-lit base (scene copy + deferred diffuse). "
		"Skips classic lit-surf projector pass. Set 0 for legacy multiply composite." );
	ri.Cvar_SetGroup( r_deferredUnlitBase, CVG_RENDERER );
	if ( r_deferredUnlitBase && r_deferredUnlitBase->integer && r_deferredLighting && r_deferredLighting->integer ) {
		ri.Printf( PRINT_ALL, "[VK][deferred] r_deferredUnlitBase=1 (additive dynamic on static base; skip lit-surf pass)\n" );
	}
	r_deferredLightingStrength = ri.Cvar_Get( "r_deferredLightingStrength", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_deferredLightingStrength, "0", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_deferredLightingStrength,
		"Scale for deferred dynamic diffuse contribution (0=off, 1=default)." );
	ri.Cvar_SetGroup( r_deferredLightingStrength, CVG_RENDERER );
	r_deferredSpecular = ri.Cvar_Get( "r_deferredSpecular", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_deferredSpecular, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredSpecular,
		"With r_deferredLighting 1: GGX + Smith + Fresnel specular on dynamic lights in deferred pass (0=diffuse only)." );
	ri.Cvar_SetGroup( r_deferredSpecular, CVG_RENDERER );
	if ( r_deferredSpecular && r_deferredSpecular->integer && r_deferredLighting && r_deferredLighting->integer ) {
		ri.Printf( PRINT_ALL, "[VK][deferred] r_deferredSpecular=1 (dynamic specular in deferred pass)\n" );
	}
	r_deferredSpecularStrength = ri.Cvar_Get( "r_deferredSpecularStrength", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_deferredSpecularStrength, "0", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_deferredSpecularStrength,
		"With r_deferredLighting 1 and r_deferredSpecular 1: scale deferred dynamic specular highlights (0=off, 1=default)." );
	ri.Cvar_SetGroup( r_deferredSpecularStrength, CVG_RENDERER );
	r_deferredAOCoupling = ri.Cvar_Get( "r_deferredAOCoupling", "0.65", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_deferredAOCoupling, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_deferredAOCoupling,
		"With r_deferredLighting 1: attenuate deferred dynamic light by the G-buffer material AO channel (0=off, 1=full)." );
	ri.Cvar_SetGroup( r_deferredAOCoupling, CVG_RENDERER );
	r_deferredDefaultMetalness = ri.Cvar_Get( "r_deferredDefaultMetalness", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_deferredDefaultMetalness, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_deferredDefaultMetalness,
		"Fallback metalness written by the deferred depth-derived G-buffer until true material export is available." );
	ri.Cvar_SetGroup( r_deferredDefaultMetalness, CVG_RENDERER );
	r_deferredDefaultRoughness = ri.Cvar_Get( "r_deferredDefaultRoughness", "0.55", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_deferredDefaultRoughness, "0.04", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_deferredDefaultRoughness,
		"Fallback roughness written by the deferred depth-derived G-buffer until true material export is available." );
	ri.Cvar_SetGroup( r_deferredDefaultRoughness, CVG_RENDERER );
	r_deferredNormalEdgeThreshold = ri.Cvar_Get( "r_deferredNormalEdgeThreshold", "0.08", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_deferredNormalEdgeThreshold, "0.001", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_deferredNormalEdgeThreshold,
		"View-space depth delta threshold for deferred normal reconstruction. Lower values reject silhouette-crossing neighbors more aggressively." );
	ri.Cvar_SetGroup( r_deferredNormalEdgeThreshold, CVG_RENDERER );
	r_deferredMaterialClassify = ri.Cvar_Get( "r_deferredMaterialClassify", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_deferredMaterialClassify, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_deferredMaterialClassify,
		"When r_materialClassify 1 and visibility class map is filled, deferred lighting uses class IDs "
		"(EMPTY/LAYERED/TRANSMISSION/EMISSIVE) with calibrated energy scales. Default 1. "
		"Requires r_visibilityBuffer. See docs/RENDERER_2027.md." );
	ri.Cvar_SetGroup( r_deferredMaterialClassify, CVG_RENDERER );
	if ( r_deferredMaterialClassify && r_deferredMaterialClassify->integer ) {
		ri.Printf( PRINT_ALL, "[VK][deferred] r_deferredMaterialClassify=1 (class map drives deferred dispatch when classify fill is on)\n" );
	}
	r_hdr = ri.Cvar_Get( "r_hdr", "2", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_hdr, "-1", "3", CV_INTEGER );
	ri.Cvar_SetDescription(r_hdr, "HDR frame buffer format. Requires \\r_fbo 1.\n -1: 4-bit (B4G4R4A4), testing only\n  0: 8-bit, moderate banding\n  1: 16-bit float (RGBA16F)\n  2: 32-bit float (RGBA32F), default, fallback to 16F if unsupported\n  3: aliases to mode 2 (32F) — true RGBA64F color output is not implemented\n" );
	ri.Cvar_SetGroup( r_hdr, CVG_RENDERER );
	r_bloom = ri.Cvar_Get( "r_bloom", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_bloom, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription(r_bloom, "Enables bloom post-processing effect. Requires \\r_fbo 1.");
	ri.Cvar_SetGroup( r_bloom, CVG_RENDERER );

	r_lensFlare = ri.Cvar_Get( "r_lensFlare", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_lensFlare, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_lensFlare, "Screen-space lens flare post-pass (directional sun ghosts). Requires \\r_fbo 1." );
	ri.Cvar_SetGroup( r_lensFlare, CVG_RENDERER );

	r_lensFlareStrength = ri.Cvar_Get( "r_lensFlareStrength", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_lensFlareStrength, "0.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_lensFlareStrength, "Overall lens flare intensity multiplier." );
	ri.Cvar_SetGroup( r_lensFlareStrength, CVG_RENDERER );

	r_lensFlareF1 = ri.Cvar_Get( "r_lensFlareF1", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_lensFlareF1, "0.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_lensFlareF1, "Primary ghost ring strength (f1 term)." );
	ri.Cvar_SetGroup( r_lensFlareF1, CVG_RENDERER );

	r_lensFlareF2 = ri.Cvar_Get( "r_lensFlareF2", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_lensFlareF2, "0.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_lensFlareF2, "Secondary chromatic ghost strength (f2/f4 terms)." );
	ri.Cvar_SetGroup( r_lensFlareF2, CVG_RENDERER );

	r_lensFlareF3 = ri.Cvar_Get( "r_lensFlareF3", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_lensFlareF3, "0.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_lensFlareF3, "Tight halo ghost strength (f5 terms)." );
	ri.Cvar_SetGroup( r_lensFlareF3, CVG_RENDERER );

	r_lensFlareTintR = ri.Cvar_Get( "r_lensFlareTintR", "1.4", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_lensFlareTintR, "0.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_lensFlareTintR, "Lens flare warm tint (red channel)." );
	ri.Cvar_SetGroup( r_lensFlareTintR, CVG_RENDERER );

	r_lensFlareTintG = ri.Cvar_Get( "r_lensFlareTintG", "1.2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_lensFlareTintG, "0.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_lensFlareTintG, "Lens flare warm tint (green channel)." );
	ri.Cvar_SetGroup( r_lensFlareTintG, CVG_RENDERER );

	r_lensFlareTintB = ri.Cvar_Get( "r_lensFlareTintB", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_lensFlareTintB, "0.0", "4.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_lensFlareTintB, "Lens flare warm tint (blue channel)." );
	ri.Cvar_SetGroup( r_lensFlareTintB, CVG_RENDERER );

	r_fp64Points = ri.Cvar_Get( "r_fp64Points", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_fp64Points, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_fp64Points,
		"Draw double-precision point datasets (arXiv:2408.09699). Requires vid_restart; needs GPU shaderFloat64 for native mode." );
	ri.Cvar_SetGroup( r_fp64Points, CVG_RENDERER );

	r_fp64PointsMode = ri.Cvar_Get( "r_fp64PointsMode", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fp64PointsMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_fp64PointsMode, "0=native fp64, 1=emulated high/low vec3, 2=single-precision baseline." );
	ri.Cvar_SetGroup( r_fp64PointsMode, CVG_RENDERER );

	r_fp64PointsSize = ri.Cvar_Get( "r_fp64PointsSize", "2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fp64PointsSize, "1", "32", CV_FLOAT );
	ri.Cvar_SetDescription( r_fp64PointsSize, "Point sprite size for fp64 point visualization." );
	ri.Cvar_SetGroup( r_fp64PointsSize, CVG_RENDERER );

	r_fp64PointsMaxVerts = ri.Cvar_Get( "r_fp64PointsMaxVerts", "1000000", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_fp64PointsMaxVerts, "1000", "10000000", CV_INTEGER );
	ri.Cvar_SetDescription( r_fp64PointsMaxVerts, "Maximum vertices for fp64_points_gen / fp64_points_load." );
	ri.Cvar_SetGroup( r_fp64PointsMaxVerts, CVG_RENDERER );

	r_ssao = ri.Cvar_Get( "r_ssao", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ssao, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ssao, "Enables screen-space ambient occlusion (SSAO). Requires \\r_fbo 1." );
	ri.Cvar_SetGroup( r_ssao, CVG_RENDERER );

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
	ri.Cvar_CheckRange( r_oit, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_oit, "Order-independent transparency:\n 0 - off\n 1 - WBOIT (weighted blended OIT)\n 2 - MBOIT / Moment Transparency (glass, smoke, particles, overlapping translucent layers)\n Requires \\r_fbo 1." );
	ri.Cvar_SetGroup( r_oit, CVG_RENDERER );
	r_oitForwardPlus = ri.Cvar_Get( "r_oitForwardPlus", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_oitForwardPlus, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitForwardPlus, "Forward+-lit OIT accumulation (tile lights on transparent surfaces). Default 1. Applies to \\r_oit 1 (WBOIT) and \\r_oit 2 (MBOIT accum; moments pass stays unlit). Requires \\r_forwardPlus 1. Mode 3: use with OIT instead of Forward+ transparent shade." );
	ri.Cvar_SetGroup( r_oitForwardPlus, CVG_RENDERER );
	r_oitClassify = ri.Cvar_Get( "r_oitClassify", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_oitClassify, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitClassify,
		"Class-specialized OIT buckets (P4):\n"
		" 0 - single transparent bucket (default)\n"
		" 1 - split alpha-blend (MBOIT/WBOIT) vs additive particles (WBOIT, no moments)\n"
		"Requires \\r_oit 1 or 2. Hair cards stay on \\r_stochasticAlpha." );
	ri.Cvar_SetGroup( r_oitClassify, CVG_RENDERER );
	r_oitDebug = ri.Cvar_Get( "r_oitDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_oitDebug, "0", "15", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitDebug,
		"OIT resolve debug view (cheat):\n"
		" 0 - composite\n"
		" 1 - raw accumulation RGB\n"
		" 2 - accumulated alpha/weight\n"
		" 3 - revealage\n"
		" 4 - transmittance (revealage)\n"
		" 5 - resolved transparent only\n"
		" 6 - opaque background\n"
		" 7 - coverage (1-revealage)\n"
		" 8 - pass ownership (green=OIT, blue=opaque)\n"
		" 9 - moment RGB (MBOIT)\n"
		" 10 - optical depth b0 (MBOIT)\n"
		" 11 - coverage×weight heat\n"
		" 12 - estimated fragment/layer count\n"
		" 13 - opaque depth at transparent pixels (WBOIT)\n"
		" 14 - constant magenta×coverage (ignore accum RGB)\n"
		" 15 - FragCoord UV addressing diagnostic\n"
		"NaN/Inf → magenta." );
	ri.Cvar_SetGroup( r_oitDebug, CVG_RENDERER );
	{
		cvar_t *r_oitDirectTest;
		r_oitDirectTest = ri.Cvar_Get( "r_oitDirectTest", "0", CVAR_CHEAT );
		ri.Cvar_CheckRange( r_oitDirectTest, "0", "2", CV_INTEGER );
		ri.Cvar_SetDescription( r_oitDirectTest,
			"OIT lifecycle isolation (cheat):\n"
			" 0 - off\n"
			" 1 - clear accum/reveal, skip transparent draws, resolve (pure opaque)\n"
			" 2 - same as 1, but resolve composites a synthetic UV gradient (addressing test)" );
		ri.Cvar_SetGroup( r_oitDirectTest, CVG_RENDERER );
	}
	if ( r_oitClassify && r_oitClassify->integer ) {
		ri.Printf( PRINT_ALL, "[VK] OIT: r_oitClassify=1 (alpha-blend + additive buckets)\n" );
	}
	if ( r_oit->integer == 1 ) {
		ri.Printf( PRINT_ALL, "[VK] OIT: WBOIT (weighted blended) enabled%s.\n",
			( r_oitForwardPlus && r_oitForwardPlus->integer ) ? " + Forward+ lit" : "" );
	} else if ( r_oit->integer == 2 ) {
		ri.Printf( PRINT_ALL, "[VK] OIT: MBOIT (Moment Transparency) enabled%s.\n",
			( r_oitForwardPlus && r_oitForwardPlus->integer ) ? " + Forward+ lit accum" : "" );
	}
	r_stochasticAlpha = ri.Cvar_Get( "r_stochasticAlpha", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_stochasticAlpha, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_stochasticAlpha,
		"Stochastic alpha-clipped materials (foliage, grates, hair cards, fabric holes, decals):\n"
		" 0 - hard alphaFunc discard\n"
		" 1 - screen-space hashed alpha\n"
		" 2 - temporal hashed alpha (frame-seeded noise; pair with r_taa 1)\n"
		"Applies to shader alphaFunc GT0/LT128/GE128." );
	ri.Cvar_SetGroup( r_stochasticAlpha, CVG_RENDERER );
	if ( r_stochasticAlpha->integer > 0 ) {
		ri.Printf( PRINT_ALL, "[VK] Stochastic alpha-clipped materials: mode %d\n", r_stochasticAlpha->integer );
	}
	r_ssaoDebugView = ri.Cvar_Get( "r_ssaoDebugView", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_ssaoDebugView, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_ssaoDebugView, "SSAO debug view:\n 0: off\n 1: show AO only\n 2: show depth" );
	if ( r_ssao->integer ) {
		ri.Printf( PRINT_ALL, "%s enabled.\n", ( r_ssaoMethod && r_ssaoMethod->integer ) ? "HBAO" : "SSAO" );
	}

	r_ext_multisample = ri.Cvar_Get( "r_ext_multisample", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_multisample, "0", "16", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_multisample, "MSAA sample count for geometry edges: 0=off, 2|4|8|16. Requires \\r_fbo 1. Use with SMAA for alpha edges." );
	ri.Cvar_SetGroup( r_ext_multisample, CVG_RENDERER );

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
	ri.Cvar_SetDescription( r_ext_smaa, "Enables SMAA post-processing, requires \\r_fbo 1. Mutually exclusive with \\r_ext_fxaa." );
	ri.Cvar_SetGroup( r_ext_smaa, CVG_RENDERER );

	r_ext_fxaa = ri.Cvar_Get( "r_ext_fxaa", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_ext_fxaa, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_ext_fxaa, "Enables FXAA post-processing (lightweight edge AA after main pass). Requires \\r_fbo 1. Mutually exclusive with \\r_ext_smaa." );
	ri.Cvar_SetGroup( r_ext_fxaa, CVG_RENDERER );

	r_fxaa_subpix = ri.Cvar_Get( "r_fxaa_subpix", "0.75", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_fxaa_subpix, "0.0", "1.0", CV_FLOAT );
	ri.Cvar_SetDescription( r_fxaa_subpix, "FXAA sub-pixel quality (higher = softer edges, more blur)." );

	r_fxaa_edgeThreshold = ri.Cvar_Get( "r_fxaa_edgeThreshold", "0.166", CVAR_ARCHIVE );
	ri.Cvar_CheckRange( r_fxaa_edgeThreshold, "0.031", "0.5", CV_FLOAT );
	ri.Cvar_SetDescription( r_fxaa_edgeThreshold, "FXAA edge detection threshold (lower = more edges filtered)." );

	r_simRenderProfile = ri.Cvar_Get( "r_simRenderProfile", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_simRenderProfile, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_simRenderProfile, "Active simulation render profile: 0=off, 1=AMBF lightweight, 2=volumetric accurate. Use sim_render_profile then vid_restart." );
	ri.Cvar_SetGroup( r_simRenderProfile, CVG_RENDERER );

	r_simRenderProfileAutoApply = ri.Cvar_Get( "r_simRenderProfileAutoApply", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_simRenderProfileAutoApply, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_simRenderProfileAutoApply, "When 1, re-applies r_simRenderProfile cvars at each vid_restart (simulation lock-in)." );
	ri.Cvar_SetGroup( r_simRenderProfileAutoApply, CVG_RENDERER );

	r_simRenderDebug = ri.Cvar_Get( "r_simRenderDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_simRenderDebug, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_simRenderDebug,
		"Simulation render debug: 0=off, 1=console stats (~1 Hz), 2=ImGui HUD (needs r_imgui 1). "
		"Enables volumetric GPU timestamps while active." );
	ri.Cvar_SetGroup( r_simRenderDebug, CVG_RENDERER );
	VK_SimRenderDebugStartupLog();

	r_volumetricFogAccurate = ri.Cvar_Get( "r_volumetricFogAccurate", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_volumetricFogAccurate, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_volumetricFogAccurate, "Informational flag: set to 1 by volumetric_accurate / sim profile 2. Use volumetric_accurate command to apply settings." );
	ri.Cvar_SetGroup( r_volumetricFogAccurate, CVG_RENDERER );

	r_postAaAfterBloom = ri.Cvar_Get( "r_postAaAfterBloom", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_postAaAfterBloom, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_postAaAfterBloom, "Re-run FXAA/SMAA after bloom so tonemap samples the final HDR image (default 1)." );
	ri.Cvar_SetGroup( r_postAaAfterBloom, CVG_RENDERER );

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

	vk_aa_policy_register_cvars();
	vk_present_recon_register_cvars();
	vk_present_recon_init();

	r_taa = ri.Cvar_Get( "r_taa", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_taa, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_taa,
		"Temporal / adaptive reconstruction for Vulkan HDR post (after post-fog). "
		"r_aaMode 2 = SMAA (default); 3 = Present-Time Adaptive Reconstruction; "
		"4/5 = Temporal Reconstruction. Uses vk_temporal reset policy; "
		"r_taaMotionVectors samples main-pass motion when available." );
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
	r_taaMotionVectors = ri.Cvar_Get( "r_taaMotionVectors", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_taaMotionVectors, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_taaMotionVectors, "TAA history UV: 1=main-pass motion vectors (gen_frag out_motion), 0=depth reprojection only." );
	r_temporalCpuSkinPrev = ri.Cvar_Get( "r_temporalCpuSkinPrev", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalCpuSkinPrev, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalCpuSkinPrev,
		"Temporal motion on animated RT_MODEL entities:\n"
		" 1 (default) per-entity fallback (prev MVP = current when CPU skin lacks prev pose; TAA keeps running).\n"
		" 0 conservative: new/spawning animated entities mark the whole frame motion-unreliable." );
	ri.Cvar_SetGroup( r_temporalCpuSkinPrev, CVG_RENDERER );
	ri.Cvar_Get( "r_temporalScopeReduce", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetGroup( r_taaMotionVectors, CVG_RENDERER );

	r_rtx = ri.Cvar_Get( "r_rtx", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_rtx, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtx, "Ray tracing (0=off, 1=shadows, 2=reflections, 3=full). When built with USE_VULKAN_RTX and set >0 before vid_restart, enables KHR ray tracing device extensions. See r_rtxDemo and docs/RENDERERS_FUTURE.md." );
	r_rtxDemo = ri.Cvar_Get( "r_rtxDemo", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_rtxDemo, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxDemo, "When USE_VULKAN_RTX and r_rtx>0: 1=world BSP ray trace + composite each frame; 0=extensions only (no demo GPU work)." );
	r_rtxWorldPrimCap = ri.Cvar_Get( "r_rtxWorldPrimCap", "262144", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_rtxWorldPrimCap, "4096", "1048576", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxWorldPrimCap, "Max triangles packed into the RTX world BLAS (latched). Lower on huge maps if BLAS build fails." );
	r_rtxWorldMaterials = ri.Cvar_Get( "r_rtxWorldMaterials", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_rtxWorldMaterials, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxWorldMaterials,
		"Pack world RT hit albedo from diffuse shader avgColor (fallback: BSP vertex colors). Rebuild BLAS via map load / vid_restart." );
	ri.Cvar_SetGroup( r_rtxWorldMaterials, CVG_RENDERER );
	ri.Printf( PRINT_ALL, "[VK][RTX] r_rtxWorldMaterials=%d (world hit albedo from diffuse shaders when packed)\n",
		r_rtxWorldMaterials->integer );
	r_rtxWorldUvSample = ri.Cvar_Get( "r_rtxWorldUvSample", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_rtxWorldUvSample, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxWorldUvSample,
		"When \\r_rtxWorldMaterials: pack world hit albedo from UV-centroid samples of diffuse thumbs (fallback to avgColor / vertex color)." );
	ri.Cvar_SetGroup( r_rtxWorldUvSample, CVG_RENDERER );
	ri.Printf( PRINT_ALL, "[VK][RTX] r_rtxWorldUvSample=%d (UV centroid sample into world albedo SSBO)\n",
		r_rtxWorldUvSample->integer );
	r_rtxWorldAlbedoMode = ri.Cvar_Get( "r_rtxWorldAlbedoMode", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_rtxWorldAlbedoMode, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxWorldAlbedoMode,
		"When \\r_rtxWorldMaterials: 0=replace BSP vertex color with material/UV albedo; "
		"1=modulate material/UV × vertex color (keeps lightmap bake in bounce)." );
	ri.Cvar_SetGroup( r_rtxWorldAlbedoMode, CVG_RENDERER );
	ri.Printf( PRINT_ALL, "[VK][RTX] r_rtxWorldAlbedoMode=%d (0=replace, 1=modulate × vertex)\n",
		r_rtxWorldAlbedoMode->integer );
	r_rtxComposite = ri.Cvar_Get( "r_rtxComposite", "0.55", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_rtxComposite, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_rtxComposite,
		"When USE_VULKAN_RTX and \\r_rtxDemo 1: blend resolved raster HDR color into the RT output per pixel. "
		"0 = traced (flat albedo) only — looks grey; 1 = scene only; 0.4..0.7 recommended. "
		"Values below 0.05 are clamped to 0.55 in the demo pass so the screen is not replaced with grey." );
	ri.Cvar_SetGroup( r_rtxComposite, CVG_RENDERER );
	r_rtxSamples = ri.Cvar_Get( "r_rtxSamples", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_rtxSamples, "1", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxSamples, "Primary ray samples per pixel for the Vulkan RT output. Higher values smooth edge shimmer at extra GPU cost." );
	ri.Cvar_SetGroup( r_rtxSamples, CVG_RENDERER );
	r_rtxEntities = ri.Cvar_Get( "r_rtxEntities", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_rtxEntities, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxEntities,
		"When USE_VULKAN_RTX: pack RT_MODEL entities into a second BLAS (MD3 LOD0, CPU-skinned IQM/MDR, static/CPU-skinned glTF; AABB for pack fail). Default 0 (latched)." );
	ri.Cvar_SetGroup( r_rtxEntities, CVG_RENDERER );
	r_rtxEntityCap = ri.Cvar_Get( "r_rtxEntityCap", "128", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_rtxEntityCap, "0", "1024", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxEntityCap, "Max RT_MODEL entities packed into the entity BLAS when \\r_rtxEntities 1 (latched)." );
	ri.Cvar_SetGroup( r_rtxEntityCap, CVG_RENDERER );
	r_rtxEntityTriCap = ri.Cvar_Get( "r_rtxEntityTriCap", "65536", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_rtxEntityTriCap, "12", "1048576", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxEntityTriCap,
		"Max triangles in the entity BLAS when \\r_rtxEntities 1 (MD3/IQM/glTF mesh + AABB proxies; latched)." );
	ri.Cvar_SetGroup( r_rtxEntityTriCap, CVG_RENDERER );
	r_rtxEntityMaterials = ri.Cvar_Get( "r_rtxEntityMaterials", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_rtxEntityMaterials, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxEntityMaterials,
		"When \\r_rtxEntities 1: pack per-surface/shader texture-average albedo into entity hit SSBOs (Hybrid1/Surfel). 0=refEntity tint/gray only." );
	ri.Cvar_SetGroup( r_rtxEntityMaterials, CVG_RENDERER );
	ri.Printf( PRINT_ALL, "[VK][RTX] r_rtxEntityMaterials=%d (entity hit albedo from shaders/texture averages when entities are packed)\n",
		r_rtxEntityMaterials->integer );
	r_rtxEntityUvSample = ri.Cvar_Get( "r_rtxEntityUvSample", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_rtxEntityUvSample, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxEntityUvSample,
		"When \\r_rtxEntities + \\r_rtxEntityMaterials: pack entity hit albedo from UV centroid samples of diffuse thumbs (fallback to average)." );
	ri.Cvar_SetGroup( r_rtxEntityUvSample, CVG_RENDERER );
	ri.Printf( PRINT_ALL, "[VK][RTX] r_rtxEntityUvSample=%d (UV centroid sample into entity albedo SSBO)\n",
		r_rtxEntityUvSample->integer );
	r_rtxEntityBlasUpdate = ri.Cvar_Get( "r_rtxEntityBlasUpdate", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_rtxEntityBlasUpdate, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxEntityBlasUpdate,
		"When \\r_rtxEntities 1: 1=entity BLAS UPDATE when triangle count stable (faster skinned path); 0=full BLAS rebuild each frame." );
	ri.Cvar_SetGroup( r_rtxEntityBlasUpdate, CVG_RENDERER );
	ri.Printf( PRINT_ALL, "[VK][RTX] r_rtxEntityBlasUpdate=%d (entity BLAS UPDATE when prim count stable)\n",
		r_rtxEntityBlasUpdate->integer );
	r_rtxTlasUpdate = ri.Cvar_Get( "r_rtxTlasUpdate", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_rtxTlasUpdate, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxTlasUpdate,
		"When \\r_rtxEntities 1: 1=TLAS UPDATE when instance count stable (faster hybrid path); 0=full TLAS rebuild each frame." );
	ri.Cvar_SetGroup( r_rtxTlasUpdate, CVG_RENDERER );
	r_rtxBindless = ri.Cvar_Get( "r_rtxBindless", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_rtxBindless, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxBindless,
		"D2 Phase A: RTX hit-shader bindless diffuse table (USE_VULKAN_RTX). 0=SSBO albedo fallback only; latch + vid_restart. See docs/RTX_HIT_SHADER_UV.md." );
	ri.Cvar_SetGroup( r_rtxBindless, CVG_RENDERER );
	ri.Printf( PRINT_ALL, "[VK][RTX] r_rtxBindless=%d (hit bindless default on; r_rtxBindlessMode 1 for Phase A.1b centroid sample)\n",
		r_rtxBindless->integer );
	r_rtxBindlessCap = ri.Cvar_Get( "r_rtxBindlessCap", "4096", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_rtxBindlessCap, "1", "16384", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxBindlessCap,
		"Max textures in the RTX bindless descriptor array when \\r_rtxBindless 1 (latched)." );
	ri.Cvar_SetGroup( r_rtxBindlessCap, CVG_RENDERER );
	r_rtxBindlessMode = ri.Cvar_Get( "r_rtxBindlessMode", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_rtxBindlessMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_rtxBindlessMode,
		"RTX bindless path: 0=force SSBO albedo, 1=descriptor indexing (Phase A.1b centroid UV), 2=atlas fallback (stub)." );
	ri.Cvar_SetGroup( r_rtxBindlessMode, CVG_RENDERER );
	r_pathtrace = ri.Cvar_Get( "r_pathtrace", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_pathtrace, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_pathtrace,
		"Experimental multi-bounce path trace over RTX world TLAS (USE_VULKAN_RTX). Requires r_rtx 1, r_rtxDemo 1, vid_restart. See docs/PATHTRACE_ARCH_BENCHMARK.md." );
	ri.Cvar_SetGroup( r_pathtrace, CVG_RENDERER );
	r_pathtrace_arch = ri.Cvar_Get( "r_pathtrace_arch", "megakernel", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_pathtrace_arch,
		"Path trace scheduler: megakernel (single dispatch, in-shader bounce loop) or wavefront (per-bounce TraceRays + compact). Latched; vid_restart." );
	ri.Cvar_SetGroup( r_pathtrace_arch, CVG_RENDERER );
	r_pathtrace_bounces = ri.Cvar_Get( "r_pathtrace_bounces", "4", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pathtrace_bounces, "1", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_pathtrace_bounces, "Max path depth for r_pathtrace (1-8)." );
	ri.Cvar_SetGroup( r_pathtrace_bounces, CVG_RENDERER );
	r_pathtrace_samples = ri.Cvar_Get( "r_pathtrace_samples", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pathtrace_samples, "1", "64", CV_INTEGER );
	ri.Cvar_SetDescription( r_pathtrace_samples, "Per-pixel samples (megakernel averages; wavefront uses primary only)." );
	ri.Cvar_SetGroup( r_pathtrace_samples, CVG_RENDERER );
	r_pathtrace_denoise = ri.Cvar_Get( "r_pathtrace_denoise", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pathtrace_denoise, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_pathtrace_denoise, "Depth-guided 3x3 spatial denoise on path-trace buffer before composite (cost measurement; not OIDN)." );
	ri.Cvar_SetGroup( r_pathtrace_denoise, CVG_RENDERER );
	r_pathtrace_denoiseStrength = ri.Cvar_Get( "r_pathtrace_denoiseStrength", "0.65", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pathtrace_denoiseStrength, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_pathtrace_denoiseStrength, "PathTrace denoise blend strength when r_pathtrace_denoise 1 (0=off effect, 1=full neighbor mix)." );
	ri.Cvar_SetGroup( r_pathtrace_denoiseStrength, CVG_RENDERER );
	r_pathtrace_denoiseDepthTol = ri.Cvar_Get( "r_pathtrace_denoiseDepthTol", "0.02", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pathtrace_denoiseDepthTol, "0.0001", "0.5", CV_FLOAT );
	ri.Cvar_SetDescription( r_pathtrace_denoiseDepthTol, "PathTrace denoise depth edge stop (reject neighbors beyond this depth delta)." );
	ri.Cvar_SetGroup( r_pathtrace_denoiseDepthTol, CVG_RENDERER );
	r_pathtrace_debug = ri.Cvar_Get( "r_pathtrace_debug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pathtrace_debug, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_pathtrace_debug,
		"Debug view: 0=off, 1=bounce heatmap (megakernel), 2=wavefront alive count per bounce (developer)." );
	ri.Cvar_SetGroup( r_pathtrace_debug, CVG_RENDERER );
	r_pathtrace_composite = ri.Cvar_Get( "r_pathtrace_composite", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_pathtrace_composite, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_pathtrace_composite, "Blend path trace output into HDR color (1=full replace via blit)." );
	ri.Cvar_SetGroup( r_pathtrace_composite, CVG_RENDERER );
	r_hybrid1 = ri.Cvar_Get( "r_hybrid1", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_hybrid1, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1,
		"Chocolate RT tier — Granja/Pereira Hybrid Rendering 1 (USE_VULKAN_RTX): 1-SPP shadow/spec/diffuse RT, SVGF, A-trous, IBL, composite. Requires r_rtxDemo 1, r_fbo 1, vid_restart. Quality tiers: r_hybrid1Quality. See docs/HYBRID_RENDERING1.md." );
	ri.Cvar_SetGroup( r_hybrid1, CVG_RENDERER );
	r_hybrid1Quality = ri.Cvar_Get( "r_hybrid1Quality", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1Quality, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1Quality,
		"Hybrid1 quality preset when r_hybrid1 1: 0=custom (leave individual r_hybrid1_* alone), 1=performance, 2=balanced, 3=quality (≈ demo_hybrid1.cfg). Live; entity BLAS still needs r_rtxEntities + vid_restart." );
	ri.Cvar_SetGroup( r_hybrid1Quality, CVG_RENDERER );
	r_hybrid1_shadow = ri.Cvar_Get( "r_hybrid1_shadow", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_shadow, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_shadow, "Hybrid1: trace and denoise sun shadow visibility channel." );
	ri.Cvar_SetGroup( r_hybrid1_shadow, CVG_RENDERER );
	r_hybrid1_spec = ri.Cvar_Get( "r_hybrid1_spec", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_spec, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_spec, "Hybrid1: trace and denoise indirect specular reflections channel." );
	ri.Cvar_SetGroup( r_hybrid1_spec, CVG_RENDERER );
	r_hybrid1_historyClamp = ri.Cvar_Get( "r_hybrid1_historyClamp", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_historyClamp, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_historyClamp, "Hybrid1: variance color clamping (history rectification) on temporal reprojection." );
	ri.Cvar_SetGroup( r_hybrid1_historyClamp, CVG_RENDERER );
	r_hybrid1_historyGamma = ri.Cvar_Get( "r_hybrid1_historyGamma", "1.25", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_historyGamma, "0.5", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_historyGamma, "Hybrid1: variance neighborhood bounding box scale for history clamp." );
	ri.Cvar_SetGroup( r_hybrid1_historyGamma, CVG_RENDERER );
	r_hybrid1_temporalAlpha = ri.Cvar_Get( "r_hybrid1_temporalAlpha", "0.1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_temporalAlpha, "0.02", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_temporalAlpha, "Hybrid1: temporal blend toward current noisy sample (lower = smoother)." );
	ri.Cvar_SetGroup( r_hybrid1_temporalAlpha, CVG_RENDERER );
	r_hybrid1_adaptiveBlur = ri.Cvar_Get( "r_hybrid1_adaptiveBlur", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_adaptiveBlur, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_adaptiveBlur, "Hybrid1: start A-trous at coarser step when roughness>r_hybrid1_adaptiveRough or shadow angle>r_hybrid1_adaptiveAngle." );
	ri.Cvar_SetGroup( r_hybrid1_adaptiveBlur, CVG_RENDERER );
	r_hybrid1_separableBlur = ri.Cvar_Get( "r_hybrid1_separableBlur", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_separableBlur, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_separableBlur, "Hybrid1: separable 5-tap horizontal+vertical A-trous passes per iteration." );
	ri.Cvar_SetGroup( r_hybrid1_separableBlur, CVG_RENDERER );
	r_hybrid1_reinhard = ri.Cvar_Get( "r_hybrid1_reinhard", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_reinhard, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_reinhard, "Hybrid1: Reinhard tone map specular before denoise; inverse after A-trous vertical pass." );
	ri.Cvar_SetGroup( r_hybrid1_reinhard, CVG_RENDERER );
	r_hybrid1_atrousIters = ri.Cvar_Get( "r_hybrid1_atrousIters", "4", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_atrousIters, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_atrousIters, "Hybrid1: edge-avoiding A-trous iterations (0=temporal only)." );
	ri.Cvar_SetGroup( r_hybrid1_atrousIters, CVG_RENDERER );
	r_hybrid1_phiColor = ri.Cvar_Get( "r_hybrid1_phiColor", "0.35", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_phiColor, "0.01", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_phiColor, "Hybrid1 A-trous luminance/color edge weight scale (higher = sharper, less blur across contrast)." );
	ri.Cvar_SetGroup( r_hybrid1_phiColor, CVG_RENDERER );
	r_hybrid1_rayBias = ri.Cvar_Get( "r_hybrid1_rayBias", "0.02", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_rayBias, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_rayBias, "Hybrid1: world-space normal offset for RT origins (reduces self-intersection acne)." );
	ri.Cvar_SetGroup( r_hybrid1_rayBias, CVG_RENDERER );
	r_hybrid1_tMin = ri.Cvar_Get( "r_hybrid1_tMin", "0.01", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_tMin, "0.0001", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_tMin, "Hybrid1: ray tMin for shadow/spec/diffuse traces." );
	ri.Cvar_SetGroup( r_hybrid1_tMin, CVG_RENDERER );
	r_hybrid1_depthTol = ri.Cvar_Get( "r_hybrid1_depthTol", "0.002", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_depthTol, "0.0001", "0.1", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_depthTol, "Hybrid1 A-trous: reject neighbors beyond this depth delta." );
	ri.Cvar_SetGroup( r_hybrid1_depthTol, CVG_RENDERER );
	r_hybrid1_normalDot = ri.Cvar_Get( "r_hybrid1_normalDot", "0.92", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_normalDot, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_normalDot, "Hybrid1 A-trous: minimum normal dot product to accept a neighbor sample." );
	ri.Cvar_SetGroup( r_hybrid1_normalDot, CVG_RENDERER );
	r_hybrid1_adaptiveAngle = ri.Cvar_Get( "r_hybrid1_adaptiveAngle", "6", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_adaptiveAngle, "0", "90", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_adaptiveAngle, "Hybrid1 adaptive A-trous: shadow N·L angle (degrees) above which first iteration starts coarser." );
	ri.Cvar_SetGroup( r_hybrid1_adaptiveAngle, CVG_RENDERER );
	r_hybrid1_adaptiveRough = ri.Cvar_Get( "r_hybrid1_adaptiveRough", "0.2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_adaptiveRough, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_adaptiveRough, "Hybrid1 adaptive A-trous: roughness above which first specular iteration starts coarser." );
	ri.Cvar_SetGroup( r_hybrid1_adaptiveRough, CVG_RENDERER );
	r_hybrid1_specRoughMax = ri.Cvar_Get( "r_hybrid1_specRoughMax", "0.98", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_specRoughMax, "0.5", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_specRoughMax, "Hybrid1: skip specular RT when G-buffer roughness is at or above this threshold." );
	ri.Cvar_SetGroup( r_hybrid1_specRoughMax, CVG_RENDERER );
	r_hybrid1_sunRadius = ri.Cvar_Get( "r_hybrid1_sunRadius", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_sunRadius, "0", "5", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_sunRadius, "Hybrid1: sun angular radius in degrees for soft / penumbra shadows (0=hard)." );
	ri.Cvar_SetGroup( r_hybrid1_sunRadius, CVG_RENDERER );
	r_hybrid1_contactHarden = ri.Cvar_Get( "r_hybrid1_contactHarden", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_contactHarden, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_contactHarden, "Hybrid1: shrink soft-sun penumbra when N·L is high (contact hardening)." );
	ri.Cvar_SetGroup( r_hybrid1_contactHarden, CVG_RENDERER );
	r_hybrid1_ggx = ri.Cvar_Get( "r_hybrid1_ggx", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_ggx, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_ggx, "Hybrid1: GGX/VNDF specular sampling + Fresnel metalness weight." );
	ri.Cvar_SetGroup( r_hybrid1_ggx, CVG_RENDERER );
	r_hybrid1_glint = ri.Cvar_Get( "r_hybrid1_glint", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_glint, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_glint, "Hybrid1: apply raster glint NDF weight on specular RT (also requires \\r_glint 1; screen-UV jacobian proxy)." );
	ri.Cvar_SetGroup( r_hybrid1_glint, CVG_RENDERER );
	r_hybrid1_iblMode = ri.Cvar_Get( "r_hybrid1_iblMode", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_iblMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_iblMode, "Hybrid1 IBL: 0=off path, 1=prefilter*(1-rough), 2=split-sum EnvBRDF LUT." );
	ri.Cvar_SetGroup( r_hybrid1_iblMode, CVG_RENDERER );
	r_hybrid1_diffuseDirect = ri.Cvar_Get( "r_hybrid1_diffuseDirect", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_diffuseDirect, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_diffuseDirect, "Hybrid1: light diffuse secondary hits with sun + irradiance (needs r_hybrid1_diffuse 1)." );
	ri.Cvar_SetGroup( r_hybrid1_diffuseDirect, CVG_RENDERER );
	r_hybrid1_dlightShadows = ri.Cvar_Get( "r_hybrid1_dlightShadows", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_dlightShadows, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_dlightShadows, "Hybrid1: RT shadows for top-N Forward+ dlights (0=off, 1-4). Falls back to first refdef dlight if FP SSBO empty." );
	ri.Cvar_SetGroup( r_hybrid1_dlightShadows, CVG_RENDERER );
	r_hybrid1_shadowStrength = ri.Cvar_Get( "r_hybrid1_shadowStrength", "0.85", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_shadowStrength, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_shadowStrength, "Hybrid1 composite: shadow visibility blend (1=full RT shadow)." );
	ri.Cvar_SetGroup( r_hybrid1_shadowStrength, CVG_RENDERER );
	r_hybrid1_specStrength = ri.Cvar_Get( "r_hybrid1_specStrength", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_specStrength, "0", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_specStrength, "Hybrid1 composite: additive denoised specular weight." );
	ri.Cvar_SetGroup( r_hybrid1_specStrength, CVG_RENDERER );
	r_hybrid1_debug = ri.Cvar_Get( "r_hybrid1_debug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_debug, "0", "12", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_debug,
		"Hybrid1 debug: 0=composite 1=filtered shadow 2=spec 3=shadow angle 4=diffuse 5=surfel "
		"6=raw RT shadow 7=hitDist 8=TLAS coverage 9=alphaCandidates 10=histWeight 11=reject 12=diff." );
	ri.Cvar_SetGroup( r_hybrid1_debug, CVG_RENDERER );
	r_hybrid1_diffuse = ri.Cvar_Get( "r_hybrid1_diffuse", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_diffuse, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_diffuse, "Hybrid1: trace and denoise indirect diffuse GI (A-trous only, no variance)." );
	ri.Cvar_SetGroup( r_hybrid1_diffuse, CVG_RENDERER );
	r_hybrid1_diffuseStrength = ri.Cvar_Get( "r_hybrid1_diffuseStrength", "1.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_diffuseStrength, "0", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_hybrid1_diffuseStrength, "Hybrid1 composite: additive indirect diffuse weight (multiplied by G-buffer albedo)." );
	ri.Cvar_SetGroup( r_hybrid1_diffuseStrength, CVG_RENDERER );
	r_hybrid1_ibl = ri.Cvar_Get( "r_hybrid1_ibl", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_ibl, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_ibl, "Hybrid1: sample prefiltered/irradiance cubemaps on RT miss and secondary hits." );
	ri.Cvar_SetGroup( r_hybrid1_ibl, CVG_RENDERER );
	r_hybrid1_taa = ri.Cvar_Get( "r_hybrid1_taa", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_taa, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_taa,
		"Legacy hint only: Hybrid1 keeps separate SVGF channel histories and does not drive world taa_history. "
		"Use r_aaMode 4/5 (or r_taa 1) for presentation Temporal Reconstruction after Hybrid1 composite." );
	ri.Cvar_SetGroup( r_hybrid1_taa, CVG_RENDERER );
	r_hybrid1_motion = ri.Cvar_Get( "r_hybrid1_motion", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_motion, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_motion, "Hybrid1 temporal: use screen-space motion vectors for history reprojection when available." );
	ri.Cvar_SetGroup( r_hybrid1_motion, CVG_RENDERER );
	r_hybrid1_restir = ri.Cvar_Get( "r_hybrid1_restir", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_hybrid1_restir, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_hybrid1_restir,
		"Hybrid1 ReSTIR DI scaffold (P3): 0=off, 1=allocate temporal reservoir SSBO ping-pong (shade pass TBD). Quality preset 3 enables this." );
	ri.Cvar_SetGroup( r_hybrid1_restir, CVG_RENDERER );
	if ( r_hybrid1 && r_hybrid1->integer ) {
		ri.Printf( PRINT_ALL,
			"[VK][Hybrid1] r_hybrid1=1 (latched; USE_VULKAN_RTX, r_rtxDemo 1 + r_fbo 1, vid_restart; r_hybrid1Quality 0=custom/1=perf/2=balanced/3=quality)\n" );
		if ( r_hybrid1_restir && r_hybrid1_restir->integer ) {
			ri.Printf( PRINT_ALL,
				"[VK][Hybrid1] r_hybrid1_restir=1 (ReSTIR DI scaffold; hybrid1_status for reservoir buffer)\n" );
		}
	}
	r_vdbFog = ri.Cvar_Get( "r_vdbFog", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vdbFog, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vdbFog, "Blend bound VDB fog density (\\vdb_bind_fog) into volumetric global density when uploaded to GPU. Requires \\r_volumetricFog 1." );
	ri.Cvar_SetGroup( r_vdbFog, CVG_RENDERER );
	r_vdbFogBlend = ri.Cvar_Get( "r_vdbFogBlend", "0.5", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vdbFogBlend, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_vdbFogBlend, "VDB density blend weight when \\r_vdbFog 1." );
	ri.Cvar_SetGroup( r_vdbFogBlend, CVG_RENDERER );
	r_forwardPlus = ri.Cvar_Get( "r_forwardPlus", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_forwardPlus, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_forwardPlus, "Forward+ (default 1 on Vulkan): device-local light SSBO + per-tile cull (16px tiles; \\r_forwardPlusMaxPerTile 4-8). Packs up to 64 refdef dlights on GPU; tess.dlightBits skip applies to indices 0-31 only. PBR: \\r_forwardPlusDebug, \\r_forwardPlusShade. r_renderMode 2 forces this on. See docs/FORWARD_PLUS_PIPELINE_AUDIT.md." );
	ri.Cvar_SetGroup( r_forwardPlus, CVG_RENDERER );
	r_forwardPlusMaxPerTile = ri.Cvar_Get( "r_forwardPlusMaxPerTile", "8", CVAR_ARCHIVE_ND | CVAR_LATCH );
	{
		char fp_min[12];
		char fp_max[12];
		Com_sprintf( fp_min, sizeof( fp_min ), "%u", vk_forward_plus_get_min_per_tile_cap() );
		Com_sprintf( fp_max, sizeof( fp_max ), "%u", vk_forward_plus_get_max_per_tile_cap() );
		ri.Cvar_CheckRange( r_forwardPlusMaxPerTile, fp_min, fp_max, CV_INTEGER );
	}
	ri.Cvar_SetDescription( r_forwardPlusMaxPerTile, "Forward+ tile list length per 16px tile (min VK_FP_MIN_PER_TILE, max VK_FP_MAX_PER_TILE in vk_forward_plus.c; latched). Lower values reduce GPU work when \\r_forwardPlus 1. Requires vid_restart after change." );
	ri.Cvar_SetGroup( r_forwardPlusMaxPerTile, CVG_RENDERER );
	r_forwardPlusDebug = ri.Cvar_Get( "r_forwardPlusDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_forwardPlusDebug, "0", "3", CV_FLOAT );
	ri.Cvar_SetDescription( r_forwardPlusDebug,
		"Forward+ / clustered debug overlay:\n"
		" 0 = off\n"
		" 0.08–1.0 = lights-per-cluster occupancy heatmap (uses Z-slice when r_forwardPlusZSlices>1)\n"
		" 2 = Z-slice ID colors\n"
		"Requires \\r_forwardPlus 1." );
	ri.Cvar_SetGroup( r_forwardPlusDebug, CVG_RENDERER );
	r_forwardPlusShade = ri.Cvar_Get( "r_forwardPlusShade", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_forwardPlusShade, "0", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_forwardPlusShade, "PBR Forward+ diffuse+spec from tile-culled dynamic lights (0=off). Skips packed indices in \\r_forwardPlus tess.dlightBits mask (first 32). Primary direct is softly scaled vs Forward+ energy. Works with deluxe/lightmap; rebuilds pipelines when changed. Requires \\r_forwardPlus 1." );
	ri.Cvar_SetGroup( r_forwardPlusShade, CVG_RENDERER );
	r_forwardPlusOverflowShade = ri.Cvar_Get( "r_forwardPlusOverflowShade", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_forwardPlusOverflowShade, "0", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_forwardPlusOverflowShade,
		"PBR Forward+ shade for dlight indices 32..63 (beyond classic tess.dlightBits). "
		"No pipeline rebuild; passed per draw via pbrForwardPlus.x. Requires r_classicLighting 0. "
		"Try 0.5 when r_forwardPlusOverflowShade is enabled with modern lighting." );
	ri.Cvar_SetGroup( r_forwardPlusOverflowShade, CVG_RENDERER );
	if ( r_forwardPlusOverflowShade && r_forwardPlusOverflowShade->value > 0.0f && r_forwardPlus && r_forwardPlus->integer &&
		r_classicLighting && !r_classicLighting->integer ) {
		ri.Printf( PRINT_ALL, "[VK][Forward+] r_forwardPlusOverflowShade=%.2f (lights 32..%d)\n",
			r_forwardPlusOverflowShade->value, VK_FP_MAX_GPU_LIGHTS - 1 );
	}
	r_forwardPlusLuminanceSort = ri.Cvar_Get( "r_forwardPlusLuminanceSort", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_forwardPlusLuminanceSort, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_forwardPlusLuminanceSort,
		"When 1 and a tile has more overlapping lights than \\r_forwardPlusMaxPerTile, the compute pass keeps the brightest by RGB sum (approximate importance). When 0, first light index order wins (legacy). Requires \\r_forwardPlus 1 (no vid_restart)." );
	ri.Cvar_SetGroup( r_forwardPlusLuminanceSort, CVG_RENDERER );
	r_forwardPlusDistanceSort = ri.Cvar_Get( "r_forwardPlusDistanceSort", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_forwardPlusDistanceSort, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_forwardPlusDistanceSort,
		"When 1 and a tile is overloaded, the compute pass prefers lights nearest the camera (vieworg). When 0, overload order follows \\r_forwardPlusLuminanceSort / index order. Requires \\r_forwardPlus 1 (no vid_restart)." );
	ri.Cvar_SetGroup( r_forwardPlusDistanceSort, CVG_RENDERER );
	r_forwardPlusDepthCull = ri.Cvar_Get( "r_forwardPlusDepthCull", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_forwardPlusDepthCull, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_forwardPlusDepthCull,
		"When 1, a depth prepass fills the depth buffer, then tile cull rejects lights behind the nearest surface in each tile (lightVolumeDepthCull), then opaque color draws. When 0, cull runs at view start without depth (legacy). Requires \\r_forwardPlus 1 (no vid_restart). modern_vulkan.cfg sets 1." );
	ri.Cvar_SetGroup( r_forwardPlusDepthCull, CVG_RENDERER );
	r_forwardPlusHiZ = ri.Cvar_Get( "r_forwardPlusHiZ", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_forwardPlusHiZ, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_forwardPlusHiZ,
		"When 1 with \\r_forwardPlusDepthCull 1, expand depth probes (forwardPlusHiZPyramid) for hierarchical occlusion of large lights. Default 1." );
	ri.Cvar_SetGroup( r_forwardPlusHiZ, CVG_RENDERER );
	r_forwardPlusZSlices = ri.Cvar_Get( "r_forwardPlusZSlices", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_forwardPlusZSlices, "1", "16", CV_INTEGER );
	ri.Cvar_SetDescription( r_forwardPlusZSlices,
		"Forward+ / Unified Clustered Z-slice count. 1 = 2D tiled light lists (legacy). "
		"2-16 = depth-partitioned frustum clusters shared by deferred, Forward+, and OIT. Latched; vid_restart." );
	ri.Cvar_SetGroup( r_forwardPlusZSlices, CVG_RENDERER );
	r_forwardPlusZSliceMode = ri.Cvar_Get( "r_forwardPlusZSliceMode", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_forwardPlusZSliceMode, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_forwardPlusZSliceMode,
		"Z-cluster depth partitioning: 0=linear view depth, 1=logarithmic (default). Requires \\r_forwardPlusZSlices > 1." );
	ri.Cvar_SetGroup( r_forwardPlusZSliceMode, CVG_RENDERER );
	if ( r_forwardPlusZSlices && r_forwardPlusZSlices->integer > 1 && r_forwardPlus && r_forwardPlus->integer ) {
		ri.Printf( PRINT_ALL, "[VK][Forward+] Z-clustered light grid: %d slices (%s)\n",
			r_forwardPlusZSlices->integer,
			( r_forwardPlusZSliceMode && r_forwardPlusZSliceMode->integer ) ? "log" : "linear" );
	}
	/* Clustered Hybrid M1 aliases — same light grid as Forward+ (docs/RENDERER_PATH_OWNERSHIP.md). */
	{
		cvar_t *clusterZ = ri.Cvar_Get( "r_clusterZSlices", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
		cvar_t *clusterDebug = ri.Cvar_Get( "r_clusterDebug", "-1", CVAR_ARCHIVE_ND );
		cvar_t *clusterTile = ri.Cvar_Get( "r_clusterTileSize", "16", CVAR_ROM );
		ri.Cvar_SetDescription( clusterZ,
			"Alias for r_forwardPlusZSlices (shared deferred/Forward+/OIT cluster grid). "
			"When set > 0 at latch, copies into r_forwardPlusZSlices. See docs/RENDERER_PATH_OWNERSHIP.md." );
		ri.Cvar_SetDescription( clusterDebug,
			"Alias for r_forwardPlusDebug. When >= 0, copies into r_forwardPlusDebug each frame start." );
		ri.Cvar_SetDescription( clusterTile,
			"Shared cluster tile size in pixels (fixed 16; must match Forward+ cull). Read-only." );
		if ( clusterTile && clusterTile->integer != 16 ) {
			ri.Cvar_Set( "r_clusterTileSize", "16" );
		}
		if ( clusterZ && clusterZ->integer > 0 && r_forwardPlusZSlices ) {
			char buf[16];
			Com_sprintf( buf, sizeof( buf ), "%d", clusterZ->integer );
			ri.Cvar_Set( "r_forwardPlusZSlices", buf );
			r_forwardPlusZSlices->integer = clusterZ->integer;
			r_forwardPlusZSlices->modified = qtrue;
			ri.Printf( PRINT_ALL, "[VK][cluster] r_clusterZSlices=%d → r_forwardPlusZSlices\n", clusterZ->integer );
		}
	}
	r_forwardPlusSpecularStrength = ri.Cvar_Get( "r_forwardPlusSpecularStrength", "0.65", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_forwardPlusSpecularStrength, "0", "4", CV_FLOAT );
	ri.Cvar_SetDescription( r_forwardPlusSpecularStrength,
		"Forward+ dynamic specular scale (default 0.65 preserves prior art balance). Mirrors r_deferredSpecularStrength for the mode-2 path. Requires r_forwardPlusShade > 0 (no vid_restart)." );
	ri.Cvar_SetGroup( r_forwardPlusSpecularStrength, CVG_RENDERER );
	r_forwardPlusEnergyRenorm = ri.Cvar_Get( "r_forwardPlusEnergyRenorm", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_forwardPlusEnergyRenorm, "0", "2", CV_FLOAT );
	ri.Cvar_SetDescription( r_forwardPlusEnergyRenorm,
		"Legacy primary×Forward+ renorm (0=off, recommended). When r_forwardPlusShade > 0 the classic projector is skipped so Forward+ owns dynamics; renorm is unused. Non-zero restores soft attenuate of primaryDirect when both paths were stacked." );
	ri.Cvar_SetGroup( r_forwardPlusEnergyRenorm, CVG_RENDERER );
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
	ri.Cvar_CheckRange( r_temporalDebug, "0", "35", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalDebug,
		"Temporal ghosting diagnostics (see docs/RENDERER_TEMPORAL_GHOSTING.md):\n"
		" 0 off\n"
		" 1 final motion vectors (velocity)\n"
		" 2 depth / history rejection\n"
		" 3 temporal history weight\n"
		" 4 disocclusion / reactive mask\n"
		" 5 weapon depth mask (TAA near-weapon or SSR weapon-range depth)\n"
		" 6 current vs history contribution\n"
		" 7–13 history UV / variance / NaN\n"
		" 14 pre-weapon world-resolve velocity\n"
		" 15 prior-class-gated world-resolve velocity\n"
		" 16–27 Architecture B class/velocity/reactive/confidence views (weapon resolve)\n"
		" 28 raw stored velocity (world)\n"
		" 29 velocity as UV\n"
		" 30 velocity as pixels (abs/64)\n"
		" 31 history UV displacement\n"
		" 32 velocity error ratio vs matrix reprojection (green=1x yellow=2x red=4x)\n"
		" 33 previous-matrix temporal age (green=1 red>1)\n"
		" 34 temporal resolves last frame (green=1 red>1)\n"
		" 35 reprojection correspondence (cyan=current magenta=history)\n"
		"Any non-zero also logs temporal reset reasons (developer)." );
	{
		cvar_t *resDbg = ri.Cvar_Get( "r_temporalResolutionDebug", "0", CVAR_TEMP );
		cvar_t *velProbe = ri.Cvar_Get( "r_temporalVelocityProbe", "0", CVAR_TEMP );
		ri.Cvar_CheckRange( resDbg, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( resDbg,
			"Print scene/velocity/TAA/display extents + velocity-space convention each time they change. "
			"Also run temporal_resolution_status." );
		ri.Cvar_CheckRange( velProbe, "0", "2", CV_INTEGER );
		ri.Cvar_SetDescription( velProbe,
			"CPU reprojection probe: 0=off, 1=warn on 2x/4x/0.5x/0.25x scale or stale prev matrices, "
			"2=print measured vs reconstructed displacement every ~60 frames." );
	}
	{
		cvar_t *r_tsr = ri.Cvar_Get( "r_tsr", "1", CVAR_ARCHIVE_ND );
		cvar_t *r_temporalAO = ri.Cvar_Get( "r_temporalAO", "1", CVAR_ARCHIVE_ND );
		cvar_t *r_temporalSSR = ri.Cvar_Get( "r_temporalSSR", "1", CVAR_ARCHIVE_ND );
		cvar_t *r_temporalFog = ri.Cvar_Get( "r_temporalFog", "1", CVAR_ARCHIVE_ND );
		cvar_t *r_temporalTransparency = ri.Cvar_Get( "r_temporalTransparency", "1", CVAR_ARCHIVE_ND );
		cvar_t *r_dof = ri.Cvar_Get( "r_dof", "0", CVAR_ARCHIVE_ND );

		ri.Cvar_CheckRange( r_tsr, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( r_tsr,
			"Master enable for Temporal Super-Resolution / present adaptive reconstruction "
			"(aaMode 3–5 and upscale temporal). 0 disables without changing r_taa." );
		ri.Cvar_SetGroup( r_tsr, CVG_RENDERER );

		ri.Cvar_CheckRange( r_temporalAO, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( r_temporalAO, "Master enable for SSAO (temporal AO consumer). 0 skips the SSAO pass." );
		ri.Cvar_SetGroup( r_temporalAO, CVG_RENDERER );

		ri.Cvar_CheckRange( r_temporalSSR, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( r_temporalSSR,
			"Master enable for SSR as a temporal-adjacent consumer. 0 disables SSR even when r_ssr 1 "
			"(use for weapon-trail bisect; see docs/RENDERER_TEMPORAL_GHOSTING.md)." );
		ri.Cvar_SetGroup( r_temporalSSR, CVG_RENDERER );

		ri.Cvar_CheckRange( r_temporalFog, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( r_temporalFog,
			"Master enable for volumetric froxel temporal history. 0 forces temporal weight to 0." );
		ri.Cvar_SetGroup( r_temporalFog, CVG_RENDERER );

		ri.Cvar_CheckRange( r_temporalTransparency, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( r_temporalTransparency,
			"Master enable for transparency reactive-mask stamping into Temporal Reconstruction. "
			"0 skips the stamped OIT/transparent reactive path." );
		ri.Cvar_SetGroup( r_temporalTransparency, CVG_RENDERER );

		ri.Cvar_CheckRange( r_dof, "0", "1", CV_INTEGER );
		ri.Cvar_SetDescription( r_dof, "Alias for r_depthOfField (thin-lens DoF in the gamma post pass)." );
		ri.Cvar_SetGroup( r_dof, CVG_RENDERER );
		if ( r_dof->integer && ri.Cvar_VariableIntegerValue( "r_depthOfField" ) == 0 ) {
			ri.Cvar_Set( "r_depthOfField", "1" );
		}

		ri.Printf( PRINT_ALL,
			"[VK][temporal] independent gates: taa=%d tsr=%d ao=%d ssr=%d fog=%d transparency=%d "
			"motionBlur=%d dof=%d bloom=%d sharpen=%s (debug=%d)\n",
			ri.Cvar_VariableIntegerValue( "r_taa" ),
			r_tsr->integer,
			r_temporalAO->integer,
			r_temporalSSR->integer,
			r_temporalFog->integer,
			r_temporalTransparency->integer,
			ri.Cvar_VariableIntegerValue( "r_motionBlur" ),
			ri.Cvar_VariableIntegerValue( "r_depthOfField" ),
			ri.Cvar_VariableIntegerValue( "r_bloom" ),
			ri.Cvar_VariableString( "r_sharpen" ),
			r_temporalDebug->integer );
	}
	r_temporalCustomShaderMotion = ri.Cvar_Get( "r_temporalCustomShaderMotion", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_temporalCustomShaderMotion, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_temporalCustomShaderMotion,
		"Temporal motion vectors: 0 (default) treats customShader entities as motion-unreliable (prev MVP = current; TAA-safe).\n"
		"1 allows prev-model matrices for customShader RT_MODEL draws — only enable for materials that do not animate vertices per frame." );
	ri.Cvar_SetGroup( r_temporalCustomShaderMotion, CVG_RENDERER );
	if ( r_temporalCustomShaderMotion && r_temporalCustomShaderMotion->integer ) {
		ri.Printf( PRINT_ALL, "[VK][temporal] r_temporalCustomShaderMotion=1 (customShader entities use prev-model motion; may ghost if stages deform)\n" );
	}
	if ( r_temporalCpuSkinPrev && !r_temporalCpuSkinPrev->integer ) {
		ri.Printf( PRINT_ALL, "[VK][temporal] r_temporalCpuSkinPrev=0 (conservative whole-frame motion invalidation on spawning animated entities)\n" );
	}
	R_RendererPrintCompatibilityWarnings( qfalse );
	#endif // USE_VULKAN

	// Register modular subsystem cvars
	CBTerrain_RegisterCvars();
	VK_Biome_Init();
	VK_VegGpu_Init();
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
#ifdef USE_VK_PBR
	R_PBR_ResetBindLog();
#endif

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
	R_SpriteProps_Init();
	R_DecalProps_Init();
	R_Upscale_Init();
	R_VT_Init();
	R_Meshlets_Init();
	vk_gpu_scene_init();
	vk_sky_owner_init();
	vk_weather_init();
	vk_volumetric_clouds_init();
	vk_material_ir_init();
	vk_material_graph_init();
	vk_material_instance_init();
	vk_material_cache_init();
	vk_surface_evolution_init();
	vk_vshadow_init();
	vk_present_color_init();
	vk_exposure_histogram_init();
	vk_cinematic_camera_init();
	vk_capture_pipeline_init();
	vk_color_grade_init();
	vk_reference_lab_init();
	vk_frequency_aware_init();
	vk_spatial_aa_init();
	vk_scene_platform_init();
	vk_ltc_init();
	vk_photometric_init();
	vk_ht_throughput_init();
	vk_ht_animation_init();
	if ( vk_ltc_uploaded() ) {
		ri.Printf( PRINT_DEVELOPER, "[VK] Photometric LTC GPU path ready\n" );
	}
#ifdef USE_VULKAN
	R_NDGI_Init();
	R_NIV_Init();
	R_NSLM_Init();
	R_NIST_Init();
	R_NVC_Init();
	R_FSA_Init();
	R_VFGI_Init();
	R_RenderFormer_Init();
	R_WPT_Init();
	R_VUDA_Init();
	R_GRTX_Init();
	R_MGS_Init();
	R_VKSplat_Init();
	R_CuRast_Init();
	R_GraphBfs_Init();
	R_Mimir_Init();
	R_Iris_Init();
	R_SQZ_Init();
	R_WSP_Init();
	R_Dressi_Init();
	R_Raygun_Init();
#ifdef USE_EXPERIMENTAL_RENDERERS
	ri.Printf( PRINT_ALL,
		"[VK] Experimental renderers linked (NDGI/NIV/NSLM/NIST/NVC/FSA/VFGI/RenderFormer/WPT/GRTX/VkSplat/...); enable r_* + vid_restart. See docs/NEURAL_RENDERER_PHASES.md\n" );
#else
	/* Individual stub lines come from vk_experimental_renderer_stubs.c */
#endif
#ifdef USE_VULKAN_RTX
	ri.Printf( PRINT_ALL,
		"[VK][RTX] USE_VULKAN_RTX=ON (Hybrid1/Raygun/pathtrace scaffolds linked; latch r_rtx/r_hybrid1/r_raygun before vid_restart)\n" );
#endif
#endif
	R_ApplyRenderModeLatch();
#ifdef USE_VULKAN
	R_RenderPath_RegisterCvars();
#endif
	VK_RasterUltra_Enforce();
	vk_aa_policy_apply();
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
	ri.Printf( PRINT_ALL, "[VK][gltf] GPU tangent mode: %d (r_gltfGpuTangentFix 0=bind 1=Gram–Schmidt 2=topology)\n",
		r_gltfGpuTangentFix ? r_gltfGpuTangentFix->integer : 1 );


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
	vk_surf_log_temporal_config();
	vk_ui_blur_init();
#endif

	R_InitShaders();
	R_VectorFont_Init();

	R_InitSkins();

	R_MeshNormalPolicy_Init();
	VK_RasterUltra_Init();
	VK_RasterUltra_Enforce();
	VK_SunCSM_Init();
	R_ModelInit();
	R_AuthoredFlares_Init();

	R_InitFreeType();

	R_BspStream_Init();
	R_ArcBlanc_Init();
	R_Emulator_Init();
	R_Webcam_Init();

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

	vk_ui_blur_shutdown();
	R_MeshNormalPolicy_Shutdown();
	VK_SunCSM_Shutdown();
	VK_RasterUltra_Shutdown();
	vk_spatial_aa_shutdown();
	vk_scene_platform_shutdown();
	vk_photometric_shutdown();
	vk_ltc_shutdown();
	vk_ht_throughput_shutdown();
	vk_ht_animation_shutdown();
	vk_frequency_aware_shutdown();
	VK_VegGpu_Shutdown();
	VK_Biome_Shutdown();
	CBTerrain_OnWorldUnload();
	vk_scene_platform_on_world_unload();
	vk_gpu_scene_on_world_unload();

	ri.Cmd_RemoveCommand( "modellist" );
	ri.Cmd_RemoveCommand( "screenshotBMP" );
	ri.Cmd_RemoveCommand( "screenshotJPEG" );
	ri.Cmd_RemoveCommand( "screenshotEXR" );
	ri.Cmd_RemoveCommand( "screenshot" );
	ri.Cmd_RemoveCommand( "imagelist" );
	ri.Cmd_RemoveCommand( "shaderlist" );
	ri.Cmd_RemoveCommand( "skinlist" );
	ri.Cmd_RemoveCommand( "gfxinfo" );
	ri.Cmd_RemoveCommand( "shaderstate" );
#ifdef USE_VULKAN
	ri.Cmd_RemoveCommand( "vkinfo" );
	ri.Cmd_RemoveCommand( "vulkaninfo" );
	ri.Cmd_RemoveCommand( "renderer_status" );
	ri.Cmd_RemoveCommand( "havenrp_renderer_status" );
	ri.Cmd_RemoveCommand( "renderer_profile" );
	ri.Cmd_RemoveCommand( "ui_blur_status" );
	ri.Cmd_RemoveCommand( "renderer_health" );
	ri.Cmd_RemoveCommand( "renderer_deferred_safe" );
	ri.Cmd_RemoveCommand( "renderer_modern_safe" );
	ri.Cmd_RemoveCommand( "renderer_clustered_safe" );
	ri.Cmd_RemoveCommand( "renderer_spine_1_1_cert" );
	ri.Cmd_RemoveCommand( "spine_1_1_stress" );
	ri.Cmd_RemoveCommand( "spine_1_1_focus_pulse" );
	ri.Cmd_RemoveCommand( "spine_1_1_stress_report" );
	ri.Cmd_RemoveCommand( "renderer_subsystems" );
	ri.Cmd_RemoveCommand( "renderer_compat" );
	ri.Cmd_RemoveCommand( "renderer_compatibility" );
	ri.Cmd_RemoveCommand( "vkVolumetricValidate" );
	ri.Cmd_RemoveCommand( "r_aaQuality" );
	ri.Cmd_RemoveCommand( "r_dumpTemporalState" );
	ri.Cmd_RemoveCommand( "r_captureTemporalDebug" );
	ri.Cmd_RemoveCommand( "r_printWeaponPresentation" );
	ri.Cmd_RemoveCommand( "surf_validateTemporalConfig" );
	ri.Cmd_RemoveCommand( "r_printViewmodelProjection" );
#endif

	//if ( tr.registered ) {
		//R_IssuePendingRenderCommands();
#ifdef USE_VULKAN
	R_VT_Shutdown();
#endif
		R_DeleteTextures();
	//}

#ifdef USE_IMGUI
	VkImgui_Shutdown();
#endif

#ifdef USE_VULKAN
	R_NDGI_Shutdown();
	R_NIV_Shutdown();
	R_NSLM_Shutdown();
	R_NIST_Shutdown();
	R_NVC_Shutdown();
	R_FSA_Shutdown();
	R_VFGI_Shutdown();
	R_RenderFormer_Shutdown();
	R_WPT_Shutdown();
	R_VUDA_Shutdown();
	R_GRTX_Shutdown();
	R_SQZ_Shutdown();
	R_MGS_Shutdown();
	R_VKSplat_Shutdown();
	R_GraphBfs_Shutdown();
	R_CuRast_Shutdown();
	R_Mimir_Shutdown();
	R_Iris_Shutdown();
	R_WSP_Shutdown();
	vk_release_resources();
#endif

	R_DoneFreeType();

	R_Emulator_Shutdown();
	R_Webcam_Shutdown();

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

	/*
	 * Drop renderer-owned zone pointers before FreeAll. RGI probes are allocated
	 * with TAG_RENDERER; FreeAll frees them by tag walk but leaves rgi.probes as a
	 * dangling pointer. The next map's vk_raster_gi_invalidate() would then
	 * Z_Free a recycled block (often a pk3 pack_t) and corrupt fs_searchpaths.
	 */
	vk_raster_gi_invalidate();

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
=============
RE_NotifyWindowRestored

Client focus / un-minimize / keep-window vid_restart → presentation sticky reset.
=============
*/
static void RE_NotifyWindowRestored( const char *reason )
{
	vk_presentation_note_window_restored( reason ? reason : "client" );
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
	re.BspStreamMergeSector = RE_BspStream_MergeSector;
	re.BspStreamUnmergeSector = RE_BspStream_UnmergeSector;
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
	re.AddEngineSpriteToScene = RE_AddEngineSpriteToScene;
	re.AddEngineSpriteToSceneAtTime = RE_AddEngineSpriteToSceneAtTime;
	re.AddEngineDecalToScene = RE_AddEngineDecalToScene;
	re.ArcBlancUploadHeightMap = RE_ArcBlancUploadHeightMap;
	re.ArcBlancGpuOceanStep = RE_ArcBlancGpuOceanStep;
	re.EmulatorUploadFrame = RE_EmulatorUploadFrame;
	re.WebcamUploadFrame = RE_WebcamUploadFrame;
	re.NotifyWindowRestored = RE_NotifyWindowRestored;
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
	re.ClearTrueTypeFontCache = RE_ClearTrueTypeFontCache;
	re.GetFontKerning = RE_GetFontKerning;
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
	re.DrawStretchPicEx = RE_StretchPicEx;
	re.DrawStretchPicSubpixel = RE_StretchPicSubpixel;
	re.LoadVectorFont = RE_LoadVectorFont;
	re.VectorFontActive = RE_VectorFontActive;
	re.DrawVectorString = RE_DrawVectorString;
	re.DrawVectorGlyph = RE_DrawVectorGlyph;
	re.UIBackdropBlur = RE_UIBackdropBlur;
	re.UIFilterLayer = RE_UIFilterLayer;

#ifdef USE_VUDA
	re.VudaActive = R_VUDA_Active;
	re.VudaInteropReady = R_VUDA_InteropReady;
	re.VudaGetExportBundle = R_VUDA_GetExportBundle;
	re.VudaConsumeComputeWindow = vk_vuda_consume_compute_window;
	re.VudaNotifyCudaComplete = vk_vuda_notify_cuda_complete;
#endif

	return &re;
}
