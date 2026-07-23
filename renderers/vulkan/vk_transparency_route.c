/*
===========================================================================
Raster Ultra 1.4 — transparency classification + refractive exclusion helpers.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_transparency_route.h"
#include "vk_forward_plus.h"
#include "vk_oit_certify.h"
#include "vk_oit_contract.h"
#include "vk_oit_alpha.h"
#include "vk_depth_contract.h"
#include "vk_hdr_resolve_contract.h"

static cvar_t *r_transparencyDebug;
static cvar_t *r_refractiveExcludeOit;
static qboolean s_inited;

static const char *VK_Oit_ImplName( int mode )
{
	if ( mode == 1 ) {
		return "WBOIT";
	}
	if ( mode == 2 ) {
		return "MBOIT";
	}
	return "off";
}

static void VK_Oit_Status_f( void )
{
	const int requested = r_oit ? r_oit->integer : 0;
	int effective = 0;
	const char *viewClass = "world";
	const char *stateName = "UNTOUCHED";
	const int msaa = ( r_ext_multisample && r_ext_multisample->integer ) ? r_ext_multisample->integer : 0;
	const int taa = ( r_taa && r_taa->integer ) ? r_taa->integer : 0;
	const int aaMode = ( r_aaMode && r_aaMode->integer ) ? r_aaMode->integer : 0;
	const int renderMode = ( r_renderMode && r_renderMode->integer ) ? r_renderMode->integer : 0;
	const oitContract_t *oitContract = vk_oit_contract_wboit();

	if ( requested > 0 && vk.fboActive &&
		vk.oitDescriptorGeneration == vk.oitAttachmentGeneration &&
		vk.oitAttachmentGeneration > 0 &&
		vk.framebuffers.oit_accum != VK_NULL_HANDLE &&
		vk.framebuffers.oit_resolve != VK_NULL_HANDLE &&
		!vk.oitUnhealthy ) {
		effective = requested;
	}

	if ( backEnd.refdef.rdflags & RDF_NOWORLDMODEL ) {
		viewClass = "noworldmodel";
	}

	switch ( vk.oitFrameState ) {
	case VK_OIT_FRAME_CLEARED: stateName = "CLEARED"; break;
	case VK_OIT_FRAME_ACCUMULATED: stateName = "ACCUMULATED"; break;
	case VK_OIT_FRAME_RESOLVED: stateName = "RESOLVED"; break;
	default: stateName = "UNTOUCHED"; break;
	}

	ri.Printf( PRINT_ALL,
		"oit_status:\n"
		"  implementation=%s mode=%d effective=%d classify=%d forwardPlus=%d refractiveExclude=%d directTest=%d\n"
		"  contract: WBOIT v%u hash=0x%08x (oit_contract_status)\n"
		"  alpha: cert=%s (oit_alpha_status)\n"
		"  depth: contract v%u hash=0x%08x (depth_contract_status)\n"
		"  profileSource=%s renderMode=%d\n"
		"  formats: accum=R16G16B16A16_SFLOAT reveal=R16_SFLOAT color=%s\n"
		"  litPath=%s\n"
		"  activeExtent=%ux%u allocatedExtent=%ux%u (mainColor=%ux%u render=%ux%u)\n"
		"  sampleCount=%d msaa=%d taa=%d aaMode=%d\n"
		"  attachmentGen=%u descriptorGen=%u match=%d\n"
		"  clusterGen=%u lightBufferGen=%u clusterMismatch=%u\n"
		"  clearCount=%u accumPassCount=%u resolveCount=%u drawSurfs=%u\n"
		"  clearedThisFrame=%d frameState=%s weaponExcluded=%d unhealthy=%d fallbacks=%u\n"
		"  corruption=%u boundsViolations=%u\n"
		"  resourceValid=%d\n"
		"  lastInvalidation=%s\n"
		"  lastFallback=%s\n"
		"  FrameContext: frame=%u cmdIndex=%u swapchainImage=%u (fif=%d swapCount=%u)\n"
		"  viewClass=%s passOrder=opaque->deferred->oit_accum->oit_resolve->refractive->weapon->bloom->exposure->tonemap->grade->display->ui\n"
		"  resolveRP=UNDEFINED/DONT_CARE→SHADER_READ (discard; fullscreen rewrite)\n"
		"  perfUs: clear=%u accum=%u resolve=%u (CPU markers; see oit_perf)\n",
		VK_Oit_ImplName( requested ), requested, effective,
		r_oitClassify ? r_oitClassify->integer : 0,
		r_oitForwardPlus ? r_oitForwardPlus->integer : 0,
		r_refractiveExcludeOit ? r_refractiveExcludeOit->integer : 1,
		ri.Cvar_VariableIntegerValue( "r_oitDirectTest" ),
		oitContract->contractVersion, oitContract->contractHash,
		vk_oit_alpha_cert_level_name( vk_oit_alpha_certification_level() ),
		vk_depth_contract_get()->contractVersion, vk_depth_contract_get()->contractHash,
		vk.oitProfileSourceHint[0] ? vk.oitProfileSourceHint : "(runtime)",
		renderMode,
		vk_format_string( vk.color_format ),
		( r_oitForwardPlus && r_oitForwardPlus->integer ) ? "Forward+ clustered" : "unlit",
		vk.oitExtentWidth, vk.oitExtentHeight,
		vk.oitAllocatedExtentWidth ? vk.oitAllocatedExtentWidth : vk.oitExtentWidth,
		vk.oitAllocatedExtentHeight ? vk.oitAllocatedExtentHeight : vk.oitExtentHeight,
		vk.mainColorWidth, vk.mainColorHeight,
		vk.renderWidth, vk.renderHeight,
		msaa > 0 ? msaa : 1, msaa, taa, aaMode,
		vk.oitAttachmentGeneration, vk.oitDescriptorGeneration,
		( vk.oitDescriptorGeneration == vk.oitAttachmentGeneration && vk.oitAttachmentGeneration > 0 ) ? 1 : 0,
		vk.oitClusterGenAtAccum ? vk.oitClusterGenAtAccum : vk_cluster_list_generation(),
		vk.oitLightBufferGenAtAccum ? vk.oitLightBufferGenAtAccum : vk_cluster_list_generation(),
		vk.oitClusterMismatchCount,
		vk.oitClearCount, vk.oitAccumPassCount, vk.oitResolveCount, vk.oitDrawCount,
		vk.oitClearedThisFrame ? 1 : 0,
		stateName,
		vk.oitWeaponExcluded ? 1 : 0,
		vk.oitUnhealthy ? 1 : 0,
		vk.oitFallbackCount,
		vk.oitCorruptionCount,
		vk.oitBoundsViolationCount,
		effective > 0 ? 1 : 0,
		vk.oitLastInvalidationReason[0] ? vk.oitLastInvalidationReason : "(none)",
		vk.oitLastFallbackReason[0] ? vk.oitLastFallbackReason : "(none)",
		vk.oitFrameNumber,
		vk.oitCmdIndex,
		vk.oitSwapchainImageIndex,
		NUM_COMMAND_BUFFERS,
		vk.swapchain_image_count,
		viewClass,
		vk.oitLastPerfClearUs, vk.oitLastPerfAccumUs, vk.oitLastPerfResolveUs );
}

static void VK_Oit_Perf_f( void )
{
	const int requested = r_oit ? r_oit->integer : 0;
	ri.Printf( PRINT_ALL,
		"oit_perf:\n"
		"  mode=%d (%s) resolution=%ux%u\n"
		"  clearUs=%u accumUs=%u resolveUs=%u totalUs=%u\n"
		"  clearCount=%u accumPassCount=%u resolveCount=%u drawSurfs=%u\n"
		"  clusterGen=%u lightRefs=clustered Forward+\n"
		"  resourceMemory=approx accum+reveal R16F targets at active extent\n"
		"  overdrawEstimate=see r_oitDebug 12\n",
		requested, VK_Oit_ImplName( requested ),
		vk.oitExtentWidth, vk.oitExtentHeight,
		vk.oitLastPerfClearUs, vk.oitLastPerfAccumUs, vk.oitLastPerfResolveUs,
		vk.oitLastPerfClearUs + vk.oitLastPerfAccumUs + vk.oitLastPerfResolveUs,
		vk.oitClearCount, vk.oitAccumPassCount, vk.oitResolveCount, vk.oitDrawCount,
		vk.oitClusterGenAtAccum ? vk.oitClusterGenAtAccum : vk_cluster_list_generation() );
}

static void VK_Oit_Capture_f( void )
{
	const char *arg = ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "context";

	if ( !Q_stricmp( arg, "stages" ) || !Q_stricmp( arg, "all" ) ) {
		vk.oitCapturePending = VK_OIT_CAPTURE_CONTEXT | VK_OIT_CAPTURE_STAGES;
		ri.Printf( PRINT_ALL, "oit_capture: next OIT pass will log FrameContext + accum/resolve stages\n" );
	} else if ( !Q_stricmp( arg, "context" ) ) {
		vk.oitCapturePending = VK_OIT_CAPTURE_CONTEXT;
		ri.Printf( PRINT_ALL, "oit_capture: next OIT pass will log FrameContext\n" );
	} else {
		ri.Printf( PRINT_ALL, "usage: oit_capture [context|stages|all]\n" );
		ri.Printf( PRINT_ALL, "  Then screenshot with r_oitDebug 1..16 to save stage visuals.\n" );
	}
}

const char *vk_transparency_class_name( vkTransparencyClass_t cls )
{
	switch ( cls ) {
	case VK_XPARENT_ALPHA_TESTED: return "alpha_tested";
	case VK_XPARENT_SORTED_ALPHA: return "sorted_alpha";
	case VK_XPARENT_WBOIT: return "wboit";
	case VK_XPARENT_ADDITIVE: return "additive";
	case VK_XPARENT_MODULATE: return "modulate";
	case VK_XPARENT_REFRACTIVE: return "refractive";
	case VK_XPARENT_WATER: return "water";
	case VK_XPARENT_GLASS: return "glass";
	case VK_XPARENT_DISTORTION_ONLY: return "distortion_only";
	case VK_XPARENT_PARTICLE: return "particle";
	case VK_XPARENT_DECAL: return "decal";
	case VK_XPARENT_UI: return "ui";
	default: return "unknown";
	}
}

qboolean vk_transparency_is_additive( const shader_t *shader )
{
	unsigned stageBits, src, dst;

	if ( !shader || !shader->stages[0] ) {
		return qfalse;
	}
	stageBits = shader->stages[0]->stateBits;
	src = stageBits & GLS_SRCBLEND_BITS;
	dst = stageBits & GLS_DSTBLEND_BITS;
	return ( src == GLS_SRCBLEND_ONE && dst == GLS_DSTBLEND_ONE ) ? qtrue : qfalse;
}

qboolean vk_transparency_is_refractive( const shader_t *shader )
{
	if ( !shader ) {
		return qfalse;
	}
	/* Real refraction / portal / distortion: needs screenMap or explicit name.
	 * Do NOT treat every "glass"/"water" blend shader as refractive — OpenArena
	 * glass is usually simple alpha and belongs in WBOIT when r_oit 1. */
	if ( shader->hasScreenMap ) {
		return qtrue;
	}
	if ( shader->name[0] ) {
		if ( Q_stristr( shader->name, "portal" ) ||
			Q_stristr( shader->name, "refract" ) ||
			Q_stristr( shader->name, "distort" ) ||
			Q_stristr( shader->name, "screenmap" ) ) {
			if ( shader->sort >= SS_BLEND0 ) {
				return qtrue;
			}
		}
	}
	return qfalse;
}

/* Classify water/glass for debug / routing without forcing refractive exclusion. */
static qboolean VK_Transparency_NameIsWater( const shader_t *shader )
{
	return ( shader && shader->name[0] && Q_stristr( shader->name, "water" ) ) ? qtrue : qfalse;
}

static qboolean VK_Transparency_NameIsGlass( const shader_t *shader )
{
	return ( shader && shader->name[0] &&
		( Q_stristr( shader->name, "glass" ) || Q_stristr( shader->name, "window" ) ||
			Q_stristr( shader->name, "trans" ) ) ) ? qtrue : qfalse;
}

vkTransparencyClass_t vk_transparency_classify_shader( const shader_t *shader )
{
	unsigned stageBits, src, dst;
	qboolean oitOn;

	if ( !shader ) {
		return VK_XPARENT_SORTED_ALPHA;
	}
	if ( shader->isSky ) {
		return VK_XPARENT_SORTED_ALPHA;
	}

	/* Alpha test / stochastic clip: opaque-ish cutout. */
	if ( shader->stages[0] && ( shader->stages[0]->stateBits & GLS_ATEST_BITS ) ) {
		return VK_XPARENT_ALPHA_TESTED;
	}

	if ( vk_transparency_is_refractive( shader ) ) {
		if ( VK_Transparency_NameIsWater( shader ) ) {
			return VK_XPARENT_WATER;
		}
		if ( VK_Transparency_NameIsGlass( shader ) ) {
			return VK_XPARENT_GLASS;
		}
		return VK_XPARENT_REFRACTIVE;
	}

	if ( !shader->stages[0] ) {
		return VK_XPARENT_SORTED_ALPHA;
	}
	stageBits = shader->stages[0]->stateBits;
	src = stageBits & GLS_SRCBLEND_BITS;
	dst = stageBits & GLS_DSTBLEND_BITS;

	if ( src == GLS_SRCBLEND_ONE && dst == GLS_DSTBLEND_ONE ) {
		if ( shader->entityMergable ) {
			return VK_XPARENT_PARTICLE;
		}
		return VK_XPARENT_ADDITIVE;
	}
	if ( src == GLS_SRCBLEND_ZERO &&
		( dst == GLS_DSTBLEND_SRC_COLOR || dst == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) ) {
		return VK_XPARENT_MODULATE;
	}

	oitOn = ( r_oit && r_oit->integer == 1 ) ? qtrue : qfalse;
	if ( oitOn && shader->sort >= SS_BLEND0 && shader->sort <= SS_BLEND6 ) {
		materialTransparencyInfo_t ainfo;
		vk_oit_alpha_query_shader( shader, &ainfo );
		if ( !ainfo.wboitEligible ) {
			return VK_XPARENT_SORTED_ALPHA;
		}
		/* Label OA glass/water for debug; they accumulate in WBOIT (not refractive). */
		if ( VK_Transparency_NameIsWater( shader ) ) {
			return VK_XPARENT_WATER;
		}
		if ( VK_Transparency_NameIsGlass( shader ) ) {
			return VK_XPARENT_GLASS;
		}
		return VK_XPARENT_WBOIT;
	}
	if ( shader->sort >= SS_BLEND0 ) {
		return VK_XPARENT_SORTED_ALPHA;
	}
	return VK_XPARENT_SORTED_ALPHA;
}

qboolean vk_transparency_debug_active( void )
{
	return ( r_transparencyDebug && r_transparencyDebug->integer ) ? qtrue : qfalse;
}

static void VK_TransparencyRoute_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"transparencyRoute: oit=%d classify=%d refractiveExclude=%d debug=%d\n"
		"  classes: alpha_tested sorted_alpha wboit additive modulate refractive "
		"water glass distortion particle decal ui\n",
		r_oit ? r_oit->integer : 0,
		r_oitClassify ? r_oitClassify->integer : 0,
		r_refractiveExcludeOit ? r_refractiveExcludeOit->integer : 0,
		r_transparencyDebug ? r_transparencyDebug->integer : 0 );
}

void vk_transparency_route_init( void )
{
	if ( s_inited ) {
		return;
	}
	r_transparencyDebug = ri.Cvar_Get( "r_transparencyDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_transparencyDebug, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_transparencyDebug,
		"Raster Ultra 1.4 transparency routing debug:\n"
		" 0 - off\n"
		" 1 - log classify counts (developer)\n"
		" 2 - force refractive OIT exclusion visualization via oitDebug" );
	ri.Cvar_SetGroup( r_transparencyDebug, CVG_RENDERER );

	r_refractiveExcludeOit = ri.Cvar_Get( "r_refractiveExcludeOit", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_refractiveExcludeOit, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_refractiveExcludeOit,
		"Exclude screenMap/portal/refract/distort shaders from WBOIT/MBOIT;\n"
		"draw them sorted after OIT resolve. Plain glass/water alpha stays in WBOIT." );
	ri.Cvar_SetGroup( r_refractiveExcludeOit, CVG_RENDERER );

	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "transparency_route_status", VK_TransparencyRoute_Status_f );
		ri.Cmd_AddCommand( "oit_status", VK_Oit_Status_f );
		ri.Cmd_AddCommand( "oit_capture", VK_Oit_Capture_f );
		ri.Cmd_AddCommand( "oit_perf", VK_Oit_Perf_f );
	}
	s_inited = qtrue;
	ri.Printf( PRINT_ALL, "[VK][Xparent] transparency routing initialized (refractiveExcludeOit=%d)\n",
		r_refractiveExcludeOit->integer );
	vk_oit_contract_register();
	vk_oit_alpha_register();
	vk_oit_certify_init();
	vk_hdr_resolve_contract_register();
}

void vk_transparency_route_shutdown( void )
{
	vk_oit_certify_shutdown();
	if ( ri.Cmd_RemoveCommand ) {
		ri.Cmd_RemoveCommand( "transparency_route_status" );
		ri.Cmd_RemoveCommand( "oit_status" );
		ri.Cmd_RemoveCommand( "oit_capture" );
		ri.Cmd_RemoveCommand( "oit_perf" );
		ri.Cmd_RemoveCommand( "oit_contract_status" );
		ri.Cmd_RemoveCommand( "oit_contract_validate" );
		ri.Cmd_RemoveCommand( "hdr_resolve_status" );
		ri.Cmd_RemoveCommand( "oit_resolve_status" );
		ri.Cmd_RemoveCommand( "hdr_resolve_validate" );
	}
	s_inited = qfalse;
}

/* Used by tr_backend filter — keep cvar readable without header export of cvar. */
qboolean vk_transparency_refractive_exclude_oit( void )
{
	return ( !r_refractiveExcludeOit || r_refractiveExcludeOit->integer ) ? qtrue : qfalse;
}
