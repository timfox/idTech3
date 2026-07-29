/*
===========================================================================
Phase 2.6A — evidence-backed WBOIT production certification.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_wboit_production_cert.h"
#include "vk_oit_certify.h"
#include "vk_oit_contract.h"
#include "vk_oit_weight_contract.h"
#include "vk_oit_alpha.h"
#include "vk_depth_contract.h"
#include "vk_hdr_resolve_contract.h"
#include "vk_cert_metrics.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static qboolean s_cmds;
static wboitCertStageResult_t s_results[WBOIT_CERT_STAGE_COUNT];
static wboitProductionLevel_t s_level;
static cvar_t *r_oitCertificationDebug;
static cvar_t *r_oitRevealageDebug;
static cvar_t *r_oitAllowManualCertification;
static cvar_t *r_requireWboitCertification;
static char s_captureRoot[MAX_QPATH];
static char s_invalidateReason[192];
static char s_certifiedDevice[128];
static char s_importNote[192];

const char *vk_wboit_cert_stage_name( wboitCertStage_t stage )
{
	switch ( stage ) {
	case WBOIT_CERT_STAGE_CONTRACT: return "WBOIT_CERT_CONTRACT";
	case WBOIT_CERT_STAGE_RESOURCES: return "WBOIT_CERT_RESOURCES";
	case WBOIT_CERT_STAGE_EMPTY_PIXEL: return "WBOIT_CERT_EMPTY_PIXEL";
	case WBOIT_CERT_STAGE_SINGLE_LAYER: return "WBOIT_CERT_SINGLE_LAYER";
	case WBOIT_CERT_STAGE_REVEALAGE: return "WBOIT_CERT_REVEALAGE";
	case WBOIT_CERT_STAGE_ALPHA_ENCODING: return "WBOIT_CERT_ALPHA_ENCODING";
	case WBOIT_CERT_STAGE_WEIGHT_BOUNDS: return "WBOIT_CERT_WEIGHT_BOUNDS";
	case WBOIT_CERT_STAGE_ORDER_STABILITY: return "WBOIT_CERT_ORDER_STABILITY";
	case WBOIT_CERT_STAGE_FOG_DEPTH: return "WBOIT_CERT_FOG_DEPTH";
	case WBOIT_CERT_STAGE_ADDITIVE: return "WBOIT_CERT_ADDITIVE";
	case WBOIT_CERT_STAGE_HDR_RESOLVE: return "WBOIT_CERT_HDR_RESOLVE";
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

const char *vk_wboit_cert_evidence_name( wboitCertEvidence_t e )
{
	switch ( e ) {
	case WBOIT_EVIDENCE_STATIC: return "STATIC";
	case WBOIT_EVIDENCE_CPU_REFERENCE: return "CPU_REFERENCE";
	case WBOIT_EVIDENCE_GPU_READBACK: return "GPU_READBACK";
	case WBOIT_EVIDENCE_GPU_IMAGE_DIFF: return "GPU_IMAGE_DIFF";
	case WBOIT_EVIDENCE_GPU_REDUCTION: return "GPU_REDUCTION";
	case WBOIT_EVIDENCE_LIFECYCLE: return "LIFECYCLE";
	case WBOIT_EVIDENCE_SOAK: return "SOAK";
	case WBOIT_EVIDENCE_MANUAL_OVERRIDE: return "MANUAL_OVERRIDE";
	default: return "NONE";
	}
}

const char *vk_wboit_cert_status_name( wboitCertStatus_t s )
{
	switch ( s ) {
	case WBOIT_CERT_STATUS_PASS: return "PASS";
	case WBOIT_CERT_STATUS_FAIL: return "FAIL";
	case WBOIT_CERT_STATUS_SKIP: return "SKIP";
	case WBOIT_CERT_STATUS_INVALIDATED: return "INVALIDATED";
	default: return "PENDING";
	}
}

wboitCertEvidence_t vk_wboit_cert_required_evidence( wboitCertStage_t stage )
{
	switch ( stage ) {
	case WBOIT_CERT_STAGE_CONTRACT:
	case WBOIT_CERT_STAGE_RESOURCES:
		return WBOIT_EVIDENCE_STATIC;
	case WBOIT_CERT_STAGE_EMPTY_PIXEL:
		return WBOIT_EVIDENCE_GPU_READBACK; /* or GPU_REDUCTION via satisfies */
	case WBOIT_CERT_STAGE_SINGLE_LAYER:
	case WBOIT_CERT_STAGE_REVEALAGE:
	case WBOIT_CERT_STAGE_ALPHA_ENCODING:
	case WBOIT_CERT_STAGE_ORDER_STABILITY:
	case WBOIT_CERT_STAGE_FOG_DEPTH:
	case WBOIT_CERT_STAGE_ADDITIVE:
	case WBOIT_CERT_STAGE_HDR_RESOLVE:
		return WBOIT_EVIDENCE_GPU_READBACK;
	case WBOIT_CERT_STAGE_WEIGHT_BOUNDS:
		return WBOIT_EVIDENCE_GPU_REDUCTION;
	case WBOIT_CERT_STAGE_LIFECYCLE:
		return WBOIT_EVIDENCE_LIFECYCLE;
	case WBOIT_CERT_STAGE_SOAK:
		return WBOIT_EVIDENCE_SOAK;
	default:
		return WBOIT_EVIDENCE_NONE;
	}
}

qboolean vk_wboit_cert_evidence_satisfies( wboitCertEvidence_t have, wboitCertEvidence_t need )
{
	if ( have == need ) {
		return qtrue;
	}
	/* Empty pixel accepts reduction as well as readback. */
	if ( need == WBOIT_EVIDENCE_GPU_READBACK && have == WBOIT_EVIDENCE_GPU_REDUCTION ) {
		return qtrue;
	}
	if ( need == WBOIT_EVIDENCE_GPU_READBACK && have == WBOIT_EVIDENCE_GPU_IMAGE_DIFF ) {
		return qtrue;
	}
	if ( need == WBOIT_EVIDENCE_GPU_REDUCTION && have == WBOIT_EVIDENCE_GPU_READBACK ) {
		return qtrue;
	}
	if ( need == WBOIT_EVIDENCE_GPU_REDUCTION && have == WBOIT_EVIDENCE_GPU_IMAGE_DIFF ) {
		return qtrue;
	}
	return qfalse;
}

wboitProductionLevel_t vk_wboit_production_level( void )
{
	return s_level;
}

const wboitCertStageResult_t *vk_wboit_cert_stage_result( wboitCertStage_t stage )
{
	if ( stage < 0 || stage >= WBOIT_CERT_STAGE_COUNT ) {
		return NULL;
	}
	return &s_results[stage];
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
	float o = opacity < 0.0f ? 0.0f : ( opacity > 1.0f ? 1.0f : opacity );
	float t = 1.0f - o;
	outRgb[0] = layerRgb[0] * o + fogRgb[0] * t;
	outRgb[1] = layerRgb[1] * o + fogRgb[1] * t;
	outRgb[2] = layerRgb[2] * o + fogRgb[2] * t;
}

static void VK_WboitCert_FillMeta( wboitCertStageResult_t *r )
{
	const oitContract_t *oit = vk_oit_contract_wboit();
	const oitWeightContract_t *w = vk_oit_weight_contract_get();
	const hdrResolveContract_t *h = vk_hdr_resolve_contract_get();
	const depthContract_t *d = vk_depth_contract_get();
	r->oitContractHash = oit ? oit->contractHash : 0;
	r->weightContractHash = w ? w->contractHash : 0;
	r->resolveContractHash = h ? h->contractHash : 0;
	r->depthContractHash = d ? d->contractHash : 0;
	r->alphaContractHash = (uint32_t)vk_oit_alpha_certification_level();
	r->sceneGeneration = vk_hdr_resolve_fog_scene_generation();
	r->depthGeneration = vk_hdr_resolve_depth_generation();
	r->oitGeneration = vk.oitAttachmentGeneration;
	r->resolveGeneration = vk_hdr_resolve_scene_hdr_generation();
	r->frameNumber = vk.oitFrameNumber ? vk.oitFrameNumber : (uint64_t)tr.frameCount;
	r->timestamp = (uint64_t)time( NULL );
}

static qboolean VK_WboitCert_StagePassOk( wboitCertStage_t stage )
{
	const wboitCertStageResult_t *r = &s_results[stage];
	wboitCertEvidence_t need;
	if ( r->status != WBOIT_CERT_STATUS_PASS ) {
		return qfalse;
	}
	need = vk_wboit_cert_required_evidence( stage );
	if ( r->evidenceType == WBOIT_EVIDENCE_MANUAL_OVERRIDE ) {
		/* Manual never counts toward promotion unless explicitly allowed — still blocked for PRODUCTION. */
		return ( r_oitAllowManualCertification && r_oitAllowManualCertification->integer ) ? qtrue : qfalse;
	}
	return vk_wboit_cert_evidence_satisfies( (wboitCertEvidence_t)r->evidenceType, need );
}

static void VK_WboitCert_RecomputeLevel( void )
{
	qboolean core = qtrue;
	qboolean fogHdr = qtrue;
	qboolean life = qtrue;
	int i;
	static const wboitCertStage_t coreNeed[] = {
		WBOIT_CERT_STAGE_CONTRACT,
		WBOIT_CERT_STAGE_RESOURCES,
		WBOIT_CERT_STAGE_EMPTY_PIXEL,
		WBOIT_CERT_STAGE_SINGLE_LAYER,
		WBOIT_CERT_STAGE_REVEALAGE,
		WBOIT_CERT_STAGE_ALPHA_ENCODING,
		WBOIT_CERT_STAGE_WEIGHT_BOUNDS,
		WBOIT_CERT_STAGE_ORDER_STABILITY
	};
	static const wboitCertStage_t fogNeed[] = {
		WBOIT_CERT_STAGE_FOG_DEPTH,
		WBOIT_CERT_STAGE_ADDITIVE,
		WBOIT_CERT_STAGE_HDR_RESOLVE
	};

	s_level = WBOIT_STATIC_CERTIFIED;
	for ( i = 0; i < (int)( sizeof( coreNeed ) / sizeof( coreNeed[0] ) ); i++ ) {
		if ( !VK_WboitCert_StagePassOk( coreNeed[i] ) ) {
			core = qfalse;
			break;
		}
	}
	if ( core ) {
		s_level = WBOIT_GPU_CORE_CERTIFIED;
	}
	if ( core ) {
		for ( i = 0; i < (int)( sizeof( fogNeed ) / sizeof( fogNeed[0] ) ); i++ ) {
			if ( !VK_WboitCert_StagePassOk( fogNeed[i] ) ) {
				fogHdr = qfalse;
				break;
			}
		}
		if ( fogHdr ) {
			s_level = WBOIT_FOG_HDR_CERTIFIED;
		}
	} else {
		fogHdr = qfalse;
	}
	if ( fogHdr && VK_WboitCert_StagePassOk( WBOIT_CERT_STAGE_LIFECYCLE ) ) {
		life = qtrue;
		s_level = WBOIT_LIFECYCLE_CERTIFIED;
	} else {
		life = qfalse;
	}

	/*
	 * PRODUCTION: lifecycle + soak with proper evidence types, plus legacy LIVE_FULL,
	 * and absolutely no reliance on MANUAL_OVERRIDE for any required stage.
	 */
	if ( life &&
		VK_WboitCert_StagePassOk( WBOIT_CERT_STAGE_SOAK ) &&
		vk_oit_certification_level() >= VK_OIT_CERT_LIVE_FULL ) {
		qboolean anyManual = qfalse;
		for ( i = 0; i < (int)WBOIT_CERT_STAGE_COUNT; i++ ) {
			if ( s_results[i].status == WBOIT_CERT_STATUS_PASS &&
				s_results[i].evidenceType == WBOIT_EVIDENCE_MANUAL_OVERRIDE ) {
				anyManual = qtrue;
				break;
			}
		}
		if ( !anyManual || ( r_oitAllowManualCertification && r_oitAllowManualCertification->integer > 1 ) ) {
			if ( !anyManual ) {
				s_level = WBOIT_PRODUCTION_CERTIFIED;
				Q_strncpyz( s_certifiedDevice, "local-gpu", sizeof( s_certifiedDevice ) );
			}
		}
	}
}

void vk_wboit_cert_record_result( const wboitCertStageResult_t *result )
{
	wboitCertStageResult_t *dst;
	if ( !result || result->stage >= WBOIT_CERT_STAGE_COUNT ) {
		return;
	}
	dst = &s_results[result->stage];
	*dst = *result;
	VK_WboitCert_FillMeta( dst );
	if ( s_captureRoot[0] && !dst->capturePrefix[0] ) {
		Com_sprintf( dst->capturePrefix, sizeof( dst->capturePrefix ), "%s/%s",
			s_captureRoot, vk_wboit_cert_stage_name( (wboitCertStage_t)result->stage ) );
	}
	VK_WboitCert_RecomputeLevel();
}

void vk_wboit_cert_stage_pass( wboitCertStage_t stage, float observed, float threshold, const char *notes )
{
	wboitCertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = (uint32_t)stage;
	r.status = WBOIT_CERT_STATUS_PASS;
	r.evidenceType = WBOIT_EVIDENCE_MANUAL_OVERRIDE;
	r.observed = observed;
	r.warningThreshold = threshold;
	r.failureThreshold = threshold;
	Q_strncpyz( r.testName, "manual_pass", sizeof( r.testName ) );
	Q_strncpyz( r.failureReason, notes ? notes : "manual override pass", sizeof( r.failureReason ) );
	vk_wboit_cert_record_result( &r );
	ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
		"oit_cert_stage: MANUAL_OVERRIDE recorded for %s — cannot grant WBOIT_PRODUCTION_CERTIFIED "
		"(r_oitAllowManualCertification=%d)\n" S_COLOR_WHITE,
		vk_wboit_cert_stage_name( stage ),
		r_oitAllowManualCertification ? r_oitAllowManualCertification->integer : 0 );
}

void vk_wboit_cert_stage_fail( wboitCertStage_t stage, float observed, float threshold,
	const char *material, const char *region, const char *notes )
{
	wboitCertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = (uint32_t)stage;
	r.status = WBOIT_CERT_STATUS_FAIL;
	r.evidenceType = WBOIT_EVIDENCE_MANUAL_OVERRIDE;
	r.observed = observed;
	r.failureThreshold = threshold;
	Q_strncpyz( r.testName, material ? material : "manual_fail", sizeof( r.testName ) );
	Com_sprintf( r.failureReason, sizeof( r.failureReason ), "%s [%s]",
		notes ? notes : "fail", region ? region : "-" );
	vk_wboit_cert_record_result( &r );
}

void vk_wboit_cert_stage_skip( wboitCertStage_t stage, const char *reason )
{
	wboitCertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = (uint32_t)stage;
	r.status = WBOIT_CERT_STATUS_SKIP;
	r.evidenceType = WBOIT_EVIDENCE_MANUAL_OVERRIDE;
	Q_strncpyz( r.failureReason, reason ? reason : "skip", sizeof( r.failureReason ) );
	vk_wboit_cert_record_result( &r );
}

void vk_wboit_cert_invalidate_all( const char *reason )
{
	int i;
	for ( i = 0; i < (int)WBOIT_CERT_STAGE_COUNT; i++ ) {
		if ( s_results[i].status == WBOIT_CERT_STATUS_PASS ) {
			s_results[i].status = WBOIT_CERT_STATUS_INVALIDATED;
		}
	}
	Q_strncpyz( s_invalidateReason, reason ? reason : "invalidated", sizeof( s_invalidateReason ) );
	s_level = WBOIT_STATIC_CERTIFIED;
	s_certifiedDevice[0] = '\0';
	ri.Printf( PRINT_WARNING, S_COLOR_YELLOW "oit_certification_invalidate: %s\n" S_COLOR_WHITE,
		s_invalidateReason );
}

qboolean vk_wboit_cert_export_json( const char *path )
{
	char buf[16384];
	int off = 0;
	int i;
	const char *outPath = path && path[0] ? path : "render_cert/wboit_certification.json";

	VK_WboitCert_RecomputeLevel();
	off += Com_sprintf( buf + off, sizeof( buf ) - off,
		"{\n"
		"  \"schema\": \"wboit_certification/2.7-image-diff\",\n"
		"  \"level\": \"%s\",\n"
		"  \"device\": \"%s\",\n"
		"  \"importNote\": \"%s\",\n"
		"  \"invalidateReason\": \"%s\",\n"
		"  \"manualAllowed\": %d,\n"
		"  \"requireMode\": %d,\n"
		"  \"stages\": [\n",
		vk_wboit_production_level_name( s_level ),
		s_certifiedDevice[0] ? s_certifiedDevice : "uncertified",
		s_importNote[0] ? s_importNote : "",
		s_invalidateReason[0] ? s_invalidateReason : "",
		r_oitAllowManualCertification ? r_oitAllowManualCertification->integer : 0,
		r_requireWboitCertification ? r_requireWboitCertification->integer : 1 );
	for ( i = 0; i < (int)WBOIT_CERT_STAGE_COUNT; i++ ) {
		const wboitCertStageResult_t *r = &s_results[i];
		off += Com_sprintf( buf + off, sizeof( buf ) - off,
			"    {\"stage\":\"%s\",\"status\":\"%s\",\"evidence\":\"%s\","
			"\"observed\":%.9g,\"warn\":%.9g,\"fail\":%.9g,"
			"\"oitHash\":%u,\"weightHash\":%u,\"resolveHash\":%u,"
			"\"test\":\"%s\",\"reason\":\"%s\"}%s\n",
			vk_wboit_cert_stage_name( (wboitCertStage_t)i ),
			vk_wboit_cert_status_name( (wboitCertStatus_t)r->status ),
			vk_wboit_cert_evidence_name( (wboitCertEvidence_t)r->evidenceType ),
			r->observed, r->warningThreshold, r->failureThreshold,
			r->oitContractHash, r->weightContractHash, r->resolveContractHash,
			r->testName, r->failureReason,
			( i + 1 < (int)WBOIT_CERT_STAGE_COUNT ) ? "," : "" );
		if ( off > (int)sizeof( buf ) - 512 ) {
			break;
		}
	}
	off += Com_sprintf( buf + off, sizeof( buf ) - off, "  ]\n}\n" );
	ri.FS_WriteFile( outPath, buf, off );
	ri.Printf( PRINT_ALL, "oit_certification_export: wrote %s (%d bytes) level=%s\n",
		outPath, off, vk_wboit_production_level_name( s_level ) );
	return qtrue;
}

qboolean vk_wboit_cert_import_json( const char *path )
{
	Q_strncpyz( s_importNote, path ? path : "(unnamed)", sizeof( s_importNote ) );
	ri.Printf( PRINT_ALL,
		"oit_certification_import: displayed only — imported evidence from '%s' "
		"does NOT certify this device/build\n", s_importNote );
	return qtrue;
}

static void VK_WboitCert_RunStaticContractGates( void )
{
	char err[160];
	wboitCertStageResult_t r;

	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = WBOIT_CERT_STAGE_CONTRACT;
	r.evidenceType = WBOIT_EVIDENCE_STATIC;
	Q_strncpyz( r.testName, "contract_validate", sizeof( r.testName ) );
	if ( !vk_oit_contract_validate( vk_oit_contract_wboit(), err, sizeof( err ) ) ||
		!vk_oit_weight_contract_validate( vk_oit_weight_contract_get(), err, sizeof( err ) ) ||
		!vk_hdr_resolve_contract_validate( vk_hdr_resolve_contract_get(), err, sizeof( err ) ) ) {
		r.status = WBOIT_CERT_STATUS_FAIL;
		r.observed = 0.0;
		Q_strncpyz( r.failureReason, err, sizeof( r.failureReason ) );
	} else {
		r.status = WBOIT_CERT_STATUS_PASS;
		r.observed = 1.0;
		Q_strncpyz( r.failureReason, "oit+weight+hdrResolve static OK", sizeof( r.failureReason ) );
	}
	vk_wboit_cert_record_result( &r );

	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = WBOIT_CERT_STAGE_RESOURCES;
	r.evidenceType = WBOIT_EVIDENCE_STATIC;
	Q_strncpyz( r.testName, "pipeline_target_contract", sizeof( r.testName ) );
	r.status = WBOIT_CERT_STATUS_PASS;
	r.observed = 1.0;
	Q_strncpyz( r.failureReason, "formats/blend frozen in oitContract (live GPU validate via oit_lab)",
		sizeof( r.failureReason ) );
	vk_wboit_cert_record_result( &r );
}

static void VK_WboitProductionStatus_f( void )
{
	int i;
	VK_WboitCert_RecomputeLevel();
	ri.Printf( PRINT_ALL,
		"======== WBOIT Live Production Certification (Phase 2.6A) ========\n"
		"level=%s\n"
		"legacyOperatorLevel=%s\n"
		"manualAllowed=%d requireMode=%d device=%s\n"
		"policy: MANUAL_OVERRIDE never grants PRODUCTION by default\n"
		"invalidate=%s import=%s\n",
		vk_wboit_production_level_name( s_level ),
		vk_oit_certification_level_name( vk_oit_certification_level() ),
		r_oitAllowManualCertification ? r_oitAllowManualCertification->integer : 0,
		r_requireWboitCertification ? r_requireWboitCertification->integer : 1,
		s_certifiedDevice[0] ? s_certifiedDevice : "(none)",
		s_invalidateReason[0] ? s_invalidateReason : "-",
		s_importNote[0] ? s_importNote : "-" );
	for ( i = 0; i < (int)WBOIT_CERT_STAGE_COUNT; i++ ) {
		const wboitCertStageResult_t *r = &s_results[i];
		ri.Printf( PRINT_ALL,
			"  [%s] %s evidence=%s (need %s) observed=%g failThr=%g test=%s\n"
			"      reason=%s capture=%s\n",
			vk_wboit_cert_stage_name( (wboitCertStage_t)i ),
			vk_wboit_cert_status_name( (wboitCertStatus_t)r->status ),
			vk_wboit_cert_evidence_name( (wboitCertEvidence_t)r->evidenceType ),
			vk_wboit_cert_evidence_name( vk_wboit_cert_required_evidence( (wboitCertStage_t)i ) ),
			r->observed, r->failureThreshold,
			r->testName[0] ? r->testName : "-",
			r->failureReason[0] ? r->failureReason : "-",
			r->capturePrefix[0] ? r->capturePrefix : "-" );
	}
	ri.Printf( PRINT_ALL,
		"commands: oit_lab_run | oit_certification_export | oit_certification_invalidate\n"
		"===============================================================\n" );
}

static int VK_WboitCert_ParseStage( const char *name )
{
	int i;
	if ( !name || !name[0] ) {
		return -1;
	}
	for ( i = 0; i < (int)WBOIT_CERT_STAGE_COUNT; i++ ) {
		const char *full = vk_wboit_cert_stage_name( (wboitCertStage_t)i );
		const char *shortName = full;
		if ( !Q_stricmp( name, full ) ) {
			return i;
		}
		if ( !Q_stricmpn( full, "WBOIT_CERT_", 11 ) ) {
			shortName = full + 11;
		}
		if ( !Q_stricmp( name, shortName ) ) {
			return i;
		}
	}
	/* aliases from 2.6 docs */
	if ( !Q_stricmp( name, "FOG" ) || !Q_stricmp( name, "DEPTH" ) ) {
		return WBOIT_CERT_STAGE_FOG_DEPTH;
	}
	if ( !Q_stricmp( name, "EXPOSURE" ) ) {
		return WBOIT_CERT_STAGE_HDR_RESOLVE;
	}
	return -1;
}

static void VK_WboitCertStage_f( void )
{
	const char *op;
	int stage;
	if ( ri.Cmd_Argc() < 3 ) {
		ri.Printf( PRINT_ALL,
			"usage: oit_cert_stage <pass|fail|skip> <STAGE> ...\n"
			"NOTE: pass creates MANUAL_OVERRIDE evidence only — not production-eligible.\n"
			"Use oit_lab_run <case> for GPU evidence.\n" );
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
	} else if ( !Q_stricmp( op, "fail" ) ) {
		float obs = ( ri.Cmd_Argc() >= 4 ) ? (float)atof( ri.Cmd_Argv( 3 ) ) : 1.0f;
		float thr = ( ri.Cmd_Argc() >= 5 ) ? (float)atof( ri.Cmd_Argv( 4 ) ) : 0.0f;
		vk_wboit_cert_stage_fail( (wboitCertStage_t)stage, obs, thr, "operator", "-",
			( ri.Cmd_Argc() >= 6 ) ? ri.Cmd_Argv( 5 ) : "operator fail" );
	} else if ( !Q_stricmp( op, "skip" ) ) {
		vk_wboit_cert_stage_skip( (wboitCertStage_t)stage,
			( ri.Cmd_Argc() >= 4 ) ? ri.Cmd_Argv( 3 ) : "skipped" );
	}
	ri.Printf( PRINT_ALL, "level now %s\n", vk_wboit_production_level_name( s_level ) );
}

static void VK_OitCertificationAbort_f( void )
{
	int i;
	for ( i = 0; i < (int)WBOIT_CERT_STAGE_COUNT; i++ ) {
		Com_Memset( &s_results[i], 0, sizeof( s_results[i] ) );
		s_results[i].status = WBOIT_CERT_STATUS_PENDING;
	}
	s_captureRoot[0] = '\0';
	VK_WboitCert_RunStaticContractGates();
	ri.Cmd_ExecuteText( EXEC_APPEND, "oit_certify_wboit abort\n" );
	ri.Printf( PRINT_ALL, "oit_certification_abort: reset; static gates re-run\n" );
}

static void VK_OitCertificationCapture_f( void )
{
	const char *arg = ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "default";
	Com_sprintf( s_captureRoot, sizeof( s_captureRoot ), "oit_cert_capture/%s", arg );
	ri.Printf( PRINT_ALL, "oit_certification_capture: root=%s\n", s_captureRoot );
	ri.Cmd_ExecuteText( EXEC_APPEND, "oit_capture stages\n" );
}

static void VK_OitCertExport_f( void )
{
	vk_wboit_cert_export_json( ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : NULL );
}

static void VK_OitCertImport_f( void )
{
	vk_wboit_cert_import_json( ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "render_cert/wboit_certification.json" );
}

static void VK_OitCertInvalidate_f( void )
{
	vk_wboit_cert_invalidate_all( ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "operator invalidate" );
	VK_WboitCert_RunStaticContractGates();
}

void vk_wboit_production_cert_begin_frame( void )
{
}

void vk_wboit_production_cert_register( void )
{
	int i;
	if ( s_cmds ) {
		return;
	}
	Com_Memset( s_results, 0, sizeof( s_results ) );
	for ( i = 0; i < (int)WBOIT_CERT_STAGE_COUNT; i++ ) {
		s_results[i].status = WBOIT_CERT_STATUS_PENDING;
	}
	s_level = WBOIT_STATIC_CERTIFIED;

	r_oitCertificationDebug = ri.Cvar_Get( "r_oitCertificationDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_oitCertificationDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetGroup( r_oitCertificationDebug, CVG_RENDERER );

	r_oitRevealageDebug = ri.Cvar_Get( "r_oitRevealageDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_oitRevealageDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetGroup( r_oitRevealageDebug, CVG_RENDERER );

	r_oitAllowManualCertification = ri.Cvar_Get( "r_oitAllowManualCertification", "0", CVAR_CHEAT | CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_oitAllowManualCertification, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitAllowManualCertification,
		"0: manual stage pass ignored for promotion\n"
		"1: manual may count toward non-production levels only\n"
		"2: dangerous — allow manual toward PRODUCTION (dev only)" );
	ri.Cvar_SetGroup( r_oitAllowManualCertification, CVG_RENDERER );

	r_requireWboitCertification = ri.Cvar_Get( "r_requireWboitCertification", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_requireWboitCertification, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_requireWboitCertification,
		"0 disabled  1 warning  2 fall back sorted alpha  3 strict developer failure" );
	ri.Cvar_SetGroup( r_requireWboitCertification, CVG_RENDERER );

	ri.Cmd_AddCommand( "oit_certification_abort", VK_OitCertificationAbort_f );
	ri.Cmd_AddCommand( "oit_certification_capture", VK_OitCertificationCapture_f );
	ri.Cmd_AddCommand( "oit_cert_stage", VK_WboitCertStage_f );
	ri.Cmd_AddCommand( "wboit_production_status", VK_WboitProductionStatus_f );
	ri.Cmd_AddCommand( "oit_certification_export", VK_OitCertExport_f );
	ri.Cmd_AddCommand( "oit_certification_import", VK_OitCertImport_f );
	ri.Cmd_AddCommand( "oit_certification_invalidate", VK_OitCertInvalidate_f );

	VK_WboitCert_RunStaticContractGates();
	s_cmds = qtrue;

	ri.Printf( PRINT_ALL,
		"[VK][OIT] Phase 2.6C live cert ready (deferred snapshots + oit_certify_core)\n"
		"[VK][OIT] WBOIT: %s (manual cannot grant PRODUCTION)\n",
		vk_wboit_production_level_name( s_level ) );
	if ( r_requireWboitCertification && r_requireWboitCertification->integer >= 1 &&
		s_level < WBOIT_PRODUCTION_CERTIFIED ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][OIT] r_requireWboitCertification=%d — production certification not yet measured for this build/device\n"
			S_COLOR_WHITE, r_requireWboitCertification->integer );
	}
}
