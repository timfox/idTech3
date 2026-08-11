/*
===========================================================================
Renderer IQ Phase 1.5 — evidence-backed P1 certification ladder.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_renderer_p1_cert.h"
#include "vk_bloom_source_contract.h"
#include "vk_scene_hdr_ownership.h"

#include <stdio.h>
#include <string.h>
#include <time.h>


static qboolean s_cmds;
static p1CertStageResult_t s_results[P1_CERT_STAGE_COUNT];
static rendererP1Level_t s_level;
static cvar_t *r_iqAllowManualCertification;
static char s_invalidateReason[192];
static char s_certifiedDevice[128];

const char *vk_renderer_p1_cert_stage_name( p1CertStage_t stage )
{
	switch ( stage ) {
	case P1_CERT_STAGE_STATIC: return "P1_CERT_STATIC";
	case P1_CERT_STAGE_PROFILE: return "P1_CERT_PROFILE";
	case P1_CERT_STAGE_BLOOM_SOURCE: return "P1_CERT_BLOOM_SOURCE";
	case P1_CERT_STAGE_BLOOM_FIREFLY: return "P1_CERT_BLOOM_FIREFLY";
	case P1_CERT_STAGE_BLOOM_PYRAMID: return "P1_CERT_BLOOM_PYRAMID";
	case P1_CERT_STAGE_GBUFFER_QUANT: return "P1_CERT_GBUFFER_QUANT";
	case P1_CERT_STAGE_MATERIAL_DECODE: return "P1_CERT_MATERIAL_DECODE";
	case P1_CERT_STAGE_TEMPORAL_HISTORY: return "P1_CERT_TEMPORAL_HISTORY";
	case P1_CERT_STAGE_VELOCITY: return "P1_CERT_VELOCITY";
	case P1_CERT_STAGE_TEMPORAL_RESET: return "P1_CERT_TEMPORAL_RESET";
	case P1_CERT_STAGE_GHOSTING: return "P1_CERT_GHOSTING";
	case P1_CERT_STAGE_SPECULAR_STABILITY: return "P1_CERT_SPECULAR_STABILITY";
	case P1_CERT_STAGE_NORMAL_MIP: return "P1_CERT_NORMAL_MIP";
	case P1_CERT_STAGE_EDGE: return "P1_CERT_EDGE";
	case P1_CERT_STAGE_SMAA: return "P1_CERT_SMAA";
	case P1_CERT_STAGE_MSAA_POLICY: return "P1_CERT_MSAA_POLICY";
	case P1_CERT_STAGE_TEXTURE_LOD: return "P1_CERT_TEXTURE_LOD";
	case P1_CERT_STAGE_LIGHTING_PARITY: return "P1_CERT_LIGHTING_PARITY";
	case P1_CERT_STAGE_LIGHTING_OWNERSHIP: return "P1_CERT_LIGHTING_OWNERSHIP";
	case P1_CERT_STAGE_CLUSTER_PARITY: return "P1_CERT_CLUSTER_PARITY";
	case P1_CERT_STAGE_LIFECYCLE: return "P1_CERT_LIFECYCLE";
	case P1_CERT_STAGE_SOAK: return "P1_CERT_SOAK";
	default: return "UNKNOWN";
	}
}

const char *vk_renderer_p1_level_name( rendererP1Level_t lvl )
{
	switch ( lvl ) {
	case RENDERER_P1_STATIC_READY: return "RENDERER_P1_STATIC_READY";
	case RENDERER_P1_PROFILE_CERTIFIED: return "RENDERER_P1_PROFILE_CERTIFIED";
	case RENDERER_P1_GPU_CORE_CERTIFIED: return "RENDERER_P1_GPU_CORE_CERTIFIED";
	case RENDERER_P1_TEMPORAL_CERTIFIED: return "RENDERER_P1_TEMPORAL_CERTIFIED";
	case RENDERER_P1_EDGE_CERTIFIED: return "RENDERER_P1_EDGE_CERTIFIED";
	case RENDERER_P1_LIGHTING_PARITY_CERTIFIED: return "RENDERER_P1_LIGHTING_PARITY_CERTIFIED";
	case RENDERER_P1_IMAGE_QUALITY_CERTIFIED: return "RENDERER_P1_IMAGE_QUALITY_CERTIFIED";
	default: return "RENDERER_P1_UNCERTIFIED";
	}
}

const char *vk_renderer_p1_evidence_name( rendererP1Evidence_t e )
{
	switch ( e ) {
	case P1_EVIDENCE_STATIC: return "STATIC";
	case P1_EVIDENCE_GPU_READBACK: return "GPU_READBACK";
	case P1_EVIDENCE_SOAK: return "SOAK";
	case P1_EVIDENCE_PENDING: return "PENDING";
	case P1_EVIDENCE_MANUAL_OVERRIDE: return "MANUAL_OVERRIDE";
	default: return "NONE";
	}
}

const char *vk_renderer_p1_cert_status_name( p1CertStatus_t s )
{
	switch ( s ) {
	case P1_CERT_STATUS_PASS: return "PASS";
	case P1_CERT_STATUS_FAIL: return "FAIL";
	case P1_CERT_STATUS_SKIP: return "SKIP";
	case P1_CERT_STATUS_INVALIDATED: return "INVALIDATED";
	default: return "PENDING";
	}
}

rendererP1Evidence_t vk_renderer_p1_cert_required_evidence( p1CertStage_t stage )
{
	switch ( stage ) {
	case P1_CERT_STAGE_STATIC:
	case P1_CERT_STAGE_PROFILE:
		return P1_EVIDENCE_STATIC;
	case P1_CERT_STAGE_SOAK:
		return P1_EVIDENCE_SOAK;
	case P1_CERT_STAGE_BLOOM_SOURCE:
	case P1_CERT_STAGE_BLOOM_FIREFLY:
	case P1_CERT_STAGE_BLOOM_PYRAMID:
	case P1_CERT_STAGE_GBUFFER_QUANT:
	case P1_CERT_STAGE_MATERIAL_DECODE:
	case P1_CERT_STAGE_TEMPORAL_HISTORY:
	case P1_CERT_STAGE_VELOCITY:
	case P1_CERT_STAGE_TEMPORAL_RESET:
	case P1_CERT_STAGE_GHOSTING:
	case P1_CERT_STAGE_SPECULAR_STABILITY:
	case P1_CERT_STAGE_NORMAL_MIP:
	case P1_CERT_STAGE_EDGE:
	case P1_CERT_STAGE_SMAA:
	case P1_CERT_STAGE_MSAA_POLICY:
	case P1_CERT_STAGE_TEXTURE_LOD:
	case P1_CERT_STAGE_LIGHTING_PARITY:
	case P1_CERT_STAGE_LIGHTING_OWNERSHIP:
	case P1_CERT_STAGE_CLUSTER_PARITY:
	case P1_CERT_STAGE_LIFECYCLE:
		return P1_EVIDENCE_GPU_READBACK;
	default:
		return P1_EVIDENCE_NONE;
	}
}

qboolean vk_renderer_p1_cert_evidence_satisfies( rendererP1Evidence_t have, rendererP1Evidence_t need )
{
	if ( need == P1_EVIDENCE_NONE ) {
		return qtrue;
	}
	if ( have == P1_EVIDENCE_MANUAL_OVERRIDE ) {
		return qfalse;
	}
	if ( need == P1_EVIDENCE_STATIC ) {
		return ( have == P1_EVIDENCE_STATIC || have == P1_EVIDENCE_GPU_READBACK ||
			have == P1_EVIDENCE_SOAK ) ? qtrue : qfalse;
	}
	if ( need == P1_EVIDENCE_GPU_READBACK ) {
		return ( have == P1_EVIDENCE_GPU_READBACK || have == P1_EVIDENCE_SOAK ) ? qtrue : qfalse;
	}
	if ( need == P1_EVIDENCE_SOAK ) {
		return ( have == P1_EVIDENCE_SOAK ) ? qtrue : qfalse;
	}
	return ( have == need ) ? qtrue : qfalse;
}

static qboolean P1_Cert_StagePassOk( p1CertStage_t stage )
{
	const p1CertStageResult_t *r = &s_results[stage];
	rendererP1Evidence_t need;

	if ( r->status != P1_CERT_STATUS_PASS ) {
		return qfalse;
	}
	need = vk_renderer_p1_cert_required_evidence( stage );
	if ( r->evidenceType == P1_EVIDENCE_MANUAL_OVERRIDE ) {
		return ( r_iqAllowManualCertification && r_iqAllowManualCertification->integer > 1 )
			? qtrue : qfalse;
	}
	return vk_renderer_p1_cert_evidence_satisfies( (rendererP1Evidence_t)r->evidenceType, need );
}

static void P1_Cert_RecomputeLevel( void )
{
	qboolean gpuCore, temporal, edge, lighting;

	s_level = RENDERER_P1_UNCERTIFIED;

	if ( !P1_Cert_StagePassOk( P1_CERT_STAGE_STATIC ) ) {
		return;
	}
	s_level = RENDERER_P1_STATIC_READY;

	if ( !P1_Cert_StagePassOk( P1_CERT_STAGE_PROFILE ) ) {
		return;
	}
	s_level = RENDERER_P1_PROFILE_CERTIFIED;

	/* GPU_CORE: bloom source + firefly + pyramid + G-buffer full fidelity */
	gpuCore = P1_Cert_StagePassOk( P1_CERT_STAGE_BLOOM_SOURCE ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_BLOOM_FIREFLY ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_BLOOM_PYRAMID ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_GBUFFER_QUANT );
	if ( !gpuCore ) {
		return;
	}
	s_level = RENDERER_P1_GPU_CORE_CERTIFIED;

	/* TEMPORAL: velocity + history + resets + ghosting + specular */
	temporal = P1_Cert_StagePassOk( P1_CERT_STAGE_TEMPORAL_HISTORY ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_VELOCITY ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_TEMPORAL_RESET ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_GHOSTING ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_SPECULAR_STABILITY );
	if ( !temporal ) {
		return;
	}
	s_level = RENDERER_P1_TEMPORAL_CERTIFIED;

	/* EDGE: native edges + SMAA + MSAA policy + texture LOD */
	edge = P1_Cert_StagePassOk( P1_CERT_STAGE_EDGE ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_SMAA ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_MSAA_POLICY ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_TEXTURE_LOD );
	if ( !edge ) {
		return;
	}
	s_level = RENDERER_P1_EDGE_CERTIFIED;

	/* LIGHTING_PARITY: material decode + deferred/forward + ownership + clusters */
	lighting = P1_Cert_StagePassOk( P1_CERT_STAGE_MATERIAL_DECODE ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_LIGHTING_PARITY ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_LIGHTING_OWNERSHIP ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_CLUSTER_PARITY );
	if ( !lighting ) {
		return;
	}
	s_level = RENDERER_P1_LIGHTING_PARITY_CERTIFIED;

	/* IMAGE_QUALITY: all prior + lifecycle + soak; no manual evidence */
	if ( P1_Cert_StagePassOk( P1_CERT_STAGE_LIFECYCLE ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_SOAK ) &&
		P1_Cert_StagePassOk( P1_CERT_STAGE_NORMAL_MIP ) ) {
		int i;
		qboolean anyManual = qfalse;
		for ( i = 0; i < (int)P1_CERT_STAGE_COUNT; i++ ) {
			if ( s_results[i].status == P1_CERT_STATUS_PASS &&
				s_results[i].evidenceType == P1_EVIDENCE_MANUAL_OVERRIDE ) {
				anyManual = qtrue;
				break;
			}
		}
		if ( !anyManual ) {
			s_level = RENDERER_P1_IMAGE_QUALITY_CERTIFIED;
			Q_strncpyz( s_certifiedDevice, "local-gpu", sizeof( s_certifiedDevice ) );
		}
	}
}

rendererP1Level_t vk_renderer_p1_cert_level( void )
{
	return s_level;
}

rendererP1Level_t vk_renderer_p1_level( void )
{
	return s_level;
}

const p1CertStageResult_t *vk_renderer_p1_cert_stage_result( p1CertStage_t stage )
{
	if ( stage >= P1_CERT_STAGE_COUNT ) {
		return NULL;
	}
	return &s_results[stage];
}

void vk_renderer_p1_cert_record_result( const p1CertStageResult_t *result )
{
	p1CertStageResult_t *dst;
	if ( !result || result->stage >= P1_CERT_STAGE_COUNT ) {
		return;
	}
	dst = &s_results[result->stage];
	*dst = *result;
	if ( !dst->timestamp ) {
		dst->timestamp = (uint64_t)time( NULL );
	}
	if ( !dst->frameNumber ) {
		dst->frameNumber = (uint64_t)tr.frameCount;
	}
	P1_Cert_RecomputeLevel();
}

void vk_renderer_p1_cert_stage_pass( p1CertStage_t stage, float observed, float threshold, const char *notes )
{
	p1CertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = (uint32_t)stage;
	r.status = P1_CERT_STATUS_PASS;
	r.evidenceType = P1_EVIDENCE_MANUAL_OVERRIDE;
	r.observed = observed;
	r.failureThreshold = threshold;
	Q_strncpyz( r.testName, "manual_pass", sizeof( r.testName ) );
	Q_strncpyz( r.failureReason, notes ? notes : "manual override", sizeof( r.failureReason ) );
	vk_renderer_p1_cert_record_result( &r );
	ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
		"iq_cert_stage: MANUAL_OVERRIDE for %s — cannot grant IMAGE_QUALITY_CERTIFIED\n"
		S_COLOR_WHITE, vk_renderer_p1_cert_stage_name( stage ) );
}

void vk_renderer_p1_cert_stage_fail( p1CertStage_t stage, float observed, float threshold, const char *notes )
{
	p1CertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = (uint32_t)stage;
	r.status = P1_CERT_STATUS_FAIL;
	r.evidenceType = P1_EVIDENCE_MANUAL_OVERRIDE;
	r.observed = observed;
	r.failureThreshold = threshold;
	Q_strncpyz( r.testName, "manual_fail", sizeof( r.testName ) );
	Q_strncpyz( r.failureReason, notes ? notes : "fail", sizeof( r.failureReason ) );
	vk_renderer_p1_cert_record_result( &r );
}

void vk_renderer_p1_cert_stage_skip( p1CertStage_t stage, const char *reason )
{
	p1CertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = (uint32_t)stage;
	r.status = P1_CERT_STATUS_SKIP;
	r.evidenceType = P1_EVIDENCE_MANUAL_OVERRIDE;
	Q_strncpyz( r.failureReason, reason ? reason : "skip", sizeof( r.failureReason ) );
	vk_renderer_p1_cert_record_result( &r );
}

void vk_renderer_p1_cert_invalidate_all( const char *reason )
{
	int i;
	Q_strncpyz( s_invalidateReason, reason ? reason : "invalidate", sizeof( s_invalidateReason ) );
	for ( i = 0; i < (int)P1_CERT_STAGE_COUNT; i++ ) {
		s_results[i].status = P1_CERT_STATUS_INVALIDATED;
		s_results[i].evidenceType = P1_EVIDENCE_NONE;
		Q_strncpyz( s_results[i].failureReason, s_invalidateReason, sizeof( s_results[i].failureReason ) );
	}
	s_level = RENDERER_P1_UNCERTIFIED;
	s_certifiedDevice[0] = '\0';
	ri.Printf( PRINT_ALL, "iq_certification_invalidate: %s\n", s_invalidateReason );
}

qboolean vk_renderer_p1_cert_export_json( const char *path )
{
	char buf[32768];
	char outPath[MAX_OSPATH];
	int off = 0;
	int i;

	if ( !path || !path[0] ) {
		path = "render_cert/renderer_iq_p1.json";
	}
	Q_strncpyz( outPath, path, sizeof( outPath ) );

	P1_Cert_RecomputeLevel();
	off += Com_sprintf( buf + off, sizeof( buf ) - off,
		"{\n"
		"  \"schema\": \"renderer_iq_p1\",\n"
		"  \"level\": \"%s\",\n"
		"  \"device\": \"%s\",\n"
		"  \"invalidate\": \"%s\",\n"
		"  \"stages\": [\n",
		vk_renderer_p1_level_name( s_level ),
		s_certifiedDevice[0] ? s_certifiedDevice : "",
		s_invalidateReason[0] ? s_invalidateReason : "" );

	for ( i = 0; i < (int)P1_CERT_STAGE_COUNT; i++ ) {
		const p1CertStageResult_t *r = &s_results[i];
		off += Com_sprintf( buf + off, sizeof( buf ) - off,
			"    {\"stage\":\"%s\",\"status\":\"%s\",\"evidence\":\"%s\","
			"\"observed\":%.6g,\"failThr\":%.6g,\"test\":\"%s\",\"reason\":\"%s\"}%s\n",
			vk_renderer_p1_cert_stage_name( (p1CertStage_t)i ),
			vk_renderer_p1_cert_status_name( (p1CertStatus_t)r->status ),
			vk_renderer_p1_evidence_name( (rendererP1Evidence_t)r->evidenceType ),
			r->observed, r->failureThreshold,
			r->testName[0] ? r->testName : "",
			r->failureReason[0] ? r->failureReason : "",
			( i + 1 < (int)P1_CERT_STAGE_COUNT ) ? "," : "" );
		if ( off > (int)sizeof( buf ) - 512 ) {
			break;
		}
	}
	off += Com_sprintf( buf + off, sizeof( buf ) - off, "  ]\n}\n" );
	ri.FS_WriteFile( outPath, buf, off );
	ri.Printf( PRINT_ALL, "iq_certification_export: wrote %s (%d bytes) level=%s\n",
		outPath, off, vk_renderer_p1_level_name( s_level ) );
	return qtrue;
}

void vk_renderer_p1_cert_refresh_static( void )
{
	p1CertStageResult_t r;
	char err[160];
	qboolean bloomOk, profileOk, msaaOk, lodOk, gbufOk;

	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = P1_CERT_STAGE_STATIC;
	r.evidenceType = P1_EVIDENCE_STATIC;
	Q_strncpyz( r.testName, "static_contracts", sizeof( r.testName ) );

	bloomOk = vk_bloom_source_contract_validate( err, sizeof( err ) );
	msaaOk = !( ri.Cvar_VariableIntegerValue( "r_ext_multisample" ) > 0 &&
		ri.Cvar_VariableIntegerValue( "r_oit" ) >= 1 );
	lodOk = ( atof( ri.Cvar_VariableString( "r_lodBias" ) ) >= -0.001 );
	gbufOk = ( vk_gbuffer_quality_effective() >= 2 &&
		ri.Cvar_VariableIntegerValue( "r_gbufferCompact" ) == 0 );

	if ( bloomOk && msaaOk && lodOk && gbufOk ) {
		r.status = P1_CERT_STATUS_PASS;
		r.observed = 1.0;
		Q_strncpyz( r.failureReason, "bloom+msaa+lod+gbuffer static OK", sizeof( r.failureReason ) );
	} else {
		r.status = P1_CERT_STATUS_FAIL;
		r.observed = 0.0;
		Com_sprintf( r.failureReason, sizeof( r.failureReason ),
			"static fail bloom=%d msaa=%d lod=%d gbuf=%d %s",
			bloomOk, msaaOk, lodOk, gbufOk, err );
	}
	vk_renderer_p1_cert_record_result( &r );

	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = P1_CERT_STAGE_PROFILE;
	r.evidenceType = P1_EVIDENCE_STATIC;
	Q_strncpyz( r.testName, "modern_raster_iq_reference", sizeof( r.testName ) );
	profileOk = vk_renderer_iq_profile_validate( err, sizeof( err ) );
	if ( profileOk ) {
		r.status = P1_CERT_STATUS_PASS;
		r.observed = 1.0;
		Q_strncpyz( r.failureReason, "IQ reference profile validates", sizeof( r.failureReason ) );
	} else {
		r.status = P1_CERT_STATUS_FAIL;
		r.observed = 0.0;
		Q_strncpyz( r.failureReason, err[0] ? err : "profile invalid", sizeof( r.failureReason ) );
	}
	vk_renderer_p1_cert_record_result( &r );
}

static void P1_Cert_Status_f( void )
{
	int i;
	vk_renderer_p1_cert_refresh_static();
	P1_Cert_RecomputeLevel();
	ri.Printf( PRINT_ALL,
		"======== Renderer IQ P1 Live Certification (Phase 1.5) ========\n"
		"level=%s\n"
		"device=%s invalidate=%s\n"
		"policy: MANUAL_OVERRIDE never grants IMAGE_QUALITY_CERTIFIED\n"
		"note: PROFILE alone cannot reach GPU_CORE or higher\n",
		vk_renderer_p1_level_name( s_level ),
		s_certifiedDevice[0] ? s_certifiedDevice : "(none)",
		s_invalidateReason[0] ? s_invalidateReason : "-" );
	for ( i = 0; i < (int)P1_CERT_STAGE_COUNT; i++ ) {
		const p1CertStageResult_t *r = &s_results[i];
		ri.Printf( PRINT_ALL,
			"  [%s] %s evidence=%s (need %s) observed=%g failThr=%g test=%s\n"
			"      reason=%s\n",
			vk_renderer_p1_cert_stage_name( (p1CertStage_t)i ),
			vk_renderer_p1_cert_status_name( (p1CertStatus_t)r->status ),
			vk_renderer_p1_evidence_name( (rendererP1Evidence_t)r->evidenceType ),
			vk_renderer_p1_evidence_name( vk_renderer_p1_cert_required_evidence( (p1CertStage_t)i ) ),
			r->observed, r->failureThreshold,
			r->testName[0] ? r->testName : "-",
			r->failureReason[0] ? r->failureReason : "-" );
	}
	ri.Printf( PRINT_ALL,
		"commands: iq_lab_run | iq_certify_core | iq_certification_export\n"
		"===============================================================\n" );
}

static void P1_Cert_Export_f( void )
{
	const char *path = ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "render_cert/renderer_iq_p1.json";
	vk_renderer_p1_cert_export_json( path );
}

static void P1_Cert_Invalidate_f( void )
{
	vk_renderer_p1_cert_invalidate_all( ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "manual" );
}

void vk_renderer_p1_cert_begin_frame( void )
{
	(void)0;
}

void vk_renderer_p1_cert_register( void )
{
	int i;
	if ( s_cmds ) {
		return;
	}
	s_cmds = qtrue;
	Com_Memset( s_results, 0, sizeof( s_results ) );
	s_level = RENDERER_P1_UNCERTIFIED;
	r_iqAllowManualCertification = ri.Cvar_Get( "r_iqAllowManualCertification", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_iqAllowManualCertification, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_iqAllowManualCertification,
		"0=manual never promotes; 1=warn only; 2=allow manual toward non-final levels" );

	for ( i = 0; i < (int)P1_CERT_STAGE_COUNT; i++ ) {
		s_results[i].stage = (uint32_t)i;
		s_results[i].status = P1_CERT_STATUS_PENDING;
		s_results[i].evidenceType = P1_EVIDENCE_PENDING;
	}

	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "iq_certification_status", P1_Cert_Status_f );
		ri.Cmd_AddCommand( "iq_certification_export", P1_Cert_Export_f );
		ri.Cmd_AddCommand( "iq_certification_invalidate", P1_Cert_Invalidate_f );
	}
	ri.Printf( PRINT_ALL, "[VK][IQ] P1 live certification registered (honest multi-level ladder)\n" );
	vk_renderer_p1_cert_refresh_static();
}

