/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Deferred G-buffer sidecar (r_renderMode 1/2/3) + lighting (mode 1/3).
Mode 3 = Unified Clustered Renderer (deferred opaque + Forward+ transparent).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_deferred_gbuffer.h"
#include "vk_render_path.h"
#include "tr_render_mode_vk.h"
#include "vk_visibility_buffer.h"
#include "vk_vrcs.h"
#include "vk_image_layout.h"
#include "vk_post_fog.h"
#include "vk_render_pass.h"
#include "vk_scene_pass.h"
#include "vk_pass_registry.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_frequency_aware.h"
#include "vk_ltc.h"
#include "vk_forward_plus.h"
#include "vk_shadow_contract.h"
#include "vk_sun_csm.h"
#include "vk_day_night.h"
#include "vk_deferred_honesty.h"
#include "vk_skybox_hdr.h"

static void vk_dgb_validate_compute_break( const char *stage, qboolean resume_main )
{
	if ( !r_fboDebug || r_fboDebug->integer < 1 || !vk_post_fog_fbo_debug_throttle() ) {
		return;
	}

	if ( vk.inRenderPass ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][deferred] %s: expected out-of-pass compute/transfer window, still in render pass %d\n",
			stage ? stage : "compute_break", (int)vk.renderPassIndex );
	}

	if ( resume_main && vk.renderPassIndex != RENDER_PASS_MAIN ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][deferred] %s: resume_main requested but renderPassIndex=%d instead of main\n",
			stage ? stage : "compute_break", (int)vk.renderPassIndex );
	}

	if ( vk.cmd == NULL || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][deferred] %s: command buffer unavailable during deferred compute break\n",
			stage ? stage : "compute_break" );
	}
}

typedef struct {
	vec4_t projInfo;
	vec4_t materialParams;
	uint32_t extent[2];
	uint32_t gbufferFlags;
} vk_deferred_gbuf_push_t;

typedef struct {
	int mode;
} vk_deferred_gbuf_debug_push_t;

typedef struct {
	float viewMatrix[16];
	float projInfo[4];
	uint32_t extent[2];
	uint32_t tileGrid[2];
	float strength;
	uint32_t maxPerTile;
	uint32_t numLights;
	uint32_t additive;
	uint32_t specular;
	float aoStrength;
	float specularStrength;
	uint32_t normalsAreWorld;
	uint32_t useMaterialClass;
	uint32_t zSlices;
	uint32_t zSliceMode;
	float zNear;
	float zFar;
	float specularAA; /* normal-variance roughness inflate (parity with Forward+ ApplySpecularAA) */
	uint32_t compactLists;
	uint32_t clusterCount;
	uint32_t gbufferCompact; /* 1: decode octahedral from material.ba; AO/clearcoat defaults */
	uint32_t mixedMaterial; /* 1: MIXED_MATERIAL_DEFERRED — LM + owner from SurfaceData */
	uint32_t shadowFlags;
	float shadowStrength;
	float shadowNear;
	float shadowSplits[4]; /* scalar-packed to match deferred_lighting.comp */
	float shadowBlend;
	uint32_t shadowCascadeCount; /* 1..4 CSM cascades */
	float forwardPlusDebug; /* deferred clustered occupancy blend; shadow generation is not shader-consumed */
	/* Milestone 3: directional sun BRDF (world-space). */
	float sunDir[4];      /* xyz = L (toward sun), w = angular radius (rad, 0=dirac) */
	float sunRadiance[4]; /* rgb = radiance, w = flags: bit0=BRDF enable, bit1=LM owns diffuse */
	uint32_t iblFlags;    /* bit0: sky IBL enable */
	float iblStrength;
	uint32_t lightmapMode; /* 0=irradiance, 1=deluxe approximation, 2=compare */
	float lightmapDeluxeStrength;
} vk_deferred_light_push_t;

typedef struct {
	uint32_t additive;
	uint32_t hybridCompare;
	uint32_t mixedMaterial;
} vk_deferred_composite_push_t;

static_assert( sizeof( vk_deferred_light_push_t ) % 4 == 0, "deferred light push align" );
static_assert( sizeof( vk_deferred_light_push_t ) <= 256, "deferred light push exceeds common PC limit" );
static_assert( offsetof( vk_deferred_light_push_t, shadowSplits ) == 180,
	"deferred push shadow split ABI changed" );
static_assert( offsetof( vk_deferred_light_push_t, forwardPlusDebug ) == 204,
	"deferred push debug ABI changed" );
static_assert( offsetof( vk_deferred_light_push_t, sunDir ) == 208,
	"deferred push sun ABI changed" );
static_assert( sizeof( vk_deferred_light_push_t ) == 256,
	"deferred push size changed; update GLSL offsets and pipeline range" );

static qboolean s_gbufferCompactDualWriteLogged;
/* Bump when deferred lighting descriptor/push layout changes. */
static const uint32_t s_deferredLightingLayoutVersion = 18u;
static uint32_t s_deferredLightingLayoutBuilt;
static void vk_dgb_create_pipeline( void );
static void vk_dgb_create_lighting_pipeline( void );
static void vk_dgb_create_composite_gfx_pipeline( void );
static void vk_dgb_create_debug_gfx_pipeline( void );

static void vk_dgb_destroy_composite_gfx_pipeline( void )
{
	if ( vk.deferred_gbuffer.composite_gfx_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.deferred_gbuffer.composite_gfx_pipeline, NULL );
		vk.deferred_gbuffer.composite_gfx_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.composite_gfx_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.deferred_gbuffer.composite_gfx_pipeline_layout, NULL );
		vk.deferred_gbuffer.composite_gfx_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.composite_gfx_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.deferred_gbuffer.composite_gfx_layout, NULL );
		vk.deferred_gbuffer.composite_gfx_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.composite_gfx_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.deferred_gbuffer.composite_gfx_pool, NULL );
		vk.deferred_gbuffer.composite_gfx_pool = VK_NULL_HANDLE;
	}
	vk.deferred_gbuffer.composite_gfx_descriptor = VK_NULL_HANDLE;
	vk.deferred_gbuffer.composite_gfx_ready = qfalse;
	vk.deferred_gbuffer.composite_create_failed = qfalse;
}

static void vk_dgb_destroy_lighting_pipeline( void )
{
	if ( vk.deferred_gbuffer.lighting_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.deferred_gbuffer.lighting_pipeline, NULL );
		vk.deferred_gbuffer.lighting_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.lighting_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.deferred_gbuffer.lighting_pipeline_layout, NULL );
		vk.deferred_gbuffer.lighting_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.lighting_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.deferred_gbuffer.lighting_layout, NULL );
		vk.deferred_gbuffer.lighting_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.lighting_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.deferred_gbuffer.lighting_pool, NULL );
		vk.deferred_gbuffer.lighting_pool = VK_NULL_HANDLE;
	}
	vk.deferred_gbuffer.lighting_descriptor = VK_NULL_HANDLE;
	vk.deferred_gbuffer.lighting_pipeline_ready = qfalse;
	vk.deferred_gbuffer.lighting_logged = qfalse;
	vk.deferred_gbuffer.lighting_create_failed = qfalse;
	vk.deferred_gbuffer.composite_logged = qfalse;
}

static void vk_dgb_destroy_debug_gfx_pipeline( void )
{
	if ( vk.deferred_gbuffer.debug_gfx_pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.deferred_gbuffer.debug_gfx_pipeline, NULL );
		vk.deferred_gbuffer.debug_gfx_pipeline = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.debug_gfx_pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.deferred_gbuffer.debug_gfx_pipeline_layout, NULL );
		vk.deferred_gbuffer.debug_gfx_pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.debug_gfx_layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.deferred_gbuffer.debug_gfx_layout, NULL );
		vk.deferred_gbuffer.debug_gfx_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.debug_gfx_pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.deferred_gbuffer.debug_gfx_pool, NULL );
		vk.deferred_gbuffer.debug_gfx_pool = VK_NULL_HANDLE;
	}
	vk.deferred_gbuffer.debug_gfx_descriptor = VK_NULL_HANDLE;
	vk.deferred_gbuffer.debug_gfx_ready = qfalse;
	vk.deferred_gbuffer.debug_create_failed = qfalse;
}

/*
===============
vk_classify_current_view
===============
*/
vkViewClass_t vk_classify_current_view( void )
{
	if ( !tr.world ) {
		return VK_VIEW_CLASS_NO_WORLD;
	}
	if ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {
		return VK_VIEW_CLASS_WEAPON;
	}
	if ( backEnd.viewParms.portalView == PV_PORTAL ) {
		if ( backEnd.viewParms.isSkyPortal ) {
			return VK_VIEW_CLASS_SKY_PORTAL;
		}
		return VK_VIEW_CLASS_PORTAL;
	}
	if ( backEnd.viewParms.portalView == PV_MIRROR ) {
		return VK_VIEW_CLASS_MIRROR;
	}
	if ( backEnd.projection2D ) {
		return VK_VIEW_CLASS_UI;
	}
	return VK_VIEW_CLASS_MAIN_WORLD;
}

const char *vk_view_class_name( vkViewClass_t cls )
{
	switch ( cls ) {
	case VK_VIEW_CLASS_MAIN_WORLD: return "main_world";
	case VK_VIEW_CLASS_PORTAL: return "portal";
	case VK_VIEW_CLASS_SKY_PORTAL: return "sky_portal";
	case VK_VIEW_CLASS_MIRROR: return "mirror";
	case VK_VIEW_CLASS_WEAPON: return "weapon";
	case VK_VIEW_CLASS_NO_WORLD: return "no_world";
	case VK_VIEW_CLASS_UI: return "ui";
	default: return "unknown";
	}
}

qboolean vk_deferred_gbuffer_resources_wanted( void )
{
	if ( !vk.fboActive || !r_renderMode || !r_deferredGBuffer || !r_deferredGBuffer->integer ) {
		return qfalse;
	}
	if ( !R_RenderMode_WantsGBuffer() ) {
		return qfalse;
	}
	return qtrue;
}

qboolean vk_deferred_gbuffer_active( void )
{
	return ( vk.deferredGbufferAllocated && vk_deferred_gbuffer_resources_wanted() ) ? qtrue : qfalse;
}

qboolean vk_deferred_gbuffer_fill_wanted( void )
{
	vkViewClass_t cls;

	if ( !vk_deferred_gbuffer_active() || !r_deferredGBufferFill || !r_deferredGBufferFill->integer ) {
		return qfalse;
	}
	if ( !vk_deferred_gbuffer_generation_valid() ) {
		return qfalse;
	}
	/*
	 * Menu / no-map: no world pointer. Do NOT require doneWorldScene — that flag is
	 * set only *after* geometry capture in RB_DrawSurfs, so gating on it would
	 * permanently skip fill/AV/deferred lighting.
	 */
	if ( !tr.world ) {
		return qfalse;
	}
	cls = vk_classify_current_view();
	/* G-buffer fill is main-world only — never weapon/menu/portal/mirror. */
	if ( cls != VK_VIEW_CLASS_MAIN_WORLD ) {
		return qfalse;
	}
	return qtrue;
}

uint32_t vk_deferred_gbuffer_generation( void )
{
	return vk.deferredGbufferGeneration;
}

qboolean vk_deferred_gbuffer_generation_valid( void )
{
	uint32_t w = 0, h = 0;

	if ( !vk.deferredGbufferAllocated || vk.deferredGbufferGeneration == 0u ) {
		return qfalse;
	}
	vk_get_active_render_extent( &w, &h );
	if ( w == 0u || h == 0u ) {
		w = vk.mainColorWidth;
		h = vk.mainColorHeight;
	}
	if ( w != vk.deferredGbufferExtentW || h != vk.deferredGbufferExtentH ) {
		return qfalse;
	}
	return qtrue;
}

void vk_deferred_gbuffer_note_recreate( const char *reason )
{
	Q_strncpyz( vk.deferredGbufferLastRecreateReason,
		reason && reason[0] ? reason : "unspecified",
		sizeof( vk.deferredGbufferLastRecreateReason ) );
}

void vk_deferred_gbuffer_set_fallback( const char *reason )
{
	const char *msg = reason && reason[0] ? reason : "unknown";

	if ( vk.deferredGbufferFallbackActive &&
		!Q_stricmp( vk.deferredGbufferFallbackReason, msg ) ) {
		return;
	}
	vk.deferredGbufferFallbackActive = qtrue;
	Q_strncpyz( vk.deferredGbufferFallbackReason, msg,
		sizeof( vk.deferredGbufferFallbackReason ) );
	ri.Cvar_Set( "r_havenrpFallbackReason", vk.deferredGbufferFallbackReason );

	/* Keep Forward+ alive with a stable AO owner when experimental AV/G-buffer fails. */
	if ( r_deferredGBufferFill && r_deferredGBufferFill->integer ) {
		ri.Cvar_Set( "r_deferredGBufferFill", "0" );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_ambientVisibilityMode" ) >= 2 ) {
		ri.Cvar_Set( "r_ambientVisibilityMode", "1" );
	}
	if ( r_ssao && !r_ssao->integer ) {
		ri.Cvar_Set( "r_ssao", "1" );
	}

	ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
		"[VK][deferred] fallback: %s (Forward+ continues; G-buffer/AV off → legacy_ssao)\n" S_COLOR_WHITE,
		vk.deferredGbufferFallbackReason );
}

void vk_deferred_gbuffer_clear_fallback( void )
{
	vk.deferredGbufferFallbackActive = qfalse;
	vk.deferredGbufferFallbackReason[0] = '\0';
}

static qboolean vk_dgb_fail_inject( const char *which )
{
	cvar_t *c;

	c = ri.Cvar_Get( "r_dgbFailInject", "0", CVAR_TEMP | CVAR_CHEAT );
	if ( !c || !c->string || !c->string[0] || !Q_stricmp( c->string, "0" ) ) {
		return qfalse;
	}
	if ( !Q_stricmp( c->string, which ) || !Q_stricmp( c->string, "all" ) ) {
		return qtrue;
	}
	return qfalse;
}

/* Pipelines may be built whenever G-buffer resources exist — not gated on per-frame fill. */
static qboolean vk_deferred_lighting_pipelines_wanted( void )
{
	if ( !r_deferredArchitecture ||
		r_deferredArchitecture->integer == DEFERRED_ARCH_FORWARD_PLUS_REFERENCE ) {
		return qfalse;
	}
	if ( !vk_deferred_gbuffer_active() || !r_deferredLighting || !r_deferredLighting->integer ) {
		return qfalse;
	}
	if ( !r_renderMode || !r_forwardPlus || !r_forwardPlus->integer ) {
		return qfalse;
	}
	return R_RenderMode_WantsDeferredLighting();
}

static qboolean vk_deferred_lighting_wanted( void )
{
	return ( vk_deferred_lighting_pipelines_wanted() && vk_deferred_gbuffer_fill_wanted() ) ? qtrue : qfalse;
}

qboolean vk_deferred_lighting_active( void )
{
	return vk_deferred_lighting_wanted();
}

qboolean vk_deferred_lighting_path_ready( void )
{
	/* Readiness is a renderer/resource property, not a property of the view
	 * currently being inspected.  vk_deferred_gbuffer_fill_wanted() correctly
	 * rejects UI/weapon views, but using it here made deferred_status report
	 * pathReady=no whenever the command was printed from the console. */
	if ( !vk_deferred_lighting_pipelines_wanted() ) {
		return qfalse;
	}
	if ( vk.deferredGbufferFallbackActive ) {
		return qfalse;
	}
	if ( !vk.deferredGbufferAllocated ||
		vk.deferred_gbuffer_albedo == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_albedo_view == VK_NULL_HANDLE ||
		vk.deferred_lighting_image == VK_NULL_HANDLE ||
		vk.deferred_lighting_view == VK_NULL_HANDLE ) {
		return qfalse;
	}
	if ( vk.forward_plus.buffer == VK_NULL_HANDLE || vk.forward_plus.tile_buffer == VK_NULL_HANDLE ) {
		return qfalse;
	}
	/* Sticky soft-fails: do not hand off dynamics if lighting/composite cannot run. */
	if ( vk.deferred_gbuffer.lighting_create_failed || vk.deferred_gbuffer.composite_create_failed ) {
		return qfalse;
	}
	return qtrue;
}

qboolean vk_unified_clustered_active( void )
{
	return ( R_RenderMode_IsUnifiedClustered() &&
		vk_deferred_lighting_pipelines_wanted() ) ? qtrue : qfalse;
}

qboolean vk_deferred_opaque_transparent_split( void )
{
	/* Mode 3/4 unified, mode 1 deferred, or mode 5 PT scaffold: opaque→deferred→transparent. */
	if ( vk_unified_clustered_active() ) {
		return qtrue;
	}
	if ( r_renderMode && r_renderMode->integer == 1 && vk_deferred_lighting_pipelines_wanted() ) {
		return qtrue;
	}
	if ( R_RenderMode_IsPathTracedReference() && vk_deferred_lighting_pipelines_wanted() ) {
		return qtrue;
	}
	return qfalse;
}

qboolean vk_unified_clustered_opaque_handoff( void )
{
	/* Opaque world pass: hand dynamics to deferred. Skip weapon/UI views.
	 * Fail open to Forward+ when the deferred path cannot finish (avoids black REPLACE).
	 * Authoritative class comes from R_SelectSurfaceRenderPath. */
	renderPath_t path;

	if ( backEnd.drawSurfFilter != 1 ) {
		return qfalse;
	}
	if ( vk_classify_current_view() != VK_VIEW_CLASS_MAIN_WORLD ) {
		return qfalse;
	}
	if ( !vk_deferred_lighting_path_ready() ) {
		return qfalse;
	}
	path = R_SelectSurfaceRenderPath( tess.shader, NULL, 0u, (int)VK_VIEW_CLASS_MAIN_WORLD );
	if ( !R_RenderPath_WantsDeferredHandoff( path ) ) {
		return qfalse;
	}
	if ( !vk.deferred_gbuffer.handoff_ready_logged ) {
		ri.Printf( PRINT_ALL,
			"[VK][deferred] opaque handoff ready (mode %d) — Forward+ dynamics deferred to compute\n",
			r_renderMode ? r_renderMode->integer : -1 );
		vk.deferred_gbuffer.handoff_ready_logged = qtrue;
	}
	return qtrue;
}

qboolean vk_deferred_unlit_base_wanted( void )
{
	if ( !vk_deferred_lighting_wanted() ) {
		return qfalse;
	}
	if ( !r_deferredUnlitBase || !r_deferredUnlitBase->integer ) {
		return qfalse;
	}
	/* Mode 3: composite still additive; per-draw handoff is via fragment uniform. */
	return qtrue;
}

static void vk_dgb_destroy_pipeline( void )
{
	vk_dgb_destroy_debug_gfx_pipeline();
	vk_dgb_destroy_composite_gfx_pipeline();
	vk_dgb_destroy_lighting_pipeline();
	if ( vk.deferred_gbuffer.pipeline != VK_NULL_HANDLE ) {
		qvkDestroyPipeline( vk.device, vk.deferred_gbuffer.pipeline, NULL );
		vk.deferred_gbuffer.pipeline = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.pipeline_layout != VK_NULL_HANDLE ) {
		qvkDestroyPipelineLayout( vk.device, vk.deferred_gbuffer.pipeline_layout, NULL );
		vk.deferred_gbuffer.pipeline_layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.layout != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorSetLayout( vk.device, vk.deferred_gbuffer.layout, NULL );
		vk.deferred_gbuffer.layout = VK_NULL_HANDLE;
	}
	if ( vk.deferred_gbuffer.pool != VK_NULL_HANDLE ) {
		qvkDestroyDescriptorPool( vk.device, vk.deferred_gbuffer.pool, NULL );
		vk.deferred_gbuffer.pool = VK_NULL_HANDLE;
	}
	vk.deferred_gbuffer.descriptor = VK_NULL_HANDLE;
	vk.deferred_gbuffer.pipeline_ready = qfalse;
	vk.deferred_gbuffer.fill_logged = qfalse;
	vk.deferred_gbuffer.descriptor_generation = 0u;
	vk.deferred_gbuffer.lighting_descriptor_generation = 0u;
	vk.deferred_gbuffer.frame_capture_ok = qfalse;
	vk.deferred_gbuffer.frame_lighting_ok = qfalse;
}

void vk_deferred_gbuffer_invalidate_runtime( void )
{
	vk_dgb_destroy_pipeline();
	vk.deferred_gbuffer.lighting_logged = qfalse;
	vk.deferred_gbuffer.composite_logged = qfalse;
	vk.deferred_gbuffer.composite_skip_logged = qfalse;
	vk.deferred_gbuffer.handoff_ready_logged = qfalse;
	vk.deferred_gbuffer.frame_capture_ok = qfalse;
	vk.deferred_gbuffer.frame_lighting_ok = qfalse;
	vk.deferred_gbuffer.lighting_create_failed = qfalse;
	vk.deferred_gbuffer.composite_create_failed = qfalse;
	vk.deferred_gbuffer.debug_create_failed = qfalse;
}

void vk_deferred_gbuffer_init( void )
{
	Com_Memset( &vk.deferred_gbuffer, 0, sizeof( vk.deferred_gbuffer ) );
	ri.Cvar_Get( "r_dgbFailInject", "0", CVAR_TEMP | CVAR_CHEAT );
}

void vk_deferred_gbuffer_shutdown( void )
{
	vk_deferred_gbuffer_invalidate_runtime();
}

void vk_deferred_gbuffer_ensure_runtime( void )
{
	if ( !vk_deferred_gbuffer_active() || !vk.device || vk.device_lost ) {
		return;
	}
	/*
	 * Soft fallback sticks until cleared. Fail-inject causes can recover when the
	 * inject cvar returns to 0 (re-enable fill if G-buffer images are still live).
	 */
	if ( vk.deferredGbufferFallbackActive ) {
		if ( Q_stristr( vk.deferredGbufferFallbackReason, "r_dgbFailInject" ) &&
			!vk_dgb_fail_inject( "pipeline" ) && !vk_dgb_fail_inject( "descriptor" ) &&
			!vk_dgb_fail_inject( "alloc" ) && !vk_dgb_fail_inject( "view" ) &&
			!vk_dgb_fail_inject( "all" ) ) {
			vk_deferred_gbuffer_clear_fallback();
			if ( vk.deferredGbufferAllocated && r_deferredGBufferFill && !r_deferredGBufferFill->integer ) {
				ri.Cvar_Set( "r_deferredGBufferFill", "1" );
			}
			ri.Printf( PRINT_ALL, "[VK][deferred] fail-inject cleared — retrying G-buffer runtime\n" );
		} else {
			return;
		}
	}
	if ( vk_dgb_fail_inject( "pipeline" ) || vk_dgb_fail_inject( "descriptor" ) ) {
		vk_deferred_gbuffer_set_fallback( "r_dgbFailInject forced pipeline/descriptor failure" );
		vk_deferred_gbuffer_invalidate_runtime();
		return;
	}

	if ( vk.deferred_gbuffer.layout == VK_NULL_HANDLE ||
		vk.deferred_gbuffer.pool == VK_NULL_HANDLE ||
		vk.deferred_gbuffer.descriptor == VK_NULL_HANDLE ||
		( !vk.deferred_gbuffer.pipeline_ready && !vk.deferredGbufferDirectExport ) ) {
		vk_dgb_create_pipeline();
	}

	if ( vk_deferred_lighting_pipelines_wanted() ) {
		if ( s_deferredLightingLayoutBuilt != s_deferredLightingLayoutVersion &&
			vk.deferred_gbuffer.lighting_pipeline_ready ) {
			ri.Printf( PRINT_ALL, "[VK][deferred] recreating lighting pipeline for shadow contract bindings\n" );
			vk_dgb_destroy_lighting_pipeline();
			s_deferredLightingLayoutBuilt = 0u;
		}
		if ( !vk.deferred_gbuffer.lighting_pipeline_ready ||
			vk.deferred_gbuffer.lighting_pipeline == VK_NULL_HANDLE ||
			vk.deferred_gbuffer.lighting_descriptor == VK_NULL_HANDLE ) {
			vk_dgb_create_lighting_pipeline();
		}
		if ( !vk.deferred_gbuffer.composite_gfx_ready ||
			vk.deferred_gbuffer.composite_gfx_pipeline == VK_NULL_HANDLE ||
			vk.deferred_gbuffer.composite_gfx_descriptor == VK_NULL_HANDLE ) {
			vk_dgb_create_composite_gfx_pipeline();
		}
	}

	if ( r_deferredGBufferDebug && r_deferredGBufferDebug->integer > 0 &&
		( !vk.deferred_gbuffer.debug_gfx_ready ||
		  vk.deferred_gbuffer.debug_gfx_pipeline == VK_NULL_HANDLE ||
		  vk.deferred_gbuffer.debug_gfx_descriptor == VK_NULL_HANDLE ) ) {
		vk_dgb_create_debug_gfx_pipeline();
	}
}

void vk_deferred_gbuffer_status_f( void )
{
	uint32_t w = 0, h = 0;
	qboolean ok = qtrue;

	vk_get_active_render_extent( &w, &h );
	ri.Printf( PRINT_ALL, "======== Deferred G-buffer Status ========\n" );
	ri.Printf( PRINT_ALL,
		"architecture=%s\n"
		"  use deferred_status for eligibility / lit-as-base / mixed export counts\n",
		R_DeferredArchitecture_Name( (deferredArchitecture_t)(
			r_deferredArchitecture ? r_deferredArchitecture->integer : 0 ) ) );
	ri.Printf( PRINT_ALL, "requested : resources=%s fillCvar=%d\n",
		vk_deferred_gbuffer_resources_wanted() ? "yes" : "no",
		r_deferredGBufferFill ? r_deferredGBufferFill->integer : 0 );
	ri.Printf( PRINT_ALL, "allocated : %s directExport=%s\n",
		vk.deferredGbufferAllocated ? "yes" : "no",
		vk.deferredGbufferDirectExport ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "fill      : wanted=%s\n",
		vk_deferred_gbuffer_fill_wanted() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "generation: %u valid=%s extent=%ux%u (active=%ux%u)\n",
		vk.deferredGbufferGeneration,
		vk_deferred_gbuffer_generation_valid() ? "yes" : "no",
		vk.deferredGbufferExtentW, vk.deferredGbufferExtentH, w, h );
	ri.Printf( PRINT_ALL, "viewClass : %s\n", vk_view_class_name( vk_classify_current_view() ) );
	ri.Printf( PRINT_ALL, "runtime   : fillPipe=%s lightPipe=%s composite=%s descGen=%u lightDescGen=%u\n",
		vk.deferred_gbuffer.pipeline_ready ? "yes" : "no",
		vk.deferred_gbuffer.lighting_pipeline_ready ? "yes" : "no",
		vk.deferred_gbuffer.composite_gfx_ready ? "yes" : "no",
		vk.deferred_gbuffer.descriptor_generation,
		vk.deferred_gbuffer.lighting_descriptor_generation );
	ri.Printf( PRINT_ALL, "pathReady : %s handoff=%s frame capture=%s lighting=%s\n",
		vk_deferred_lighting_path_ready() ? "yes" : "no",
		vk_unified_clustered_opaque_handoff() ? "yes" : "no",
		vk.deferred_gbuffer.frame_capture_ok ? "yes" : "no",
		vk.deferred_gbuffer.frame_lighting_ok ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "fallback  : active=%s reason=%s\n",
		vk.deferredGbufferFallbackActive ? "yes" : "no",
		vk.deferredGbufferFallbackReason[0] ? vk.deferredGbufferFallbackReason : "none" );
	ri.Printf( PRINT_ALL, "recreate  : %s\n",
		vk.deferredGbufferLastRecreateReason[0] ? vk.deferredGbufferLastRecreateReason : "none" );

	if ( vk_deferred_gbuffer_resources_wanted() ) {
		if ( !vk.deferredGbufferAllocated ) {
			ri.Printf( PRINT_ALL, "verify    : FAIL allocated=0 while resources wanted\n" );
			ok = qfalse;
		} else if ( !vk_deferred_gbuffer_generation_valid() ) {
			ri.Printf( PRINT_ALL, "verify    : FAIL generation/extent mismatch\n" );
			ok = qfalse;
		} else if ( vk.deferred_gbuffer_albedo_view == VK_NULL_HANDLE ||
			vk.deferred_gbuffer_normal_view == VK_NULL_HANDLE ||
			vk.deferred_gbuffer_material_view == VK_NULL_HANDLE ||
			vk.deferred_lighting_view == VK_NULL_HANDLE ) {
			ri.Printf( PRINT_ALL, "verify    : FAIL missing image views\n" );
			ok = qfalse;
		} else if ( !vk.deferredGbufferDirectExport &&
			vk.deferred_gbuffer.descriptor_generation != vk.deferredGbufferGeneration ) {
			ri.Printf( PRINT_ALL, "verify    : FAIL fill descriptor gen %u != resource gen %u\n",
				vk.deferred_gbuffer.descriptor_generation, vk.deferredGbufferGeneration );
			ok = qfalse;
		} else if ( !vk.deferredGbufferDirectExport &&
			( !vk.deferred_gbuffer.pipeline_ready || vk.deferred_gbuffer.descriptor == VK_NULL_HANDLE ) ) {
			ri.Printf( PRINT_ALL, "verify    : FAIL fill pipeline/descriptor not ready\n" );
			ok = qfalse;
		} else {
			ri.Printf( PRINT_ALL, "verify    : OK gen=%u descGen=%u extent %ux%u\n",
				vk.deferredGbufferGeneration, vk.deferred_gbuffer.descriptor_generation, w, h );
		}
	} else {
		ri.Printf( PRINT_ALL, "verify    : skipped (resources not wanted)\n" );
	}
	ri.Printf( PRINT_ALL, "==========================================\n" );
	(void)ok;
}

static void vk_dgb_create_descriptor_layout( void )
{
	VkDescriptorSetLayoutBinding bindings[3];
	VkDescriptorSetLayoutCreateInfo desc;

	if ( vk.deferred_gbuffer.layout != VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.bindingCount = 3;
	desc.pBindings = bindings;
	if ( qvkCreateDescriptorSetLayout( vk.device, &desc, NULL, &vk.deferred_gbuffer.layout ) != VK_SUCCESS ) {
		vk.deferred_gbuffer.layout = VK_NULL_HANDLE;
	}
}

static void vk_dgb_create_pipeline( void )
{
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;
	VkResult res;

	if ( vk.deferred_gbuffer.pipeline_ready ) {
		return;
	}
	if ( vk.modules.deferred_gbuffer_fill_cs == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][deferred] deferred_gbuffer_fill compute shader missing\n" S_COLOR_WHITE );
		vk_deferred_gbuffer_set_fallback( "gbuffer_fill_shader_missing" );
		return;
	}
	if ( vk_dgb_fail_inject( "pipeline" ) || vk_dgb_fail_inject( "descriptor" ) ) {
		vk_deferred_gbuffer_set_fallback( "r_dgbFailInject forced pipeline/descriptor failure" );
		vk_dgb_destroy_pipeline();
		return;
	}

	vk_dgb_create_descriptor_layout();
	if ( vk.deferred_gbuffer.layout == VK_NULL_HANDLE ) {
		vk_deferred_gbuffer_set_fallback( "gbuffer_descriptor_layout_failed" );
		return;
	}

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_deferred_gbuf_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.deferred_gbuffer.layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	res = qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.deferred_gbuffer.pipeline_layout );
	if ( res != VK_SUCCESS ) {
		vk_deferred_gbuffer_set_fallback( va( "gbuffer_pipeline_layout_%s", vk_result_string( res ) ) );
		vk_dgb_destroy_pipeline();
		return;
	}

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.deferred_gbuffer_fill_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.deferred_gbuffer.pipeline_layout;
	res = qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.deferred_gbuffer.pipeline );
	if ( res != VK_SUCCESS ) {
		vk_deferred_gbuffer_set_fallback( va( "gbuffer_pipeline_%s", vk_result_string( res ) ) );
		vk_dgb_destroy_pipeline();
		return;
	}
	SET_OBJECT_NAME( vk.deferred_gbuffer.pipeline, "pipeline - deferred gbuffer fill", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	{
		VkDescriptorPoolSize pool_sizes[2];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc;

		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[0].descriptorCount = 1;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_sizes[1].descriptorCount = 2;

		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 2;
		pool_ci.pPoolSizes = pool_sizes;
		res = qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.deferred_gbuffer.pool );
		if ( res != VK_SUCCESS ) {
			vk_deferred_gbuffer_set_fallback( va( "gbuffer_descriptor_pool_%s", vk_result_string( res ) ) );
			vk_dgb_destroy_pipeline();
			return;
		}

		Com_Memset( &alloc, 0, sizeof( alloc ) );
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.descriptorPool = vk.deferred_gbuffer.pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.deferred_gbuffer.layout;
		res = qvkAllocateDescriptorSets( vk.device, &alloc, &vk.deferred_gbuffer.descriptor );
		if ( res != VK_SUCCESS ) {
			vk_deferred_gbuffer_set_fallback( va( "gbuffer_descriptor_alloc_%s", vk_result_string( res ) ) );
			vk_dgb_destroy_pipeline();
			return;
		}
	}

	vk.deferred_gbuffer.pipeline_ready = qtrue;
	ri.Printf( PRINT_ALL, "[VK][deferred] G-buffer fill compute pipeline ready\n" );
}

static void vk_dgb_update_descriptors( void )
{
	VkDescriptorImageInfo depth_info;
	VkDescriptorImageInfo normal_info;
	VkDescriptorImageInfo material_info;
	VkWriteDescriptorSet writes[3];
	Vk_Sampler_Def depth_sd;
	VkImageView depth_view;

	if ( vk.deferred_gbuffer.descriptor == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.deferred_gbuffer_normal_view == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_material_view == VK_NULL_HANDLE ) {
		return;
	}

	depth_view = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	Com_Memset( &depth_sd, 0, sizeof( depth_sd ) );
	depth_sd.gl_mag_filter = depth_sd.gl_min_filter = GL_NEAREST;
	depth_sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	depth_sd.noAnisotropy = qtrue;

	Com_Memset( &depth_info, 0, sizeof( depth_info ) );
	depth_info.sampler = vk_find_sampler( &depth_sd );
	depth_info.imageView = depth_view;
	depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	Com_Memset( &normal_info, 0, sizeof( normal_info ) );
	normal_info.imageView = vk.deferred_gbuffer_normal_view;
	normal_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( &material_info, 0, sizeof( material_info ) );
	material_info.imageView = vk.deferred_gbuffer_material_view;
	material_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = vk.deferred_gbuffer.descriptor;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &depth_info;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = vk.deferred_gbuffer.descriptor;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[1].pImageInfo = &normal_info;

	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = vk.deferred_gbuffer.descriptor;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].pImageInfo = &material_info;

	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
	vk.deferred_gbuffer.descriptor_generation = vk.deferredGbufferGeneration;
}

static void vk_dgb_fill_proj_info( vk_deferred_gbuf_push_t *push )
{
	const float *proj = backEnd.useFirstPersonProjection ? backEnd.firstPersonProjectionMatrix :
		backEnd.viewParms.projectionMatrix;
	float proj_vk[16];
	float aspect = ( backEnd.viewParms.viewportHeight > 0 ) ?
		( (float)backEnd.viewParms.viewportWidth / (float)backEnd.viewParms.viewportHeight ) : 1.0f;

	vk_get_projection_matrix_vk( proj, proj_vk );
	push->projInfo[0] = 1.0f / ( proj_vk[0] * aspect );
	push->projInfo[1] = 1.0f / proj_vk[5];
	push->projInfo[2] = proj_vk[10];
	push->projInfo[3] = proj_vk[14];
	push->materialParams[0] = r_deferredDefaultMetalness ?
		Com_Clamp( 0.0f, 1.0f, r_deferredDefaultMetalness->value ) : 0.0f;
	push->materialParams[1] = r_deferredDefaultRoughness ?
		Com_Clamp( 0.04f, 1.0f, r_deferredDefaultRoughness->value ) : 0.55f;
	push->materialParams[2] = r_deferredNormalEdgeThreshold ?
		Com_Clamp( 0.001f, 1.0f, r_deferredNormalEdgeThreshold->value ) : 0.08f;
	push->materialParams[3] = vk.msaaActive ? 1.0f : 0.0f;
	push->gbufferFlags = 0u;
	if ( r_gbufferCompact && r_gbufferCompact->integer ) {
		push->gbufferFlags |= 1u;
		if ( !s_gbufferCompactDualWriteLogged ) {
			ri.Printf( PRINT_ALL, "[VK][gbuffer] compact dual-write active\n" );
			s_gbufferCompactDualWriteLogged = qtrue;
		}
	}
}

void vk_deferred_gbuffer_capture_after_geometry( void )
{
	VkImageCopy region;
	VkImageAspectFlags depth_aspect;
	uint32_t width, height;
	vk_deferred_gbuf_push_t push;
	uint32_t gx, gy;
	qboolean resume_main;

	vk.deferred_gbuffer.frame_capture_ok = qfalse;
	vk.deferred_gbuffer.frame_lighting_ok = qfalse;

	if ( !vk_deferred_gbuffer_fill_wanted() ) {
		return;
	}
	vk_deferred_gbuffer_ensure_runtime();
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.color_image == VK_NULL_HANDLE || vk.deferred_gbuffer_albedo == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_normal == VK_NULL_HANDLE || vk.deferred_gbuffer_material == VK_NULL_HANDLE ||
		vk.depth_image == VK_NULL_HANDLE ) {
		return;
	}

	vk_spine_pass_begin( VK_SPINE_PASS_GBUFFER_FILL );

	resume_main = ( vk.inRenderPass && vk.renderPassIndex == RENDER_PASS_MAIN ) ? qtrue : qfalse;
	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}
	vk_dgb_validate_compute_break( "gbuffer_capture_after_geometry", resume_main );

	if ( !vk.deferredGbufferDirectExport ) {
		vk_dgb_create_pipeline();
		if ( !vk.deferred_gbuffer.pipeline_ready || vk.deferred_gbuffer.pipeline == VK_NULL_HANDLE ) {
			if ( resume_main ) {
				vk_resume_current_render_pass();
			}
			vk_spine_pass_end( VK_SPINE_PASS_GBUFFER_FILL );
			return;
		}
	}

	if ( !vk.deferred_gbuffer.fill_logged ) {
		ri.Printf( PRINT_ALL, "[VK][deferred] r_deferredGBufferFill=1 (%s)\n",
			vk.deferredGbufferDirectExport ? "albedo copy + direct material/motion export" : "albedo copy + depth normals" );
		vk.deferred_gbuffer.fill_logged = qtrue;
	}

	width = vk_get_render_target_width();
	height = vk_get_render_target_height();
	if ( width == 0 || height == 0 ) {
		if ( resume_main ) {
			vk_resume_current_render_pass();
		}
		vk_spine_pass_end( VK_SPINE_PASS_GBUFFER_FILL );
		return;
	}

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		0, 0 );
	record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_albedo, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		0, 0 );
	if ( !vk.deferredGbufferDirectExport ) {
		record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_normal, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
			0, 0 );
		record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_material, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
			0, 0 );
	}

	Com_Memset( &region, 0, sizeof( region ) );
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.layerCount = 1;
	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.dstSubresource.layerCount = 1;
	region.extent.width = width;
	region.extent.height = height;
	region.extent.depth = 1;
	qvkCmdCopyImage( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.deferred_gbuffer_albedo, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

	record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_albedo, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	/* Scene base for additive composite is the color copy — mark even if normal fill fails. */
	vk.deferred_gbuffer.frame_capture_ok = qtrue;

	if ( !vk.deferredGbufferDirectExport ) {
		vk_dgb_update_descriptors();
		if ( vk.deferred_gbuffer.descriptor_generation != vk.deferredGbufferGeneration ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][deferred] skip fill dispatch: descriptor gen %u != resource gen %u\n" S_COLOR_WHITE,
				vk.deferred_gbuffer.descriptor_generation, vk.deferredGbufferGeneration );
			/* Restore color layout before resume — early return used to leave TRANSFER_SRC. */
			record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				0, 0 );
			if ( !vk.deferredGbufferDirectExport ) {
				record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_normal, VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					0, 0 );
				record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_material, VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					0, 0 );
			}
			record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
			if ( resume_main ) {
				vk_resume_current_render_pass();
			}
			vk_spine_pass_end( VK_SPINE_PASS_GBUFFER_FILL );
			return;
		}

		vk_dgb_fill_proj_info( &push );
		push.extent[0] = width;
		push.extent[1] = height;

		qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.deferred_gbuffer.pipeline );
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			vk.deferred_gbuffer.pipeline_layout, 0, 1, &vk.deferred_gbuffer.descriptor, 0, NULL );
		qvkCmdPushConstants( vk.cmd->command_buffer, vk.deferred_gbuffer.pipeline_layout,
			VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

		gx = ( width + 7u ) / 8u;
		gy = ( height + 7u ) / 8u;
		qvkCmdDispatch( vk.cmd->command_buffer, gx, gy, 1 );

		record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_normal, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			0, 0 );
		record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_gbuffer_material, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			0, 0 );
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );

	if ( resume_main ) {
		vk_resume_current_render_pass();
	}
	vk_spine_pass_end( VK_SPINE_PASS_GBUFFER_FILL );
}

static void vk_dgb_create_lighting_descriptor_layout( void )
{
	VkDescriptorSetLayoutBinding bindings[16];
	VkDescriptorSetLayoutCreateInfo desc;

	if ( vk.deferred_gbuffer.lighting_layout != VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[5].binding = 5;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[6].binding = 6;
	bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[6].descriptorCount = 1;
	bindings[6].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[7].binding = 7;
	bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[7].descriptorCount = 1;
	bindings[7].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	/* LTC mat / amp for rectangular area lights */
	bindings[8].binding = 8;
	bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[8].descriptorCount = 1;
	bindings[8].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[9].binding = 9;
	bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[9].descriptorCount = 1;
	bindings[9].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	/* Shadow contract SSBO + sun CSM atlas */
	bindings[10].binding = 10;
	bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[10].descriptorCount = 1;
	bindings[10].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[11].binding = 11;
	bindings[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[11].descriptorCount = 1;
	bindings[11].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	/* M3 sky IBL: BRDF LUT + prefilter + irradiance */
	bindings[12].binding = 12;
	bindings[12].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[12].descriptorCount = 1;
	bindings[12].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[13].binding = 13;
	bindings[13].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[13].descriptorCount = 1;
	bindings[13].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[14].binding = 14;
	bindings[14].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[14].descriptorCount = 1;
	bindings[14].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	/* M3 GBufferSurfaceData (lightmap + ownership) */
	bindings[15].binding = 15;
	bindings[15].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[15].descriptorCount = 1;
	bindings[15].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &desc, 0, sizeof( desc ) );
	desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	desc.bindingCount = 16;
	desc.pBindings = bindings;
	if ( qvkCreateDescriptorSetLayout( vk.device, &desc, NULL, &vk.deferred_gbuffer.lighting_layout ) != VK_SUCCESS ) {
		vk.deferred_gbuffer.lighting_layout = VK_NULL_HANDLE;
	}
}

static void vk_dgb_create_lighting_pipeline( void )
{
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stage;
	VkComputePipelineCreateInfo pipe_ci;
	VkResult res;

	if ( vk.deferred_gbuffer.lighting_pipeline_ready ) {
		return;
	}
	/* Soft-fail sticks until invalidate/recreate — avoid per-frame create spam. */
	if ( vk.deferred_gbuffer.lighting_create_failed ) {
		return;
	}
	if ( vk.modules.deferred_lighting_cs == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][deferred] deferred_lighting compute shader missing\n" S_COLOR_WHITE );
		vk.deferred_gbuffer.lighting_create_failed = qtrue;
		return;
	}

	vk_dgb_create_lighting_descriptor_layout();
	if ( vk.deferred_gbuffer.lighting_layout == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][deferred] lighting descriptor layout failed\n" S_COLOR_WHITE );
		vk.deferred_gbuffer.lighting_create_failed = qtrue;
		return;
	}

	push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_deferred_light_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.deferred_gbuffer.lighting_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	res = qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.deferred_gbuffer.lighting_pipeline_layout );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] lighting pipeline layout failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
		vk_dgb_destroy_lighting_pipeline();
		vk.deferred_gbuffer.lighting_create_failed = qtrue;
		return;
	}

	Com_Memset( &stage, 0, sizeof( stage ) );
	stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = vk.modules.deferred_lighting_cs;
	stage.pName = "main";

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipe_ci.stage = stage;
	pipe_ci.layout = vk.deferred_gbuffer.lighting_pipeline_layout;
	res = qvkCreateComputePipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.deferred_gbuffer.lighting_pipeline );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] lighting pipeline failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
		vk_dgb_destroy_lighting_pipeline();
		vk.deferred_gbuffer.lighting_create_failed = qtrue;
		return;
	}
	SET_OBJECT_NAME( vk.deferred_gbuffer.lighting_pipeline, "pipeline - deferred lighting", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT );

	{
		VkDescriptorPoolSize pool_sizes[3];
		VkDescriptorPoolCreateInfo pool_ci;
		VkDescriptorSetAllocateInfo alloc;

		pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		pool_sizes[0].descriptorCount = 3;
		pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		pool_sizes[1].descriptorCount = 12;
		pool_sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		pool_sizes[2].descriptorCount = 1;

		Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
		pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_ci.maxSets = 1;
		pool_ci.poolSizeCount = 3;
		pool_ci.pPoolSizes = pool_sizes;
		res = qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.deferred_gbuffer.lighting_pool );
		if ( res != VK_SUCCESS ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][deferred] lighting descriptor pool failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
			vk_dgb_destroy_lighting_pipeline();
			vk.deferred_gbuffer.lighting_create_failed = qtrue;
			return;
		}

		Com_Memset( &alloc, 0, sizeof( alloc ) );
		alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc.descriptorPool = vk.deferred_gbuffer.lighting_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk.deferred_gbuffer.lighting_layout;
		res = qvkAllocateDescriptorSets( vk.device, &alloc, &vk.deferred_gbuffer.lighting_descriptor );
		if ( res != VK_SUCCESS ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][deferred] lighting descriptor alloc failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
			vk_dgb_destroy_lighting_pipeline();
			vk.deferred_gbuffer.lighting_create_failed = qtrue;
			return;
		}
	}

	vk.deferred_gbuffer.lighting_pipeline_ready = qtrue;
	vk.deferred_gbuffer.lighting_create_failed = qfalse;
	s_deferredLightingLayoutBuilt = s_deferredLightingLayoutVersion;
	ri.Printf( PRINT_ALL, "[VK][deferred] lighting compute pipeline ready\n" );
}

static void vk_dgb_update_lighting_descriptors( void )
{
	VkDescriptorBufferInfo buf_infos[3];
	VkDescriptorImageInfo img_infos[11];
	VkWriteDescriptorSet writes[16];
	Vk_Sampler_Def sd;
	Vk_Sampler_Def sd_linear;
	VkImageView depth_view;
	VkImageView class_view;
	VkImageView shadow_view;
	VkImageView brdf_view;
	VkImageView prefilter_view;
	VkImageView irradiance_view;
	VkImageView surface_view;
	VkBuffer shadow_ssbo;
	VkImageLayout shadow_layout;
	int i;
	static qboolean s_shadowBindLogged;
	static qboolean s_iblBindLogged;
	static qboolean s_surfaceBindLogged;
	qboolean haveRealShadow;
	qboolean haveIbl;

	if ( vk.deferred_gbuffer.lighting_descriptor == VK_NULL_HANDLE ) {
		return;
	}

	/* Always refresh core light/G-buffer bindings. Shadow SSBO is best-effort. */
	shadow_ssbo = vk_shadow_contract_ssbo();
	if ( shadow_ssbo != VK_NULL_HANDLE ) {
		vk_shadow_contract_upload_ssbo();
	}

	Com_Memset( buf_infos, 0, sizeof( buf_infos ) );
	buf_infos[0].buffer = vk.forward_plus.buffer;
	buf_infos[0].offset = 0;
	buf_infos[0].range = VK_WHOLE_SIZE;
	buf_infos[1].buffer = vk.forward_plus.tile_buffer;
	buf_infos[1].offset = 0;
	buf_infos[1].range = VK_WHOLE_SIZE;
	if ( shadow_ssbo != VK_NULL_HANDLE ) {
		buf_infos[2].buffer = shadow_ssbo;
		buf_infos[2].offset = 0;
		buf_infos[2].range = VK_WHOLE_SIZE;
	}

	depth_view = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;
	Com_Memset( &sd_linear, 0, sizeof( sd_linear ) );
	sd_linear.gl_mag_filter = sd_linear.gl_min_filter = GL_LINEAR;
	sd_linear.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	/* Prefer material class map; fall back to stub R8UI when classify is off. */
	class_view = vk.visibility_buffer_class_view;
	if ( class_view == VK_NULL_HANDLE ) {
		class_view = vk.deferred_class_stub_view;
	}
	if ( class_view == VK_NULL_HANDLE || depth_view == VK_NULL_HANDLE ||
		vk.deferred_gbuffer_albedo_view == VK_NULL_HANDLE ||
		vk.deferred_lighting_view == VK_NULL_HANDLE ) {
		return;
	}

	surface_view = vk.deferred_gbuffer_surface_view;
	if ( surface_view == VK_NULL_HANDLE && tr.whiteImage ) {
		surface_view = tr.whiteImage->view;
	}

	haveRealShadow = qfalse;
	shadow_view = vk.sun_shadow_sample_view ? vk.sun_shadow_sample_view : vk.sun_shadow_view;
	shadow_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	if ( shadow_view != VK_NULL_HANDLE ) {
		haveRealShadow = qtrue;
	} else if ( tr.whiteImage && tr.whiteImage->view != VK_NULL_HANDLE ) {
		/* Color stub — never claim DEPTH layout on a color view (validation hazard). */
		shadow_view = tr.whiteImage->view;
		shadow_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	brdf_view = vk.brdflut_image_view;
	if ( brdf_view == VK_NULL_HANDLE && tr.whiteImage ) {
		brdf_view = tr.whiteImage->view;
	}
	prefilter_view = VK_NULL_HANDLE;
	irradiance_view = VK_NULL_HANDLE;
	SkyboxHDR_GetCubemapViews( &prefilter_view, &irradiance_view );
	if ( prefilter_view == VK_NULL_HANDLE && tr.emptyCubemap ) {
		prefilter_view = tr.emptyCubemap->view;
	}
	if ( irradiance_view == VK_NULL_HANDLE && tr.emptyCubemap ) {
		irradiance_view = tr.emptyCubemap->view;
	}
	/* Bindings 12–14 must always be written (layout requires them). */
	haveIbl = ( brdf_view != VK_NULL_HANDLE && prefilter_view != VK_NULL_HANDLE &&
		irradiance_view != VK_NULL_HANDLE );
	if ( !haveIbl ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][deferred] IBL stub views incomplete (brdf=%p pref=%p irr=%p)\n" S_COLOR_WHITE,
			(void *)brdf_view, (void *)prefilter_view, (void *)irradiance_view );
	}

	Com_Memset( img_infos, 0, sizeof( img_infos ) );
	img_infos[0].sampler = vk_find_sampler( &sd );
	img_infos[0].imageView = depth_view;
	img_infos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	img_infos[1].sampler = vk_find_sampler( &sd );
	img_infos[1].imageView = vk.deferred_gbuffer_albedo_view;
	img_infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[2].sampler = vk_find_sampler( &sd );
	img_infos[2].imageView = vk.deferred_gbuffer_normal_view;
	img_infos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[3].sampler = vk_find_sampler( &sd );
	img_infos[3].imageView = vk.deferred_gbuffer_material_view;
	img_infos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[4].imageView = vk.deferred_lighting_view;
	img_infos[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	img_infos[5].sampler = vk_find_sampler( &sd );
	img_infos[5].imageView = class_view;
	img_infos[5].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	if ( shadow_view != VK_NULL_HANDLE ) {
		img_infos[6].sampler = ( haveRealShadow && vk.sun_shadow_sampler ) ?
			vk.sun_shadow_sampler : vk_find_sampler( &sd );
		img_infos[6].imageView = shadow_view;
		img_infos[6].imageLayout = shadow_layout;
	}
	if ( haveIbl ) {
		img_infos[7].sampler = vk_find_sampler( &sd_linear );
		img_infos[7].imageView = brdf_view;
		img_infos[7].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img_infos[8].sampler = vk_find_sampler( &sd_linear );
		img_infos[8].imageView = prefilter_view;
		img_infos[8].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		img_infos[9].sampler = vk_find_sampler( &sd_linear );
		img_infos[9].imageView = irradiance_view;
		img_infos[9].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}
	if ( surface_view != VK_NULL_HANDLE ) {
		img_infos[10].sampler = vk_find_sampler( &sd );
		img_infos[10].imageView = surface_view;
		img_infos[10].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	Com_Memset( writes, 0, sizeof( writes ) );
	for ( i = 0; i < 2; i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = vk.deferred_gbuffer.lighting_descriptor;
		writes[i].dstBinding = (uint32_t)i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[i].pBufferInfo = &buf_infos[i];
	}
	for ( i = 0; i < 4; i++ ) {
		writes[2 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2 + i].dstSet = vk.deferred_gbuffer.lighting_descriptor;
		writes[2 + i].dstBinding = (uint32_t)( 2 + i );
		writes[2 + i].descriptorCount = 1;
		writes[2 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[2 + i].pImageInfo = &img_infos[i];
	}
	writes[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[6].dstSet = vk.deferred_gbuffer.lighting_descriptor;
	writes[6].dstBinding = 6;
	writes[6].descriptorCount = 1;
	writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[6].pImageInfo = &img_infos[4];
	writes[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[7].dstSet = vk.deferred_gbuffer.lighting_descriptor;
	writes[7].dstBinding = 7;
	writes[7].descriptorCount = 1;
	writes[7].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[7].pImageInfo = &img_infos[5];
	/* Bindings 10/11 are declared in the lighting shader — always update when possible. */
	if ( shadow_ssbo == VK_NULL_HANDLE || shadow_view == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][deferred] lighting descriptors incomplete (shadow ssbo=%p view=%p)\n" S_COLOR_WHITE,
			(void *)shadow_ssbo, (void *)shadow_view );
		/* Still update 0–7 so lighting works; shadowFlags stay 0 so shader skips sample. */
		qvkUpdateDescriptorSets( vk.device, 8, writes, 0, NULL );
		vk_ltc_init();
		vk_ltc_update_deferred_lighting_descriptors( vk.deferred_gbuffer.lighting_descriptor );
		if ( haveIbl ) {
			writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet = vk.deferred_gbuffer.lighting_descriptor;
			writes[0].dstBinding = 12;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[0].pImageInfo = &img_infos[7];
			writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstSet = vk.deferred_gbuffer.lighting_descriptor;
			writes[1].dstBinding = 13;
			writes[1].descriptorCount = 1;
			writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[1].pImageInfo = &img_infos[8];
			writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[2].dstSet = vk.deferred_gbuffer.lighting_descriptor;
			writes[2].dstBinding = 14;
			writes[2].descriptorCount = 1;
			writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[2].pImageInfo = &img_infos[9];
			qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
		}
		if ( surface_view != VK_NULL_HANDLE ) {
			writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet = vk.deferred_gbuffer.lighting_descriptor;
			writes[0].dstBinding = 15;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[0].pImageInfo = &img_infos[10];
			qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );
		}
		vk.deferred_gbuffer.lighting_descriptor_generation = vk.deferredGbufferGeneration;
		return;
	}
	writes[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[8].dstSet = vk.deferred_gbuffer.lighting_descriptor;
	writes[8].dstBinding = 10;
	writes[8].descriptorCount = 1;
	writes[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[8].pBufferInfo = &buf_infos[2];
	writes[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[9].dstSet = vk.deferred_gbuffer.lighting_descriptor;
	writes[9].dstBinding = 11;
	writes[9].descriptorCount = 1;
	writes[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[9].pImageInfo = &img_infos[6];

	qvkUpdateDescriptorSets( vk.device, 10, writes, 0, NULL );
	vk_ltc_init();
	vk_ltc_update_deferred_lighting_descriptors( vk.deferred_gbuffer.lighting_descriptor );

	if ( haveIbl ) {
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = vk.deferred_gbuffer.lighting_descriptor;
		writes[0].dstBinding = 12;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &img_infos[7];
		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = vk.deferred_gbuffer.lighting_descriptor;
		writes[1].dstBinding = 13;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = &img_infos[8];
		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = vk.deferred_gbuffer.lighting_descriptor;
		writes[2].dstBinding = 14;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[2].pImageInfo = &img_infos[9];
		qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
		if ( !s_iblBindLogged ) {
			ri.Printf( PRINT_ALL,
				"[VK][deferred] sky IBL bound (bindings 12–14: BRDF LUT + prefilter + irradiance)\n" );
			s_iblBindLogged = qtrue;
		}
	}
	if ( surface_view != VK_NULL_HANDLE ) {
		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = vk.deferred_gbuffer.lighting_descriptor;
		writes[0].dstBinding = 15;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &img_infos[10];
		qvkUpdateDescriptorSets( vk.device, 1, writes, 0, NULL );
		if ( !s_surfaceBindLogged ) {
			ri.Printf( PRINT_ALL,
				"[VK][deferred] SurfaceData bound (binding 15: lightmap + ownership)\n" );
			s_surfaceBindLogged = qtrue;
		}
	}

	vk.deferred_gbuffer.lighting_descriptor_generation = vk.deferredGbufferGeneration;

	if ( !s_shadowBindLogged && shadow_ssbo != VK_NULL_HANDLE && haveRealShadow ) {
		ri.Printf( PRINT_ALL,
			"[VK][deferred] shadow contract SSBO bound (binding 10) + sunShadowMap (binding 11)\n" );
		s_shadowBindLogged = qtrue;
	}
}

static void vk_dgb_fill_light_push( vk_deferred_light_push_t *push, uint32_t width, uint32_t height )
{
	const float *view = backEnd.viewParms.world.modelViewMatrix;
	const float *proj = backEnd.useFirstPersonProjection ? backEnd.firstPersonProjectionMatrix :
		backEnd.viewParms.projectionMatrix;
	float proj_vk[16];
	float aspect = ( backEnd.viewParms.viewportHeight > 0 ) ?
		( (float)backEnd.viewParms.viewportWidth / (float)backEnd.viewParms.viewportHeight ) : 1.0f;

	Com_Memcpy( push->viewMatrix, view, sizeof( push->viewMatrix ) );
	vk_get_projection_matrix_vk( proj, proj_vk );
	push->projInfo[0] = 1.0f / ( proj_vk[0] * aspect );
	push->projInfo[1] = 1.0f / proj_vk[5];
	push->projInfo[2] = proj_vk[10];
	push->projInfo[3] = proj_vk[14];
	push->extent[0] = width;
	push->extent[1] = height;
	push->tileGrid[0] = vk.forward_plus.tiles_x;
	push->tileGrid[1] = vk.forward_plus.tiles_y;
	push->strength = ( r_deferredLightingStrength && r_deferredLightingStrength->value > 0.0f ) ?
		Com_Clamp( 0.0f, 4.0f, r_deferredLightingStrength->value ) : 1.0f;
	push->maxPerTile = vk.forward_plus.max_per_tile;
	push->numLights = vk.forward_plus.last_packed_count;
	push->additive = vk_deferred_unlit_base_wanted() ? 1u : 0u;
	push->specular = ( r_deferredSpecular && r_deferredSpecular->integer ) ? 1u : 0u;
	push->aoStrength = r_deferredAOCoupling ?
		Com_Clamp( 0.0f, 1.0f, r_deferredAOCoupling->value ) : 0.65f;
	push->specularStrength = r_deferredSpecularStrength ?
		Com_Clamp( 0.0f, 4.0f, r_deferredSpecularStrength->value ) : 1.0f;
	/* Direct MRT export writes world-space normals; depth fill writes view-space. */
	push->normalsAreWorld = vk.deferredGbufferDirectExport ? 1u : 0u;
	push->useMaterialClass = ( r_deferredMaterialClassify && r_deferredMaterialClassify->integer &&
		vk_material_classify_wanted() && vk.visibility_buffer_class_view != VK_NULL_HANDLE ) ? 1u : 0u;
	push->zSlices = vk.forward_plus.z_slices > 0u ? vk.forward_plus.z_slices : 1u;
	push->zSliceMode = ( r_forwardPlusZSliceMode && r_forwardPlusZSliceMode->integer ) ? 1u : 0u;
	{
		float zn = vk.forward_plus.cluster_z_near;
		float zf = vk.forward_plus.cluster_z_far;
		if ( zn < 1e-3f ) {
			zn = ( r_znear && r_znear->value > 0.0f ) ? r_znear->value : 4.0f;
		}
		if ( zf <= zn + 1e-3f ) {
			zf = zn + 4000.0f;
		}
		push->zNear = zn;
		push->zFar = zf;
	}
	push->specularAA = ( r_pbr_specularAA && r_pbr_specularAA->integer ) ?
		Com_Clamp( 0.0f, 2.0f, r_pbr_specularAAStrength ? r_pbr_specularAAStrength->value : 0.5f ) : 0.0f;
	push->compactLists = vk.forward_plus.compact_lists ? 1u : 0u;
	push->clusterCount = vk.forward_plus.tile_capacity_tiles;
	push->gbufferCompact = ( r_gbufferCompact && r_gbufferCompact->integer ) ? 1u : 0u;
	push->mixedMaterial = R_DeferredMixedMaterialWanted() ? 1u : 0u;
	push->lightmapMode = ( r_deferredLightmapMode ) ? (uint32_t)Com_Clamp( 0.0f, 2.0f, (float)r_deferredLightmapMode->integer ) : 0u;
	push->lightmapDeluxeStrength = ( push->lightmapMode != 0u ) ? 1.0f : 0.0f;
	push->forwardPlusDebug = ( r_forwardPlusDebug && r_forwardPlusDebug->value > 0.0f ) ?
		Com_Clamp( 0.0f, 6.0f, r_forwardPlusDebug->value ) : 0.0f;
	push->shadowFlags = 0u;
	push->shadowStrength = 0.0f;
	push->shadowNear = ( vk.sun_shadow_near > 0.0f ) ? vk.sun_shadow_near : 4.0f;
	push->shadowSplits[0] = vk.sun_shadow_splits[0];
	push->shadowSplits[1] = vk.sun_shadow_splits[1];
	push->shadowSplits[2] = vk.sun_shadow_splits[2];
	push->shadowSplits[3] = vk.sun_shadow_splits[3];
	push->shadowBlend = VK_SunCSM_CascadeBlend();
	push->shadowCascadeCount = 1u;
	/* Directional sun for M3 BRDF (world space; L points toward the sun). */
	{
		vec3_t sunL;
		float len;
		VectorCopy( tr.sunDirection, sunL );
		len = VectorLength( sunL );
		if ( len > 1e-5f ) {
			VectorScale( sunL, 1.0f / len, sunL );
		} else {
			sunL[0] = 0.35f;
			sunL[1] = 0.75f;
			sunL[2] = 0.55f;
			VectorNormalize( sunL );
		}
		push->sunDir[0] = sunL[0];
		push->sunDir[1] = sunL[1];
		push->sunDir[2] = sunL[2];
		push->sunDir[3] = 0.0f; /* angular radius reserved */
		push->sunRadiance[0] = tr.sunLight[0];
		push->sunRadiance[1] = tr.sunLight[1];
		push->sunRadiance[2] = tr.sunLight[2];
		/* Enable sun BRDF for mixed material deferred; bit1 = lightmap owns static diffuse. */
		{
			uint32_t sunFlags = 0u;
			cvar_t *sunBrdf = ri.Cvar_Get( "r_deferredSunBrdf", "1", CVAR_ARCHIVE_ND );
			if ( R_DeferredMixedMaterialWanted() && ( !sunBrdf || sunBrdf->integer ) ) {
				sunFlags |= 1u;
				sunFlags |= 2u; /* LM owns diffuse when packed; sun adds specular (+ outdoor diffuse if no LM) */
			}
			push->sunRadiance[3] = (float)sunFlags;
		}
	}
	/* Sky IBL for mixed deferred (spec always; diffuse skipped when LM owns static). */
	{
		cvar_t *ibl = ri.Cvar_Get( "r_deferredIbl", "1", CVAR_ARCHIVE_ND );
		cvar_t *iblStr = ri.Cvar_Get( "r_deferredIblStrength", "1", CVAR_ARCHIVE_ND );
		push->iblFlags = 0u;
		push->iblStrength = 0.0f;
		if ( R_DeferredMixedMaterialWanted() && ( !ibl || ibl->integer ) &&
			vk.brdflut_image_view != VK_NULL_HANDLE ) {
			push->iblFlags = 1u;
			push->iblStrength = iblStr ? Com_Clamp( 0.0f, 4.0f, iblStr->value ) : 1.0f;
		}
	}
	if ( vk.sun_shadow_valid && vk_shadow_contract_ssbo() != VK_NULL_HANDLE &&
		( vk.sun_shadow_sample_view || vk.sun_shadow_view ) &&
		( !r_pbrSunShadow || r_pbrSunShadow->integer ) ) {
		const GpuShadowRecord *rec0 = vk_shadow_contract_record( 0 );
		if ( rec0 && rec0->allocated ) {
			uint32_t cascades = vk.sun_shadow_cascade_count;
			uint32_t ci;
			if ( cascades < 1u ) {
				cascades = 1u;
			}
			if ( cascades > 4u ) {
				cascades = 4u;
			}
			/* Require at least cascade 0; count only allocated consecutive slots. */
			for ( ci = 1u; ci < cascades; ci++ ) {
				const GpuShadowRecord *rec = vk_shadow_contract_record( ci );
				if ( !rec || !rec->allocated ) {
					cascades = ci;
					break;
				}
			}
			push->shadowFlags = 1u;
			push->shadowStrength = ( ( r_pbrSunShadowStrength ) ?
				Com_Clamp( 0.0f, 1.0f, r_pbrSunShadowStrength->value ) : 1.0f ) *
				vk_day_night_shadow_factor();
			push->shadowCascadeCount = cascades;
			vk_shadow_contract_note_consumer( 0, "deferred" );
			for ( ci = 1u; ci < cascades; ci++ ) {
				vk_shadow_contract_note_consumer( ci, "deferred" );
			}
		}
	}
	if ( vk_frequency_aware_active() ) {
		float fa = vk_frequency_aware_specular_aa_strength();
		if ( fa > push->specularAA ) {
			push->specularAA = Com_Clamp( 0.0f, 2.0f, fa );
		}
	}
}

static qboolean vk_dgb_dispatch_lighting_compute( uint32_t width, uint32_t height )
{
	vk_deferred_light_push_t push;
	uint32_t gx, gy;
	VkImageAspectFlags depth_aspect;

	if ( !vk_deferred_lighting_wanted() ) {
		return qfalse;
	}
	vk_deferred_gbuffer_ensure_runtime();
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return qfalse;
	}
	if ( vk.forward_plus.buffer == VK_NULL_HANDLE || vk.forward_plus.tile_buffer == VK_NULL_HANDLE ||
		vk.deferred_lighting_image == VK_NULL_HANDLE || vk.deferred_lighting_view == VK_NULL_HANDLE ) {
		return qfalse;
	}

	/* VRCS path: SRI + pack + wave-packed lighting + deblock (own pipelines). */
	if ( vk_vrcs_dispatch_deferred_lighting( width, height ) ) {
		return qtrue;
	}

	vk_dgb_create_lighting_pipeline();
	if ( !vk.deferred_gbuffer.lighting_pipeline_ready || vk.deferred_gbuffer.lighting_pipeline == VK_NULL_HANDLE ) {
		return qfalse;
	}

	if ( !vk.deferred_gbuffer.lighting_logged ) {
		ri.Printf( PRINT_ALL,
			"[VK][deferred] r_deferredLighting=1 (%s dynamic; point+spot; strength=%.2f; specular=%s %.2f; ao=%.2f; "
			"normals=%s; materialClassify=%s)\n",
			vk_deferred_unlit_base_wanted()
				? "HYBRID_ADDITIVE (SceneBaseLit+dynamics)" : "multiply",
			( r_deferredLightingStrength && r_deferredLightingStrength->value > 0.0f ) ?
				r_deferredLightingStrength->value : 1.0f,
			( r_deferredSpecular && r_deferredSpecular->integer ) ? "on" : "off",
			r_deferredSpecularStrength ? r_deferredSpecularStrength->value : 1.0f,
			r_deferredAOCoupling ? r_deferredAOCoupling->value : 0.65f,
			vk.deferredGbufferDirectExport ? "world->view" : "view",
			( r_deferredMaterialClassify && r_deferredMaterialClassify->integer &&
				vk_material_classify_wanted() ) ? "on" : "off" );
		vk.deferred_gbuffer.lighting_logged = qtrue;
	}

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_lighting_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		0, 0 );

	vk_dgb_update_lighting_descriptors();
	if ( vk.deferred_gbuffer.lighting_descriptor_generation != vk.deferredGbufferGeneration ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] skip lighting dispatch: descriptor gen %u != resource gen %u\n" S_COLOR_WHITE,
			vk.deferred_gbuffer.lighting_descriptor_generation, vk.deferredGbufferGeneration );
		record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_lighting_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			0, 0 );
		return qfalse;
	}
	vk_dgb_fill_light_push( &push, width, height );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.deferred_gbuffer.lighting_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
		vk.deferred_gbuffer.lighting_pipeline_layout, 0, 1, &vk.deferred_gbuffer.lighting_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.deferred_gbuffer.lighting_pipeline_layout,
		VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );

	gx = ( width + 7u ) / 8u;
	gy = ( height + 7u ) / 8u;
	qvkCmdDispatch( vk.cmd->command_buffer, gx, gy, 1 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.deferred_lighting_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		0, 0 );

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
	return qtrue;
}

static void vk_dgb_create_composite_gfx_pipeline( void )
{
	VkDescriptorSetLayoutBinding bindings[2];
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input;
	VkPipelineInputAssemblyStateCreateInfo input_assembly;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineRasterizationStateCreateInfo rasterization;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineColorBlendAttachmentState blend_att;
	VkPipelineColorBlendStateCreateInfo blend;
	VkPipelineDepthStencilStateCreateInfo depth_stencil;
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkDynamicState dynamic_states[2];
	VkGraphicsPipelineCreateInfo pipe_ci;
	VkDescriptorPoolSize pool_size;
	VkDescriptorPoolCreateInfo pool_ci;
	VkDescriptorSetAllocateInfo alloc;
	VkResult res;

	if ( vk.deferred_gbuffer.composite_gfx_ready ) {
		return;
	}
	if ( vk.deferred_gbuffer.composite_create_failed ) {
		return;
	}
	if ( vk.modules.deferred_lighting_composite_fs == VK_NULL_HANDLE || vk.modules.gamma_vs == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.render_pass.post_bloom == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 2;
	layout_ci.pBindings = bindings;
	res = qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.deferred_gbuffer.composite_gfx_layout );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] composite descriptor layout failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
		vk.deferred_gbuffer.composite_create_failed = qtrue;
		return;
	}

	push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_deferred_composite_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.deferred_gbuffer.composite_gfx_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	res = qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.deferred_gbuffer.composite_gfx_pipeline_layout );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] composite pipeline layout failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
		vk_dgb_destroy_composite_gfx_pipeline();
		vk.deferred_gbuffer.composite_create_failed = qtrue;
		return;
	}

	Com_Memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk.modules.gamma_vs;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = vk.modules.deferred_lighting_composite_fs;
	stages[1].pName = "main";

	Com_Memset( &vertex_input, 0, sizeof( vertex_input ) );
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	Com_Memset( &input_assembly, 0, sizeof( input_assembly ) );
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	Com_Memset( &viewport_state, 0, sizeof( viewport_state ) );
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	Com_Memset( &rasterization, 0, sizeof( rasterization ) );
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = VK_CULL_MODE_NONE;
	rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization.lineWidth = 1.0f;

	Com_Memset( &multisample, 0, sizeof( multisample ) );
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	Com_Memset( &blend_att, 0, sizeof( blend_att ) );
	blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	Com_Memset( &blend, 0, sizeof( blend ) );
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &blend_att;

	Com_Memset( &depth_stencil, 0, sizeof( depth_stencil ) );
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_FALSE;
	depth_stencil.depthWriteEnable = VK_FALSE;

	/* Viewport/scissor are set dynamically at draw time (vk_set_fullscreen_viewport_scissor);
	 * declare them dynamic so pViewports/pScissors may stay NULL without a driver NULL-deref. */
	dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
	Com_Memset( &dynamic_state, 0, sizeof( dynamic_state ) );
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = 2;
	dynamic_state.pDynamicStates = dynamic_states;

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipe_ci.stageCount = 2;
	pipe_ci.pStages = stages;
	pipe_ci.pVertexInputState = &vertex_input;
	pipe_ci.pInputAssemblyState = &input_assembly;
	pipe_ci.pViewportState = &viewport_state;
	pipe_ci.pRasterizationState = &rasterization;
	pipe_ci.pMultisampleState = &multisample;
	pipe_ci.pDepthStencilState = &depth_stencil;
	pipe_ci.pColorBlendState = &blend;
	pipe_ci.pDynamicState = &dynamic_state;
	pipe_ci.layout = vk.deferred_gbuffer.composite_gfx_pipeline_layout;
	pipe_ci.renderPass = vk.render_pass.post_bloom;
	pipe_ci.subpass = 0;
	res = qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.deferred_gbuffer.composite_gfx_pipeline );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] composite pipeline failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
		vk_dgb_destroy_composite_gfx_pipeline();
		vk.deferred_gbuffer.composite_create_failed = qtrue;
		return;
	}

	pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_size.descriptorCount = 2;
	Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
	pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_ci.maxSets = 1;
	pool_ci.poolSizeCount = 1;
	pool_ci.pPoolSizes = &pool_size;
	res = qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.deferred_gbuffer.composite_gfx_pool );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] composite descriptor pool failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
		vk_dgb_destroy_composite_gfx_pipeline();
		vk.deferred_gbuffer.composite_create_failed = qtrue;
		return;
	}

	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = vk.deferred_gbuffer.composite_gfx_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.deferred_gbuffer.composite_gfx_layout;
	res = qvkAllocateDescriptorSets( vk.device, &alloc, &vk.deferred_gbuffer.composite_gfx_descriptor );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] composite descriptor alloc failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
		vk_dgb_destroy_composite_gfx_pipeline();
		vk.deferred_gbuffer.composite_create_failed = qtrue;
		return;
	}

	vk.deferred_gbuffer.composite_gfx_ready = qtrue;
	vk.deferred_gbuffer.composite_create_failed = qfalse;
}

static void vk_dgb_update_composite_descriptor( void )
{
	VkDescriptorImageInfo img_infos[2];
	VkWriteDescriptorSet writes[2];
	Vk_Sampler_Def sd;
	int i;

	if ( vk.deferred_gbuffer.composite_gfx_descriptor == VK_NULL_HANDLE ||
		vk.deferred_lighting_view == VK_NULL_HANDLE || vk.deferred_gbuffer_albedo_view == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;

	Com_Memset( img_infos, 0, sizeof( img_infos ) );
	img_infos[0].sampler = vk_find_sampler( &sd );
	img_infos[0].imageView = vk.deferred_gbuffer_albedo_view;
	img_infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	img_infos[1].sampler = vk_find_sampler( &sd );
	img_infos[1].imageView = vk.deferred_lighting_view;
	img_infos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( writes, 0, sizeof( writes ) );
	for ( i = 0; i < 2; i++ ) {
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = vk.deferred_gbuffer.composite_gfx_descriptor;
		writes[i].dstBinding = (uint32_t)i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[i].pImageInfo = &img_infos[i];
	}
	qvkUpdateDescriptorSets( vk.device, 2, writes, 0, NULL );
}

static void vk_dgb_composite_lit_to_color( uint32_t width, uint32_t height )
{
	vk_deferred_composite_push_t push;
	qboolean resume_main = ( vk.inRenderPass && vk.renderPassIndex == RENDER_PASS_MAIN ) ? qtrue : qfalse;

	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	/* post_bloom initialLayout is SHADER_READ_ONLY (non-RTX). Do not pre-transition
	 * to COLOR_ATTACHMENT — that mismatched the RP and left tile-shaped undefined
	 * loads into the deferred composite that OIT later copies as opaqueTex. */
	vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "pre-deferred-composite" );

	vk_begin_post_bloom_render_pass();
	vk_dgb_update_composite_descriptor();

	push.additive = vk_deferred_unlit_base_wanted() ? 1u : 0u;
	/* Never REPLACE with dynamic-only when we have a captured scene base — that path blacks maps.
	 * MIXED_MATERIAL_DEFERRED still keeps additive=1 for non-owned Forward+ pixels; owned
	 * pixels replace via lit.a in the composite shader. */
	if ( vk.deferred_gbuffer.frame_capture_ok ) {
		push.additive = 1u;
	}
	push.hybridCompare = ( r_hybridCompare && r_hybridCompare->integer > 0 ) ? (uint32_t)r_hybridCompare->integer : 0u;
	if ( r_deferredArchitecture && r_deferredArchitecture->integer == DEFERRED_ARCH_COMPARE &&
		push.hybridCompare == 0u ) {
		push.hybridCompare = 1u;
	}
	push.mixedMaterial = R_DeferredMixedMaterialWanted() ? 1u : 0u;

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.deferred_gbuffer.composite_gfx_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.deferred_gbuffer.composite_gfx_pipeline_layout, 0, 1, &vk.deferred_gbuffer.composite_gfx_descriptor, 0, NULL );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.deferred_gbuffer.composite_gfx_pipeline_layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
	vk_set_fullscreen_viewport_scissor( width, height );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	if ( resume_main ) {
		/* Mode-3 deferred composite temporarily switches to post_bloom for the fullscreen
		 * add. Restore the original scene-pass identity before resuming so transparent
		 * draws continue in the main scene pass rather than accidentally re-entering
		 * post_bloom. */
		vk_resume_main_render_pass();
	} else if ( vk.color_image_view != VK_NULL_HANDLE ) {
		vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "deferred lighting composite" );
	}
}

void vk_deferred_lighting_apply_after_geometry( void )
{
	uint32_t width, height;
	qboolean lighting_ok;

	if ( !vk_deferred_lighting_wanted() ) {
		return;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}

	width = vk_get_render_target_width();
	height = vk_get_render_target_height();
	if ( width == 0 || height == 0 ) {
		return;
	}

	vk_spine_pass_begin( VK_SPINE_PASS_DEFERRED_LIGHTING );
	vk_spine_note_write( VK_SPINE_RES_DEFERRED_LIGHTING, VK_SPINE_PASS_DEFERRED_LIGHTING,
		VK_SPINE_ACCESS_STORAGE_WRITE | VK_SPINE_ACCESS_COLOR_WRITE );
	vk_spine_note_read( VK_SPINE_RES_GBUFFER_ALBEDO, VK_SPINE_PASS_DEFERRED_LIGHTING,
		VK_SPINE_ACCESS_SAMPLED_READ );
	vk_spine_note_read( VK_SPINE_RES_GBUFFER_NORMAL, VK_SPINE_PASS_DEFERRED_LIGHTING,
		VK_SPINE_ACCESS_SAMPLED_READ );
	vk_spine_note_read( VK_SPINE_RES_DEPTH, VK_SPINE_PASS_DEFERRED_LIGHTING,
		VK_SPINE_ACCESS_DEPTH_READ );

	lighting_ok = vk_dgb_dispatch_lighting_compute( width, height );
	vk.deferred_gbuffer.frame_lighting_ok = lighting_ok;
	if ( r_pbrSunShadow && r_pbrSunShadow->integer && vk.sun_shadow_valid && !R_ClassicLightingActive() ) {
		vk_shadow_contract_note_consumer( 0, "deferred" );
	}
	vk_cluster_assert_shared_consumers( "deferred_lighting" );

	/*
	 * Fullscreen composite REPLACES HDR color. Only do that when this frame has a valid
	 * scene-base capture and lighting RT write — otherwise leave the opaque lightmapped
	 * buffer intact (fail open) instead of painting black / empty albedo.
	 */
	if ( !vk.deferred_gbuffer.frame_capture_ok || !lighting_ok ) {
		if ( !vk.deferred_gbuffer.composite_skip_logged ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][deferred] skip composite REPLACE (capture_ok=%d lighting_ok=%d) — keeping opaque color\n"
				S_COLOR_WHITE,
				vk.deferred_gbuffer.frame_capture_ok ? 1 : 0,
				lighting_ok ? 1 : 0 );
			vk.deferred_gbuffer.composite_skip_logged = qtrue;
		}
		vk_spine_pass_end( VK_SPINE_PASS_DEFERRED_LIGHTING );
		return;
	}

	vk_dgb_create_composite_gfx_pipeline();
	if ( !vk.deferred_gbuffer.composite_gfx_ready || vk.deferred_gbuffer.composite_gfx_pipeline == VK_NULL_HANDLE ) {
		if ( !vk.deferred_gbuffer.composite_skip_logged ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][deferred] skip composite REPLACE (composite pipeline unavailable) — keeping opaque color\n"
				S_COLOR_WHITE );
			vk.deferred_gbuffer.composite_skip_logged = qtrue;
		}
		vk_spine_pass_end( VK_SPINE_PASS_DEFERRED_LIGHTING );
		return;
	}

	if ( !vk.deferred_gbuffer.composite_logged ) {
		ri.Printf( PRINT_ALL,
			"[VK][deferred] composite scene base + dynamic lighting to color (additive=%s)\n",
			( vk_deferred_unlit_base_wanted() || vk.deferred_gbuffer.frame_capture_ok ) ? "1" : "0" );
		vk.deferred_gbuffer.composite_logged = qtrue;
	}

	vk_dgb_composite_lit_to_color( width, height );
	vk_spine_note_write( VK_SPINE_RES_HDR_COLOR, VK_SPINE_PASS_DEFERRED_LIGHTING,
		VK_SPINE_ACCESS_COLOR_WRITE );
	vk_spine_pass_end( VK_SPINE_PASS_DEFERRED_LIGHTING );
}

static void vk_dgb_create_debug_gfx_pipeline( void )
{
	VkDescriptorSetLayoutBinding binding;
	VkDescriptorSetLayoutCreateInfo layout_ci;
	VkPushConstantRange push_range;
	VkPipelineLayoutCreateInfo pl_ci;
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineVertexInputStateCreateInfo vertex_input;
	VkPipelineInputAssemblyStateCreateInfo input_assembly;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineRasterizationStateCreateInfo rasterization;
	VkPipelineMultisampleStateCreateInfo multisample;
	VkPipelineColorBlendAttachmentState blend_att;
	VkPipelineColorBlendStateCreateInfo blend;
	VkPipelineDepthStencilStateCreateInfo depth_stencil;
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkDynamicState dynamic_states[2];
	VkGraphicsPipelineCreateInfo pipe_ci;
	VkDescriptorPoolSize pool_size;
	VkDescriptorPoolCreateInfo pool_ci;
	VkDescriptorSetAllocateInfo alloc;
	VkResult res;

	if ( vk.deferred_gbuffer.debug_gfx_ready ) {
		return;
	}
	if ( vk.deferred_gbuffer.debug_create_failed ) {
		return;
	}
	if ( vk.modules.deferred_gbuffer_debug_fs == VK_NULL_HANDLE || vk.modules.gamma_vs == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.render_pass.post_bloom == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &binding, 0, sizeof( binding ) );
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	Com_Memset( &layout_ci, 0, sizeof( layout_ci ) );
	layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_ci.bindingCount = 1;
	layout_ci.pBindings = &binding;
	res = qvkCreateDescriptorSetLayout( vk.device, &layout_ci, NULL, &vk.deferred_gbuffer.debug_gfx_layout );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] debug descriptor layout failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
		vk.deferred_gbuffer.debug_create_failed = qtrue;
		return;
	}

	push_range.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	push_range.offset = 0;
	push_range.size = sizeof( vk_deferred_gbuf_debug_push_t );

	Com_Memset( &pl_ci, 0, sizeof( pl_ci ) );
	pl_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pl_ci.setLayoutCount = 1;
	pl_ci.pSetLayouts = &vk.deferred_gbuffer.debug_gfx_layout;
	pl_ci.pushConstantRangeCount = 1;
	pl_ci.pPushConstantRanges = &push_range;
	res = qvkCreatePipelineLayout( vk.device, &pl_ci, NULL, &vk.deferred_gbuffer.debug_gfx_pipeline_layout );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] debug pipeline layout failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
		vk_dgb_destroy_debug_gfx_pipeline();
		vk.deferred_gbuffer.debug_create_failed = qtrue;
		return;
	}

	Com_Memset( stages, 0, sizeof( stages ) );
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vk.modules.gamma_vs;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = vk.modules.deferred_gbuffer_debug_fs;
	stages[1].pName = "main";

	Com_Memset( &vertex_input, 0, sizeof( vertex_input ) );
	vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	Com_Memset( &input_assembly, 0, sizeof( input_assembly ) );
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	Com_Memset( &viewport_state, 0, sizeof( viewport_state ) );
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;

	Com_Memset( &rasterization, 0, sizeof( rasterization ) );
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = VK_CULL_MODE_NONE;
	rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization.lineWidth = 1.0f;

	Com_Memset( &multisample, 0, sizeof( multisample ) );
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	Com_Memset( &blend_att, 0, sizeof( blend_att ) );
	blend_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	Com_Memset( &blend, 0, sizeof( blend ) );
	blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend.attachmentCount = 1;
	blend.pAttachments = &blend_att;

	Com_Memset( &depth_stencil, 0, sizeof( depth_stencil ) );
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = VK_FALSE;
	depth_stencil.depthWriteEnable = VK_FALSE;

	/* Viewport/scissor are set dynamically at draw time; declare them dynamic so the
	 * NULL pViewports/pScissors here do not trip a driver NULL-deref at pipeline create. */
	dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamic_states[1] = VK_DYNAMIC_STATE_SCISSOR;
	Com_Memset( &dynamic_state, 0, sizeof( dynamic_state ) );
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = 2;
	dynamic_state.pDynamicStates = dynamic_states;

	Com_Memset( &pipe_ci, 0, sizeof( pipe_ci ) );
	pipe_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipe_ci.stageCount = 2;
	pipe_ci.pStages = stages;
	pipe_ci.pVertexInputState = &vertex_input;
	pipe_ci.pInputAssemblyState = &input_assembly;
	pipe_ci.pViewportState = &viewport_state;
	pipe_ci.pRasterizationState = &rasterization;
	pipe_ci.pMultisampleState = &multisample;
	pipe_ci.pDepthStencilState = &depth_stencil;
	pipe_ci.pColorBlendState = &blend;
	pipe_ci.pDynamicState = &dynamic_state;
	pipe_ci.layout = vk.deferred_gbuffer.debug_gfx_pipeline_layout;
	pipe_ci.renderPass = vk.render_pass.post_bloom;
	pipe_ci.subpass = 0;
	res = qvkCreateGraphicsPipelines( vk.device, VK_NULL_HANDLE, 1, &pipe_ci, NULL, &vk.deferred_gbuffer.debug_gfx_pipeline );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] debug pipeline failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
		vk_dgb_destroy_debug_gfx_pipeline();
		vk.deferred_gbuffer.debug_create_failed = qtrue;
		return;
	}

	pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_size.descriptorCount = 1;
	Com_Memset( &pool_ci, 0, sizeof( pool_ci ) );
	pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_ci.maxSets = 1;
	pool_ci.poolSizeCount = 1;
	pool_ci.pPoolSizes = &pool_size;
	res = qvkCreateDescriptorPool( vk.device, &pool_ci, NULL, &vk.deferred_gbuffer.debug_gfx_pool );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] debug descriptor pool failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
		vk_dgb_destroy_debug_gfx_pipeline();
		vk.deferred_gbuffer.debug_create_failed = qtrue;
		return;
	}

	Com_Memset( &alloc, 0, sizeof( alloc ) );
	alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc.descriptorPool = vk.deferred_gbuffer.debug_gfx_pool;
	alloc.descriptorSetCount = 1;
	alloc.pSetLayouts = &vk.deferred_gbuffer.debug_gfx_layout;
	res = qvkAllocateDescriptorSets( vk.device, &alloc, &vk.deferred_gbuffer.debug_gfx_descriptor );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][deferred] debug descriptor alloc failed: %s\n" S_COLOR_WHITE, vk_result_string( res ) );
		vk_dgb_destroy_debug_gfx_pipeline();
		vk.deferred_gbuffer.debug_create_failed = qtrue;
		return;
	}

	vk.deferred_gbuffer.debug_gfx_ready = qtrue;
	vk.deferred_gbuffer.debug_create_failed = qfalse;
}

static void vk_dgb_update_debug_descriptor( VkImageView view )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet write;
	Vk_Sampler_Def sd;

	if ( vk.deferred_gbuffer.debug_gfx_descriptor == VK_NULL_HANDLE || view == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.noAnisotropy = qtrue;

	Com_Memset( &info, 0, sizeof( info ) );
	info.sampler = vk_find_sampler( &sd );
	info.imageView = view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = vk.deferred_gbuffer.debug_gfx_descriptor;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

qboolean vk_deferred_gbuffer_draw_debug( void )
{
	vk_deferred_gbuf_debug_push_t push;
	VkImageView src_view;
	int mode;
	uint32_t width;
	uint32_t height;
	static qboolean s_debug_logged;

	if ( !vk_deferred_gbuffer_active() || !r_deferredGBufferFill || !r_deferredGBufferFill->integer ) {
		return qfalse;
	}
	if ( !r_deferredGBufferDebug || r_deferredGBufferDebug->integer <= 0 ) {
		return qfalse;
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE || !backEnd.doneSurfaces || !vk.fboActive ) {
		return qfalse;
	}

	mode = r_deferredGBufferDebug->integer;
	if ( mode < 1 ) {
		mode = 1;
	}
	if ( mode > 6 ) {
		mode = 6;
	}

	if ( mode == 1 ) {
		src_view = vk.deferred_gbuffer_albedo_view;
	} else if ( mode == 2 || mode == 5 ) {
		src_view = vk.deferred_gbuffer_normal_view;
	} else if ( mode == 3 ) {
		src_view = vk.deferred_gbuffer_material_view;
	} else if ( mode == 6 ) {
		src_view = vk.motion_vector_view;
	} else {
		src_view = vk.deferred_lighting_view;
	}
	if ( src_view == VK_NULL_HANDLE || vk.color_image == VK_NULL_HANDLE ) {
		return qfalse;
	}

	vk_dgb_create_debug_gfx_pipeline();
	if ( !vk.deferred_gbuffer.debug_gfx_ready || vk.deferred_gbuffer.debug_gfx_pipeline == VK_NULL_HANDLE ) {
		return qfalse;
	}

	if ( !s_debug_logged ) {
		ri.Printf( PRINT_ALL, "[VK][deferred] r_deferredGBufferDebug=%d (1=albedo 2=normal 3=material 4=lighting 5=normal confidence 6=motion)\n", mode );
		s_debug_logged = qtrue;
	}

	width = vk_get_render_target_width();
	height = vk_get_render_target_height();
	if ( width == 0 || height == 0 ) {
		return qfalse;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		0, 0 );

	vk_begin_post_bloom_render_pass();
	vk_dgb_update_debug_descriptor( src_view );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.deferred_gbuffer.debug_gfx_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.deferred_gbuffer.debug_gfx_pipeline_layout, 0, 1, &vk.deferred_gbuffer.debug_gfx_descriptor, 0, NULL );

	push.mode = mode;
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.deferred_gbuffer.debug_gfx_pipeline_layout,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );

	vk_set_fullscreen_viewport_scissor( width, height );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	if ( vk.color_image_view != VK_NULL_HANDLE ) {
		vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "deferred gbuffer debug" );
	}

	return qtrue;
}
