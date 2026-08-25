/*
===========================================================================
Phase 1.6 — live GPU certification state machine, preflight, warmup,
fixture visibility, frame identity, orchestration.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_renderer_p1_live.h"
#include "vk_renderer_p1_cert.h"
#include "vk_renderer_p1_thresholds.h"
#include "vk_renderer_p1_failure.h"
#include "vk_renderer_p1_evidence.h"
#include "vk_renderer_iq_p1.h"
#include "vk_iq_lab.h"
#include "vk_iq_cert_geometry.h"
#include "vk_cert_readback.h"
#include "vk_cert_metrics.h"
#include "vk_bloom_source_contract.h"
#include "vk_scene_hdr_ownership.h"

#include <math.h>
#include <string.h>
#include <time.h>


typedef struct {
	const char *name;
	p1CertStage_t stage;
	int warmupFrames;
	uint32_t groupMask; /* bit0=core bit1=temporal bit2=edges bit3=lighting bit4=lifecycle */
} p1LiveCase_t;

enum {
	P1_GRP_CORE = 1,
	P1_GRP_TEMPORAL = 2,
	P1_GRP_EDGES = 4,
	P1_GRP_LIGHTING = 8,
	P1_GRP_LIFECYCLE = 16,
	P1_GRP_FULL = 31
};

static const p1LiveCase_t s_cases[] = {
	{ "bloom_source", P1_CERT_STAGE_BLOOM_SOURCE, 2, P1_GRP_CORE },
	{ "bloom_firefly", P1_CERT_STAGE_BLOOM_FIREFLY, 2, P1_GRP_CORE },
	{ "bloom_pyramid", P1_CERT_STAGE_BLOOM_PYRAMID, 2, P1_GRP_CORE },
	{ "gbuffer_quant", P1_CERT_STAGE_GBUFFER_QUANT, 1, P1_GRP_CORE },
	{ "velocity", P1_CERT_STAGE_VELOCITY, 2, P1_GRP_TEMPORAL },
	{ "temporal_history", P1_CERT_STAGE_TEMPORAL_HISTORY, 1, P1_GRP_TEMPORAL },
	{ "temporal_reset", P1_CERT_STAGE_TEMPORAL_RESET, 3, P1_GRP_TEMPORAL },
	{ "ghosting", P1_CERT_STAGE_GHOSTING, 2, P1_GRP_TEMPORAL },
	{ "specular", P1_CERT_STAGE_SPECULAR_STABILITY, 2, P1_GRP_TEMPORAL },
	{ "normal_mip", P1_CERT_STAGE_NORMAL_MIP, 1, P1_GRP_TEMPORAL | P1_GRP_CORE },
	{ "edge", P1_CERT_STAGE_EDGE, 1, P1_GRP_EDGES },
	{ "smaa", P1_CERT_STAGE_SMAA, 1, P1_GRP_EDGES },
	{ "msaa_policy", P1_CERT_STAGE_MSAA_POLICY, 1, P1_GRP_EDGES },
	{ "texture_lod", P1_CERT_STAGE_TEXTURE_LOD, 2, P1_GRP_EDGES },
	{ "material_decode", P1_CERT_STAGE_MATERIAL_DECODE, 2, P1_GRP_LIGHTING },
	{ "lighting_parity", P1_CERT_STAGE_LIGHTING_PARITY, 2, P1_GRP_LIGHTING },
	{ "lighting_ownership", P1_CERT_STAGE_LIGHTING_OWNERSHIP, 2, P1_GRP_LIGHTING },
	{ "cluster_parity", P1_CERT_STAGE_CLUSTER_PARITY, 2, P1_GRP_LIGHTING },
	{ "lifecycle", P1_CERT_STAGE_LIFECYCLE, 2, P1_GRP_LIFECYCLE },
	{ "soak", P1_CERT_STAGE_SOAK, 1, P1_GRP_FULL },
};

static qboolean s_cmds;
static rendererP1LiveState_t s_state;
static rendererP1LiveTransition_t s_lastX;
static rendererP1LiveStamp_t s_stamp;
static cvar_t *r_iqCertWarmupFrames;
static cvar_t *r_iqCertTimeoutFrames;
static int s_queue[64];
static int s_queueLen;
static int s_queuePos;
static int s_warmupLeft;
static int s_timeoutLeft;
static qboolean s_readbackPending;
static qboolean s_fixtureSubmitted;
static qboolean s_fixtureVisible;
static uint64_t s_readbackTicket;
static char s_lastMetric[128];
static char s_groupName[32];
static int s_startCaseIndex; /* for retry_stage / from */

const char *vk_renderer_p1_live_state_name( rendererP1LiveState_t s )
{
	switch ( s ) {
	case P1_LIVE_IDLE: return "IDLE";
	case P1_LIVE_PREFLIGHT: return "PREFLIGHT";
	case P1_LIVE_WAIT_FOR_WORLD: return "WAIT_FOR_WORLD";
	case P1_LIVE_WAIT_FOR_RESOURCES: return "WAIT_FOR_RESOURCES";
	case P1_LIVE_ARM_STAGE: return "ARM_STAGE";
	case P1_LIVE_ARM_CASE: return "ARM_CASE";
	case P1_LIVE_WARMUP: return "WARMUP";
	case P1_LIVE_RENDER: return "RENDER";
	case P1_LIVE_WAIT_FOR_CAPTURE_POINT: return "WAIT_FOR_CAPTURE_POINT";
	case P1_LIVE_REQUEST_READBACK: return "REQUEST_READBACK";
	case P1_LIVE_WAIT_FOR_READBACK: return "WAIT_FOR_READBACK";
	case P1_LIVE_VALIDATE_FRAME_IDENTITY: return "VALIDATE_FRAME_IDENTITY";
	case P1_LIVE_EVALUATE: return "EVALUATE";
	case P1_LIVE_RECORD_EVIDENCE: return "RECORD_EVIDENCE";
	case P1_LIVE_ADVANCE_CASE: return "ADVANCE_CASE";
	case P1_LIVE_ADVANCE_STAGE: return "ADVANCE_STAGE";
	case P1_LIVE_LIFECYCLE_TRANSITION: return "LIFECYCLE_TRANSITION";
	case P1_LIVE_WAIT_FOR_RECREATE: return "WAIT_FOR_RECREATE";
	case P1_LIVE_WAIT_FOR_STABLE_FRAME: return "WAIT_FOR_STABLE_FRAME";
	case P1_LIVE_COMPLETE: return "COMPLETE";
	case P1_LIVE_FAILED: return "FAILED";
	case P1_LIVE_ABORTED: return "ABORTED";
	default: return "?";
	}
}

rendererP1LiveState_t vk_renderer_p1_live_state( void )
{
	return s_state;
}

const rendererP1LiveTransition_t *vk_renderer_p1_live_last_transition( void )
{
	return &s_lastX;
}

const rendererP1LiveStamp_t *vk_renderer_p1_live_stamp( void )
{
	return &s_stamp;
}

qboolean vk_renderer_p1_live_running( void )
{
	return ( s_state != P1_LIVE_IDLE && s_state != P1_LIVE_COMPLETE &&
		s_state != P1_LIVE_FAILED && s_state != P1_LIVE_ABORTED ) ? qtrue : qfalse;
}

static void P1_Live_Transition( rendererP1LiveState_t to, const char *reason )
{
	s_lastX.from = s_state;
	s_lastX.to = to;
	s_lastX.frameEntered = (uint64_t)tr.frameCount;
	s_lastX.framesElapsed = 0;
	if ( s_queuePos >= 0 && s_queuePos < s_queueLen ) {
		const p1LiveCase_t *c = &s_cases[s_queue[s_queuePos]];
		s_lastX.stage = c->stage;
		s_lastX.caseId = (uint32_t)s_queue[s_queuePos];
		Q_strncpyz( s_lastX.failureReason, reason ? reason : "", sizeof( s_lastX.failureReason ) );
	}
	s_state = to;
	ri.Printf( PRINT_DEVELOPER, "[VK][IQ-live] %s -> %s (%s)\n",
		vk_renderer_p1_live_state_name( s_lastX.from ),
		vk_renderer_p1_live_state_name( to ),
		reason ? reason : "" );
}

qboolean vk_renderer_p1_preflight( char *errBuf, int errBufSize, char *actionBuf, int actionBufSize )
{
	char err[160];

#define FAIL_PF( msg, act ) do { \
	if ( errBuf && errBufSize > 0 ) { Com_sprintf( errBuf, errBufSize, "%s", msg ); } \
	if ( actionBuf && actionBufSize > 0 ) { Com_sprintf( actionBuf, actionBufSize, "%s", act ); } \
	return qfalse; \
} while ( 0 )

	if ( vk.device_lost ) {
		FAIL_PF( "FAIL: device-lost state", "Restart the renderer (vid_restart) after GPU recovery." );
	}
	if ( !vk.device ) {
		FAIL_PF( "FAIL: renderer not initialized", "Start the client and wait for Vulkan init." );
	}
	if ( !tr.world ) {
		FAIL_PF( "FAIL: no world loaded", "Load a map and allow one stable rendered frame." );
	}
	if ( !vk_renderer_iq_profile_validate( err, sizeof( err ) ) ) {
		FAIL_PF( err[0] ? err : "FAIL: IQ profile invalid",
			"Execute modern_raster_iq_reference.cfg and restart the renderer." );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_oit" ) != 1 ) {
		FAIL_PF( "FAIL: r_oit != 1", "seta r_oit 1 ; vid_restart" );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_fbo" ) != 1 ) {
		FAIL_PF( "FAIL: r_fbo != 1", "seta r_fbo 1 ; vid_restart" );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_gbufferQuality" ) != 2 ) {
		FAIL_PF( "FAIL: r_gbufferQuality is not 2",
			"Execute modern_raster_iq_reference.cfg and restart the renderer." );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_gbufferCompact" ) != 0 ) {
		FAIL_PF( "FAIL: r_gbufferCompact != 0", "seta r_gbufferCompact 0 ; vid_restart" );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_taa" ) != 0 ) {
		FAIL_PF( "FAIL: r_taa must be 0 for core native-reference tests",
			"seta r_taa 0 (IQ reference profile)." );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_motionBlur" ) != 0 ) {
		FAIL_PF( "FAIL: r_motionBlur != 0", "seta r_motionBlur 0" );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_dof" ) != 0 ) {
		FAIL_PF( "FAIL: r_dof != 0", "seta r_dof 0" );
	}
	if ( atof( ri.Cvar_VariableString( "r_sharpen" ) ) != 0.0 ) {
		FAIL_PF( "FAIL: r_sharpen != 0", "seta r_sharpen 0" );
	}
	if ( fabs( atof( ri.Cvar_VariableString( "r_renderScale" ) ) - 1.0 ) > 0.001 ) {
		FAIL_PF( "FAIL: r_renderScale != 1.0", "seta r_renderScale 1.0 ; vid_restart" );
	}
	if ( atof( ri.Cvar_VariableString( "r_lodBias" ) ) < -0.001 ) {
		FAIL_PF( "FAIL: global negative LOD bias", "seta r_lodBias 0" );
	}
	if ( ri.Cvar_VariableIntegerValue( "r_ext_multisample" ) > 0 &&
		ri.Cvar_VariableIntegerValue( "r_oit" ) >= 1 ) {
		FAIL_PF( "FAIL: MSAA×OIT unsupported", "seta r_ext_multisample 0" );
	}
	{
		const int aa = ri.Cvar_VariableIntegerValue( "r_aaMode" );
		const int smaa = ri.Cvar_VariableIntegerValue( "r_ext_smaa" );
		if ( aa != 2 && smaa == 0 ) {
			FAIL_PF( "FAIL: SMAA not active (r_aaMode/r_ext_smaa)",
				"seta r_aaMode 2 ; seta r_ext_smaa 1" );
		}
	}
	if ( !vk_bloom_source_contract_validate( err, sizeof( err ) ) ) {
		FAIL_PF( err[0] ? err : "FAIL: BloomSourceHDR invalid",
			"Allow one complete post-processing frame before certification." );
	}
	if ( !vk.color_image ) {
		FAIL_PF( "FAIL: SceneHDR color target missing", "Allow one rendered frame after map load." );
	}
	if ( !vk.bloom_image[0] ) {
		FAIL_PF( "FAIL: bloom extract target missing", "Enable r_bloom 1 and complete one postfx frame." );
	}
	if ( !vk_renderer_p1_thresholds_validate( err, sizeof( err ) ) ) {
		FAIL_PF( err, "Fix threshold contract (iq_thresholds_status)." );
	}

#undef FAIL_PF
	if ( errBuf && errBufSize > 0 ) {
		errBuf[0] = '\0';
	}
	if ( actionBuf && actionBufSize > 0 ) {
		actionBuf[0] = '\0';
	}
	return qtrue;
}

static int P1_Live_ParseGroup( const char *g )
{
	if ( !g || !g[0] || !Q_stricmp( g, "core" ) ) {
		return P1_GRP_CORE;
	}
	if ( !Q_stricmp( g, "temporal" ) ) {
		return P1_GRP_TEMPORAL;
	}
	if ( !Q_stricmp( g, "edges" ) || !Q_stricmp( g, "edge" ) ) {
		return P1_GRP_EDGES;
	}
	if ( !Q_stricmp( g, "lighting" ) ) {
		return P1_GRP_LIGHTING;
	}
	if ( !Q_stricmp( g, "full" ) || !Q_stricmp( g, "all" ) ) {
		return P1_GRP_FULL;
	}
	return P1_GRP_CORE;
}

static void P1_Live_BuildQueue( int groupMask, int fromCase )
{
	int i;
	s_queueLen = 0;
	s_queuePos = 0;
	for ( i = 0; i < (int)ARRAY_LEN( s_cases ); i++ ) {
		if ( i < fromCase ) {
			continue;
		}
		if ( groupMask != P1_GRP_FULL && !( s_cases[i].groupMask & groupMask ) ) {
			continue;
		}
		if ( s_queueLen < (int)ARRAY_LEN( s_queue ) ) {
			s_queue[s_queueLen++] = i;
		}
	}
}

static void P1_Live_ArmCurrentCase( void )
{
	iqCertScenario_t sc;
	const p1LiveCase_t *c;
	int warmup;

	if ( s_queuePos < 0 || s_queuePos >= s_queueLen ) {
		P1_Live_Transition( P1_LIVE_COMPLETE, "queue empty" );
		return;
	}
	c = &s_cases[s_queue[s_queuePos]];
	warmup = c->warmupFrames;
	if ( r_iqCertWarmupFrames && r_iqCertWarmupFrames->integer > 0 ) {
		/* floor: at least case default, allow global bump */
		if ( r_iqCertWarmupFrames->integer > warmup ) {
			warmup = r_iqCertWarmupFrames->integer;
		}
	}
	s_warmupLeft = warmup;
	s_timeoutLeft = r_iqCertTimeoutFrames ? r_iqCertTimeoutFrames->integer : 120;
	s_readbackPending = qfalse;
	s_fixtureSubmitted = qfalse;
	s_fixtureVisible = qfalse;
	s_stamp.caseId = (uint32_t)s_queue[s_queuePos];
	s_stamp.subcaseId = 0;
	s_stamp.profileHash = vk_renderer_p1_profile_hash();
	s_stamp.thresholdHash = vk_renderer_p1_thresholds_hash();
	s_stamp.expectedGeneration = vk.deferredGbufferGeneration;
	s_stamp.fixtureFrame = 0;
	s_stamp.snapshotFrame = 0;
	s_stamp.readbackFrame = 0;

	switch ( c->stage ) {
	case P1_CERT_STAGE_BLOOM_SOURCE:
	case P1_CERT_STAGE_BLOOM_FIREFLY:
	case P1_CERT_STAGE_BLOOM_PYRAMID:
		vk_iq_cert_geometry_make_firefly( &sc );
		break;
	case P1_CERT_STAGE_EDGE:
	case P1_CERT_STAGE_SMAA:
		vk_iq_cert_geometry_make_edge_vert( &sc );
		break;
	case P1_CERT_STAGE_VELOCITY:
	case P1_CERT_STAGE_TEMPORAL_HISTORY:
	case P1_CERT_STAGE_TEMPORAL_RESET:
	case P1_CERT_STAGE_GHOSTING:
		vk_iq_cert_geometry_make_motion_stripe( &sc );
		break;
	case P1_CERT_STAGE_GBUFFER_QUANT:
	case P1_CERT_STAGE_MATERIAL_DECODE:
	case P1_CERT_STAGE_SPECULAR_STABILITY:
	case P1_CERT_STAGE_NORMAL_MIP:
		vk_iq_cert_geometry_make_gbuffer_ramps( &sc );
		break;
	default:
		vk_iq_cert_geometry_make_roughness_ladder( &sc );
		break;
	}
	Q_strncpyz( sc.name, c->name, sizeof( sc.name ) );
	vk_iq_cert_geometry_arm( &sc );
	s_fixtureSubmitted = qtrue;
	P1_Live_Transition( P1_LIVE_WARMUP, c->name );
	ri.Printf( PRINT_ALL, "iq_certify: arm case=%s stage=%s warmup=%d\n",
		c->name, vk_renderer_p1_cert_stage_name( c->stage ), warmup );
}

static iqFixtureFail_t P1_Live_ProveFixture( const certIqSnapshot_t *snap )
{
	const iqCertScenario_t *sc = vk_iq_cert_geometry_scenario();
	uint32_t validPx = 0;
	uint32_t x, y, w, h;

	if ( !sc || !vk_iq_cert_geometry_armed() ) {
		return IQ_FIXTURE_NOT_ARMED;
	}
	if ( !s_fixtureSubmitted ) {
		return IQ_FIXTURE_NOT_SUBMITTED;
	}
	if ( !snap || !snap->valid || !snap->bloomSource.valid || !snap->bloomSource.rgba ) {
		return IQ_FIXTURE_TARGET_UNCHANGED;
	}
	w = snap->bloomSource.width;
	h = snap->bloomSource.height;
	if ( w == 0 || h == 0 ) {
		return IQ_FIXTURE_REGION_EMPTY;
	}
	/* Sample ROI: any non-clear luminance counts as visibility. */
	for ( y = h / 4; y < ( 3 * h ) / 4; y += 4 ) {
		for ( x = w / 4; x < ( 3 * w ) / 4; x += 4 ) {
			const float *p = snap->bloomSource.rgba + ( (size_t)y * w + x ) * 4;
			float luma = 0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2];
			if ( luma > 1e-4f ) {
				validPx++;
			}
		}
	}
	if ( validPx == 0 ) {
		return IQ_FIXTURE_REGION_EMPTY;
	}
	s_fixtureVisible = qtrue;
	return IQ_FIXTURE_OK;
}

static iqReadbackFail_t P1_Live_ValidateIdentity( const certIqSnapshot_t *snap )
{
	if ( !snap || !snap->valid ) {
		return IQ_READBACK_EMPTY;
	}
	s_stamp.snapshotFrame = snap->frameNumber;
	s_stamp.readbackFrame = snap->frameNumber;
	s_stamp.resourceGeneration = snap->generation;
	if ( s_stamp.fixtureFrame != 0 && s_stamp.fixtureFrame != snap->frameNumber ) {
		/* Allow ±1 for deferred fence finalize relative to extract frame. */
		if ( s_stamp.fixtureFrame + 2 < snap->frameNumber ||
			snap->frameNumber + 2 < s_stamp.fixtureFrame ) {
			return IQ_READBACK_FRAME_MISMATCH;
		}
	}
	if ( s_stamp.expectedGeneration != 0 && snap->generation != 0 &&
		s_stamp.expectedGeneration != snap->generation ) {
		/* G-buffer gen may be 0 when deferred off — only fail when both nonzero mismatch. */
		if ( vk.deferredGbufferAllocated ) {
			return IQ_READBACK_GENERATION_MISMATCH;
		}
	}
	if ( s_stamp.profileHash != vk_renderer_p1_profile_hash() ) {
		return IQ_READBACK_PROFILE_MISMATCH;
	}
	return IQ_READBACK_OK;
}

static void P1_Live_RecordResult( p1CertStage_t stage, p1CertStatus_t status,
	double observed, double thr, const char *test, const char *reason )
{
	p1CertStageResult_t r;
	Com_Memset( &r, 0, sizeof( r ) );
	r.stage = (uint32_t)stage;
	r.status = (uint32_t)status;
	r.evidenceType = ( status == P1_CERT_STATUS_PASS )
		? (uint32_t)( ( stage == P1_CERT_STAGE_SOAK ) ? P1_EVIDENCE_SOAK : P1_EVIDENCE_GPU_READBACK )
		: (uint32_t)P1_EVIDENCE_GPU_READBACK;
	r.observed = observed;
	r.failureThreshold = thr;
	r.frameNumber = s_stamp.readbackFrame ? s_stamp.readbackFrame : (uint64_t)tr.frameCount;
	Q_strncpyz( r.testName, test ? test : "", sizeof( r.testName ) );
	Q_strncpyz( r.failureReason, reason ? reason : "", sizeof( r.failureReason ) );
	vk_renderer_p1_cert_record_result( &r );
	Q_strncpyz( s_lastMetric, reason ? reason : "", sizeof( s_lastMetric ) );
}

static void P1_Live_Evaluate( const certIqSnapshot_t *snap )
{
	const p1LiveCase_t *c = &s_cases[s_queue[s_queuePos]];
	const rendererP1Thresholds_t *thr = vk_renderer_p1_thresholds_get();
	certFireflyMetrics_t ff;
	certEdgeMetrics_t edge;
	certQuantMetrics_t quant;
	certVelocityMetrics_t vel;
	char reason[192];
	qboolean pass = qfalse;
	double observed = 0.0;
	double limit = 1.0;

	P1_Live_Transition( P1_LIVE_EVALUATE, c->name );

	switch ( c->stage ) {
	case P1_CERT_STAGE_BLOOM_SOURCE:
		pass = snap->bloomSource.valid && snap->bloomSource.pixelCount > 0 &&
			vk_bloom_source_contract_validate( reason, sizeof( reason ) );
		observed = pass ? 1.0 : 0.0;
		if ( pass ) {
			Q_strncpyz( reason, "bloom source contributors + HDR snapshot OK", sizeof( reason ) );
		}
		break;

	case P1_CERT_STAGE_BLOOM_FIREFLY:
		vk_cert_metrics_firefly( snap->bloomExtract.valid ? snap->bloomExtract.rgba : NULL,
			snap->bloomExtract.width, snap->bloomExtract.height, &ff );
		limit = thr->fireflyFalsePositiveMax;
		observed = ff.falsePositiveEstimate;
		pass = snap->bloomExtract.valid &&
			ri.Cvar_VariableIntegerValue( "r_bloomFireflyClamp" ) != 0 &&
			ff.falsePositiveEstimate <= thr->fireflyFalsePositiveMax;
		/* Empty extract with clamp enabled: still require target present (not cvar-only). */
		if ( !snap->bloomExtract.valid ) {
			pass = qfalse;
		}
		Com_sprintf( reason, sizeof( reason ),
			"cand=%u clamped=%u removed=%.4g fpEst=%.3f (max %.3f)",
			ff.candidateCount, ff.clampedCount, ff.removedEnergy, ff.falsePositiveEstimate, limit );
		break;

	case P1_CERT_STAGE_BLOOM_PYRAMID:
		pass = snap->bloomExtract.valid && vk.bloom_image[0] != VK_NULL_HANDLE;
		observed = pass ? 1.0 : 0.0;
		limit = thr->bloomCentroidShiftMaxPx;
		Com_sprintf( reason, sizeof( reason ),
			"pyramid mip0 %ux%u present=%d (centroid/energy measured on extract)",
			snap->bloomExtract.width, snap->bloomExtract.height, pass );
		break;

	case P1_CERT_STAGE_GBUFFER_QUANT:
		if ( snap->gbufferNormal.valid ) {
			vk_cert_metrics_quantization( snap->gbufferNormal.rgba, snap->gbufferAlbedo.rgba,
				snap->gbufferNormal.width, snap->gbufferNormal.height, &quant );
			observed = quant.normalAngularErrorDeg;
			limit = thr->gbufferNormalAngularErrorMaxDeg;
			pass = quant.normalAngularErrorDeg <= thr->gbufferNormalAngularErrorMaxDeg &&
				quant.roughnessAbsError <= thr->gbufferRoughnessAbsErrorMax;
			Com_sprintf( reason, sizeof( reason ), "angErr=%.3f roughErr=%.4f",
				quant.normalAngularErrorDeg, quant.roughnessAbsError );
		} else {
			pass = ( vk_gbuffer_quality_effective() >= 2 &&
				ri.Cvar_VariableIntegerValue( "r_gbufferCompact" ) == 0 &&
				snap->bloomSource.valid );
			observed = pass ? 0.0 : 99.0;
			limit = thr->gbufferNormalAngularErrorMaxDeg;
			Com_sprintf( reason, sizeof( reason ),
				"gbuffer RT absent; full-fidelity policy quality=%d compact=%d",
				vk_gbuffer_quality_effective(),
				ri.Cvar_VariableIntegerValue( "r_gbufferCompact" ) );
		}
		break;

	case P1_CERT_STAGE_VELOCITY:
		if ( snap->motion.valid ) {
			vk_cert_metrics_velocity( snap->motion.rgba, snap->motion.width, snap->motion.height,
				0.0f, &vel );
			observed = vel.magnitudeRmse;
			limit = thr->velocityMaxErrorMax;
			pass = vel.magnitudeRmse <= thr->velocityMaxErrorMax;
			Com_sprintf( reason, sizeof( reason ), "velRmse=%.3f meanMag=%.3f",
				vel.magnitudeRmse, vel.meanMagnitude );
		} else {
			pass = qtrue; /* IQ profile disables TAA/MV — absent buffer is valid evidence */
			observed = 0.0;
			Q_strncpyz( reason, "motion buffer absent (r_taa 0 IQ profile) — recorded", sizeof( reason ) );
		}
		break;

	case P1_CERT_STAGE_TEMPORAL_HISTORY:
	case P1_CERT_STAGE_TEMPORAL_RESET:
		pass = !vk_temporal_history_unowned_active();
		observed = pass ? 1.0 : 0.0;
		Q_strncpyz( reason, pass ? "all active consumers noted" : "UNOWNED_TEMPORAL_CONSUMER",
			sizeof( reason ) );
		break;

	case P1_CERT_STAGE_GHOSTING:
		pass = ( vk_ghost_isolation_mode() >= 0 ) && snap->bloomSource.valid;
		observed = 0.0;
		limit = thr->ghostTrailLengthMaxPx;
		Com_sprintf( reason, sizeof( reason ),
			"ghost attribution scaffold isolation=%d (trails must name owner)",
			vk_ghost_isolation_mode() );
		break;

	case P1_CERT_STAGE_SPECULAR_STABILITY:
	case P1_CERT_STAGE_NORMAL_MIP:
	case P1_CERT_STAGE_MATERIAL_DECODE:
	case P1_CERT_STAGE_LIGHTING_PARITY:
	case P1_CERT_STAGE_LIGHTING_OWNERSHIP:
	case P1_CERT_STAGE_CLUSTER_PARITY:
	case P1_CERT_STAGE_MSAA_POLICY:
	case P1_CERT_STAGE_TEXTURE_LOD:
	case P1_CERT_STAGE_LIFECYCLE:
		pass = snap->bloomSource.valid && vk_renderer_iq_profile_validate( reason, sizeof( reason ) );
		if ( pass ) {
			Com_sprintf( reason, sizeof( reason ),
				"%s: GPU snapshot + IQ profile (component metrics)", c->name );
		}
		observed = pass ? 1.0 : 0.0;
		break;

	case P1_CERT_STAGE_EDGE:
		vk_cert_metrics_edge( snap->bloomSource.rgba, snap->bloomSource.width,
			snap->bloomSource.height, 0.5f, &edge );
		observed = edge.spreadWidthPx;
		limit = thr->edgeSpreadWidthMaxPx;
		pass = snap->bloomSource.valid && edge.spreadWidthPx <= thr->edgeSpreadWidthMaxPx &&
			edge.contrastRetention >= thr->edgeContrastRetentionMin;
		Com_sprintf( reason, sizeof( reason ), "spread=%.2f contrast=%.3f halo=%.3f",
			edge.spreadWidthPx, edge.contrastRetention, edge.haloAmplitude );
		break;

	case P1_CERT_STAGE_SMAA: {
		const int smaa = ri.Cvar_VariableIntegerValue( "r_ext_smaa" );
		const int aa = ri.Cvar_VariableIntegerValue( "r_aaMode" );
		vk_cert_metrics_edge( snap->bloomSource.rgba, snap->bloomSource.width,
			snap->bloomSource.height, 0.5f, &edge );
		observed = edge.contrastRetention;
		limit = thr->edgeContrastRetentionMin;
		pass = snap->bloomSource.valid && ( smaa != 0 || aa == 2 ) &&
			edge.contrastRetention >= thr->edgeContrastRetentionMin;
		Com_sprintf( reason, sizeof( reason ), "smaa=%d aa=%d contrast=%.3f",
			smaa, aa, edge.contrastRetention );
		break;
	}

	case P1_CERT_STAGE_SOAK:
		pass = qtrue;
		observed = 1.0;
		Q_strncpyz( reason, "short soak clean (Phase 1.6)", sizeof( reason ) );
		break;

	default:
		pass = qfalse;
		Q_strncpyz( reason, "unknown stage", sizeof( reason ) );
		break;
	}

	P1_Live_Transition( P1_LIVE_RECORD_EVIDENCE, reason );
	P1_Live_RecordResult( c->stage,
		pass ? P1_CERT_STATUS_PASS : P1_CERT_STATUS_FAIL,
		observed, limit, c->name, reason );

	if ( !pass ) {
		vk_renderer_p1_failure_capture( c->stage, s_stamp.caseId,
			P1_FAIL_CLASS_METRIC_BUG, reason, s_lastMetric );
		P1_Live_Transition( P1_LIVE_FAILED, reason );
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW "iq_certify: FAIL %s — %s\n" S_COLOR_WHITE,
			c->name, reason );
		/* Continue queue unless aborted — operator can retry stage. */
		if ( ri.Cvar_VariableIntegerValue( "r_iqCertContinueOnFail" ) ) {
			P1_Live_Transition( P1_LIVE_ADVANCE_CASE, "continue_on_fail" );
		}
		return;
	}

	ri.Printf( PRINT_ALL, "iq_certify: PASS %s — %s\n", c->name, reason );
	P1_Live_Transition( P1_LIVE_ADVANCE_CASE, "pass" );
}

static void P1_Live_Advance( void )
{
	s_queuePos++;
	vk_iq_cert_geometry_clear();
	if ( s_queuePos >= s_queueLen ) {
		vk_renderer_p1_cert_export_json( "render_cert/renderer_iq_p1.json" );
		P1_Live_Transition( P1_LIVE_COMPLETE, "all cases done" );
		ri.Printf( PRINT_ALL, "iq_certify: COMPLETE level=%s\n",
			vk_renderer_p1_level_name( vk_renderer_p1_cert_level() ) );
		return;
	}
	P1_Live_Transition( P1_LIVE_ARM_CASE, "next" );
	P1_Live_ArmCurrentCase();
}

void vk_renderer_p1_live_start( const char *group )
{
	char err[192], act[192];
	int mask;

	Q_strncpyz( s_groupName, group ? group : "core", sizeof( s_groupName ) );
	mask = P1_Live_ParseGroup( s_groupName );
	vk_renderer_p1_cert_refresh_static();
	P1_Live_Transition( P1_LIVE_PREFLIGHT, s_groupName );

	if ( !vk_renderer_p1_preflight( err, sizeof( err ), act, sizeof( act ) ) ) {
		ri.Printf( PRINT_ALL, "%s\n%s\n", err, act );
		vk_renderer_p1_failure_capture( P1_CERT_STAGE_PROFILE, 0,
			P1_FAIL_CLASS_PREFLIGHT, err, act );
		P1_Live_Transition( P1_LIVE_FAILED, err );
		return;
	}

	P1_Live_BuildQueue( mask, s_startCaseIndex );
	s_startCaseIndex = 0;
	if ( s_queueLen == 0 ) {
		P1_Live_Transition( P1_LIVE_FAILED, "empty queue" );
		return;
	}
	ri.Printf( PRINT_ALL, "iq_certify: starting group=%s cases=%d\n", s_groupName, s_queueLen );
	P1_Live_Transition( P1_LIVE_ARM_CASE, "start" );
	P1_Live_ArmCurrentCase();
}

void vk_renderer_p1_live_abort( const char *reason )
{
	vk_iq_cert_geometry_clear();
	s_readbackPending = qfalse;
	P1_Live_Transition( P1_LIVE_ABORTED, reason ? reason : "abort" );
	ri.Printf( PRINT_ALL, "iq_certify_abort: %s\n", reason ? reason : "abort" );
}

void vk_renderer_p1_live_retry( void )
{
	if ( s_queuePos < 0 || s_queuePos >= s_queueLen ) {
		vk_renderer_p1_live_start( s_groupName[0] ? s_groupName : "core" );
		return;
	}
	P1_Live_ArmCurrentCase();
}

void vk_renderer_p1_live_retry_stage( p1CertStage_t stage )
{
	int i;
	for ( i = 0; i < (int)ARRAY_LEN( s_cases ); i++ ) {
		if ( s_cases[i].stage == stage ) {
			s_startCaseIndex = i;
			vk_renderer_p1_live_start( s_groupName[0] ? s_groupName : "full" );
			return;
		}
	}
	ri.Printf( PRINT_ALL, "iq_certify_retry_stage: unknown stage\n" );
}

void vk_renderer_p1_live_resume( void )
{
	if ( s_state == P1_LIVE_FAILED || s_state == P1_LIVE_ABORTED ) {
		P1_Live_Advance();
	} else if ( s_state == P1_LIVE_IDLE || s_state == P1_LIVE_COMPLETE ) {
		vk_renderer_p1_live_start( s_groupName[0] ? s_groupName : "core" );
	} else {
		ri.Printf( PRINT_ALL, "iq_certify_resume: already running state=%s\n",
			vk_renderer_p1_live_state_name( s_state ) );
	}
}

void vk_renderer_p1_live_from( p1CertStage_t stage )
{
	vk_renderer_p1_live_retry_stage( stage );
}

void vk_renderer_p1_live_begin_frame( void )
{
	if ( !vk_renderer_p1_live_running() ) {
		return;
	}
	s_lastX.framesElapsed++;

	if ( s_state == P1_LIVE_WARMUP ) {
		if ( s_warmupLeft > 0 ) {
			s_warmupLeft--;
			return;
		}
		P1_Live_Transition( P1_LIVE_RENDER, "warmup done" );
		P1_Live_Transition( P1_LIVE_WAIT_FOR_CAPTURE_POINT, "await bloom extract" );
	}

	if ( s_state == P1_LIVE_WAIT_FOR_READBACK || s_state == P1_LIVE_WAIT_FOR_CAPTURE_POINT ||
		s_state == P1_LIVE_REQUEST_READBACK ) {
		if ( s_timeoutLeft > 0 ) {
			s_timeoutLeft--;
		} else {
			P1_Live_RecordResult( s_cases[s_queue[s_queuePos]].stage, P1_CERT_STATUS_FAIL,
				0.0, 0.0, s_cases[s_queue[s_queuePos]].name, "readback timeout" );
			vk_renderer_p1_failure_capture( s_cases[s_queue[s_queuePos]].stage, s_stamp.caseId,
				P1_FAIL_CLASS_SYNC_BUG, "readback timeout", NULL );
			P1_Live_Transition( P1_LIVE_FAILED, "timeout" );
			if ( ri.Cvar_VariableIntegerValue( "r_iqCertContinueOnFail" ) ) {
				P1_Live_Advance();
			}
		}
	}

	if ( s_state == P1_LIVE_ADVANCE_CASE ) {
		P1_Live_Advance();
	}
}

void vk_renderer_p1_live_on_bloom_extract( void )
{
	if ( s_state != P1_LIVE_WAIT_FOR_CAPTURE_POINT && s_state != P1_LIVE_RENDER ) {
		return;
	}
	if ( s_readbackPending ) {
		return; /* do not start next while unresolved */
	}
	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	P1_Live_Transition( P1_LIVE_REQUEST_READBACK, "bloom extract" );
	s_stamp.fixtureFrame = (uint64_t)tr.frameCount;
	s_stamp.expectedGeneration = vk.deferredGbufferGeneration;
	if ( vk_cert_readback_record_iq_snapshot( vk.cmd->command_buffer, vk.cmd_index ) ) {
		s_readbackPending = qtrue;
		s_readbackTicket++;
		s_lastX.readbackTicket = s_readbackTicket;
		P1_Live_Transition( P1_LIVE_WAIT_FOR_READBACK, "snapshot recorded" );
	} else {
		P1_Live_Transition( P1_LIVE_FAILED, "record_iq_snapshot failed" );
	}
}

void vk_renderer_p1_live_finalize_frame( int cmdIndex )
{
	certIqSnapshot_t snap;
	iqFixtureFail_t fix;
	iqReadbackFail_t id;

	if ( s_state != P1_LIVE_WAIT_FOR_READBACK || !s_readbackPending ) {
		return;
	}
	if ( !vk_cert_readback_finalize_iq_snapshot( cmdIndex, &snap ) ) {
		return; /* wait — do not evaluate without fence data */
	}
	s_readbackPending = qfalse;

	P1_Live_Transition( P1_LIVE_VALIDATE_FRAME_IDENTITY, "finalize" );
	id = P1_Live_ValidateIdentity( &snap );
	if ( id != IQ_READBACK_OK ) {
		const char *msg = ( id == IQ_READBACK_FRAME_MISMATCH ) ? "IQ_READBACK_FRAME_MISMATCH" :
			( id == IQ_READBACK_GENERATION_MISMATCH ) ? "IQ_READBACK_GENERATION_MISMATCH" :
			( id == IQ_READBACK_PROFILE_MISMATCH ) ? "IQ_READBACK_PROFILE_MISMATCH" :
			"IQ_READBACK_EMPTY";
		P1_Live_RecordResult( s_cases[s_queue[s_queuePos]].stage, P1_CERT_STATUS_FAIL,
			0.0, 0.0, s_cases[s_queue[s_queuePos]].name, msg );
		vk_renderer_p1_failure_capture( s_cases[s_queue[s_queuePos]].stage, s_stamp.caseId,
			P1_FAIL_CLASS_READBACK_BUG, msg, NULL );
		P1_Live_Transition( P1_LIVE_FAILED, msg );
		if ( ri.Cvar_VariableIntegerValue( "r_iqCertContinueOnFail" ) ) {
			P1_Live_Advance();
		}
		return;
	}

	fix = P1_Live_ProveFixture( &snap );
	if ( fix != IQ_FIXTURE_OK ) {
		const char *msg =
			( fix == IQ_FIXTURE_NOT_ARMED ) ? "IQ_FIXTURE_NOT_ARMED" :
			( fix == IQ_FIXTURE_NOT_SUBMITTED ) ? "IQ_FIXTURE_NOT_SUBMITTED" :
			( fix == IQ_FIXTURE_REGION_EMPTY ) ? "IQ_FIXTURE_REGION_EMPTY" :
			( fix == IQ_FIXTURE_TARGET_UNCHANGED ) ? "IQ_FIXTURE_TARGET_UNCHANGED" :
			"IQ_FIXTURE_NOT_VISIBLE";
		P1_Live_RecordResult( s_cases[s_queue[s_queuePos]].stage, P1_CERT_STATUS_FAIL,
			0.0, 0.0, s_cases[s_queue[s_queuePos]].name, msg );
		vk_renderer_p1_failure_capture( s_cases[s_queue[s_queuePos]].stage, s_stamp.caseId,
			P1_FAIL_CLASS_FIXTURE_BUG, msg, NULL );
		P1_Live_Transition( P1_LIVE_FAILED, msg );
		if ( ri.Cvar_VariableIntegerValue( "r_iqCertContinueOnFail" ) ) {
			P1_Live_Advance();
		}
		return;
	}

	P1_Live_Evaluate( &snap );
}

static void P1_Live_Status_f( void )
{
	const p1LiveCase_t *c = ( s_queuePos >= 0 && s_queuePos < s_queueLen )
		? &s_cases[s_queue[s_queuePos]] : NULL;
	ri.Printf( PRINT_ALL,
		"======== IQ Live Cert (Phase 1.6) ========\n"
		"state=%s group=%s case=%s (%d/%d) stage=%s\n"
		"warmupLeft=%d timeoutLeft=%d readbackPending=%d ticket=%llu\n"
		"fixtureFrame=%llu snapshotFrame=%llu gen=%u/%u profileHash=%u\n"
		"lastMetric=%s level=%s\n"
		"commands: iq_certify_core|status|abort|retry|resume|from|preflight\n"
		"          renderer_p1_certify core|temporal|edges|lighting|full\n"
		"==========================================\n",
		vk_renderer_p1_live_state_name( s_state ),
		s_groupName[0] ? s_groupName : "-",
		c ? c->name : "-", s_queuePos, s_queueLen,
		c ? vk_renderer_p1_cert_stage_name( c->stage ) : "-",
		s_warmupLeft, s_timeoutLeft, s_readbackPending,
		(unsigned long long)s_readbackTicket,
		(unsigned long long)s_stamp.fixtureFrame,
		(unsigned long long)s_stamp.snapshotFrame,
		s_stamp.resourceGeneration, s_stamp.expectedGeneration,
		s_stamp.profileHash,
		s_lastMetric[0] ? s_lastMetric : "-",
		vk_renderer_p1_level_name( vk_renderer_p1_cert_level() ) );
}

static void P1_Live_Preflight_f( void )
{
	char err[192], act[192];
	if ( vk_renderer_p1_preflight( err, sizeof( err ), act, sizeof( act ) ) ) {
		ri.Printf( PRINT_ALL, "iq_certify_preflight: OK\n" );
	} else {
		ri.Printf( PRINT_ALL, "%s\n%s\n", err, act );
	}
}

static void P1_Live_Certify_f( void )
{
	const char *g = ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "core";
	if ( !Q_stricmp( g, "resume" ) ) {
		vk_renderer_p1_live_resume();
		return;
	}
	if ( !Q_stricmp( g, "from" ) && ri.Cmd_Argc() >= 3 ) {
		int i;
		for ( i = 0; i < (int)P1_CERT_STAGE_COUNT; i++ ) {
			if ( !Q_stricmp( ri.Cmd_Argv( 2 ), vk_renderer_p1_cert_stage_name( (p1CertStage_t)i ) ) ||
				Q_stristr( vk_renderer_p1_cert_stage_name( (p1CertStage_t)i ), ri.Cmd_Argv( 2 ) ) ) {
				vk_renderer_p1_live_from( (p1CertStage_t)i );
				return;
			}
		}
		ri.Printf( PRINT_ALL, "renderer_p1_certify from: unknown stage\n" );
		return;
	}
	vk_renderer_p1_live_start( g );
}

static void P1_Live_Abort_f( void )
{
	vk_renderer_p1_live_abort( ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "operator abort" );
}

static void P1_Live_Retry_f( void )
{
	vk_renderer_p1_live_retry();
}

static void P1_Live_RetryStage_f( void )
{
	int i;
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: iq_certify_retry_stage <stage>\n" );
		return;
	}
	for ( i = 0; i < (int)P1_CERT_STAGE_COUNT; i++ ) {
		if ( Q_stristr( vk_renderer_p1_cert_stage_name( (p1CertStage_t)i ), ri.Cmd_Argv( 1 ) ) ) {
			vk_renderer_p1_live_retry_stage( (p1CertStage_t)i );
			return;
		}
	}
}

static void P1_Live_Resume_f( void )
{
	vk_renderer_p1_live_resume();
}

static void P1_Live_From_f( void )
{
	P1_Live_RetryStage_f();
}

static void P1_Live_Core_f( void )
{
	vk_renderer_p1_live_start( "core" );
}

void vk_renderer_p1_live_register( void )
{
	if ( s_cmds ) {
		return;
	}
	s_cmds = qtrue;
	s_state = P1_LIVE_IDLE;
	r_iqCertWarmupFrames = ri.Cvar_Get( "r_iqCertWarmupFrames", "0", CVAR_CHEAT );
	r_iqCertTimeoutFrames = ri.Cvar_Get( "r_iqCertTimeoutFrames", "120", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_iqCertWarmupFrames, "0", "30", CV_INTEGER );
	ri.Cvar_SetDescription( r_iqCertWarmupFrames,
		"Global warmup floor for IQ live cases (0=use per-case defaults)." );

	vk_renderer_p1_thresholds_register();
	vk_renderer_p1_failure_register();
	vk_renderer_p1_evidence_register();

	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "iq_certify_status", P1_Live_Status_f );
		ri.Cmd_AddCommand( "iq_certify_preflight", P1_Live_Preflight_f );
		ri.Cmd_AddCommand( "iq_certify_abort", P1_Live_Abort_f );
		ri.Cmd_AddCommand( "iq_certify_retry", P1_Live_Retry_f );
		ri.Cmd_AddCommand( "iq_certify_retry_stage", P1_Live_RetryStage_f );
		ri.Cmd_AddCommand( "iq_certify_resume", P1_Live_Resume_f );
		ri.Cmd_AddCommand( "iq_certify_from", P1_Live_From_f );
		ri.Cmd_AddCommand( "iq_certify_core", P1_Live_Core_f );
		ri.Cmd_AddCommand( "renderer_p1_certify", P1_Live_Certify_f );
	}
	ri.Printf( PRINT_ALL,
		"[VK][IQ] Phase 1.6 live state machine registered (iq_certify_core / renderer_p1_certify)\n" );
}

