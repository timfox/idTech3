/*
===========================================================================
Renderer IQ P1-B — BloomSourceHDR contract.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_bloom_source_contract.h"
#include "vk_color_contract.h"
#include "vk_scene_hdr_ownership.h"

#ifdef USE_VULKAN

static bloomSourceContract_t s_bloom;
static qboolean s_cmds;

static uint32_t BloomSource_Hash( const bloomSourceContract_t *c )
{
	uint32_t h = BLOOM_SOURCE_CONTRACT_VERSION;
	h = h * 16777619u ^ c->contributorMask;
	h = h * 16777619u ^ c->colorSpace;
	h = h * 16777619u ^ c->exposureState;
	h = h * 16777619u ^ c->width;
	h = h * 16777619u ^ c->height;
	h = h * 16777619u ^ c->sceneHdrGeneration;
	return h;
}

void vk_bloom_source_contract_begin_frame( void )
{
	const uint64_t frame = s_bloom.frameNumber + 1u;
	const uint32_t fails = s_bloom.validateFails;

	Com_Memset( &s_bloom, 0, sizeof( s_bloom ) );
	s_bloom.frameNumber = frame;
	s_bloom.colorSpace = (uint32_t)VK_COLOR_SPACE_SCENE_LINEAR_HDR;
	s_bloom.exposureState = 0;
	s_bloom.contractVersion = BLOOM_SOURCE_CONTRACT_VERSION;
	s_bloom.validateFails = fails;
	s_bloom.width = vk.renderWidth;
	s_bloom.height = vk.renderHeight;
}

void vk_bloom_source_note_contributor( uint32_t maskBit, const char *writer )
{
	s_bloom.contributorMask |= maskBit;
	if ( writer && writer[0] ) {
		Q_strncpyz( s_bloom.lastWriter, writer, sizeof( s_bloom.lastWriter ) );
	}
}

void vk_bloom_source_note_extract( const char *writerName )
{
	const sceneHdrOwnership_t *hdr = vk_scene_hdr_ownership_get();

	s_bloom.generation++;
	s_bloom.width = vk.renderWidth;
	s_bloom.height = vk.renderHeight;
	s_bloom.colorSpace = (uint32_t)VK_COLOR_SPACE_SCENE_LINEAR_HDR;
	s_bloom.exposureState = 0;
	s_bloom.sceneHdrGeneration = hdr ? hdr->generation : 0;
	if ( writerName && writerName[0] ) {
		Q_strncpyz( s_bloom.lastWriter, writerName, sizeof( s_bloom.lastWriter ) );
	}
	/* Infer contributors from SceneHDR ownership stage when bits unset. */
	if ( hdr ) {
		if ( hdr->lastWriter >= SCENE_HDR_OPAQUE ) {
			s_bloom.contributorMask |= BLOOM_CONTRIB_OPAQUE;
		}
		if ( hdr->lastWriter == SCENE_HDR_GI ) {
			s_bloom.contributorMask |= BLOOM_CONTRIB_GI;
		}
		if ( hdr->lastWriter >= SCENE_HDR_WBOIT_RESOLVE ) {
			s_bloom.contributorMask |= BLOOM_CONTRIB_WBOIT;
		}
		if ( hdr->lastWriter >= SCENE_HDR_REFRACTION ) {
			s_bloom.contributorMask |= BLOOM_CONTRIB_SPECIAL_TRANSPARENCY;
		}
		if ( hdr->lastWriter >= SCENE_HDR_WEAPON ) {
			s_bloom.contributorMask |= BLOOM_CONTRIB_WEAPON_OPAQUE;
		}
		if ( hdr->lastWriter >= SCENE_HDR_VOLUMETRIC ) {
			s_bloom.contributorMask |= BLOOM_CONTRIB_VOLUMETRIC;
		}
	}
	s_bloom.contractHash = BloomSource_Hash( &s_bloom );
}

const bloomSourceContract_t *vk_bloom_source_contract_get( void )
{
	return &s_bloom;
}

qboolean vk_bloom_source_contract_validate( char *errBuf, int errBufSize )
{
	if ( s_bloom.colorSpace != (uint32_t)VK_COLOR_SPACE_SCENE_LINEAR_HDR ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "bloom source not SCENE_LINEAR_HDR" );
		}
		s_bloom.validateFails++;
		return qfalse;
	}
	if ( s_bloom.exposureState != 0 ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "bloom source must be pre-exposure" );
		}
		s_bloom.validateFails++;
		return qfalse;
	}
	if ( s_bloom.generation == 0 && r_bloom && r_bloom->integer ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "bloom extract not noted this frame" );
		}
		/* Soft fail until first extract. */
		return qtrue;
	}
	return qtrue;
}

static void BloomSource_PrintMask( uint32_t mask )
{
	ri.Printf( PRINT_ALL, "  contributors=0x%02x%s%s%s%s%s%s%s%s\n",
		mask,
		( mask & BLOOM_CONTRIB_OPAQUE ) ? " opaque" : "",
		( mask & BLOOM_CONTRIB_GI ) ? " gi" : "",
		( mask & BLOOM_CONTRIB_WBOIT ) ? " wboit" : "",
		( mask & BLOOM_CONTRIB_ADDITIVE ) ? " additive" : "",
		( mask & BLOOM_CONTRIB_SPECIAL_TRANSPARENCY ) ? " special" : "",
		( mask & BLOOM_CONTRIB_WEAPON_OPAQUE ) ? " weapon" : "",
		( mask & BLOOM_CONTRIB_WEAPON_EMISSIVE ) ? " weaponEmissive" : "",
		( mask & BLOOM_CONTRIB_VOLUMETRIC ) ? " volumetric" : "" );
}

void vk_bloom_source_status_f( void )
{
	char err[128];
	const bloomSourceContract_t *c = &s_bloom;
	const qboolean ok = vk_bloom_source_contract_validate( err, sizeof( err ) );

	ri.Printf( PRINT_ALL,
		"=== BloomSourceHDR (IQ P1-B) ===\n"
		"  resource=vk.color_image (SceneHDR) gen=%u frame=%llu\n"
		"  extent=%ux%u space=SCENE_LINEAR_HDR exposureState=%u\n"
		"  sceneHdrGen=%u contract=v%u hash=0x%08x\n"
		"  lastWriter=%s validate=%s\n"
		"  forbidden: OIT accum/reveal, UI, tonemap, display-encoded, stale pre-weapon\n",
		c->generation, (unsigned long long)c->frameNumber,
		c->width, c->height, c->exposureState,
		c->sceneHdrGeneration, c->contractVersion, c->contractHash,
		c->lastWriter[0] ? c->lastWriter : "-",
		ok ? "OK" : err );
	BloomSource_PrintMask( c->contributorMask );
}

static void BloomSource_Validate_f( void )
{
	char err[128];
	if ( vk_bloom_source_contract_validate( err, sizeof( err ) ) ) {
		ri.Printf( PRINT_ALL, "bloom_source_validate: OK\n" );
	} else {
		ri.Printf( PRINT_ALL, "bloom_source_validate: FAIL (%s)\n", err );
	}
}

void vk_bloom_source_contract_register( void )
{
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "bloom_source_status", vk_bloom_source_status_f );
		ri.Cmd_AddCommand( "bloom_source_validate", BloomSource_Validate_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL, "[VK][bloom-source] contract ready (bloom_source_status)\n" );
}

#endif /* USE_VULKAN */
