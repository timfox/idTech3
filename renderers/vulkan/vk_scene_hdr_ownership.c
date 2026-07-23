/*
===========================================================================
Renderer IQ P0-A — SceneHDR writer ownership.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_scene_hdr_ownership.h"
#include "vk_color_contract.h"

#ifdef USE_VULKAN

static sceneHdrOwnership_t s_own;
static qboolean s_cmds;
static cvar_t *r_sceneHdrOwnershipDebug;
static qboolean s_loggedGiBlock;

const char *vk_scene_hdr_stage_name( sceneHdrStage_t stage )
{
	switch ( stage ) {
	case SCENE_HDR_NONE: return "none";
	case SCENE_HDR_OPAQUE: return "opaque";
	case SCENE_HDR_SKY_ATMOSPHERE: return "sky_atmosphere";
	case SCENE_HDR_GI: return "gi";
	case SCENE_HDR_WBOIT_RESOLVE: return "wboit_resolve";
	case SCENE_HDR_ADDITIVE: return "additive";
	case SCENE_HDR_REFRACTION: return "refraction";
	case SCENE_HDR_SPECIAL_BLEND: return "special_blend";
	case SCENE_HDR_WEAPON: return "weapon";
	case SCENE_HDR_VOLUMETRIC: return "volumetric";
	case SCENE_HDR_BLOOM_SOURCE: return "bloom_source";
	default: return "unknown";
	}
}

static uint32_t SceneHdr_StageRank( sceneHdrStage_t stage )
{
	/* Monotonic spine rank for illegal-order detection (not 1:1 with color stages). */
	switch ( stage ) {
	case SCENE_HDR_NONE: return 0;
	case SCENE_HDR_OPAQUE: return 10;
	case SCENE_HDR_SKY_ATMOSPHERE: return 20;
	case SCENE_HDR_GI: return 30;
	case SCENE_HDR_WBOIT_RESOLVE: return 40;
	case SCENE_HDR_ADDITIVE: return 45;
	case SCENE_HDR_REFRACTION: return 50;
	case SCENE_HDR_SPECIAL_BLEND: return 55;
	case SCENE_HDR_WEAPON: return 60;
	case SCENE_HDR_VOLUMETRIC: return 70;
	case SCENE_HDR_BLOOM_SOURCE: return 80;
	default: return 0;
	}
}

void vk_scene_hdr_ownership_begin_frame( void )
{
	const uint64_t frame = s_own.frameNumber + 1u;
	const uint32_t blocked = s_own.blockedWrites;
	const uint32_t allowed = s_own.allowedWrites;

	Com_Memset( &s_own, 0, sizeof( s_own ) );
	s_own.frameNumber = frame;
	s_own.colorSpace = (uint32_t)VK_COLOR_SPACE_SCENE_LINEAR_HDR;
	s_own.exposureState = 0;
	s_own.extentWidth = vk.renderWidth;
	s_own.extentHeight = vk.renderHeight;
	s_own.blockedWrites = blocked; /* cumulative diagnostics */
	s_own.allowedWrites = allowed;
	s_loggedGiBlock = qfalse;
}

const sceneHdrOwnership_t *vk_scene_hdr_ownership_get( void )
{
	return &s_own;
}

qboolean vk_scene_hdr_allows_pre_oit_gi( void )
{
	if ( vk.oitFrameState == VK_OIT_FRAME_RESOLVED ||
		vk.oitFrameState == VK_OIT_FRAME_ACCUMULATED ) {
		return qfalse;
	}
	if ( SceneHdr_StageRank( s_own.lastWriter ) >= SceneHdr_StageRank( SCENE_HDR_WBOIT_RESOLVE ) ) {
		return qfalse;
	}
	return qtrue;
}

qboolean vk_scene_hdr_note_writer( sceneHdrStage_t stage, const char *writerName,
	sceneHdrWriteMode_t mode )
{
	const uint32_t newRank = SceneHdr_StageRank( stage );
	const uint32_t curRank = SceneHdr_StageRank( s_own.lastWriter );
	const char *name = ( writerName && writerName[0] ) ? writerName : vk_scene_hdr_stage_name( stage );

	if ( stage <= SCENE_HDR_NONE || stage >= SCENE_HDR_STAGE_COUNT ) {
		return qfalse;
	}

	/*
	 * P0 hard rule: GI (and other pre-OIT geometry GI) must not REPLACE SceneHDR
	 * after WBOIT resolve / refraction / weapon / bloom.
	 */
	if ( stage == SCENE_HDR_GI &&
		mode == SCENE_HDR_WRITE_REPLACE &&
		curRank >= SceneHdr_StageRank( SCENE_HDR_WBOIT_RESOLVE ) ) {
		s_own.blockedWrites++;
		if ( r_sceneHdrOwnershipDebug && r_sceneHdrOwnershipDebug->integer ) {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"[VK][SceneHDR] blocked GI replace after %s (writer=%s)\n" S_COLOR_WHITE,
				vk_scene_hdr_stage_name( s_own.lastWriter ), name );
		}
		return qfalse;
	}

	/* Bloom extract must not run before weapon when weapon owns the spine. */
	if ( stage == SCENE_HDR_BLOOM_SOURCE &&
		curRank > 0 &&
		curRank < SceneHdr_StageRank( SCENE_HDR_WEAPON ) &&
		r_weaponBloomMode && r_weaponBloomMode->integer == 1 &&
		r_temporalWeaponAfterTaa && r_temporalWeaponAfterTaa->integer ) {
		/* Soft warn only — defer path should prevent this; do not hard-fail bloom. */
		if ( r_sceneHdrOwnershipDebug && r_sceneHdrOwnershipDebug->integer >= 2 ) {
			ri.Printf( PRINT_DEVELOPER,
				"[VK][SceneHDR] bloom before weapon (last=%s writer=%s)\n",
				vk_scene_hdr_stage_name( s_own.lastWriter ), name );
		}
	}

	s_own.lastWriter = stage;
	s_own.generation++;
	s_own.extentWidth = vk.renderWidth;
	s_own.extentHeight = vk.renderHeight;
	s_own.colorSpace = (uint32_t)VK_COLOR_SPACE_SCENE_LINEAR_HDR;
	s_own.writerContractHash = ( s_own.writerContractHash * 16777619u ) ^ (uint32_t)stage ^
		(uint32_t)mode;
	Q_strncpyz( s_own.writerName, name, sizeof( s_own.writerName ) );
	s_own.allowedWrites++;

	if ( r_sceneHdrOwnershipDebug && r_sceneHdrOwnershipDebug->integer >= 2 ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][SceneHDR] writer=%s stage=%s mode=%s gen=%u rank=%u→%u\n",
			name, vk_scene_hdr_stage_name( stage ),
			mode == SCENE_HDR_WRITE_REPLACE ? "replace" : "compose",
			s_own.generation, curRank, newRank );
	}

	(void)newRank;
	return qtrue;
}

void vk_scene_hdr_ownership_status_f( void )
{
	const sceneHdrOwnership_t *o = &s_own;

	ri.Printf( PRINT_ALL,
		"=== SceneHDR ownership (IQ P0-A) ===\n"
		"  lastWriter=%s (%s) gen=%u frame=%llu\n"
		"  extent=%ux%u colorSpace=SCENE_LINEAR_HDR exposureState=%u\n"
		"  contractHash=0x%08x allowed=%u blocked=%u\n"
		"  oitFrameState=%u allowsPreOitGi=%d\n"
		"  policy: GI replace forbidden after wboit_resolve; weapon before bloom (mode 1)\n",
		vk_scene_hdr_stage_name( o->lastWriter ),
		o->writerName[0] ? o->writerName : "-",
		o->generation,
		(unsigned long long)o->frameNumber,
		o->extentWidth, o->extentHeight,
		o->exposureState,
		o->writerContractHash,
		o->allowedWrites, o->blockedWrites,
		vk.oitFrameState,
		vk_scene_hdr_allows_pre_oit_gi() ? 1 : 0 );
}

void vk_scene_hdr_ownership_register( void )
{
	r_sceneHdrOwnershipDebug = ri.Cvar_Get( "r_sceneHdrOwnershipDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_sceneHdrOwnershipDebug, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_sceneHdrOwnershipDebug,
		"SceneHDR ownership diagnostics (0=off 1=blocks 2=all writers)." );
	ri.Cvar_SetGroup( r_sceneHdrOwnershipDebug, CVG_RENDERER );

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "scene_hdr_status", vk_scene_hdr_ownership_status_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL, "[VK][SceneHDR] ownership tracking enabled (scene_hdr_status)\n" );
}

/* Used by GI gates for a one-shot operator message. */
void vk_scene_hdr_log_gi_blocked( const char *passName )
{
	if ( s_loggedGiBlock ) {
		return;
	}
	s_loggedGiBlock = qtrue;
	ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
		"[VK][SceneHDR] skipped %s: SceneHDR owned by post-OIT path (oitState=%u last=%s)\n"
		S_COLOR_WHITE,
		passName ? passName : "GI",
		vk.oitFrameState,
		vk_scene_hdr_stage_name( s_own.lastWriter ) );
}

#endif /* USE_VULKAN */
