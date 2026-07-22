/*
===========================================================================
Black-frame / SceneHDR composition diagnostics.
Milestone 1: frame-production validation before feature expansion.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_black_frame.h"
#include "vk_deferred_gbuffer.h"
#include "vk_render_path.h"
#include "vk_frame_contract.h"
#include "vk_hdr_pipeline.h"
#include "vk_shadow_contract.h"
#include "vk_depth_contract.h"
#include "vk_renderer_perf.h"
#include "vk_reflection_hierarchy.h"
#include "vk_indirect_light.h"
#include "vk_shading_compare.h"

#define VK_BF_MAX_WRITERS 32

static cvar_t *r_frameOutputDebug;
static cvar_t *r_forceMinimalScene;
static cvar_t *r_captureBlackFrame;
static cvar_t *r_forcePassColor;
static cvar_t *r_gbufferBandwidth;
static cvar_t *r_gbufferDebug;

static char s_writers[VK_BF_MAX_WRITERS][48];
static int s_writerCount;
static uint32_t s_drawCounts[VK_BF_DRAW_COUNT];
static uint32_t s_indicesSubmitted;
static uint32_t s_blackFrameHits;
static uint32_t s_lastValidateFailCount;
static qboolean s_cmdsRegistered;
static qboolean s_forceCaptureOnce;

static uint32_t VK_BF_FormatBytesPerPixel( VkFormat fmt )
{
	switch ( fmt ) {
	case VK_FORMAT_R8_UNORM:
	case VK_FORMAT_R8_UINT:
		return 1u;
	case VK_FORMAT_R16_SFLOAT:
	case VK_FORMAT_R16_UNORM:
	case VK_FORMAT_D16_UNORM:
		return 2u;
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_R32_SFLOAT:
	case VK_FORMAT_R32_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT:
		return 4u;
	case VK_FORMAT_R16G16B16A16_SFLOAT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return 8u;
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		return 16u;
	default:
		return 8u; /* conservative HDR guess */
	}
}

static void VK_BF_PrintChain( void )
{
	int i;

	ri.Printf( PRINT_ALL, "SceneHDR writer chain (%d):\n", s_writerCount );
	if ( s_writerCount <= 0 ) {
		ri.Printf( PRINT_ALL, "  (none recorded this frame)\n" );
		return;
	}
	for ( i = 0; i < s_writerCount; i++ ) {
		ri.Printf( PRINT_ALL, "  %s\n", s_writers[i] );
	}
}

static void VK_BF_PrintResourceMeta( void )
{
	ri.Printf( PRINT_ALL,
		"SceneHDR meta: image=%p format=%u extent=%ux%u oitState=%u "
		"oitGenAtt=%u oitGenDesc=%u lightingActive=%s split=%s r_oit=%d\n",
		(void *)vk.color_image,
		(unsigned)vk.color_format,
		vk.mainColorWidth ? vk.mainColorWidth : vk.renderWidth,
		vk.mainColorHeight ? vk.mainColorHeight : vk.renderHeight,
		vk.oitFrameState,
		vk.oitAttachmentGeneration,
		vk.oitDescriptorGeneration,
		vk_deferred_lighting_active() ? "yes" : "no",
		vk_deferred_opaque_transparent_split() ? "yes" : "no",
		r_oit ? r_oit->integer : 0 );
}

static void VK_BF_PrintGBufferBandwidth( void )
{
	uint32_t w, h, bpp, writeBpp, readBpp, compactWriteBpp, compactReadBpp;
	uint64_t pixels, writeBytes, readBytes, compactWriteBytes, compactReadBytes;
	uint32_t deferredN = 0u, fpOpaqueN = 0u, opaqueTotal;
	float fpFallbackPct;
	int compactOn;

	w = vk.deferredGbufferExtentW ? vk.deferredGbufferExtentW :
		( vk.mainColorWidth ? vk.mainColorWidth : vk.renderWidth );
	h = vk.deferredGbufferExtentH ? vk.deferredGbufferExtentH :
		( vk.mainColorHeight ? vk.mainColorHeight : vk.renderHeight );
	if ( w == 0u || h == 0u ) {
		ri.Printf( PRINT_ALL, "G-buffer bandwidth: (no extent)\n" );
		return;
	}
	pixels = (uint64_t)w * (uint64_t)h;
	/* Current scaffold: albedo + normal + material as R16G16B16A16_SFLOAT each. */
	bpp = VK_BF_FormatBytesPerPixel( VK_FORMAT_R16G16B16A16_SFLOAT );
	writeBpp = bpp * 3u;
	/* Deferred lighting typically samples albedo+normal+material+depth. */
	readBpp = writeBpp + VK_BF_FormatBytesPerPixel( VK_FORMAT_D32_SFLOAT );
	writeBytes = pixels * (uint64_t)writeBpp;
	readBytes = pixels * (uint64_t)readBpp;
	/* G-buffer 2.0 compact target: 3× R8G8B8A8 (12 B/px write; ~16 B/px read with depth). */
	compactWriteBpp = VK_BF_FormatBytesPerPixel( VK_FORMAT_R8G8B8A8_UNORM ) * 3u;
	compactReadBpp = compactWriteBpp + VK_BF_FormatBytesPerPixel( VK_FORMAT_D32_SFLOAT );
	compactWriteBytes = pixels * (uint64_t)compactWriteBpp;
	compactReadBytes = pixels * (uint64_t)compactReadBpp;
	R_RenderPath_GetOpaqueCounts( &deferredN, &fpOpaqueN );
	opaqueTotal = deferredN + fpOpaqueN;
	fpFallbackPct = ( opaqueTotal > 0u ) ?
		( 100.0f * (float)fpOpaqueN / (float)opaqueTotal ) : 0.0f;
	compactOn = ( r_gbufferCompact && r_gbufferCompact->integer ) ? 1 : 0;
	ri.Printf( PRINT_ALL,
		"G-buffer bandwidth (scaffold R16F4×3 = %u B/px write, ~%u B/px deferred read):\n"
		"  extent=%ux%u pixels=%llu r_gbufferCompact=%d (dual-write + lighting oct decode)\n"
		"  scaffold write/frame≈%llu (%.2f MiB) read≈%llu (%.2f MiB)\n"
		"  compact target=%u B/px write ~%u B/px read (goal ≤16 / ≤20 B/px)\n"
		"  compact target write≈%llu (%.2f MiB) read≈%llu (%.2f MiB)\n"
		"  Forward+ opaque fallback=%.1f%% (deferred=%u fpOpaque=%u)\n"
		"  allocated=%s gen=%u directExport=%s\n",
		writeBpp, readBpp,
		w, h, (unsigned long long)pixels, compactOn,
		(unsigned long long)writeBytes, (double)writeBytes / ( 1024.0 * 1024.0 ),
		(unsigned long long)readBytes, (double)readBytes / ( 1024.0 * 1024.0 ),
		compactWriteBpp, compactReadBpp,
		(unsigned long long)compactWriteBytes, (double)compactWriteBytes / ( 1024.0 * 1024.0 ),
		(unsigned long long)compactReadBytes, (double)compactReadBytes / ( 1024.0 * 1024.0 ),
		fpFallbackPct, deferredN, fpOpaqueN,
		vk.deferredGbufferAllocated ? "yes" : "no",
		vk.deferredGbufferGeneration,
		vk.deferredGbufferDirectExport ? "yes" : "no" );
}

static void VK_BF_PrintResourceStatus( void )
{
	ri.Printf( PRINT_ALL, "======== Renderer Resource Status ========\n" );
	VK_BF_PrintResourceMeta();
	ri.Printf( PRINT_ALL,
		"depth: image=%p layout=%u\n"
		"gbuffer: albedo=%p normal=%p material=%p gen=%u ext=%ux%u\n"
		"oit: accumView=%p revealView=%p fogScene=%p genAtt=%u genDesc=%u\n"
		"debug: hybridCompare=%d renderPathDebug=%d oitDebug=%d deferredDebug=%d\n",
		(void *)vk.depth_image, (unsigned)vk.depth_image_layout,
		(void *)vk.deferred_gbuffer_albedo,
		(void *)vk.deferred_gbuffer_normal,
		(void *)vk.deferred_gbuffer_material,
		vk.deferredGbufferGeneration,
		vk.deferredGbufferExtentW, vk.deferredGbufferExtentH,
		(void *)vk.oit_accum_image_view,
		(void *)vk.oit_reveal_image_view,
		(void *)vk.fog_scene_image_view,
		vk.oitAttachmentGeneration, vk.oitDescriptorGeneration,
		r_hybridCompare ? r_hybridCompare->integer : 0,
		r_renderPathDebug ? r_renderPathDebug->integer : 0,
		ri.Cvar_VariableIntegerValue( "r_oitDebug" ),
		ri.Cvar_VariableIntegerValue( "r_deferredGBufferDebug" ) );
	VK_BF_PrintGBufferBandwidth();
	VK_BF_PrintChain();
}

static uint32_t VK_BF_ValidateFrame( qboolean printPass )
{
	uint32_t fails = 0u;
	uint32_t opaqueDraws;
	int i;
	qboolean sawResolve = qfalse;
	qboolean postOitCapture = qfalse;

	opaqueDraws = s_drawCounts[VK_BF_DRAW_OPAQUE] + s_drawCounts[VK_BF_DRAW_FORWARD_OPAQUE];

	ri.Printf( PRINT_ALL, "======== Renderer Validate Frame ========\n" );

	/* 1) SceneHDR exists when FBO active */
	if ( vk.fboActive && vk.color_image == VK_NULL_HANDLE ) {
		ri.Printf( PRINT_WARNING, "FAIL: FBO active but SceneHDR color_image is null\n" );
		fails++;
	} else if ( printPass ) {
		ri.Printf( PRINT_ALL, "PASS: SceneHDR image present (or FBO off)\n" );
	}

	/* 2) OIT descriptor generation match */
	if ( r_oit && r_oit->integer && vk.fboActive &&
		vk.oitAttachmentGeneration != vk.oitDescriptorGeneration ) {
		ri.Printf( PRINT_WARNING, "FAIL: OIT gen mismatch att=%u desc=%u\n",
			vk.oitAttachmentGeneration, vk.oitDescriptorGeneration );
		fails++;
	} else if ( printPass ) {
		ri.Printf( PRINT_ALL, "PASS: OIT attachment/descriptor generations\n" );
	}

	/* 3) Writer chain: no G-buffer capture after WBOIT resolve */
	for ( i = 0; i < s_writerCount; i++ ) {
		if ( !Q_stricmp( s_writers[i], "WBOITResolve" ) || !Q_stricmp( s_writers[i], "OITResolve" ) ) {
			sawResolve = qtrue;
		} else if ( sawResolve &&
			( !Q_stricmpn( s_writers[i], "GBufferCapture", 14 ) ||
			  !Q_stricmpn( s_writers[i], "VisBufCapture", 13 ) ) ) {
			postOitCapture = qtrue;
		}
	}
	if ( postOitCapture ) {
		ri.Printf( PRINT_WARNING, "FAIL: G-buffer/visbuf capture after WBOIT resolve (black-frame class)\n" );
		fails++;
	} else if ( printPass ) {
		ri.Printf( PRINT_ALL, "PASS: no post-OIT G-buffer capture in writer chain\n" );
	}

	/* 4) Opaque ownership when deferred lighting inactive */
	if ( !vk_deferred_lighting_active() ) {
		/* Cannot read path counters from here without export; warn on heuristic only. */
		if ( printPass ) {
			ri.Printf( PRINT_ALL,
				"NOTE: lightingActive=0 — run render_path_status verbose "
				"(deferredOpaque must be 0)\n" );
		}
	}

	/* 5) World frame should have a SceneHDR writer if opaques drew */
	if ( backEnd.doneWorldScene && opaqueDraws > 0u && s_writerCount == 0 ) {
		ri.Printf( PRINT_WARNING, "FAIL: opaque draws=%u but SceneHDR writer chain empty\n", opaqueDraws );
		fails++;
	} else if ( printPass ) {
		ri.Printf( PRINT_ALL, "PASS: SceneHDR writers present when opaques drew (or no world)\n" );
	}

	/* 6) Debug modes that can discard */
	if ( r_hybridCompare && r_hybridCompare->integer != 0 ) {
		ri.Printf( PRINT_WARNING, "WARN: r_hybridCompare=%d active (can mask half-scene)\n",
			r_hybridCompare->integer );
	}
	if ( r_renderPathDebug && r_renderPathDebug->integer != 0 ) {
		ri.Printf( PRINT_ALL, "NOTE: r_renderPathDebug=%d (path tint)\n", r_renderPathDebug->integer );
	}

	/* 7) Exposure sanity */
	if ( vk.adaptedExposure <= 0.0f || vk.adaptedExposure != vk.adaptedExposure ) {
		ri.Printf( PRINT_WARNING, "FAIL: adaptedExposure=%g (zero/NaN collapses HDR)\n",
			(double)vk.adaptedExposure );
		fails++;
	} else if ( printPass ) {
		ri.Printf( PRINT_ALL, "PASS: adaptedExposure=%g\n", (double)vk.adaptedExposure );
	}

	/* 8) Push-constant note */
	ri.Printf( PRINT_ALL,
		"NOTE: OIT push=256 B / resolve=16 B (see startup [VK][OIT] push layout)\n" );

	fails += vk_frame_contract_validate( printPass );

	VK_BF_PrintChain();
	ri.Printf( PRINT_ALL, "validate_frame: %u failure(s)\n", fails );
	s_lastValidateFailCount = fails;
	return fails;
}

static void VK_BF_FrameOutputStatus_f( void )
{
	int mode = r_frameOutputDebug ? r_frameOutputDebug->integer : 0;

	ri.Printf( PRINT_ALL, "======== Frame Output Debug ========\n" );
	ri.Printf( PRINT_ALL, "r_frameOutputDebug=%d (0=final … 6=post-WBOIT … 15=invalid mask)\n", mode );
	VK_BF_PrintResourceMeta();
	VK_BF_PrintChain();
	ri.Printf( PRINT_ALL,
		"draws: opaque=%u fpOpaque=%u gbuffer=%u transparent=%u oit=%u depthOnly=%u indices=%u\n",
		s_drawCounts[VK_BF_DRAW_OPAQUE],
		s_drawCounts[VK_BF_DRAW_FORWARD_OPAQUE],
		s_drawCounts[VK_BF_DRAW_DEFERRED_GBUFFER],
		s_drawCounts[VK_BF_DRAW_TRANSPARENT],
		s_drawCounts[VK_BF_DRAW_OIT],
		s_drawCounts[VK_BF_DRAW_DEPTH_ONLY],
		s_indicesSubmitted );
}

static void VK_BF_DrawStatus_f( void )
{
	ri.Printf( PRINT_ALL, "======== Renderer Draw Status ========\n" );
	ri.Printf( PRINT_ALL,
		"opaque=%u forwardOpaque=%u gbuffer=%u transparent=%u oit=%u depthOnly=%u indices=%u\n"
		"backend pc: surfaces=%i indexes=%i\n",
		s_drawCounts[VK_BF_DRAW_OPAQUE],
		s_drawCounts[VK_BF_DRAW_FORWARD_OPAQUE],
		s_drawCounts[VK_BF_DRAW_DEFERRED_GBUFFER],
		s_drawCounts[VK_BF_DRAW_TRANSPARENT],
		s_drawCounts[VK_BF_DRAW_OIT],
		s_drawCounts[VK_BF_DRAW_DEPTH_ONLY],
		s_indicesSubmitted,
		backEnd.pc.c_surfaces,
		backEnd.pc.c_indexes );
	ri.Printf( PRINT_ALL, "path counts:\n" );
	R_RenderPath_Status_f();
}

static void VK_BF_ValidateFrame_f( void )
{
	(void)VK_BF_ValidateFrame( qtrue );
}

static void VK_BF_ResourceStatus_f( void )
{
	VK_BF_PrintResourceStatus();
}

static void VK_BF_CaptureBlackFrame_f( void )
{
	ri.Cvar_Set( "r_captureBlackFrame", "1" );
	s_forceCaptureOnce = qtrue;
	ri.Printf( PRINT_ALL,
		"[VK][blackframe] renderer_capture_black_frame: dumping current chain + resources\n" );
	VK_BF_PrintResourceStatus();
	(void)VK_BF_ValidateFrame( qtrue );
	ri.Printf( PRINT_ALL,
		"BLACK FRAME CAPTURE DUMP complete (hits=%u). "
		"Set r_captureBlackFrame 0 when done.\n", s_blackFrameHits );
}

static void VK_BF_ForcePassColor_f( void )
{
	int passId;
	float r, g, b;

	if ( ri.Cmd_Argc() < 5 ) {
		ri.Printf( PRINT_ALL,
			"usage: force_pass_color <pass> <r> <g> <b>\n"
			"pass: 1=gbuffer 2=forward+ 3=deferred 4=sky 5=wboit 6=weapon 7=composite\n"
			"current r_forcePassColor=%d\n",
			r_forcePassColor ? r_forcePassColor->integer : 0 );
		return;
	}
	passId = atoi( ri.Cmd_Argv( 1 ) );
	r = (float)atof( ri.Cmd_Argv( 2 ) );
	g = (float)atof( ri.Cmd_Argv( 3 ) );
	b = (float)atof( ri.Cmd_Argv( 4 ) );
	(void)r;
	(void)g;
	(void)b;
	ri.Cvar_Set( "r_forcePassColor", va( "%d", passId ) );
	ri.Printf( PRINT_ALL, "[VK][blackframe] forcePassColor pass=%d\n", passId );
}

static void VK_BF_GBufferBandwidth_f( void )
{
	VK_BF_PrintGBufferBandwidth();
}

void vk_black_frame_register( void )
{
	r_frameOutputDebug = ri.Cvar_Get( "r_frameOutputDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_frameOutputDebug, "0", "15", CV_INTEGER );
	ri.Cvar_SetDescription( r_frameOutputDebug,
		"Scene composition inspection (cheat). Print: frame_output_status." );
	ri.Cvar_SetGroup( r_frameOutputDebug, CVG_RENDERER );

	r_forceMinimalScene = ri.Cvar_Get( "r_forceMinimalScene", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_forceMinimalScene, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_forceMinimalScene,
		"Safe-mode frame: skip OIT and pre-OIT G-buffer sidecars." );
	ri.Cvar_SetGroup( r_forceMinimalScene, CVG_RENDERER );

	r_captureBlackFrame = ri.Cvar_Get( "r_captureBlackFrame", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_captureBlackFrame, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_captureBlackFrame,
		"Log writer chain on anomaly. Command: renderer_capture_black_frame." );
	ri.Cvar_SetGroup( r_captureBlackFrame, CVG_RENDERER );

	r_forcePassColor = ri.Cvar_Get( "r_forcePassColor", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_forcePassColor, "0", "7", CV_INTEGER );
	ri.Cvar_SetGroup( r_forcePassColor, CVG_RENDERER );

	r_gbufferBandwidth = ri.Cvar_Get( "r_gbufferBandwidth", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_gbufferBandwidth, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_gbufferBandwidth,
		"When 1, print G-buffer bytes/pixel and MiB/frame each validate/resource status." );
	ri.Cvar_SetGroup( r_gbufferBandwidth, CVG_RENDERER );

	/* Alias / companion to r_deferredGBufferDebug for Milestone 2 naming. */
	r_gbufferDebug = ri.Cvar_Get( "r_gbufferDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_gbufferDebug, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_gbufferDebug,
		"G-buffer debug (mirrors r_deferredGBufferDebug 0–6). Prefer deferred cvar for compositing." );
	ri.Cvar_SetGroup( r_gbufferDebug, CVG_RENDERER );


	if ( !s_cmdsRegistered ) {
		ri.Cmd_AddCommand( "frame_output_status", VK_BF_FrameOutputStatus_f );
		ri.Cmd_AddCommand( "renderer_draw_status", VK_BF_DrawStatus_f );
		ri.Cmd_AddCommand( "renderer_validate_frame", VK_BF_ValidateFrame_f );
		ri.Cmd_AddCommand( "renderer_resource_status", VK_BF_ResourceStatus_f );
		ri.Cmd_AddCommand( "renderer_capture_black_frame", VK_BF_CaptureBlackFrame_f );
		ri.Cmd_AddCommand( "force_pass_color", VK_BF_ForcePassColor_f );
		ri.Cmd_AddCommand( "gbuffer_bandwidth", VK_BF_GBufferBandwidth_f );
		s_cmdsRegistered = qtrue;
		ri.Printf( PRINT_ALL,
			"[VK][blackframe] M1 ready: renderer_validate_frame, renderer_resource_status, "
			"renderer_capture_black_frame, renderer_draw_status, gbuffer_bandwidth\n" );
	}
}

void vk_black_frame_begin_frame( void )
{
	Com_Memset( s_writers, 0, sizeof( s_writers ) );
	s_writerCount = 0;
	Com_Memset( s_drawCounts, 0, sizeof( s_drawCounts ) );
	s_indicesSubmitted = 0;
	vk_frame_contract_begin_frame();
	vk_depth_contract_begin_frame();
	vk_hdr_pipeline_begin_frame();
	vk_shadow_contract_begin_frame();
	vk_reflection_hierarchy_begin_frame();
	vk_indirect_light_begin_frame();
	vk_renderer_perf_begin_frame();
	vk_shading_compare_begin_frame();
}

void vk_black_frame_note_writer( const char *passName )
{
	if ( !passName || !passName[0] ) {
		return;
	}
	if ( s_writerCount > 0 && !Q_stricmp( s_writers[s_writerCount - 1], passName ) ) {
		return;
	}
	if ( s_writerCount >= VK_BF_MAX_WRITERS ) {
		return;
	}
	Q_strncpyz( s_writers[s_writerCount], passName, sizeof( s_writers[0] ) );
	s_writerCount++;
	if ( !Q_stricmpn( passName, "ForwardOpaque", 13 ) ||
		!Q_stricmpn( passName, "DeferredComposite", 17 ) ||
		!Q_stricmpn( passName, "WBOITResolve", 12 ) ||
		!Q_stricmpn( passName, "OITResolve", 10 ) ||
		!Q_stricmpn( passName, "OITOpaqueCopy", 13 ) ||
		!Q_stricmpn( passName, "PreBloom", 8 ) ) {
		vk_frame_contract_note_writer( "SceneHDR", passName );
		vk_hdr_pipeline_note_stage( VK_HDR_STAGE_SCENE, passName );
	}
	if ( !Q_stricmpn( passName, "ForwardOpaque", 13 ) ||
		!Q_stricmpn( passName, "GBufferCapture", 14 ) ) {
		vk_frame_contract_note_writer( "SceneDepth", passName );
		vk_depth_contract_note_writer( passName );
	}
	if ( !Q_stricmpn( passName, "GBufferCapture", 14 ) ) {
		vk_frame_contract_note_writer( "GBuffer0", passName );
		vk_frame_contract_note_writer( "GBuffer1", passName );
		vk_frame_contract_note_writer( "GBuffer2", passName );
		vk_hdr_pipeline_note_stage( VK_HDR_STAGE_SCENE, passName );
	}
	if ( !Q_stricmpn( passName, "Bloom", 5 ) ) {
		vk_hdr_pipeline_note_stage( VK_HDR_STAGE_BLOOM, passName );
		vk_frame_contract_note_writer( "BloomSource", passName );
	}
	if ( !Q_stricmpn( passName, "Tonemap", 7 ) || !Q_stricmpn( passName, "ToneMap", 7 ) ) {
		vk_hdr_pipeline_note_stage( VK_HDR_STAGE_TONEMAP, passName );
		vk_frame_contract_note_writer( "ToneMapSource", passName );
	}
	if ( !Q_stricmpn( passName, "DepthPrepass", 12 ) ||
		!Q_stricmpn( passName, "OpaqueDepth", 11 ) ) {
		vk_frame_contract_note_writer( "SceneDepth", passName );
		vk_depth_contract_note_writer( passName );
	}
}

void vk_black_frame_note_draw( vkBlackFrameDrawKind_t kind, uint32_t count )
{
	if ( kind < 0 || kind >= VK_BF_DRAW_COUNT ) {
		return;
	}
	s_drawCounts[kind] += count;
}

uint32_t vk_black_frame_draw_count( vkBlackFrameDrawKind_t kind )
{
	if ( kind < 0 || kind >= VK_BF_DRAW_COUNT ) {
		return 0u;
	}
	return s_drawCounts[kind];
}

void vk_black_frame_note_indices( uint32_t indices )
{
	s_indicesSubmitted += indices;
}

qboolean vk_black_frame_force_minimal_scene( void )
{
	return ( r_forceMinimalScene && r_forceMinimalScene->integer ) ? qtrue : qfalse;
}

int vk_black_frame_output_debug( void )
{
	return r_frameOutputDebug ? r_frameOutputDebug->integer : 0;
}

int vk_black_frame_force_pass_color( void )
{
	return r_forcePassColor ? r_forcePassColor->integer : 0;
}

uint32_t vk_black_frame_last_validate_fails( void )
{
	return s_lastValidateFailCount;
}

void vk_black_frame_check_before_ui( void )
{
	uint32_t opaqueDraws;
	qboolean suspicious;
	int dbg;

	opaqueDraws = s_drawCounts[VK_BF_DRAW_OPAQUE] + s_drawCounts[VK_BF_DRAW_FORWARD_OPAQUE];
	dbg = vk_black_frame_output_debug();

	if ( r_gbufferBandwidth && r_gbufferBandwidth->integer && backEnd.doneWorldScene ) {
		static int s_bwLog;
		if ( s_bwLog < 1 ) {
			VK_BF_PrintGBufferBandwidth();
			s_bwLog++;
		}
	}

	if ( dbg > 0 && ( dbg == 4 || dbg == 6 || backEnd.doneWorldScene ) ) {
		static int s_dbgLog;
		if ( s_dbgLog < 3 || ( r_captureBlackFrame && r_captureBlackFrame->integer ) ) {
			VK_BF_FrameOutputStatus_f();
			s_dbgLog++;
		}
	}

	suspicious = qfalse;
	if ( backEnd.doneWorldScene && opaqueDraws > 0u && s_writerCount == 0 ) {
		suspicious = qtrue;
	}
	if ( backEnd.doneWorldScene && opaqueDraws > 0u && s_writerCount >= 2 ) {
		int i;
		qboolean sawResolve = qfalse;
		for ( i = 0; i < s_writerCount; i++ ) {
			if ( !Q_stricmp( s_writers[i], "WBOITResolve" ) ||
				!Q_stricmp( s_writers[i], "OITResolve" ) ) {
				sawResolve = qtrue;
			} else if ( sawResolve &&
				( !Q_stricmpn( s_writers[i], "GBufferCapture", 14 ) ||
				  !Q_stricmpn( s_writers[i], "VisBufCapture", 13 ) ) ) {
				suspicious = qtrue;
				break;
			}
		}
	}

	if ( s_forceCaptureOnce ) {
		s_forceCaptureOnce = qfalse;
		VK_BF_PrintResourceStatus();
		(void)VK_BF_ValidateFrame( qtrue );
	}

	if ( !suspicious ) {
		if ( r_captureBlackFrame && r_captureBlackFrame->integer && opaqueDraws > 0u ) {
			static int s_capLog;
			if ( s_capLog < 2 ) {
				ri.Printf( PRINT_ALL, "[VK][blackframe] capture dump (no anomaly):\n" );
				VK_BF_PrintResourceMeta();
				VK_BF_PrintChain();
				s_capLog++;
			}
		}
		return;
	}

	s_blackFrameHits++;
	ri.Printf( PRINT_ERROR, S_COLOR_RED
		"BLACK FRAME DETECTED (hit=%u)\n" S_COLOR_WHITE, s_blackFrameHits );
	ri.Printf( PRINT_ALL,
		"  first likely stage: SceneHDR composition / post-OIT capture order\n"
		"  opaqueDraw=%u forwardOpaque=%u gbuffer=%u oitDraw=%u\n"
		"  oitFrameState=%u lightingActive=%s\n",
		opaqueDraws,
		s_drawCounts[VK_BF_DRAW_FORWARD_OPAQUE],
		s_drawCounts[VK_BF_DRAW_DEFERRED_GBUFFER],
		s_drawCounts[VK_BF_DRAW_OIT],
		vk.oitFrameState,
		vk_deferred_lighting_active() ? "yes" : "no" );
	VK_BF_PrintResourceStatus();
	(void)VK_BF_ValidateFrame( qfalse );
}
