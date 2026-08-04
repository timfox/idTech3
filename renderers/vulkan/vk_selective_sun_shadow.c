/*
===========================================================================
Selective Hybrid Shadows 1.0 — exclusive sun-shadow ownership router.

Raster cascade OR Hybrid1/RQ RT sun visibility — never both.
Local lights stay on the raster shadow path for this milestone.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_selective_sun_shadow.h"
#include "vk_rtx.h"
#include "vk_hybrid1.h"
#include "vk_util.h"
#include "vk_view_state.h"
#include "vk_deferred_gbuffer.h"
#include "tr_render_mode_vk.h"
#include "vk_selective_rt.h"

#ifdef USE_VULKAN_RTX
#include "vk_selective_sun_shadow_spirv.inc"
#endif

typedef struct {
	qboolean inited;
	vkShsSunOwner_t owner;
	char fallbackReason[128];
	char lastLogOwner[32];
	qboolean rtPipelineOk;
	qboolean descriptorOk;
	qboolean historyOk;
	qboolean tlasOk;
	uint32_t lastTlasRevision;
#ifdef USE_VULKAN_RTX
	qboolean rqReady;
	VkShaderModule rqCS;
	VkDescriptorSetLayout rqLayout;
	VkPipelineLayout rqPL;
	VkPipeline rqPipe;
	VkDescriptorPool rqPool;
	VkDescriptorSet rqSet;
#endif
} shs_state_t;

static shs_state_t shs;

static cvar_t *r_selectiveHybridSunShadow;
static cvar_t *r_havenrpSunShadowOwner;
static cvar_t *r_shsFailInject;
static cvar_t *r_shsDebug;
static cvar_t *r_shsTemporalAlphaFloor;
static cvar_t *r_shsMaxHistoryAge;
static cvar_t *r_shsPreferRayQuery;
static cvar_t *r_havenrpFallbackReason;

static void SHS_SetFallback( const char *reason )
{
	if ( !reason ) {
		reason = "none";
	}
	Q_strncpyz( shs.fallbackReason, reason, sizeof( shs.fallbackReason ) );
	if ( r_havenrpFallbackReason ) {
		ri.Cvar_Set( "r_havenrpFallbackReason", shs.fallbackReason );
	}
}

static qboolean SHS_PathtraceBlocks( void )
{
	if ( R_RenderMode_IsPathTracedReference() ) {
		return qtrue;
	}
	if ( r_pathtrace && r_pathtrace->integer ) {
		return qtrue;
	}
	return qfalse;
}

static qboolean SHS_FeatureRequested( void )
{
	if ( r_selectiveHybridSunShadow && r_selectiveHybridSunShadow->integer ) {
		return qtrue;
	}
	/* Mode 4 Selective Hybrid implies sun-shadow ownership routing when Hybrid1 shadow is on. */
	if ( R_RenderMode_IsSelectiveHybrid() && r_hybrid1 && r_hybrid1->integer &&
		( !r_hybrid1_shadow || r_hybrid1_shadow->integer ) ) {
		return qtrue;
	}
	return qfalse;
}

static qboolean SHS_Fail( int bit )
{
	return ( r_shsFailInject && ( r_shsFailInject->integer & bit ) ) ? qtrue : qfalse;
}

static qboolean SHS_RtHealthReady( void )
{
	qboolean tlasReady;
	qboolean pipelineReady;
	qboolean descReady;
	qboolean histReady;

	if ( SHS_Fail( VK_SHS_FAIL_TLAS ) ) {
		shs.tlasOk = qfalse;
		SHS_SetFallback( "shs_fail_inject_tlas" );
		return qfalse;
	}
	if ( SHS_Fail( VK_SHS_FAIL_RT_PIPELINE ) ) {
		shs.rtPipelineOk = qfalse;
		SHS_SetFallback( "shs_fail_inject_rt_pipeline" );
		return qfalse;
	}
	if ( SHS_Fail( VK_SHS_FAIL_DESCRIPTOR ) ) {
		shs.descriptorOk = qfalse;
		SHS_SetFallback( "shs_fail_inject_descriptor" );
		return qfalse;
	}
	if ( SHS_Fail( VK_SHS_FAIL_HISTORY ) ) {
		shs.historyOk = qfalse;
		SHS_SetFallback( "shs_fail_inject_history" );
		return qfalse;
	}

	tlasReady = ( vk_rtx_scene_ready() && shs.tlasOk ) ? qtrue : qfalse;
	if ( !tlasReady ) {
		SHS_SetFallback( vk_rtx_scene_ready() ? "shs_tlas_unhealthy" : "shs_tlas_not_ready" );
		return qfalse;
	}
	if ( !vk_srt_admit_shadow() ) {
		SHS_SetFallback( "shs_selective_budget" );
		return qfalse;
	}

#ifdef USE_VULKAN_RTX
	/* Prefer ray-query when available; otherwise Hybrid1 RT pipeline + TLAS. */
	pipelineReady = shs.rtPipelineOk && vk.rtxAvailable &&
		( vk.rayQueryAvailable || vk_hybrid1_active() );
	if ( !pipelineReady ) {
		SHS_SetFallback( !vk.rtxAvailable ? "shs_rtx_unavailable" :
			( !vk.rayQueryAvailable && !vk_hybrid1_active() ? "shs_rt_pipeline_not_ready" :
			  "shs_rt_pipeline_unhealthy" ) );
		return qfalse;
	}
#else
	(void)pipelineReady;
	SHS_SetFallback( "shs_rtx_build_disabled" );
	return qfalse;
#endif

	descReady = shs.descriptorOk;
	if ( !descReady ) {
		SHS_SetFallback( "shs_descriptor_unhealthy" );
		return qfalse;
	}

	histReady = shs.historyOk;
	if ( !histReady ) {
		SHS_SetFallback( "shs_history_unhealthy" );
		return qfalse;
	}

	return qtrue;
}

static void SHS_ResolveOwner( void )
{
	const char *pref;
	vkShsSunOwner_t want = VK_SHS_OWNER_RASTER;

	shs.owner = VK_SHS_OWNER_RASTER;

	if ( !SHS_FeatureRequested() ) {
		SHS_SetFallback( "none" );
		return;
	}

	if ( SHS_PathtraceBlocks() ) {
		SHS_SetFallback( "shs_blocked_by_pathtrace" );
		return;
	}

	if ( !vk.fboActive ) {
		SHS_SetFallback( "shs_requires_fbo" );
		return;
	}

	pref = r_havenrpSunShadowOwner ? r_havenrpSunShadowOwner->string : "auto";
	if ( !Q_stricmp( pref, "raster" ) ) {
		SHS_SetFallback( "shs_owner_forced_raster" );
		return;
	}

	if ( !Q_stricmp( pref, "hybrid1_rt" ) || !Q_stricmp( pref, "rt" ) || !Q_stricmp( pref, "auto" ) ) {
		want = VK_SHS_OWNER_RT;
	} else {
		SHS_SetFallback( "shs_owner_unknown_pref" );
		return;
	}

	if ( want == VK_SHS_OWNER_RT && SHS_RtHealthReady() ) {
		shs.owner = VK_SHS_OWNER_RT;
		SHS_SetFallback( "none" );
	} else if ( want == VK_SHS_OWNER_RT ) {
		shs.owner = VK_SHS_OWNER_RASTER;
		/* fallback reason already set by SHS_RtHealthReady */
	}
}

static void SHS_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"[VK][SHS] active=%d owner=%s rtOwns=%d rasterOwns=%d rayQuery=%d hybrid1=%d tlas=%d\n"
		"  failInject=0x%x pipelineOk=%d descOk=%d histOk=%d tlasOk=%d tlasRev=%u\n"
		"  fallback=%s debug=%d alphaFloor=%.2f maxAge=%u pathtraceBlock=%d\n"
		"  note: sun owner is exclusive; local lights remain raster; never multiply both\n",
		vk_shs_active() ? 1 : 0,
		vk_shs_owner_name(),
		vk_shs_rt_owns_sun() ? 1 : 0,
		vk_shs_raster_owns_sun() ? 1 : 0,
#ifdef USE_VULKAN_RTX
		vk.rayQueryAvailable ? 1 : 0,
#else
		0,
#endif
		vk_hybrid1_active() ? 1 : 0,
		vk_rtx_scene_ready() ? 1 : 0,
		vk_shs_fail_inject(),
		shs.rtPipelineOk ? 1 : 0,
		shs.descriptorOk ? 1 : 0,
		shs.historyOk ? 1 : 0,
		shs.tlasOk ? 1 : 0,
		vk_rtx_tlas_revision(),
		vk_shs_fallback_reason(),
		vk_shs_debug_mode(),
		vk_shs_temporal_alpha_floor(),
		vk_shs_max_history_age(),
		vk_shs_pathtrace_blocks() ? 1 : 0 );
}

void vk_shs_init( void )
{
	if ( shs.inited ) {
		return;
	}

	r_selectiveHybridSunShadow = ri.Cvar_Get( "r_selectiveHybridSunShadow", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_selectiveHybridSunShadow, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_selectiveHybridSunShadow,
		"Selective Hybrid Shadows 1.0: exclusive RT sun-shadow visibility with raster fallback. "
		"Requires RTX build + TLAS; does not change modern_vulkan.cfg boot defaults." );
	ri.Cvar_SetGroup( r_selectiveHybridSunShadow, CVG_RENDERER );

	r_havenrpSunShadowOwner = ri.Cvar_Get( "r_havenrpSunShadowOwner", "auto", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_havenrpSunShadowOwner,
		"Sun shadow signal owner: auto|raster|hybrid1_rt. Exactly one owner per frame." );
	ri.Cvar_SetGroup( r_havenrpSunShadowOwner, CVG_RENDERER );

	r_shsFailInject = ri.Cvar_Get( "r_shsFailInject", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_shsFailInject, "0", "15", CV_INTEGER );
	ri.Cvar_SetDescription( r_shsFailInject,
		"SHS fail inject bitmask: 1=TLAS 2=RT pipeline 4=descriptor 8=history." );
	ri.Cvar_SetGroup( r_shsFailInject, CVG_RENDERER );

	r_shsDebug = ri.Cvar_Get( "r_shsDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_shsDebug, "0", "11", CV_INTEGER );
	ri.Cvar_SetDescription( r_shsDebug,
		"SHS debug: 0=off (use hybrid1_debug) 1=raw RT vis 2=raster vis 3=diff 4=hitDist "
		"5=TLAS coverage 6=alpha candidates 7=dynamic geo 8=raster fallback mask "
		"9=history weight 10=reject reason 11=final filtered." );
	ri.Cvar_SetGroup( r_shsDebug, CVG_RENDERER );

	r_shsTemporalAlphaFloor = ri.Cvar_Get( "r_shsTemporalAlphaFloor", "0.28", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_shsTemporalAlphaFloor, "0.05", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_shsTemporalAlphaFloor,
		"Minimum blend toward current raw sun visibility (prefer noise over ghosting)." );
	ri.Cvar_SetGroup( r_shsTemporalAlphaFloor, CVG_RENDERER );

	r_shsMaxHistoryAge = ri.Cvar_Get( "r_shsMaxHistoryAge", "16", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_shsMaxHistoryAge, "1", "64", CV_INTEGER );
	ri.Cvar_SetDescription( r_shsMaxHistoryAge, "Finite maximum SHS shadow history age in frames." );
	ri.Cvar_SetGroup( r_shsMaxHistoryAge, CVG_RENDERER );

	r_shsPreferRayQuery = ri.Cvar_Get( "r_shsPreferRayQuery", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_shsPreferRayQuery, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_shsPreferRayQuery,
		"Prefer compute rayQuery raw sun visibility. Default 0 uses Hybrid1 RT pipeline "
		"(supports alpha-tested any-hit). Ray-query cannot run any-hit shaders." );
	ri.Cvar_SetGroup( r_shsPreferRayQuery, CVG_RENDERER );

	r_havenrpFallbackReason = ri.Cvar_Get( "r_havenrpFallbackReason", "none", CVAR_ROM );

	ri.Cmd_AddCommand( "shs_status", SHS_Status_f );

	shs.owner = VK_SHS_OWNER_RASTER;
	shs.rtPipelineOk = qfalse;
	shs.descriptorOk = qfalse;
	shs.historyOk = qtrue;
	shs.tlasOk = qfalse;
	shs.lastTlasRevision = 0;
	SHS_SetFallback( "none" );
	shs.inited = qtrue;

	ri.Printf( PRINT_ALL,
		"[VK][SHS] Selective Hybrid Shadows 1.0 router (r_selectiveHybridSunShadow=%d owner=%s)\n",
		r_selectiveHybridSunShadow->integer, r_havenrpSunShadowOwner->string );
}

#ifdef USE_VULKAN_RTX
static void SHS_DestroyRayQuery( void )
{
	if ( shs.rqPipe ) {
		qvkDestroyPipeline( vk.device, shs.rqPipe, NULL );
		shs.rqPipe = VK_NULL_HANDLE;
	}
	if ( shs.rqPL ) {
		qvkDestroyPipelineLayout( vk.device, shs.rqPL, NULL );
		shs.rqPL = VK_NULL_HANDLE;
	}
	if ( shs.rqLayout ) {
		qvkDestroyDescriptorSetLayout( vk.device, shs.rqLayout, NULL );
		shs.rqLayout = VK_NULL_HANDLE;
	}
	if ( shs.rqPool ) {
		qvkDestroyDescriptorPool( vk.device, shs.rqPool, NULL );
		shs.rqPool = VK_NULL_HANDLE;
		shs.rqSet = VK_NULL_HANDLE;
	}
	if ( shs.rqCS ) {
		qvkDestroyShaderModule( vk.device, shs.rqCS, NULL );
		shs.rqCS = VK_NULL_HANDLE;
	}
	shs.rqReady = qfalse;
}

static qboolean SHS_EnsureRayQuery( void )
{
	VkDescriptorSetLayoutBinding bindings[4];
	VkDescriptorSetLayoutCreateInfo dci;
	VkPushConstantRange range;
	VkPipelineLayoutCreateInfo pci;
	VkShaderModuleCreateInfo sci;
	VkComputePipelineCreateInfo cpci;
	VkDescriptorPoolSize sizes[3];
	VkDescriptorPoolCreateInfo dpci;
	VkDescriptorSetAllocateInfo ai;
	VkResult res;

	if ( shs.rqReady ) {
		return qtrue;
	}
	if ( !vk.device || !vk.rayQueryAvailable ) {
		return qfalse;
	}

	Com_Memset( &sci, 0, sizeof( sci ) );
	sci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	sci.codeSize = VK_SHS_SUN_SHADOW_CS_SPV_SIZE;
	sci.pCode = (const uint32_t *)vk_shs_sun_shadow_cs_spv;
	res = qvkCreateShaderModule( vk.device, &sci, NULL, &shs.rqCS );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][SHS] ray-query shader module failed\n" S_COLOR_WHITE );
		return qfalse;
	}

	Com_Memset( bindings, 0, sizeof( bindings ) );
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	Com_Memset( &dci, 0, sizeof( dci ) );
	dci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dci.bindingCount = 4;
	dci.pBindings = bindings;
	VK_CHECK( qvkCreateDescriptorSetLayout( vk.device, &dci, NULL, &shs.rqLayout ) );

	Com_Memset( &range, 0, sizeof( range ) );
	range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	range.size = 112;
	Com_Memset( &pci, 0, sizeof( pci ) );
	pci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pci.setLayoutCount = 1;
	pci.pSetLayouts = &shs.rqLayout;
	pci.pushConstantRangeCount = 1;
	pci.pPushConstantRanges = &range;
	VK_CHECK( qvkCreatePipelineLayout( vk.device, &pci, NULL, &shs.rqPL ) );

	Com_Memset( &cpci, 0, sizeof( cpci ) );
	cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cpci.stage.module = shs.rqCS;
	cpci.stage.pName = "main";
	cpci.layout = shs.rqPL;
	res = qvkCreateComputePipelines( vk.device, vk.pipelineCache, 1, &cpci, NULL, &shs.rqPipe );
	if ( res != VK_SUCCESS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "[VK][SHS] ray-query pipeline failed\n" S_COLOR_WHITE );
		SHS_DestroyRayQuery();
		return qfalse;
	}

	Com_Memset( sizes, 0, sizeof( sizes ) );
	sizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	sizes[0].descriptorCount = 1;
	sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sizes[1].descriptorCount = 2;
	sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	sizes[2].descriptorCount = 1;
	Com_Memset( &dpci, 0, sizeof( dpci ) );
	dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.maxSets = 1;
	dpci.poolSizeCount = 3;
	dpci.pPoolSizes = sizes;
	VK_CHECK( qvkCreateDescriptorPool( vk.device, &dpci, NULL, &shs.rqPool ) );
	Com_Memset( &ai, 0, sizeof( ai ) );
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = shs.rqPool;
	ai.descriptorSetCount = 1;
	ai.pSetLayouts = &shs.rqLayout;
	VK_CHECK( qvkAllocateDescriptorSets( vk.device, &ai, &shs.rqSet ) );

	shs.rqReady = qtrue;
	ri.Printf( PRINT_ALL, "[VK][SHS] ray-query raw sun shadow pipeline ready\n" );
	return qtrue;
}
#endif /* USE_VULKAN_RTX */

void vk_shs_shutdown( void )
{
	if ( !shs.inited ) {
		return;
	}
	ri.Cmd_RemoveCommand( "shs_status" );
#ifdef USE_VULKAN_RTX
	SHS_DestroyRayQuery();
#endif
	Com_Memset( &shs, 0, sizeof( shs ) );
}

void vk_shs_frame_begin( void )
{
	uint32_t rev;

	if ( !shs.inited ) {
		vk_shs_init();
	}

	rev = vk_rtx_tlas_revision();
	shs.tlasOk = vk_rtx_scene_ready() ? qtrue : qfalse;
	if ( shs.lastTlasRevision != 0u && rev != shs.lastTlasRevision ) {
		vk_shs_invalidate_history( "tlas_revision_change" );
	}
	shs.lastTlasRevision = rev;

	/* Pipeline/descriptor notes are refreshed by Hybrid1 each frame; default optimistic
	 * when Hybrid1 is active so first resolve after init can claim RT. */
	if ( vk_hybrid1_active() ) {
		if ( !SHS_Fail( VK_SHS_FAIL_RT_PIPELINE ) ) {
			shs.rtPipelineOk = qtrue;
		}
		if ( !SHS_Fail( VK_SHS_FAIL_DESCRIPTOR ) ) {
			shs.descriptorOk = qtrue;
		}
		if ( !SHS_Fail( VK_SHS_FAIL_HISTORY ) ) {
			shs.historyOk = qtrue;
		}
	} else {
		shs.rtPipelineOk = qfalse;
		shs.descriptorOk = qfalse;
	}

	SHS_ResolveOwner();

	if ( Q_stricmp( shs.lastLogOwner, vk_shs_owner_name() ) ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][SHS] sun owner -> %s (fallback=%s)\n",
			vk_shs_owner_name(), vk_shs_fallback_reason() );
		Q_strncpyz( shs.lastLogOwner, vk_shs_owner_name(), sizeof( shs.lastLogOwner ) );
	}
}

vkShsSunOwner_t vk_shs_sun_owner( void )
{
	return shs.owner;
}

qboolean vk_shs_rt_owns_sun( void )
{
	return ( shs.owner == VK_SHS_OWNER_RT ) ? qtrue : qfalse;
}

qboolean vk_shs_raster_owns_sun( void )
{
	/* When feature off, raster may still use r_pbrSunShadow independently. */
	if ( !vk_shs_active() ) {
		return qtrue;
	}
	return ( shs.owner == VK_SHS_OWNER_RASTER ) ? qtrue : qfalse;
}

qboolean vk_shs_active( void )
{
	return SHS_FeatureRequested();
}

qboolean vk_shs_sun_only_rt( void )
{
	return vk_shs_rt_owns_sun();
}

qboolean vk_shs_prefer_ray_query( void )
{
#ifdef USE_VULKAN_RTX
	/* Default Hybrid1 RT pipeline so hybrid1_shadow.rahit can punch alpha holes.
	 * Opt-in rayQuery via r_shsPreferRayQuery (no any-hit). */
	if ( !vk_shs_rt_owns_sun() || !vk.rayQueryAvailable ) {
		return qfalse;
	}
	return ( r_shsPreferRayQuery && r_shsPreferRayQuery->integer ) ? qtrue : qfalse;
#else
	return qfalse;
#endif
}

qboolean vk_shs_pathtrace_blocks( void )
{
	return SHS_PathtraceBlocks();
}

const char *vk_shs_owner_name( void )
{
	if ( !vk_shs_active() ) {
		return "inactive";
	}
	return ( shs.owner == VK_SHS_OWNER_RT ) ? "hybrid1_rt" : "raster";
}

const char *vk_shs_fallback_reason( void )
{
	return shs.fallbackReason[0] ? shs.fallbackReason : "none";
}

uint32_t vk_shs_fail_inject( void )
{
	return r_shsFailInject ? (uint32_t)r_shsFailInject->integer : 0u;
}

qboolean vk_shs_fail_inject_active( int bit )
{
	return SHS_Fail( bit );
}

void vk_shs_note_rt_pipeline_ok( qboolean ok )
{
	shs.rtPipelineOk = ok;
}

void vk_shs_note_descriptor_ok( qboolean ok )
{
	shs.descriptorOk = ok;
}

void vk_shs_note_history_ok( qboolean ok )
{
	shs.historyOk = ok;
}

void vk_shs_note_tlas_ok( qboolean ok )
{
	shs.tlasOk = ok;
}

void vk_shs_invalidate_history( const char *reason )
{
	shs.historyOk = qfalse;
	if ( reason && reason[0] ) {
		SHS_SetFallback( reason );
	}
}

int vk_shs_debug_mode( void )
{
	return r_shsDebug ? r_shsDebug->integer : 0;
}

int vk_shs_composite_debug_mode( void )
{
	int shsDbg = vk_shs_debug_mode();
	int hybridDbg = r_hybrid1_debug ? r_hybrid1_debug->integer : 0;

	/* r_shsDebug maps onto hybrid1_composite debug codes (6+). */
	if ( shsDbg > 0 ) {
		static const int map[12] = {
			0,  /* 0 unused */
			6,  /* 1 raw RT vis */
			1,  /* 2 filtered / "raster-like" vis proxy */
			12, /* 3 RT/raster difference proxy */
			7,  /* 4 hit distance */
			8,  /* 5 TLAS coverage */
			9,  /* 6 alpha candidates */
			8,  /* 7 dynamic geo (coverage proxy until instance mask) */
			8,  /* 8 raster fallback mask proxy */
			10, /* 9 history weight */
			11, /* 10 reject reason */
			1   /* 11 final filtered */
		};
		if ( shsDbg >= 1 && shsDbg <= 11 ) {
			return map[shsDbg];
		}
	}
	return hybridDbg;
}

float vk_shs_temporal_alpha_floor( void )
{
	return r_shsTemporalAlphaFloor ? r_shsTemporalAlphaFloor->value : 0.28f;
}

uint32_t vk_shs_max_history_age( void )
{
	return r_shsMaxHistoryAge ? (uint32_t)r_shsMaxHistoryAge->integer : 16u;
}

qboolean vk_shs_record_raw_ray_query( VkCommandBuffer cmd, VkImageView rawView, uint32_t w, uint32_t h )
{
#ifdef USE_VULKAN_RTX
	struct {
		float invViewProj[16];
		float extent[4];
		float sunDirBias[4];
		float params[4];
	} push;
	VkDescriptorImageInfo depthInfo;
	VkDescriptorImageInfo normalInfo;
	VkDescriptorImageInfo outInfo;
	VkWriteDescriptorSet writes[3];
	VkSampler nearest;
	const float *view;
	const float *projection;
	float viewProj[16];
	float proj_vk[16];
	float invViewProj[16];

	if ( !cmd || rawView == VK_NULL_HANDLE || w == 0 || h == 0 ) {
		return qfalse;
	}
	if ( !vk_shs_prefer_ray_query() || !vk_rtx_scene_ready() ) {
		return qfalse;
	}
	if ( vk_shs_fail_inject_active( VK_SHS_FAIL_RT_PIPELINE ) ||
		vk_shs_fail_inject_active( VK_SHS_FAIL_DESCRIPTOR ) ) {
		return qfalse;
	}
	if ( !SHS_EnsureRayQuery() ) {
		vk_shs_note_rt_pipeline_ok( qfalse );
		return qfalse;
	}

	view = backEnd.viewParms.world.modelViewMatrix;
	projection = backEnd.useFirstPersonProjection
		? backEnd.firstPersonProjectionMatrix
		: backEnd.viewParms.projectionMatrix;
	vk_get_projection_matrix_vk( projection, proj_vk );
	myGlMultMatrix( view, proj_vk, viewProj );
	if ( !vk_mat4_inverse( viewProj, invViewProj ) ) {
		Com_Memcpy( invViewProj, viewProj, sizeof( invViewProj ) );
	}

	{
		Vk_Sampler_Def sd;
		Com_Memset( &sd, 0, sizeof( sd ) );
		sd.gl_mag_filter = sd.gl_min_filter = GL_NEAREST;
		sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sd.noAnisotropy = qtrue;
		nearest = vk_find_sampler( &sd );
	}

	Com_Memset( &depthInfo, 0, sizeof( depthInfo ) );
	depthInfo.sampler = nearest;
	depthInfo.imageView = vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	Com_Memset( &normalInfo, 0, sizeof( normalInfo ) );
	normalInfo.sampler = nearest;
	normalInfo.imageView = vk.deferred_gbuffer_normal_view ? vk.deferred_gbuffer_normal_view : depthInfo.imageView;
	normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	Com_Memset( &outInfo, 0, sizeof( outInfo ) );
	outInfo.imageView = rawView;
	outInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	vk_rtx_bind_tlas_descriptor( shs.rqSet );

	Com_Memset( writes, 0, sizeof( writes ) );
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = shs.rqSet;
	writes[0].dstBinding = 1;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &depthInfo;
	writes[1] = writes[0];
	writes[1].dstBinding = 2;
	writes[1].pImageInfo = &normalInfo;
	writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[2].dstSet = shs.rqSet;
	writes[2].dstBinding = 3;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].pImageInfo = &outInfo;
	qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );

	Com_Memcpy( push.invViewProj, invViewProj, sizeof( push.invViewProj ) );
	push.extent[0] = (float)w;
	push.extent[1] = (float)h;
	push.extent[2] = push.extent[3] = 0.0f;
	push.sunDirBias[0] = tr.sunDirection[0];
	push.sunDirBias[1] = tr.sunDirection[1];
	push.sunDirBias[2] = tr.sunDirection[2];
	push.sunDirBias[3] = r_hybrid1_rayBias ? r_hybrid1_rayBias->value : 0.02f;
	push.params[0] = r_hybrid1_tMin ? r_hybrid1_tMin->value : 0.01f;
	push.params[1] = 1.0e5f;
	push.params[2] = vk_deferred_gbuffer_fill_wanted() ? 1.0f : 0.0f;
	push.params[3] = (float)( backEnd.refdef.time & 0xffff );

	qvkCmdBindPipeline( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, shs.rqPipe );
	qvkCmdBindDescriptorSets( cmd, VK_PIPELINE_BIND_POINT_COMPUTE, shs.rqPL, 0, 1, &shs.rqSet, 0, NULL );
	qvkCmdPushConstants( cmd, shs.rqPL, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof( push ), &push );
	qvkCmdDispatch( cmd, ( w + 7u ) / 8u, ( h + 7u ) / 8u, 1 );

	vk_shs_note_rt_pipeline_ok( qtrue );
	vk_shs_note_descriptor_ok( qtrue );
	return qtrue;
#else
	(void)cmd;
	(void)rawView;
	(void)w;
	(void)h;
	return qfalse;
#endif
}
