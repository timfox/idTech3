/*
===========================================================================
Renderer frame contract — SceneHDR / depth / G-buffer ownership tracking.
Foundation Consolidation: explicit writer/reader generations per resource.
See docs/RENDERER_FRAME_CONTRACT.md
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_frame_contract.h"
#include "vk_gpu_scene.h"
#include "vk_deferred_gbuffer.h"
#include "vk_forward_plus.h"

#include <math.h>

static vkFrameContractSnapshot_t s_snap;
static qboolean s_cmdsRegistered;
static uint32_t s_frameCounter;

static const char *s_resNames[VK_FRAME_RES_COUNT] = {
	"SceneHDR",
	"SceneHDRPrevious",
	"SceneDepth",
	"SceneDepthPrevious",
	"GBufferAlbedo",
	"GBufferNormal",
	"GBufferMaterial",
	"GBufferLighting",
	"Velocity",
	"TemporalClass",
	"OITAccum",
	"OITRevealage",
	"WeaponHDR",
	"WeaponHistory",
	"BloomSource",
	"ToneMapSource",
	"FinalLDR"
};

static vkFrameContractResource_t VK_FC_ParseResource( const char *name )
{
	int i;

	if ( !name || !name[0] ) {
		return VK_FRAME_RES_COUNT;
	}
	if ( !Q_stricmp( name, "SceneHDR" ) || !Q_stricmp( name, "color" ) ) {
		return VK_FRAME_RES_SCENE_HDR;
	}
	if ( !Q_stricmp( name, "SceneHDRPrevious" ) || !Q_stricmp( name, "SceneHDRPrev" ) ) {
		return VK_FRAME_RES_SCENE_HDR_PREVIOUS;
	}
	if ( !Q_stricmp( name, "SceneDepth" ) || !Q_stricmp( name, "Depth" ) ) {
		return VK_FRAME_RES_SCENE_DEPTH;
	}
	if ( !Q_stricmp( name, "SceneDepthPrevious" ) || !Q_stricmp( name, "DepthPrev" ) ) {
		return VK_FRAME_RES_SCENE_DEPTH_PREVIOUS;
	}
	if ( !Q_stricmp( name, "GBuffer" ) || !Q_stricmp( name, "GBufferAlbedo" ) ||
		!Q_stricmp( name, "GBuffer0" ) ) {
		return VK_FRAME_RES_GBUFFER_ALBEDO;
	}
	if ( !Q_stricmp( name, "GBufferNormal" ) || !Q_stricmp( name, "GBuffer1" ) ) {
		return VK_FRAME_RES_GBUFFER_NORMAL;
	}
	if ( !Q_stricmp( name, "GBufferMaterial" ) || !Q_stricmp( name, "GBuffer2" ) ) {
		return VK_FRAME_RES_GBUFFER_MATERIAL;
	}
	if ( !Q_stricmp( name, "GBufferLighting" ) || !Q_stricmp( name, "DeferredComposite" ) ) {
		return VK_FRAME_RES_GBUFFER_LIGHTING;
	}
	if ( !Q_stricmp( name, "Velocity" ) ) {
		return VK_FRAME_RES_VELOCITY;
	}
	if ( !Q_stricmp( name, "TemporalClass" ) ) {
		return VK_FRAME_RES_TEMPORAL_CLASS;
	}
	if ( !Q_stricmp( name, "OIT" ) || !Q_stricmp( name, "OITAccum" ) ||
		!Q_stricmp( name, "WBOITResolve" ) ) {
		return VK_FRAME_RES_OIT_ACCUM;
	}
	if ( !Q_stricmp( name, "OITReveal" ) || !Q_stricmp( name, "OITRevealage" ) ) {
		return VK_FRAME_RES_OIT_REVEAL;
	}
	if ( !Q_stricmp( name, "WeaponHDR" ) ) {
		return VK_FRAME_RES_WEAPON_HDR;
	}
	if ( !Q_stricmp( name, "WeaponHistory" ) ) {
		return VK_FRAME_RES_WEAPON_HISTORY;
	}
	if ( !Q_stricmp( name, "Bloom" ) || !Q_stricmp( name, "BloomSource" ) ) {
		return VK_FRAME_RES_BLOOM_SOURCE;
	}
	if ( !Q_stricmp( name, "ToneMap" ) || !Q_stricmp( name, "ToneMapSource" ) ) {
		return VK_FRAME_RES_TONEMAP_SOURCE;
	}
	if ( !Q_stricmp( name, "FinalLDR" ) || !Q_stricmp( name, "Present" ) ) {
		return VK_FRAME_RES_FINAL_LDR;
	}
	for ( i = 0; i < (int)VK_FRAME_RES_COUNT; i++ ) {
		if ( !Q_stricmp( name, s_resNames[i] ) ) {
			return (vkFrameContractResource_t)i;
		}
	}
	return VK_FRAME_RES_COUNT;
}

static void VK_FC_RefreshLiveMeta( void )
{
	uint32_t rw = glConfig.vidWidth > 0 ? (uint32_t)glConfig.vidWidth : 0u;
	uint32_t rh = glConfig.vidHeight > 0 ? (uint32_t)glConfig.vidHeight : 0u;

	s_snap.renderMode = r_renderMode ? r_renderMode->integer : 0;
	s_snap.renderExtentW = rw;
	s_snap.renderExtentH = rh;
	s_snap.outputExtentW = rw;
	s_snap.outputExtentH = rh;
	s_snap.gpuSceneGeneration = vk_gpu_scene_generation();
	s_snap.deferredGbufferGeneration = vk.deferred_gbuffer.descriptor_generation;
	s_snap.clusterListGeneration = vk_cluster_list_generation();
	s_snap.adaptedExposure = ( r_exposure ) ? r_exposure->value : 1.0f;

	vk_frame_contract_set_meta( VK_FRAME_RES_SCENE_HDR, (uint64_t)(uintptr_t)vk.color_image,
		(uint32_t)vk.color_format, rw, rh, vk.oitAttachmentGeneration );
	vk_frame_contract_set_meta( VK_FRAME_RES_SCENE_DEPTH, (uint64_t)(uintptr_t)vk.depth_image,
		(uint32_t)vk.depth_format, rw, rh, 0u );
	vk_frame_contract_set_meta( VK_FRAME_RES_GBUFFER_ALBEDO,
		(uint64_t)(uintptr_t)vk.deferred_gbuffer_albedo,
		0u, rw, rh, vk.deferred_gbuffer.descriptor_generation );
	vk_frame_contract_set_meta( VK_FRAME_RES_OIT_ACCUM,
		(uint64_t)(uintptr_t)vk.oit_accum_image,
		0u, rw, rh, vk.oitAttachmentGeneration );
}

void vk_frame_contract_set_meta( vkFrameContractResource_t res, uint64_t handle,
	uint32_t format, uint32_t extentW, uint32_t extentH, uint32_t generation )
{
	vkFrameResourceHistory_t *h;

	if ( res < 0 || res >= VK_FRAME_RES_COUNT ) {
		return;
	}
	h = &s_snap.resources[res];
	Q_strncpyz( h->name, s_resNames[res], sizeof( h->name ) );
	h->handle = handle;
	h->format = format;
	h->extentW = extentW;
	h->extentH = extentH;
	h->generation = generation;
}

void vk_frame_contract_invalidate( vkFrameContractResource_t res, const char *reason )
{
	vkFrameResourceHistory_t *h;

	if ( res < 0 || res >= VK_FRAME_RES_COUNT ) {
		return;
	}
	h = &s_snap.resources[res];
	Q_strncpyz( h->invalidationReason,
		reason && reason[0] ? reason : "invalidated",
		sizeof( h->invalidationReason ) );
	h->clearedAfterWrite = qtrue;
}

void vk_frame_contract_snapshot( vkFrameContractSnapshot_t *out )
{
	if ( !out ) {
		return;
	}
	VK_FC_RefreshLiveMeta();
	*out = s_snap;
}

static void VK_FC_PrintResource( const vkFrameResourceHistory_t *h )
{
	uint32_t r;

	ri.Printf( PRINT_ALL,
		"  %-18s handle=0x%llx fmt=%u extent=%ux%u gen=%u write=%s last=%s readers=%u%s%s\n",
		h->name[0] ? h->name : "?",
		(unsigned long long)h->handle,
		h->format, h->extentW, h->extentH, h->generation,
		h->writtenThisFrame ? "yes" : "no",
		h->lastWriter[0] ? h->lastWriter : "(none)",
		h->readerCount,
		h->clearedAfterWrite ? " CLEARED_AFTER_WRITE" : "",
		h->invalidationReason[0] ? " invalid=" : "" );
	if ( h->invalidationReason[0] ) {
		ri.Printf( PRINT_ALL, "    reason=%s\n", h->invalidationReason );
	}
	for ( r = 0; r < h->readerCount; r++ ) {
		ri.Printf( PRINT_ALL, "    reader[%u]=%s\n", r, h->readers[r] );
	}
}

void vk_frame_contract_status_f( void )
{
	int i;

	VK_FC_RefreshLiveMeta();
	ri.Printf( PRINT_ALL, "======== Renderer Frame Contract ========\n" );
	ri.Printf( PRINT_ALL,
		"frame=%u mode=%d render=%ux%u output=%ux%u\n",
		s_snap.frame, s_snap.renderMode,
		s_snap.renderExtentW, s_snap.renderExtentH,
		s_snap.outputExtentW, s_snap.outputExtentH );
	ri.Printf( PRINT_ALL,
		"gen: cluster=%u gpuScene=%u light=%u material=%u shadow=%u probe=%u exposure=%u gbuffer=%u\n",
		s_snap.clusterListGeneration, s_snap.gpuSceneGeneration,
		s_snap.lightBufferGeneration, s_snap.materialBufferGeneration,
		s_snap.shadowGeneration, s_snap.probeGeneration,
		s_snap.exposureGeneration, s_snap.deferredGbufferGeneration );
	ri.Printf( PRINT_ALL, "exposure=%.4f validateFails=%u\n",
		s_snap.adaptedExposure, s_snap.validateFailCount );
	for ( i = 0; i < (int)VK_FRAME_RES_COUNT; i++ ) {
		VK_FC_PrintResource( &s_snap.resources[i] );
	}
}

void vk_frame_contract_capture_f( void )
{
	ri.Printf( PRINT_ALL, "[VK][frame_contract] CAPTURE begin\n" );
	vk_frame_contract_status_f();
	(void)vk_frame_contract_validate( qtrue );
	ri.Printf( PRINT_ALL, "[VK][frame_contract] CAPTURE end fails=%u\n",
		s_snap.validateFailCount );
}

void vk_frame_contract_register( void )
{
	int i;

	if ( !s_cmdsRegistered ) {
		ri.Cmd_AddCommand( "renderer_frame_status", vk_frame_contract_status_f );
		ri.Cmd_AddCommand( "renderer_capture_frame_contract", vk_frame_contract_capture_f );
		s_cmdsRegistered = qtrue;
		ri.Printf( PRINT_ALL,
			"[VK][frame_contract] renderer_frame_status, renderer_capture_frame_contract ready\n" );
	}
	Com_Memset( &s_snap, 0, sizeof( s_snap ) );
	for ( i = 0; i < (int)VK_FRAME_RES_COUNT; i++ ) {
		Q_strncpyz( s_snap.resources[i].name, s_resNames[i], sizeof( s_snap.resources[i].name ) );
	}
}

void vk_frame_contract_begin_frame( void )
{
	int i;

	s_frameCounter++;
	s_snap.frame = s_frameCounter;
	for ( i = 0; i < (int)VK_FRAME_RES_COUNT; i++ ) {
		s_snap.resources[i].writtenThisFrame = qfalse;
		s_snap.resources[i].clearedAfterWrite = qfalse;
		s_snap.resources[i].readerCount = 0u;
		s_snap.resources[i].firstWriter[0] = '\0';
		s_snap.resources[i].lastWriter[0] = '\0';
		s_snap.resources[i].invalidationReason[0] = '\0';
		Com_Memset( s_snap.resources[i].readers, 0, sizeof( s_snap.resources[i].readers ) );
	}
}

void vk_frame_contract_note_writer( const char *resourceName, const char *passName )
{
	vkFrameContractResource_t res;
	vkFrameResourceHistory_t *h;

	if ( !passName || !passName[0] ) {
		return;
	}
	res = VK_FC_ParseResource( resourceName );
	if ( res >= VK_FRAME_RES_COUNT ) {
		return;
	}
	h = &s_snap.resources[res];
	if ( !h->writtenThisFrame ) {
		Q_strncpyz( h->firstWriter, passName, sizeof( h->firstWriter ) );
		h->writtenThisFrame = qtrue;
		h->lastValidFrame = s_frameCounter;
	}
	Q_strncpyz( h->lastWriter, passName, sizeof( h->lastWriter ) );
}

void vk_frame_contract_note_reader( const char *resourceName, const char *passName )
{
	vkFrameContractResource_t res;
	vkFrameResourceHistory_t *h;

	if ( !passName || !passName[0] ) {
		return;
	}
	res = VK_FC_ParseResource( resourceName );
	if ( res >= VK_FRAME_RES_COUNT ) {
		return;
	}
	h = &s_snap.resources[res];
	if ( h->readerCount >= VK_FRAME_CONTRACT_MAX_READERS ) {
		return;
	}
	Q_strncpyz( h->readers[h->readerCount], passName, sizeof( h->readers[0] ) );
	h->readerCount++;
}

uint32_t vk_frame_contract_validate( qboolean printPass )
{
	uint32_t fails = 0u;
	const vkFrameResourceHistory_t *hdr = &s_snap.resources[VK_FRAME_RES_SCENE_HDR];

	VK_FC_RefreshLiveMeta();

	if ( vk.fboActive && vk.color_image == VK_NULL_HANDLE ) {
		if ( printPass ) {
			ri.Printf( PRINT_WARNING, "FAIL[frame_contract]: SceneHDR null while FBO active\n" );
		}
		fails++;
	} else if ( printPass ) {
		ri.Printf( PRINT_ALL, "PASS[frame_contract]: SceneHDR image present (or FBO off)\n" );
	}

	if ( vk.fboActive && backEnd.doneWorldScene && !hdr->writtenThisFrame ) {
		if ( printPass ) {
			ri.Printf( PRINT_WARNING,
				"FAIL[frame_contract]: world scene finished with no SceneHDR writer\n" );
		}
		fails++;
	} else if ( printPass && hdr->writtenThisFrame ) {
		ri.Printf( PRINT_ALL, "PASS[frame_contract]: SceneHDR writer=%s\n",
			hdr->lastWriter[0] ? hdr->lastWriter : "?" );
	}

	if ( hdr->clearedAfterWrite ) {
		if ( printPass ) {
			ri.Printf( PRINT_WARNING,
				"FAIL[frame_contract]: SceneHDR cleared after writer (%s)\n",
				hdr->invalidationReason[0] ? hdr->invalidationReason : "?" );
		}
		fails++;
	}

	if ( vk.oitAttachmentGeneration != vk.oitDescriptorGeneration &&
		r_oit && r_oit->integer ) {
		if ( printPass ) {
			ri.Printf( PRINT_WARNING,
				"FAIL[frame_contract]: OIT generation mismatch att=%u desc=%u\n",
				vk.oitAttachmentGeneration, vk.oitDescriptorGeneration );
		}
		fails++;
	} else if ( printPass ) {
		ri.Printf( PRINT_ALL, "PASS[frame_contract]: OIT generations consistent (or OIT off)\n" );
	}

	if ( r_exposure && ( !isfinite( r_exposure->value ) || r_exposure->value <= 0.0f ) ) {
		if ( printPass ) {
			ri.Printf( PRINT_WARNING,
				"FAIL[frame_contract]: invalid exposure %f\n", r_exposure->value );
		}
		fails++;
	} else if ( printPass ) {
		ri.Printf( PRINT_ALL, "PASS[frame_contract]: exposure finite and positive\n" );
	}

	if ( !s_snap.resources[VK_FRAME_RES_SCENE_DEPTH].writtenThisFrame &&
		vk.fboActive && backEnd.doneWorldScene ) {
		if ( printPass ) {
			ri.Printf( PRINT_ALL,
				"WARN[frame_contract]: SceneDepth writer not annotated (depth may still be valid)\n" );
		}
	}

	/* Extent sanity: render vs SceneHDR meta. */
	if ( hdr->extentW && hdr->extentH &&
		( hdr->extentW != s_snap.renderExtentW || hdr->extentH != s_snap.renderExtentH ) &&
		s_snap.renderExtentW && s_snap.renderExtentH ) {
		if ( printPass ) {
			ri.Printf( PRINT_WARNING,
				"FAIL[frame_contract]: SceneHDR extent %ux%u != render %ux%u\n",
				hdr->extentW, hdr->extentH, s_snap.renderExtentW, s_snap.renderExtentH );
		}
		fails++;
	} else if ( printPass ) {
		ri.Printf( PRINT_ALL, "PASS[frame_contract]: SceneHDR extent matches render (or unset)\n" );
	}

	/* Allowed multi-writer SceneHDR chain (opaque → OIT resolve → etc.). */
	if ( hdr->writtenThisFrame && hdr->firstWriter[0] && hdr->lastWriter[0] &&
		Q_stricmp( hdr->firstWriter, hdr->lastWriter ) && printPass ) {
		ri.Printf( PRINT_ALL,
			"NOTE[frame_contract]: SceneHDR multi-writer chain first=%s last=%s\n",
			hdr->firstWriter, hdr->lastWriter );
	}

	/* Black-frame class: depth/geometry activity without SceneHDR writer. */
	if ( vk.fboActive && backEnd.doneWorldScene &&
		backEnd.pc.c_surfaces > 0 && !hdr->writtenThisFrame ) {
		if ( printPass ) {
			ri.Printf( PRINT_WARNING,
				"FAIL[frame_contract]: surfaces=%i but SceneHDR unwritten (black-frame class)\n",
				backEnd.pc.c_surfaces );
		}
		fails++;
	}

	/* Stale previous-frame temporal resources (generation 0 while current scene gen bumped). */
	if ( s_snap.resources[VK_FRAME_RES_SCENE_HDR_PREVIOUS].readerCount > 0 &&
		s_snap.resources[VK_FRAME_RES_SCENE_HDR_PREVIOUS].lastValidFrame + 1u < s_snap.frame &&
		s_snap.frame > 2u ) {
		if ( printPass ) {
			ri.Printf( PRINT_WARNING,
				"WARN[frame_contract]: SceneHDRPrevious may be stale (lastValid=%u frame=%u)\n",
				s_snap.resources[VK_FRAME_RES_SCENE_HDR_PREVIOUS].lastValidFrame,
				s_snap.frame );
		}
	}

	s_snap.validateFailCount = fails;
	return fails;
}
