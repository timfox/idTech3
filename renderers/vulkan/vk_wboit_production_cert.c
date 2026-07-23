/*
===========================================================================
Color Pipeline Phase 2.6 — live WBOIT production certification controller.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_wboit_production_cert.h"
#include "vk_oit_certify.h"
#include "vk_oit_contract.h"
#include "vk_oit_weight_contract.h"
#include "vk_hdr_resolve_contract.h"

#include <math.h>
#include <stdlib.h>

static qboolean s_cmds;
static wboitCertStageReport_t s_stages[WBOIT_CERT_STAGE_COUNT];
static wboitProductionLevel_t s_level;
static cvar_t *r_oitCertificationDebug;
static cvar_t *r_oitRevealageDebug;
static char s_captureRoot[MAX_QPATH];

const char *vk_wboit_cert_stage_name( wboitCertStage_t stage )
{
	switch ( stage ) {
	case WBOIT_CERT_STAGE_CONTRACT: return "WBOIT_CERT_CONTRACT";
	case WBOIT_CERT_STAGE_RESOURCES: return "WBOIT_CERT_RESOURCES";
	case WBOIT_CERT_STAGE_EMPTY_PIXEL: return "WBOIT_CERT_EMPTY_PIXEL";
	case WBOIT_CERT_STAGE_SINGLE_LAYER: return "WBOIT_CERT_SINGLE_LAYER";
	case WBOIT_CERT_STAGE_REVEALAGE: return "WBOIT_CERT_REVEALAGE";
	case WBOIT_CERT_STAGE_ORDER_STABILITY: return "WBOIT_CERT_ORDER_STABILITY";
	case WBOIT_CERT_STAGE_ALPHA_ENCODING: return "WBOIT_CERT_ALPHA_ENCODING";
	case WBOIT_CERT_STAGE_DEPTH: return "WBOIT_CERT_DEPTH";
	case WBOIT_CERT_STAGE_FOG: return "WBOIT_CERT_FOG";
	case WBOIT_CERT_STAGE_ADDITIVE: return "WBOIT_CERT_ADDITIVE";
	case WBOIT_CERT_STAGE_HDR_RESOLVE: return "WBOIT_CERT_HDR_RESOLVE";
	case WBOIT_CERT_STAGE_EXPOSURE: return "WBOIT_CERT_EXPOSURE";
	case WBOIT_CERT_STAGE_LIFECYCLE: return "WBOIT_CERT_LIFECYCLE";
	case WBOIT_CERT_STAGE_SOAK: return "WBOIT_CERT_SOAK";
	default: return "UNKNOWN";
	}
}

const char *vk_wboit_production_level_name( wboitProductionLevel_t lvl )
{
	switch ( lvl ) {
	case WBOIT_STATIC_CERTIFIED: return "WBOIT_STATIC_CERTIFIED";
	case WBOIT_GPU_CORE_CERTIFIED: return "WBOIT_GPU_CORE_CERTIFIED";
	case WBOIT_FOG_HDR_CERTIFIED: return "WBOIT_FOG_HDR_CERTIFIED";
	case WBOIT_LIFECYCLE_CERTIFIED: return "WBOIT_LIFECYCLE_CERTIFIED";
	case WBOIT_PRODUCTION_CERTIFIED: return "WBOIT_PRODUCTION_CERTIFIED";
	default: return "WBOIT_LEVEL_NONE";
	}
}

wboitProductionLevel_t vk_wboit_production_level( void )
{
	return s_level;
}

float vk_wboit_cert_revealage_product( const float *alphas, int count )
{
	float r = 1.0f;
	int i;
	if ( !alphas || count <= 0 ) {
		return 1.0f;
	}
	for ( i = 0; i < count; i++ ) {
		float a = alphas[i];
		if ( a < 0.0f ) {
			a = 0.0f;
		} else if ( a > 1.0f ) {
			a = 1.0f;
		}
		r *= ( 1.0f - a );
	}
	return r;
}

void vk_wboit_cert_source_over( const float layerRgb[3], float opacity,
	const float fogRgb[3], float outRgb[3] )
{
	float o = opacity;
	float t;
	if ( o < 0.0f ) {
		o = 0.0f;
	} else if ( o > 1.0f ) {
		o = 1.0f;
	}
	t = 1.0f - o;
	outRgb[0] = layerRgb[0] * o + fogRgb[0] * t;
	outRgb[1] = layerRgb[1] * o + fogRgb[1] * t;
	outRgb[2] = layerRgb[2] * o + fogRgb[2] * t;
}

static void VK_WboitCert_FillHashes( wboitCertStageReport_t *r )
{
	const oitContract_t *oit = vk_oit_contract_wboit();
	const oitWeightContract_t *w = vk_oit_weight_contract_get();
	const hdrResolveContract_t *h = vk_hdr_resolve_contract_get();
	r->oitContractHash = oit ? oit->contractHash : 0u;
	r->weightContractHash = w ? w->contractHash : 0u;
	r->hdrResolveHash = h ? h->contractHash : 0u;
	r->fogSceneGen = vk_hdr_resolve_fog_scene_generation();
	r->oitAttGen = vk.oitAttachmentGeneration;
}

static void VK_WboitCert_RecomputeLevel( void )
{
	qboolean core = qtrue;
	qboolean fogHdr = qtrue;
	qboolean life = qtrue;
	int i;

	/* Static always available when module registers (foundation gates). */
	s_level = WBOIT_STATIC_CERTIFIED;

	/* GPU core: contract + resources + empty + single + revealage + alpha + weight-order */
	{
		static const wboitCertStage_t need[] = {
			WBOIT_CERT_STAGE_CONTRACT,
			WBOIT_CERT_STAGE_RESOURCES,
			WBOIT_CERT_STAGE_EMPTY_PIXEL,
			WBOIT_CERT_STAGE_SINGLE_LAYER,
			WBOIT_CERT_STAGE_REVEALAGE,
			WBOIT_CERT_STAGE_ALPHA_ENCODING,
			WBOIT_CERT_STAGE_ORDER_STABILITY
		};
		for ( i = 0; i < (int)( sizeof( need ) / sizeof( need[0] ) ); i++ ) {
			if ( Q_stricmp( s_stages[need[i]].result, "PASS" ) ) {
				core = qfalse;
				break;
			}
		}
	}
	if ( core ) {
		s_level = WBOIT_GPU_CORE_CERTIFIED;
	}

	if ( core &&
		!Q_stricmp( s_stages[WBOIT_CERT_STAGE_DEPTH].result, "PASS" ) &&
		!Q_stricmp( s_stages[WBOIT_CERT_STAGE_FOG].result, "PASS" ) &&
		!Q_stricmp( s_stages[WBOIT_CERT_STAGE_ADDITIVE].result, "PASS" ) &&
		!Q_stricmp( s_stages[WBOIT_CERT_STAGE_HDR_RESOLVE].result, "PASS" ) &&
		!Q_stricmp( s_stages[WBOIT_CERT_STAGE_EXPOSURE].result, "PASS" ) ) {
		fogHdr = qtrue;
		s_level = WBOIT_FOG_HDR_CERTIFIED;
	} else {
		fogHdr = qfalse;
	}

	if ( fogHdr &&
		!Q_stricmp( s_stages[WBOIT_CERT_STAGE_LIFECYCLE].result, "PASS" ) ) {
		life = qtrue;
		s_level = WBOIT_LIFECYCLE_CERTIFIED;
	} else {
		life = qfalse;
	}

	/* Production requires soak PASS + legacy live cert not lower than LIVE_FULL. */
	if ( life &&
		!Q_stricmp( s_stages[WBOIT_CERT_STAGE_SOAK].result, "PASS" ) &&
		vk_oit_certification_level() >= VK_OIT_CERT_LIVE_FULL ) {
		s_level = WBOIT_PRODUCTION_CERTIFIED;
	}

	(void)fogHdr;
}

static void VK_WboitCert_PrintStage( wboitCertStage_t stage )
{
	const wboitCertStageReport_t *r = &s_stages[stage];
	ri.Printf( PRINT_ALL,
		"  [%s] result=%s observed=%g threshold=%g\n"
		"      material=%s region=%s capture=%s\n"
		"      hashes: oit=0x%08x weight=0x%08x hdrResolve=0x%08x fogGen=%u oitAtt=%u\n"
		"      notes=%s\n",
		vk_wboit_cert_stage_name( stage ),
		r->result[0] ? r->result : "PENDING",
		r->observed, r->threshold,
		r->failingMaterial[0] ? r->failingMaterial : "-",
		r->failingRegion[0] ? r->failingRegion : "-",
		r->capturePath[0] ? r->capturePath : "-",
		r->oitContractHash, r->weightContractHash, r->hdrResolveHash,
		r->fogSceneGen, r->oitAttGen,
		r->notes[0] ? r->notes : "-" );
}

void vk_wboit_cert_stage_pass( wboitCertStage_t stage, float observed, float threshold, const char *notes )
{
	wboitCertStageReport_t *r;
	if ( stage < 0 || stage >= WBOIT_CERT_STAGE_COUNT ) {
		return;
	}
	r = &s_stages[stage];
	Com_Memset( r, 0, sizeof( *r ) );
	Q_strncpyz( r->result, "PASS", sizeof( r->result ) );
	r->observed = observed;
	r->threshold = threshold;
	if ( notes ) {
		Q_strncpyz( r->notes, notes, sizeof( r->notes ) );
	}
	VK_WboitCert_FillHashes( r );
	if ( s_captureRoot[0] ) {
		Com_sprintf( r->capturePath, sizeof( r->capturePath ), "%s/%s.png",
			s_captureRoot, vk_wboit_cert_stage_name( stage ) );
	}
	VK_WboitCert_RecomputeLevel();
}

void vk_wboit_cert_stage_fail( wboitCertStage_t stage, float observed, float threshold,
	const char *material, const char *region, const char *notes )
{
	wboitCertStageReport_t *r;
	if ( stage < 0 || stage >= WBOIT_CERT_STAGE_COUNT ) {
		return;
	}
	r = &s_stages[stage];
	Com_Memset( r, 0, sizeof( *r ) );
	Q_strncpyz( r->result, "FAIL", sizeof( r->result ) );
	r->observed = observed;
	r->threshold = threshold;
	if ( material ) {
		Q_strncpyz( r->failingMaterial, material, sizeof( r->failingMaterial ) );
	}
	if ( region ) {
		Q_strncpyz( r->failingRegion, region, sizeof( r->failingRegion ) );
	}
	if ( notes ) {
		Q_strncpyz( r->notes, notes, sizeof( r->notes ) );
	}
	VK_WboitCert_FillHashes( r );
	VK_WboitCert_RecomputeLevel();
}

void vk_wboit_cert_stage_skip( wboitCertStage_t stage, const char *reason )
{
	wboitCertStageReport_t *r;
	if ( stage < 0 || stage >= WBOIT_CERT_STAGE_COUNT ) {
		return;
	}
	r = &s_stages[stage];
	Com_Memset( r, 0, sizeof( *r ) );
	Q_strncpyz( r->result, "SKIP", sizeof( r->result ) );
	if ( reason ) {
		Q_strncpyz( r->notes, reason, sizeof( r->notes ) );
	}
	VK_WboitCert_FillHashes( r );
	VK_WboitCert_RecomputeLevel();
}

/*
 * Offline / CPU gates that can auto-pass without GPU readback.
 * Live GPU stages remain PENDING until operator or lab marks them.
 */
static void VK_WboitCert_RunStaticContractGates( void )
{
	char err[160];
	qboolean ok = qtrue;

	if ( !vk_oit_contract_validate( vk_oit_contract_wboit(), err, sizeof( err ) ) ) {
		vk_wboit_cert_stage_fail( WBOIT_CERT_STAGE_CONTRACT, 0.0f, 1.0f, "contract", "-", err );
		ok = qfalse;
	} else if ( !vk_oit_weight_contract_validate( vk_oit_weight_contract_get(), err, sizeof( err ) ) ) {
		vk_wboit_cert_stage_fail( WBOIT_CERT_STAGE_CONTRACT, 0.0f, 1.0f, "weight", "-", err );
		ok = qfalse;
	} else if ( !vk_hdr_resolve_contract_validate( vk_hdr_resolve_contract_get(), err, sizeof( err ) ) ) {
		vk_wboit_cert_stage_fail( WBOIT_CERT_STAGE_CONTRACT, 0.0f, 1.0f, "hdr_resolve", "-", err );
		ok = qfalse;
	} else {
		vk_wboit_cert_stage_pass( WBOIT_CERT_STAGE_CONTRACT, 1.0f, 1.0f,
			"oit+weight+hdrResolve contracts validate" );
	}

	/*
	 * Live GPU stages stay PENDING. CPU revealage/source-over helpers are
	 * covered by unit_wboit_live_cert — they must NOT auto-grant GPU_CORE.
	 */
	(void)ok;
}

static void VK_WboitProductionStatus_f( void )
{
	int i;
	VK_WboitCert_RecomputeLevel();
	ri.Printf( PRINT_ALL,
		"======== WBOIT Live Production Certification (Phase 2.6) ========\n"
		"level=%s\n"
		"legacyOperatorLevel=%s\n"
		"note=STATIC alone is NOT WBOIT_PRODUCTION_CERTIFIED — GPU stages + soak required\n"
		"captureRoot=%s r_oitCertificationDebug=%d r_oitRevealageDebug=%d\n",
		vk_wboit_production_level_name( s_level ),
		vk_oit_certification_level_name( vk_oit_certification_level() ),
		s_captureRoot[0] ? s_captureRoot : "(unset)",
		r_oitCertificationDebug ? r_oitCertificationDebug->integer : 0,
		r_oitRevealageDebug ? r_oitRevealageDebug->integer : 0 );
	for ( i = 0; i < (int)WBOIT_CERT_STAGE_COUNT; i++ ) {
		VK_WboitCert_PrintStage( (wboitCertStage_t)i );
	}
	ri.Printf( PRINT_ALL,
		"commands: oit_certify_wboit | oit_certification_status | oit_certification_capture\n"
		"          oit_certification_abort | oit_cert_stage pass|fail|skip <stage>\n"
		"docs: docs/WBOIT_LIVE_CERTIFICATION.md\n"
		"===============================================================\n" );
}

static int VK_WboitCert_ParseStage( const char *name )
{
	int i;
	if ( !name || !name[0] ) {
		return -1;
	}
	for ( i = 0; i < (int)WBOIT_CERT_STAGE_COUNT; i++ ) {
		if ( !Q_stricmp( name, vk_wboit_cert_stage_name( (wboitCertStage_t)i ) ) ) {
			return i;
		}
		/* Allow short names: EMPTY_PIXEL, FOG, ... */
		{
			const char *full = vk_wboit_cert_stage_name( (wboitCertStage_t)i );
			const char *shortName = full;
			if ( !Q_stricmpn( full, "WBOIT_CERT_", 11 ) ) {
				shortName = full + 11;
			}
			if ( !Q_stricmp( name, shortName ) ) {
				return i;
			}
		}
	}
	return -1;
}

static void VK_WboitCertStage_f( void )
{
	const char *op;
	int stage;
	if ( ri.Cmd_Argc() < 3 ) {
		ri.Printf( PRINT_ALL,
			"usage: oit_cert_stage <pass|fail|skip> <STAGE> [observed] [threshold] [notes...]\n"
			"stages: CONTRACT RESOURCES EMPTY_PIXEL SINGLE_LAYER REVEALAGE ORDER_STABILITY\n"
			"        ALPHA_ENCODING DEPTH FOG ADDITIVE HDR_RESOLVE EXPOSURE LIFECYCLE SOAK\n" );
		return;
	}
	op = ri.Cmd_Argv( 1 );
	stage = VK_WboitCert_ParseStage( ri.Cmd_Argv( 2 ) );
	if ( stage < 0 ) {
		ri.Printf( PRINT_ALL, "unknown stage '%s'\n", ri.Cmd_Argv( 2 ) );
		return;
	}
	if ( !Q_stricmp( op, "pass" ) ) {
		float obs = ( ri.Cmd_Argc() >= 4 ) ? (float)atof( ri.Cmd_Argv( 3 ) ) : 0.0f;
		float thr = ( ri.Cmd_Argc() >= 5 ) ? (float)atof( ri.Cmd_Argv( 4 ) ) : 0.0f;
		vk_wboit_cert_stage_pass( (wboitCertStage_t)stage, obs, thr,
			( ri.Cmd_Argc() >= 6 ) ? ri.Cmd_Argv( 5 ) : "operator pass" );
		ri.Printf( PRINT_ALL, "oit_cert_stage: PASS %s → level=%s\n",
			vk_wboit_cert_stage_name( (wboitCertStage_t)stage ),
			vk_wboit_production_level_name( s_level ) );
	} else if ( !Q_stricmp( op, "fail" ) ) {
		float obs = ( ri.Cmd_Argc() >= 4 ) ? (float)atof( ri.Cmd_Argv( 3 ) ) : 1.0f;
		float thr = ( ri.Cmd_Argc() >= 5 ) ? (float)atof( ri.Cmd_Argv( 4 ) ) : 0.0f;
		vk_wboit_cert_stage_fail( (wboitCertStage_t)stage, obs, thr, "operator", "-",
			( ri.Cmd_Argc() >= 6 ) ? ri.Cmd_Argv( 5 ) : "operator fail" );
		ri.Printf( PRINT_ALL, "oit_cert_stage: FAIL %s → level=%s\n",
			vk_wboit_cert_stage_name( (wboitCertStage_t)stage ),
			vk_wboit_production_level_name( s_level ) );
	} else if ( !Q_stricmp( op, "skip" ) ) {
		vk_wboit_cert_stage_skip( (wboitCertStage_t)stage,
			( ri.Cmd_Argc() >= 4 ) ? ri.Cmd_Argv( 3 ) : "skipped" );
		ri.Printf( PRINT_ALL, "oit_cert_stage: SKIP %s\n",
			vk_wboit_cert_stage_name( (wboitCertStage_t)stage ) );
	} else {
		ri.Printf( PRINT_ALL, "usage: oit_cert_stage <pass|fail|skip> <STAGE> ...\n" );
	}
}

static void VK_OitCertificationAbort_f( void )
{
	int i;
	for ( i = 0; i < (int)WBOIT_CERT_STAGE_COUNT; i++ ) {
		if ( !Q_stricmp( s_stages[i].result, "PASS" ) ) {
			continue;
		}
		/* leave PASS; reset others */
		if ( Q_stricmp( s_stages[i].result, "PASS" ) ) {
			Com_Memset( &s_stages[i], 0, sizeof( s_stages[i] ) );
			Q_strncpyz( s_stages[i].result, "PENDING", sizeof( s_stages[i].result ) );
		}
	}
	s_captureRoot[0] = '\0';
	/* Re-run CPU-only gates */
	VK_WboitCert_RunStaticContractGates();
	ri.Cmd_ExecuteText( EXEC_APPEND, "oit_certify_wboit abort\n" );
	ri.Printf( PRINT_ALL, "oit_certification_abort: stage session reset (CPU gates re-run)\n" );
}

static void VK_OitCertificationCapture_f( void )
{
	const char *arg = ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "default";
	Com_sprintf( s_captureRoot, sizeof( s_captureRoot ), "oit_cert_capture/%s", arg );
	ri.Printf( PRINT_ALL,
		"oit_certification_capture: root=%s\n"
		"  Arm screenshots via r_oitDebug / r_oitCertificationDebug; paths recorded on stage pass/fail.\n"
		"  Also: oit_capture stages\n",
		s_captureRoot );
	ri.Cmd_ExecuteText( EXEC_APPEND, "oit_capture stages\n" );
}

void vk_wboit_production_cert_begin_frame( void )
{
	/* Reserved for future GPU metric sampling. */
}

void vk_wboit_production_cert_register( void )
{
	int i;
	if ( s_cmds ) {
		return;
	}
	Com_Memset( s_stages, 0, sizeof( s_stages ) );
	for ( i = 0; i < (int)WBOIT_CERT_STAGE_COUNT; i++ ) {
		Q_strncpyz( s_stages[i].result, "PENDING", sizeof( s_stages[i].result ) );
	}
	s_level = WBOIT_STATIC_CERTIFIED;

	r_oitCertificationDebug = ri.Cvar_Get( "r_oitCertificationDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_oitCertificationDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitCertificationDebug,
		"WBOIT live cert debug: 1 empty mask  2 fog_scene  3 resolved  4 difference" );
	ri.Cvar_SetGroup( r_oitCertificationDebug, CVG_RENDERER );

	r_oitRevealageDebug = ri.Cvar_Get( "r_oitRevealageDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_oitRevealageDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitRevealageDebug,
		"Revealage cert debug: 1 GPU  2 expected  3 difference  4 additive contamination" );
	ri.Cvar_SetGroup( r_oitRevealageDebug, CVG_RENDERER );

	ri.Cmd_AddCommand( "oit_certification_abort", VK_OitCertificationAbort_f );
	ri.Cmd_AddCommand( "oit_certification_capture", VK_OitCertificationCapture_f );
	ri.Cmd_AddCommand( "oit_cert_stage", VK_WboitCertStage_f );
	ri.Cmd_AddCommand( "wboit_production_status", VK_WboitProductionStatus_f );
	/* Alias: oit_certification_status already exists — also print production levels from there via hook. */

	VK_WboitCert_RunStaticContractGates();
	s_cmds = qtrue;
	ri.Printf( PRINT_ALL,
		"[VK][OIT] Phase 2.6 production cert controller ready "
		"(wboit_production_status / oit_cert_stage / oit_certification_capture)\n"
		"[VK][OIT] level=%s — GPU stages PENDING until live pass\n",
		vk_wboit_production_level_name( s_level ) );
}
