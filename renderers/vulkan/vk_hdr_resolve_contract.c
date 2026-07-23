/*
===========================================================================
Color Pipeline Phase 2.4 — HDR resolve / SceneHDR integrity contract.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_hdr_resolve_contract.h"
#include "vk_color_contract.h"
#include "vk_oit_contract.h"

static cvar_t *r_hdrResolveDebug;
static qboolean s_cmds;
static hdrResolveContract_t s_frozen;

static uint32_t s_fogSceneGeneration;
static uint32_t s_fogSceneCopyFrame;
static uint32_t s_fogSceneCopyW;
static uint32_t s_fogSceneCopyH;
static uint32_t s_sceneHdrGeneration;
static uint32_t s_depthGeneration;
static uint32_t s_frameSerial;
static qboolean s_fogCopiedThisFrame;

uint32_t vk_hdr_resolve_contract_compute_hash( const hdrResolveContract_t *c )
{
	uint32_t h = 2166136261u;
	const unsigned char *p;
	size_t n, i;

	if ( !c ) {
		return 0u;
	}
	p = (const unsigned char *)c;
	n = sizeof( *c ) - sizeof( c->contractHash );
	for ( i = 0; i < n; i++ ) {
		h ^= p[i];
		h *= 16777619u;
	}
	return h;
}

static void VK_HdrResolve_FillFrozen( hdrResolveContract_t *c )
{
	Com_Memset( c, 0, sizeof( *c ) );
	c->resolveInSceneLinearHdr = qtrue;
	c->resolveBeforeExposure = qtrue;
	c->emptyPreservesOpaque = qtrue;
	c->opaqueFromFogSceneCopy = qtrue;
	c->noSecondFogOnResolve = qtrue;
	c->requireExtentMatch = qtrue;
	c->requireOitGenMatch = qtrue;
	c->requireFogSceneCopyThisFrame = qtrue;
	c->contractVersion = HDR_RESOLVE_CONTRACT_VERSION;
	c->contractHash = 0u;
	c->contractHash = vk_hdr_resolve_contract_compute_hash( c );
}

const hdrResolveContract_t *vk_hdr_resolve_contract_get( void )
{
	static qboolean inited;
	if ( !inited ) {
		VK_HdrResolve_FillFrozen( &s_frozen );
		inited = qtrue;
	}
	return &s_frozen;
}

qboolean vk_hdr_resolve_contract_validate( const hdrResolveContract_t *c, char *errBuf, int errBufSize )
{
	hdrResolveContract_t rebuilt;

	if ( errBuf && errBufSize > 0 ) {
		errBuf[0] = '\0';
	}
	if ( !c ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "null contract", errBufSize );
		}
		return qfalse;
	}
	if ( c->contractVersion != HDR_RESOLVE_CONTRACT_VERSION ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "version %u != %u",
				c->contractVersion, HDR_RESOLVE_CONTRACT_VERSION );
		}
		return qfalse;
	}
	VK_HdrResolve_FillFrozen( &rebuilt );
	if ( rebuilt.contractHash != c->contractHash ||
		!c->resolveInSceneLinearHdr || !c->resolveBeforeExposure ||
		!c->emptyPreservesOpaque || !c->opaqueFromFogSceneCopy ||
		!c->noSecondFogOnResolve || !c->requireExtentMatch ||
		!c->requireOitGenMatch || !c->requireFogSceneCopyThisFrame ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "policy field drift vs freeze", errBufSize );
		}
		return qfalse;
	}
	return qtrue;
}

void vk_hdr_resolve_begin_frame( void )
{
	s_frameSerial++;
	s_fogCopiedThisFrame = qfalse;
}

void vk_hdr_resolve_note_fog_scene_copy( uint32_t width, uint32_t height )
{
	s_fogSceneGeneration++;
	s_fogSceneCopyFrame = s_frameSerial;
	s_fogSceneCopyW = width;
	s_fogSceneCopyH = height;
	s_fogCopiedThisFrame = qtrue;
}

void vk_hdr_resolve_note_scene_hdr_recreate( void )
{
	s_sceneHdrGeneration++;
}

void vk_hdr_resolve_note_depth_recreate( void )
{
	s_depthGeneration++;
}

uint32_t vk_hdr_resolve_fog_scene_generation( void )
{
	return s_fogSceneGeneration;
}

uint32_t vk_hdr_resolve_scene_hdr_generation( void )
{
	return s_sceneHdrGeneration;
}

uint32_t vk_hdr_resolve_depth_generation( void )
{
	return s_depthGeneration;
}

qboolean vk_hdr_resolve_runtime_validate( qboolean requireFogCopy, char *errBuf, int errBufSize )
{
	const hdrResolveContract_t *c = vk_hdr_resolve_contract_get();
	const oitContract_t *oit;
	int fogMode;

	if ( errBuf && errBufSize > 0 ) {
		errBuf[0] = '\0';
	}
	if ( !vk_hdr_resolve_contract_validate( c, errBuf, errBufSize ) ) {
		return qfalse;
	}
	oit = vk_oit_contract_wboit();
	if ( oit && oit->preExposed ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "OIT contract preExposed (forbidden for resolve)", errBufSize );
		}
		return qfalse;
	}
	if ( c->requireOitGenMatch && vk.oitAttachmentGeneration > 0 &&
		vk.oitAttachmentGeneration != vk.oitDescriptorGeneration ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "oit gen mismatch att=%u desc=%u",
				vk.oitAttachmentGeneration, vk.oitDescriptorGeneration );
		}
		return qfalse;
	}
	if ( requireFogCopy && c->requireFogSceneCopyThisFrame && !s_fogCopiedThisFrame ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "fog_scene not copied this frame before resolve", errBufSize );
		}
		return qfalse;
	}
	fogMode = ri.Cvar_VariableIntegerValue( "r_oitFogMode" );
	if ( c->noSecondFogOnResolve && fogMode >= 1 && oit && !oit->fogAppliedPerFragment ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "fog mode>=1 but contract fogAppliedPerFragment false", errBufSize );
		}
		return qfalse;
	}
	(void)s_fogSceneCopyFrame;
	return qtrue;
}

void vk_hdr_resolve_contract_print( const hdrResolveContract_t *c )
{
	char err[160];
	const hdrResolveContract_t *base = c ? c : vk_hdr_resolve_contract_get();
	const qboolean ok = vk_hdr_resolve_contract_validate( base, err, sizeof( err ) );
	char liveErr[160];
	const qboolean liveOk = vk_hdr_resolve_runtime_validate( qfalse, liveErr, sizeof( liveErr ) );

	ri.Printf( PRINT_ALL, "======== HDR Resolve Integrity (Phase 2.4) ========\n" );
	ri.Printf( PRINT_ALL, "version=%u hash=0x%08x freeze=%s%s%s\n",
		base->contractVersion, base->contractHash,
		ok ? "PASS" : "FAIL", ok ? "" : " reason=", ok ? "" : err );
	ri.Printf( PRINT_ALL, "space: SCENE_LINEAR_HDR beforeExposure=%d emptyPreservesOpaque=%d\n",
		base->resolveBeforeExposure ? 1 : 0, base->emptyPreservesOpaque ? 1 : 0 );
	ri.Printf( PRINT_ALL, "opaqueBase=fog_scene copy  noSecondFog=%d extentMatch=%d oitGenMatch=%d\n",
		base->noSecondFogOnResolve ? 1 : 0, base->requireExtentMatch ? 1 : 0,
		base->requireOitGenMatch ? 1 : 0 );
	ri.Printf( PRINT_ALL,
		"generations: sceneHdr=%u depth=%u fogScene=%u fogCopiedThisFrame=%d copy=%ux%u\n"
		"  currentFrameSceneHdrOwnership=fog_scene snapshot before OIT (exact copy this frame)\n",
		s_sceneHdrGeneration, s_depthGeneration, s_fogSceneGeneration,
		s_fogCopiedThisFrame ? 1 : 0, s_fogSceneCopyW, s_fogSceneCopyH );
	ri.Printf( PRINT_ALL, "oit: attGen=%u descGen=%u frameState=%u\n",
		vk.oitAttachmentGeneration, vk.oitDescriptorGeneration, vk.oitFrameState );
	ri.Printf( PRINT_ALL, "runtimeValidate=%s%s%s\n",
		liveOk ? "PASS" : "FAIL", liveOk ? "" : " reason=", liveOk ? "" : liveErr );
	ri.Printf( PRINT_ALL, "color stage: %s → %s\n",
		vk_color_stage_name( VK_COLOR_STAGE_OIT_RESOLVE ),
		vk_color_space_name( VK_COLOR_SPACE_SCENE_LINEAR_HDR ) );
	ri.Printf( PRINT_ALL, "docs: docs/HDR_RESOLVE_INTEGRITY.md\n" );
	ri.Printf( PRINT_ALL, "===================================================\n" );
}

void vk_hdr_resolve_status_f( void )
{
	vk_hdr_resolve_contract_print( vk_hdr_resolve_contract_get() );
}

static void VK_HdrResolve_Validate_f( void )
{
	char err[160];
	if ( vk_hdr_resolve_contract_validate( vk_hdr_resolve_contract_get(), err, sizeof( err ) ) &&
		vk_hdr_resolve_runtime_validate( qfalse, err, sizeof( err ) ) ) {
		ri.Printf( PRINT_ALL, "hdr_resolve_validate: PASS (v%u hash=0x%08x fogGen=%u)\n",
			HDR_RESOLVE_CONTRACT_VERSION, vk_hdr_resolve_contract_get()->contractHash,
			s_fogSceneGeneration );
	} else {
		ri.Printf( PRINT_ALL, "hdr_resolve_validate: FAIL (%s)\n", err[0] ? err : "unknown" );
	}
}

void vk_hdr_resolve_contract_register( void )
{
	(void)vk_hdr_resolve_contract_get();
	r_hdrResolveDebug = ri.Cvar_Get( "r_hdrResolveDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_hdrResolveDebug, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_hdrResolveDebug,
		"HDR resolve integrity debug: 0 off, 1 log copy/resolve, 2 force status each resolve." );
	ri.Cvar_SetGroup( r_hdrResolveDebug, CVG_RENDERER );

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "hdr_resolve_status", vk_hdr_resolve_status_f );
		ri.Cmd_AddCommand( "oit_resolve_status", vk_hdr_resolve_status_f );
		ri.Cmd_AddCommand( "hdr_resolve_validate", VK_HdrResolve_Validate_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL,
		"[VK][hdr-resolve] Phase 2.4 contract frozen v%u hash=0x%08x "
		"(hdr_resolve_status / hdr_resolve_validate)\n",
		HDR_RESOLVE_CONTRACT_VERSION, s_frozen.contractHash );
}
