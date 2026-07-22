/*
===========================================================================
Selective Hybrid Reflections 1.0 — exclusive specular reflection router.

Waterfall: Hybrid1 RT (or RT pipeline) → SSR → probe/sky.
Never add RT + SSR + probe at full strength.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_selective_reflection.h"
#include "vk_reflection_hierarchy.h"
#include "vk_rtx.h"
#include "vk_hybrid1.h"
#include "vk_postfx.h"
#include "tr_render_mode_vk.h"

typedef struct {
	qboolean inited;
	vkShrReflectionOwner_t owner;
	char fallbackReason[128];
	char lastLogOwner[32];
	qboolean rtPipelineOk;
	qboolean descriptorOk;
	qboolean historyOk;
	qboolean tlasOk;
	qboolean ssrOk;
	uint32_t lastTlasRevision;
} shr_state_t;

static shr_state_t shr;

static cvar_t *r_selectiveHybridReflection;
static cvar_t *r_havenrpReflectionOwner;
static cvar_t *r_shrFailInject;
static cvar_t *r_shrDebug;
static cvar_t *r_shrRoughnessRtMax;
static cvar_t *r_shrTemporalAlphaFloor;
static cvar_t *r_shrMaxHistoryAge;
static cvar_t *r_shrProbeSpecOcclusion;
static cvar_t *r_havenrpFallbackReason;

static void SHR_SetFallback( const char *reason )
{
	if ( !reason ) {
		reason = "none";
	}
	Q_strncpyz( shr.fallbackReason, reason, sizeof( shr.fallbackReason ) );
	if ( r_havenrpFallbackReason ) {
		ri.Cvar_Set( "r_havenrpFallbackReason", shr.fallbackReason );
	}
}

static qboolean SHR_PathtraceBlocks( void )
{
	if ( R_RenderMode_IsPathTracedReference() ) {
		return qtrue;
	}
	if ( r_pathtrace && r_pathtrace->integer ) {
		return qtrue;
	}
	return qfalse;
}

static qboolean SHR_FeatureRequested( void )
{
	if ( r_selectiveHybridReflection && r_selectiveHybridReflection->integer ) {
		return qtrue;
	}
	if ( R_RenderMode_IsSelectiveHybrid() && r_hybrid1 && r_hybrid1->integer &&
		( !r_hybrid1_spec || r_hybrid1_spec->integer ) ) {
		return qtrue;
	}
	return qfalse;
}

static qboolean SHR_Fail( int bit )
{
	return ( r_shrFailInject && ( r_shrFailInject->integer & bit ) ) ? qtrue : qfalse;
}

static qboolean SHR_RtHealthReady( void )
{
#ifdef USE_VULKAN_RTX
	if ( SHR_Fail( VK_SHR_FAIL_TLAS ) ) {
		shr.tlasOk = qfalse;
		SHR_SetFallback( "shr_fail_inject_tlas" );
		return qfalse;
	}
	if ( SHR_Fail( VK_SHR_FAIL_RT_PIPELINE ) ) {
		shr.rtPipelineOk = qfalse;
		SHR_SetFallback( "shr_fail_inject_rt_pipeline" );
		return qfalse;
	}
	if ( SHR_Fail( VK_SHR_FAIL_DESCRIPTOR ) ) {
		shr.descriptorOk = qfalse;
		SHR_SetFallback( "shr_fail_inject_descriptor" );
		return qfalse;
	}
	if ( SHR_Fail( VK_SHR_FAIL_HISTORY ) ) {
		shr.historyOk = qfalse;
		SHR_SetFallback( "shr_fail_inject_history" );
		return qfalse;
	}
	if ( !vk_rtx_scene_ready() || !shr.tlasOk ) {
		SHR_SetFallback( vk_rtx_scene_ready() ? "shr_tlas_unhealthy" : "shr_tlas_not_ready" );
		return qfalse;
	}
	if ( !vk.rtxAvailable || !shr.rtPipelineOk || !vk_hybrid1_active() ) {
		SHR_SetFallback( !vk.rtxAvailable ? "shr_rtx_unavailable" : "shr_rt_pipeline_not_ready" );
		return qfalse;
	}
	if ( !shr.descriptorOk ) {
		SHR_SetFallback( "shr_descriptor_unhealthy" );
		return qfalse;
	}
	if ( !shr.historyOk ) {
		SHR_SetFallback( "shr_history_unhealthy" );
		return qfalse;
	}
	return qtrue;
#else
	SHR_SetFallback( "shr_rtx_build_disabled" );
	return qfalse;
#endif
}

static qboolean SHR_SsrHealthReady( void )
{
	if ( SHR_Fail( VK_SHR_FAIL_SSR ) ) {
		shr.ssrOk = qfalse;
		SHR_SetFallback( "shr_fail_inject_ssr" );
		return qfalse;
	}
	if ( !vk.fboActive || !shr.ssrOk ) {
		SHR_SetFallback( "shr_ssr_unavailable" );
		return qfalse;
	}
	if ( !PostFX_SSR_IsEnabled() ) {
		SHR_SetFallback( "shr_ssr_disabled" );
		return qfalse;
	}
	return qtrue;
}

static void SHR_NoteHierarchy( void )
{
	vkReflectSource_t src = VK_REFLECT_SRC_PROBE;
	float weight = 1.0f;

	switch ( shr.owner ) {
	case VK_SHR_OWNER_SSR:
		src = VK_REFLECT_SRC_SSR;
		break;
	case VK_SHR_OWNER_RT:
	case VK_SHR_OWNER_PATH_TRACER:
		src = VK_REFLECT_SRC_RAY;
		break;
	case VK_SHR_OWNER_PROBE:
	case VK_SHR_OWNER_FALLBACK:
		src = VK_REFLECT_SRC_PROBE;
		break;
	case VK_SHR_OWNER_OFF:
	default:
		src = VK_REFLECT_SRC_NONE;
		weight = 0.0f;
		break;
	}
	vk_reflection_hierarchy_note( src, weight, shr.fallbackReason );
}

static void SHR_ResolveOwner( void )
{
	const char *pref;

	shr.owner = VK_SHR_OWNER_OFF;

	if ( !SHR_FeatureRequested() ) {
		SHR_SetFallback( "none" );
		SHR_NoteHierarchy();
		return;
	}

	if ( SHR_PathtraceBlocks() ) {
		shr.owner = VK_SHR_OWNER_PATH_TRACER;
		SHR_SetFallback( "shr_blocked_by_pathtrace" );
		SHR_NoteHierarchy();
		return;
	}

	if ( !vk.fboActive ) {
		shr.owner = VK_SHR_OWNER_PROBE;
		SHR_SetFallback( "shr_requires_fbo" );
		SHR_NoteHierarchy();
		return;
	}

	pref = r_havenrpReflectionOwner ? r_havenrpReflectionOwner->string : "auto";

	if ( !Q_stricmp( pref, "off" ) ) {
		shr.owner = VK_SHR_OWNER_OFF;
		SHR_SetFallback( "shr_owner_forced_off" );
		SHR_NoteHierarchy();
		return;
	}
	if ( !Q_stricmp( pref, "probe" ) ) {
		shr.owner = VK_SHR_OWNER_PROBE;
		SHR_SetFallback( "shr_owner_forced_probe" );
		SHR_NoteHierarchy();
		return;
	}
	if ( !Q_stricmp( pref, "ssr" ) ) {
		if ( SHR_SsrHealthReady() ) {
			shr.owner = VK_SHR_OWNER_SSR;
			SHR_SetFallback( "none" );
		} else {
			shr.owner = VK_SHR_OWNER_PROBE;
		}
		SHR_NoteHierarchy();
		return;
	}
	if ( !Q_stricmp( pref, "hybrid1_rt" ) || !Q_stricmp( pref, "rt" ) || !Q_stricmp( pref, "auto" ) ) {
		if ( SHR_RtHealthReady() ) {
			shr.owner = VK_SHR_OWNER_RT;
			SHR_SetFallback( "none" );
			SHR_NoteHierarchy();
			return;
		}
		if ( SHR_SsrHealthReady() ) {
			shr.owner = VK_SHR_OWNER_SSR;
			if ( !Q_stricmp( shr.fallbackReason, "none" ) ) {
				SHR_SetFallback( "shr_rt_demote_to_ssr" );
			}
			SHR_NoteHierarchy();
			return;
		}
		shr.owner = VK_SHR_OWNER_PROBE;
		if ( !Q_stricmp( shr.fallbackReason, "none" ) ) {
			SHR_SetFallback( "shr_rt_ssr_demote_to_probe" );
		}
		SHR_NoteHierarchy();
		return;
	}

	shr.owner = VK_SHR_OWNER_PROBE;
	SHR_SetFallback( "shr_owner_unknown_pref" );
	SHR_NoteHierarchy();
}

static void SHR_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"[VK][SHR] active=%d owner=%s rt=%d ssrAllowed=%d suppressIbl=%d hybrid1=%d tlas=%d\n"
		"  failInject=0x%x pipelineOk=%d descOk=%d histOk=%d tlasOk=%d ssrOk=%d\n"
		"  fallback=%s debug=%d roughRtMax=%.2f alphaFloor=%.2f maxAge=%u pathtraceBlock=%d\n"
		"  waterfall: RT(hit) → SSR → probe/sky; never add all three at full strength\n",
		vk_shr_active() ? 1 : 0,
		vk_shr_owner_name(),
		vk_shr_rt_owns() ? 1 : 0,
		vk_shr_ssr_allowed() ? 1 : 0,
		vk_shr_suppress_gen_frag_ibl_spec() ? 1 : 0,
		vk_hybrid1_active() ? 1 : 0,
		vk_rtx_scene_ready() ? 1 : 0,
		vk_shr_fail_inject(),
		shr.rtPipelineOk ? 1 : 0,
		shr.descriptorOk ? 1 : 0,
		shr.historyOk ? 1 : 0,
		shr.tlasOk ? 1 : 0,
		shr.ssrOk ? 1 : 0,
		vk_shr_fallback_reason(),
		vk_shr_debug_mode(),
		vk_shr_roughness_rt_max(),
		vk_shr_temporal_alpha_floor(),
		vk_shr_max_history_age(),
		vk_shr_pathtrace_blocks() ? 1 : 0 );
}

void vk_shr_init( void )
{
	if ( shr.inited ) {
		return;
	}

	r_selectiveHybridReflection = ri.Cvar_Get( "r_selectiveHybridReflection", "0",
		CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_selectiveHybridReflection, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_selectiveHybridReflection,
		"Selective Hybrid Reflections 1.0: exclusive RT→SSR→probe waterfall. "
		"Does not change modern_vulkan.cfg boot defaults." );
	ri.Cvar_SetGroup( r_selectiveHybridReflection, CVG_RENDERER );

	r_havenrpReflectionOwner = ri.Cvar_Get( "r_havenrpReflectionOwner", "auto", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_havenrpReflectionOwner,
		"Reflection owner preference: auto|rt|hybrid1_rt|ssr|probe|off. "
		"Effective owner may demote on health failure." );
	ri.Cvar_SetGroup( r_havenrpReflectionOwner, CVG_RENDERER );

	r_shrFailInject = ri.Cvar_Get( "r_shrFailInject", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_shrFailInject, "0", "31", CV_INTEGER );
	ri.Cvar_SetDescription( r_shrFailInject,
		"SHR fail inject: 1=TLAS 2=RT pipeline 4=descriptor 8=history 16=SSR." );
	ri.Cvar_SetGroup( r_shrFailInject, CVG_RENDERER );

	r_shrDebug = ri.Cvar_Get( "r_shrDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_shrDebug, "0", "25", CV_INTEGER );
	ri.Cvar_SetDescription( r_shrDebug,
		"SHR debug views (see docs/SELECTIVE_HYBRID_REFLECTIONS_1.0.md)." );
	ri.Cvar_SetGroup( r_shrDebug, CVG_RENDERER );

	r_shrRoughnessRtMax = ri.Cvar_Get( "r_shrRoughnessRtMax", "0.55", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_shrRoughnessRtMax, "0.2", "0.98", CV_FLOAT );
	ri.Cvar_SetDescription( r_shrRoughnessRtMax,
		"Skip RT specular above this roughness (probe preferred for rough lobes)." );
	ri.Cvar_SetGroup( r_shrRoughnessRtMax, CVG_RENDERER );

	r_shrTemporalAlphaFloor = ri.Cvar_Get( "r_shrTemporalAlphaFloor", "0.25", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_shrTemporalAlphaFloor, "0.05", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_shrTemporalAlphaFloor,
		"Minimum blend toward current raw reflection (prefer noise over trails)." );
	ri.Cvar_SetGroup( r_shrTemporalAlphaFloor, CVG_RENDERER );

	r_shrMaxHistoryAge = ri.Cvar_Get( "r_shrMaxHistoryAge", "16", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_shrMaxHistoryAge, "1", "64", CV_INTEGER );
	ri.Cvar_SetDescription( r_shrMaxHistoryAge, "Finite maximum SHR reflection history age." );
	ri.Cvar_SetGroup( r_shrMaxHistoryAge, CVG_RENDERER );

	r_shrProbeSpecOcclusion = ri.Cvar_Get( "r_shrProbeSpecOcclusion", "0.35", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_shrProbeSpecOcclusion, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_shrProbeSpecOcclusion,
		"Specular occlusion strength applied only to unresolved probe/IBL fallback (not valid RT)." );
	ri.Cvar_SetGroup( r_shrProbeSpecOcclusion, CVG_RENDERER );

	r_havenrpFallbackReason = ri.Cvar_Get( "r_havenrpFallbackReason", "none", CVAR_ROM );

	ri.Cmd_AddCommand( "shr_status", SHR_Status_f );

	shr.owner = VK_SHR_OWNER_OFF;
	shr.rtPipelineOk = qfalse;
	shr.descriptorOk = qfalse;
	shr.historyOk = qtrue;
	shr.tlasOk = qfalse;
	shr.ssrOk = qtrue;
	SHR_SetFallback( "none" );
	shr.inited = qtrue;

	ri.Printf( PRINT_ALL,
		"[VK][SHR] Selective Hybrid Reflections 1.0 router (r_selectiveHybridReflection=%d owner=%s)\n",
		r_selectiveHybridReflection->integer, r_havenrpReflectionOwner->string );
}

void vk_shr_shutdown( void )
{
	if ( !shr.inited ) {
		return;
	}
	ri.Cmd_RemoveCommand( "shr_status" );
	Com_Memset( &shr, 0, sizeof( shr ) );
}

void vk_shr_frame_begin( void )
{
	uint32_t rev;

	if ( !shr.inited ) {
		vk_shr_init();
	}

	rev = vk_rtx_tlas_revision();
	shr.tlasOk = vk_rtx_scene_ready() ? qtrue : qfalse;
	if ( shr.lastTlasRevision != 0u && rev != shr.lastTlasRevision ) {
		vk_shr_invalidate_history( "tlas_revision_change" );
	}
	shr.lastTlasRevision = rev;

	if ( vk_hybrid1_active() ) {
		if ( !SHR_Fail( VK_SHR_FAIL_RT_PIPELINE ) ) {
			shr.rtPipelineOk = qtrue;
		}
		if ( !SHR_Fail( VK_SHR_FAIL_DESCRIPTOR ) ) {
			shr.descriptorOk = qtrue;
		}
		if ( !SHR_Fail( VK_SHR_FAIL_HISTORY ) ) {
			shr.historyOk = qtrue;
		}
	} else {
		shr.rtPipelineOk = qfalse;
		shr.descriptorOk = qfalse;
	}

	/* SSR readiness: FBO + pipeline present (detailed check at pass time). */
	if ( !SHR_Fail( VK_SHR_FAIL_SSR ) ) {
		shr.ssrOk = ( vk.fboActive && vk.ssr_pipeline != VK_NULL_HANDLE ) ? qtrue : qfalse;
	}

	SHR_ResolveOwner();

	if ( Q_stricmp( shr.lastLogOwner, vk_shr_owner_name() ) ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][SHR] reflection owner -> %s (fallback=%s)\n",
			vk_shr_owner_name(), vk_shr_fallback_reason() );
		Q_strncpyz( shr.lastLogOwner, vk_shr_owner_name(), sizeof( shr.lastLogOwner ) );
	}
}

qboolean vk_shr_active( void )
{
	return SHR_FeatureRequested();
}

vkShrReflectionOwner_t vk_shr_owner( void )
{
	return shr.owner;
}

qboolean vk_shr_rt_owns( void )
{
	return ( shr.owner == VK_SHR_OWNER_RT ) ? qtrue : qfalse;
}

qboolean vk_shr_ssr_allowed( void )
{
	if ( !vk_shr_active() ) {
		/* Legacy: SSR independent when SHR off */
		return qtrue;
	}
	if ( shr.owner == VK_SHR_OWNER_SSR ) {
		return qtrue;
	}
	/* When RT owns, SSR must not also write unrestricted energy. */
	if ( shr.owner == VK_SHR_OWNER_RT ) {
		return qfalse;
	}
	return qfalse;
}

qboolean vk_shr_suppress_gen_frag_ibl_spec( void )
{
	/* When RT owns reflections, Hybrid1 miss already supplies probe/IBL — avoid double IBL. */
	return vk_shr_rt_owns();
}

qboolean vk_shr_pathtrace_blocks( void )
{
	return SHR_PathtraceBlocks();
}

const char *vk_shr_owner_name( void )
{
	switch ( shr.owner ) {
	case VK_SHR_OWNER_OFF: return "off";
	case VK_SHR_OWNER_PROBE: return "probe";
	case VK_SHR_OWNER_SSR: return "ssr";
	case VK_SHR_OWNER_RT: return "hybrid1_rt";
	case VK_SHR_OWNER_PATH_TRACER: return "path_tracer";
	case VK_SHR_OWNER_FALLBACK: return "fallback";
	default: return "unknown";
	}
}

const char *vk_shr_fallback_reason( void )
{
	return shr.fallbackReason[0] ? shr.fallbackReason : "none";
}

uint32_t vk_shr_fail_inject( void )
{
	return r_shrFailInject ? (uint32_t)r_shrFailInject->integer : 0u;
}

qboolean vk_shr_fail_inject_active( int bit )
{
	return SHR_Fail( bit );
}

void vk_shr_note_rt_pipeline_ok( qboolean ok ) { shr.rtPipelineOk = ok; }
void vk_shr_note_descriptor_ok( qboolean ok ) { shr.descriptorOk = ok; }
void vk_shr_note_history_ok( qboolean ok ) { shr.historyOk = ok; }
void vk_shr_note_tlas_ok( qboolean ok ) { shr.tlasOk = ok; }
void vk_shr_note_ssr_ok( qboolean ok ) { shr.ssrOk = ok; }

void vk_shr_invalidate_history( const char *reason )
{
	shr.historyOk = qfalse;
	if ( reason && reason[0] ) {
		SHR_SetFallback( reason );
	}
}

int vk_shr_debug_mode( void )
{
	return r_shrDebug ? r_shrDebug->integer : 0;
}

int vk_shr_composite_debug_mode( void )
{
	int shrDbg = vk_shr_debug_mode();
	int hybridDbg = r_hybrid1_debug ? r_hybrid1_debug->integer : 0;

	/* r_shrDebug maps onto hybrid1_composite debug codes. */
	if ( shrDbg > 0 ) {
		static const int map[16] = {
			0,
			2,  /* 1 raw / filtered RT reflection */
			2,  /* 2 denoised reflection proxy */
			14, /* 3 selected source RGB */
			13, /* 4 RT hit confidence */
			13, /* 5 SSR conf proxy (same until SSR resolve) */
			14, /* 6 probe weight proxy */
			14, /* 7 weights */
			8,  /* 8 hit/miss via TLAS cov proxy */
			7,  /* 9 hit distance proxy (shadow raw) */
			10, /* 10 history weight */
			11, /* 11 reject reason */
			10, /* 12 history age (weight channel) */
			12, /* 13 RT vs SSR proxy */
			2,  /* 14 hybrid vs probe */
			14  /* 15 world/weapon mask proxy */
		};
		if ( shrDbg >= 1 && shrDbg <= 15 ) {
			return map[shrDbg];
		}
	}
	return hybridDbg;
}

float vk_shr_roughness_rt_max( void )
{
	return r_shrRoughnessRtMax ? r_shrRoughnessRtMax->value : 0.55f;
}

float vk_shr_temporal_alpha_floor( void )
{
	return r_shrTemporalAlphaFloor ? r_shrTemporalAlphaFloor->value : 0.25f;
}

uint32_t vk_shr_max_history_age( void )
{
	return r_shrMaxHistoryAge ? (uint32_t)r_shrMaxHistoryAge->integer : 16u;
}

float vk_shr_probe_specular_occlusion_strength( void )
{
	return r_shrProbeSpecOcclusion ? r_shrProbeSpecOcclusion->value : 0.35f;
}
