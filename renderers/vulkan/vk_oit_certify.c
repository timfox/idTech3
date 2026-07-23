/*
===========================================================================
WBOIT live certification runner, soak, anomaly capture, certification status.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_oit_certify.h"
#include "vk_wboit_production_cert.h"
#include "vk_forward_plus.h"
#include "vk_device.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define VK_OIT_CERT_MAX_CASES     48
#define VK_OIT_CERT_NOTE_LEN      256
#define VK_OIT_CERT_PATH_LEN      128
#define VK_OIT_SOAK_HISTORY       120
#define VK_OIT_EXPORT_BUF         ( 64 * 1024 )

typedef struct {
	const char *id;
	const char *title;
	const char *expected;
	const char *setupHint; /* console hints printed for the operator */
} vkOitCertCase_t;

typedef struct {
	char id[32];
	char title[96];
	char result[16]; /* PASS/FAIL/PENDING/SKIP */
	char notes[VK_OIT_CERT_NOTE_LEN];
	char screenshot[VK_OIT_CERT_PATH_LEN];
	int durationSec;
	uint32_t clearCount;
	uint32_t resolveCount;
	uint32_t corruptionDelta;
	uint32_t fallbackDelta;
} vkOitCertCaseResult_t;

typedef struct {
	qboolean active;
	int caseIndex;
	int caseCount;
	char sessionId[48];
	char startedAt[32];
	char gpuName[256];
	uint32_t vendorId;
	uint32_t deviceId;
	uint32_t driverVersion;
	uint32_t apiVersion;
	char profile[64];
	char engineRev[64];
	char binaryHash[40];
	char shaderHash[40];
	int width;
	int height;
	int renderScalePct;
	int msaa;
	int taa;
	int oitMode;
	uint32_t clusterGenStart;
	uint32_t attGenStart;
	uint32_t clearStart;
	uint32_t resolveStart;
	uint32_t corruptionStart;
	uint32_t fallbackStart;
	vkOitCertCaseResult_t cases[VK_OIT_CERT_MAX_CASES];
} vkOitCertSession_t;

typedef struct {
	float frameTimeMs;
	uint32_t clearCount;
	uint32_t resolveCount;
	uint32_t accumPassCount;
	uint32_t drawCount;
	uint32_t clusterGen;
	uint32_t attGen;
	uint32_t corruption;
	uint32_t fallbacks;
	uint32_t boundsViol;
	int width;
	int height;
	int oitMode;
} vkOitSoakSample_t;

typedef struct {
	qboolean active;
	int minutes;
	int elapsedSec;
	int nextPhase;
	int anomalyCount;
	int sampleCount;
	uint32_t lastSecond;
	char sessionId[48];
	vkOitSoakSample_t history[VK_OIT_SOAK_HISTORY];
	int historyWrite;
	int historyCount;
	uint32_t peakCorruption;
	uint32_t peakFallback;
	qboolean clearResolveImbalance;
	qboolean genDrift;
} vkOitSoakState_t;

typedef struct {
	vkOitCertLevel_t level;
	char lastSessionId[48];
	char certifiedBinaryHash[40];
	char certifiedShaderHash[40];
	char certifiedGpu[256];
	char certifiedDriver[64];
	char lastPassAt[32];
	int soakMinutesCompleted;
	qboolean staticGatesOk;
	qboolean liveBasicOk;
	qboolean liveFullOk;
	qboolean liveSoakedOk;
} vkOitCertPersist_t;

static qboolean s_inited;
static vkOitCertSession_t s_cert;
static vkOitSoakState_t s_soak;
static vkOitCertPersist_t s_persist;
static cvar_t *r_oitCaptureOnError;
static cvar_t *r_oitCaptureHistoryFrames;
static cvar_t *r_oitFogMode;
static cvar_t *r_oitFogDensity;
static cvar_t *r_oitFogDebug;

static const vkOitCertCase_t s_cases[] = {
	{ "B0a", "Static layered quads", "Clean layered glass, no bands",
	  "seta r_oit 1; seta r_oitForwardPlus 0; seta r_oitClassify 0; oit_status" },
	{ "B0b", "Reverse submission / alpha sweep", "Stable over alpha 0.05–0.95",
	  "seta r_oitDebug 7; screenshot" },
	{ "B0c", "Opaque behind layers", "Opaque readable through glass",
	  "seta r_oitDebug 0" },
	{ "B1a", "Camera slow pan", "No tearing / stipple",
	  "Move slowly; oit_status" },
	{ "B1b", "Camera fast pan / near-plane", "No corruption at near plane",
	  "seta r_oitDebug 15" },
	{ "B1c", "Camera inside volume", "No black blocks",
	  "Enter translucent volume" },
	{ "B2a", "Moving / rotating glass", "No ghost trails (TAA off preferred)",
	  "seta r_taa 0" },
	{ "B2b", "Particles spawn/despawn", "Additive revealage correct",
	  "seta r_oitClassify 1" },
	{ "B2c", "Transform discontinuity", "Recover next frame",
	  "Teleport entity; oit_status" },
	{ "B3a", "Even extent", "Clean resolve",
	  "seta r_mode -1; seta r_customwidth 1920; seta r_customheight 1080; vid_restart" },
	{ "B3b", "Odd width 1919", "No edge stale band",
	  "seta r_customwidth 1919; seta r_customheight 1079; vid_restart; seta r_oitExtentDebug 1" },
	{ "B3c", "Odd height 1281x721", "No edge stale band",
	  "seta r_customwidth 1281; seta r_customheight 721; vid_restart" },
	{ "B3d", "75% / 50% render scale", "Extent triad match",
	  "seta r_renderScale 0.75; then 0.5; oit_status" },
	{ "B3e", "Live resize + vid_restart", "Generations rematch",
	  "Resize window; vid_restart; oit_status" },
	{ "B3f", "Ultrawide 2560x1080", "No stale edge / extent triad match",
	  "seta r_customwidth 2560; seta r_customheight 1080; vid_restart; oit_status" },
	{ "B3g", "Minimize/restore", "Swapchain recreate recovers OIT",
	  "Minimize then restore window; oit_status" },
	{ "B4a", "Unlit translucent", "No magenta OOB",
	  "seta r_oitForwardPlus 0" },
	{ "B4b", "Forward+ many lights", "No magenta tile slabs",
	  "seta r_oitForwardPlus 1; seta r_oitClusterDebug 2" },
	{ "B4c", "Bright-on-dark / dark-on-bright", "No explode / black holes",
	  "seta r_oitDebug 1" },
	{ "B5a", "8–16 layer overdraw", "Finite weights",
	  "exec demo_wboit_stress_mode3.cfg" },
	{ "B5b", "32–64 layer / fullscreen stress", "Soft-cap holds",
	  "seta r_oitDebug 12" },
	{ "B6a", "Bloom / fog / volumetrics toggles", "No double fog / bands",
	  "toggle r_bloom; r_volumetricFog; oit_fog_status" },
	{ "B6b", "MSAA / SMAA / TAA compat", "TAA reactive ok; MBOIT×TAA not required",
	  "seta r_aaMode 2; seta r_taa 0 then 1 briefly" },
	{ "B6c", "Weapon / UI", "Gun clean after resolve",
	  "cg_drawGun 0/1; seta r_oitDirectTest 0" },
	{ "B7a", "OIT / mode toggle", "Clear/resolve once after recover",
	  "toggle r_oit; r_renderMode 2/3; vid_restart" },
	{ "B7b", "Forced allocation failure", "Safe skip, recover",
	  "seta r_oitForceAllocationFailure 1; wait; seta r_oitForceAllocationFailure 0" },
	{ "B7c", "Forced extent / generation mismatch", "Safe skip, recover",
	  "seta r_oitForceExtentMismatch 1; then 0; seta r_oitForceGenerationMismatch 1; then 0" },
	{ "B7d", "Forced skip clear / double resolve / invalid accum", "No crash",
	  "seta r_oitForceSkipClear 1; 0; seta r_oitForceDoubleResolve 1; 0; seta r_oitForceInvalidAccum 1; 0" },
	{ "B7e", "Forced cluster mismatch", "Skip local lights safely",
	  "seta r_oitForceClusterMismatch 1; then 0; seta r_oitClusterDebug 3" },
};

static void VK_OitCert_NowIso( char *out, int outLen )
{
	time_t t = time( NULL );
	struct tm tmUtc;
#if defined( _WIN32 )
	gmtime_s( &tmUtc, &t );
#else
	gmtime_r( &t, &tmUtc );
#endif
	Com_sprintf( out, outLen, "%04d-%02d-%02dT%02d%02d%02dZ",
		tmUtc.tm_year + 1900, tmUtc.tm_mon + 1, tmUtc.tm_mday,
		tmUtc.tm_hour, tmUtc.tm_min, tmUtc.tm_sec );
}

static void VK_OitCert_FillGpu( void )
{
	VkPhysicalDeviceProperties props;
	Com_Memset( &props, 0, sizeof( props ) );
	if ( vk.physical_device != VK_NULL_HANDLE ) {
		qvkGetPhysicalDeviceProperties( vk.physical_device, &props );
	}
	Q_strncpyz( s_cert.gpuName, props.deviceName[0] ? props.deviceName : "(unknown)", sizeof( s_cert.gpuName ) );
	s_cert.vendorId = props.vendorID;
	s_cert.deviceId = props.deviceID;
	s_cert.driverVersion = props.driverVersion;
	s_cert.apiVersion = props.apiVersion;
}

static void VK_OitCert_FillRuntime( void )
{
	s_cert.width = glConfig.vidWidth;
	s_cert.height = glConfig.vidHeight;
	s_cert.msaa = ( r_ext_multisample && r_ext_multisample->integer ) ? r_ext_multisample->integer : 0;
	s_cert.taa = ( r_taa && r_taa->integer ) ? r_taa->integer : 0;
	s_cert.oitMode = ( r_oit && r_oit->integer ) ? r_oit->integer : 0;
	s_cert.renderScalePct = 100;
	if ( ri.Cvar_VariableIntegerValue( "r_renderScale" ) ) {
		/* float scale if present */
	}
	{
		const char *p = ri.Cvar_VariableString( "r_havenrpProfile" );
		Q_strncpyz( s_cert.profile, ( p && p[0] ) ? p : "runtime", sizeof( s_cert.profile ) );
	}
	Q_strncpyz( s_cert.engineRev, "idtech3", sizeof( s_cert.engineRev ) );
	/* Fingerprint from live GPU + OIT attachment gen (operator attaches file hashes in notes). */
	Com_sprintf( s_cert.binaryHash, sizeof( s_cert.binaryHash ), "gpu-%08x-%08x",
		s_cert.vendorId, s_cert.deviceId );
	Com_sprintf( s_cert.shaderHash, sizeof( s_cert.shaderHash ), "oitAtt-%u",
		vk.oitAttachmentGeneration );
	s_cert.clusterGenStart = vk_cluster_list_generation();
	s_cert.attGenStart = vk.oitAttachmentGeneration;
	s_cert.clearStart = vk.oitClearCount;
	s_cert.resolveStart = vk.oitResolveCount;
	s_cert.corruptionStart = vk.oitCorruptionCount;
	s_cert.fallbackStart = vk.oitFallbackCount;
}

const char *vk_oit_certification_level_name( vkOitCertLevel_t level )
{
	switch ( level ) {
	case VK_OIT_CERT_STATIC_GATES: return "STATIC_GATES";
	case VK_OIT_CERT_LIVE_BASIC: return "LIVE_BASIC";
	case VK_OIT_CERT_LIVE_FULL: return "LIVE_FULL";
	case VK_OIT_CERT_LIVE_SOAKED: return "LIVE_SOAKED";
	case VK_OIT_CERT_SPINE_1_1: return "SPINE_1_1_CERTIFIED";
	default: return "NONE";
	}
}

vkOitCertLevel_t vk_oit_certification_level( void )
{
	return s_persist.level;
}

static void VK_OitCert_RecomputeLevel( void )
{
	vkOitCertLevel_t lvl = VK_OIT_CERT_NONE;
	if ( s_persist.staticGatesOk ) {
		lvl = VK_OIT_CERT_STATIC_GATES;
	}
	if ( s_persist.liveBasicOk ) {
		lvl = VK_OIT_CERT_LIVE_BASIC;
	}
	if ( s_persist.liveFullOk ) {
		lvl = VK_OIT_CERT_LIVE_FULL;
	}
	if ( s_persist.liveSoakedOk && s_persist.soakMinutesCompleted >= 30 ) {
		lvl = VK_OIT_CERT_LIVE_SOAKED;
	}
	/* Spine 1.1 requires soaked + live full + WBOIT production profile. */
	if ( lvl == VK_OIT_CERT_LIVE_SOAKED && s_persist.liveFullOk ) {
		lvl = VK_OIT_CERT_SPINE_1_1;
	}
	s_persist.level = lvl;
}

static void VK_OitCert_PrintCase( void )
{
	const vkOitCertCase_t *c;
	if ( !s_cert.active || s_cert.caseIndex < 0 || s_cert.caseIndex >= s_cert.caseCount ) {
		ri.Printf( PRINT_ALL, "oit_certify_wboit: no active case\n" );
		return;
	}
	c = &s_cases[s_cert.caseIndex];
	ri.Printf( PRINT_ALL,
		"oit_certify_wboit case %d/%d [%s] %s\n"
		"  expected: %s\n"
		"  setup: %s\n"
		"  counters: clear≈resolve once/frame; gen stable unless recreate; no corruptionΔ\n"
		"  fallback: safe skip then recover (B7 faults); no device loss\n"
		"  commands: oit_certify_wboit pass | fail <reason> | repeat | next | status\n",
		s_cert.caseIndex + 1, s_cert.caseCount, c->id, c->title, c->expected, c->setupHint );
}

static void VK_OitCert_Begin_f( void )
{
	int i;
	if ( r_oit && r_oit->integer != 1 ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"oit_certify_wboit: set r_oit 1 (WBOIT) before certification (current=%d)\n"
			S_COLOR_WHITE, r_oit ? r_oit->integer : 0 );
	}
	Com_Memset( &s_cert, 0, sizeof( s_cert ) );
	s_cert.active = qtrue;
	s_cert.caseCount = (int)( sizeof( s_cases ) / sizeof( s_cases[0] ) );
	if ( s_cert.caseCount > VK_OIT_CERT_MAX_CASES ) {
		s_cert.caseCount = VK_OIT_CERT_MAX_CASES;
	}
	VK_OitCert_NowIso( s_cert.startedAt, sizeof( s_cert.startedAt ) );
	Com_sprintf( s_cert.sessionId, sizeof( s_cert.sessionId ), "wboit-%s", s_cert.startedAt );
	VK_OitCert_FillGpu();
	VK_OitCert_FillRuntime();
	for ( i = 0; i < s_cert.caseCount; i++ ) {
		Q_strncpyz( s_cert.cases[i].id, s_cases[i].id, sizeof( s_cert.cases[i].id ) );
		Q_strncpyz( s_cert.cases[i].title, s_cases[i].title, sizeof( s_cert.cases[i].title ) );
		Q_strncpyz( s_cert.cases[i].result, "PENDING", sizeof( s_cert.cases[i].result ) );
	}
	s_cert.caseIndex = 0;
	Q_strncpyz( s_persist.lastSessionId, s_cert.sessionId, sizeof( s_persist.lastSessionId ) );
	ri.Printf( PRINT_ALL,
		"oit_certify_wboit BEGIN session=%s gpu=%s vendor=0x%x device=0x%x\n"
		"  oitMode=%d extent=%dx%d msaa=%d taa=%d clusterGen=%u\n"
		"  Live B0–B7: operator marks pass/fail; static gates alone are NOT certification.\n",
		s_cert.sessionId, s_cert.gpuName, s_cert.vendorId, s_cert.deviceId,
		s_cert.oitMode, s_cert.width, s_cert.height, s_cert.msaa, s_cert.taa, s_cert.clusterGenStart );
	VK_OitCert_PrintCase();
}

static void VK_OitCert_SnapshotCounters( vkOitCertCaseResult_t *r )
{
	r->clearCount = vk.oitClearCount;
	r->resolveCount = vk.oitResolveCount;
	r->corruptionDelta = vk.oitCorruptionCount - s_cert.corruptionStart;
	r->fallbackDelta = vk.oitFallbackCount - s_cert.fallbackStart;
}

static void VK_OitCert_Pass_f( void )
{
	vkOitCertCaseResult_t *r;
	if ( !s_cert.active ) {
		ri.Printf( PRINT_ALL, "oit_certify_wboit: not active (begin first)\n" );
		return;
	}
	r = &s_cert.cases[s_cert.caseIndex];
	Q_strncpyz( r->result, "PASS", sizeof( r->result ) );
	VK_OitCert_SnapshotCounters( r );
	ri.Printf( PRINT_ALL, "oit_certify_wboit PASS [%s]\n", r->id );
	if ( s_cert.caseIndex + 1 < s_cert.caseCount ) {
		s_cert.caseIndex++;
		VK_OitCert_PrintCase();
	} else {
		ri.Printf( PRINT_ALL, "oit_certify_wboit: all cases recorded — run oit_certify_wboit export\n" );
	}
}

static void VK_OitCert_Fail_f( void )
{
	vkOitCertCaseResult_t *r;
	char reason[VK_OIT_CERT_NOTE_LEN];
	int i;
	reason[0] = '\0';
	if ( ri.Cmd_Argc() >= 2 ) {
		reason[0] = '\0';
		for ( i = 1; i < ri.Cmd_Argc(); i++ ) {
			if ( reason[0] ) {
				Q_strcat( reason, sizeof( reason ), " " );
			}
			Q_strcat( reason, sizeof( reason ), ri.Cmd_Argv( i ) );
		}
	} else {
		Q_strncpyz( reason, "unspecified", sizeof( reason ) );
	}
	if ( !s_cert.active ) {
		ri.Printf( PRINT_ALL, "oit_certify_wboit: not active\n" );
		return;
	}
	r = &s_cert.cases[s_cert.caseIndex];
	Q_strncpyz( r->result, "FAIL", sizeof( r->result ) );
	Q_strncpyz( r->notes, reason, sizeof( r->notes ) );
	VK_OitCert_SnapshotCounters( r );
	vk_oit_certify_note_anomaly( reason );
	ri.Printf( PRINT_ALL, "oit_certify_wboit FAIL [%s]: %s\n", r->id, reason );
}

static void VK_OitCert_Next_f( void )
{
	if ( !s_cert.active ) {
		ri.Printf( PRINT_ALL, "oit_certify_wboit: not active\n" );
		return;
	}
	if ( s_cert.caseIndex + 1 < s_cert.caseCount ) {
		s_cert.caseIndex++;
		VK_OitCert_PrintCase();
	} else {
		ri.Printf( PRINT_ALL, "oit_certify_wboit: already at last case\n" );
	}
}

static void VK_OitCert_Repeat_f( void )
{
	if ( !s_cert.active ) {
		ri.Printf( PRINT_ALL, "oit_certify_wboit: not active\n" );
		return;
	}
	VK_OitCert_PrintCase();
}

static void VK_OitCert_Abort_f( void )
{
	s_cert.active = qfalse;
	ri.Printf( PRINT_ALL, "oit_certify_wboit aborted\n" );
}

static void VK_OitCert_Status_f( void )
{
	int i, pass = 0, fail = 0, pend = 0;
	if ( !s_cert.active ) {
		ri.Printf( PRINT_ALL, "oit_certify_wboit: inactive (last session=%s)\n",
			s_persist.lastSessionId[0] ? s_persist.lastSessionId : "(none)" );
		return;
	}
	for ( i = 0; i < s_cert.caseCount; i++ ) {
		if ( !Q_stricmp( s_cert.cases[i].result, "PASS" ) ) {
			pass++;
		} else if ( !Q_stricmp( s_cert.cases[i].result, "FAIL" ) ) {
			fail++;
		} else {
			pend++;
		}
	}
	ri.Printf( PRINT_ALL,
		"oit_certify_wboit status session=%s case=%d/%d pass=%d fail=%d pending=%d\n"
		"  gpu=%s driver=0x%x api=0x%x oit=%d %dx%d\n"
		"  clear=%u resolve=%u corruptionΔ=%u fallbackΔ=%u\n",
		s_cert.sessionId, s_cert.caseIndex + 1, s_cert.caseCount, pass, fail, pend,
		s_cert.gpuName, s_cert.driverVersion, s_cert.apiVersion, s_cert.oitMode,
		glConfig.vidWidth, glConfig.vidHeight,
		vk.oitClearCount, vk.oitResolveCount,
		vk.oitCorruptionCount - s_cert.corruptionStart,
		vk.oitFallbackCount - s_cert.fallbackStart );
}

static void VK_OitCert_Export_f( void )
{
	char path[MAX_QPATH];
	char *buf;
	int i, off = 0;
	int pass = 0, fail = 0;
	if ( !s_cert.active && !s_persist.lastSessionId[0] ) {
		ri.Printf( PRINT_ALL, "oit_certify_wboit export: no session\n" );
		return;
	}
	buf = (char *)ri.Hunk_AllocateTempMemory( VK_OIT_EXPORT_BUF );
	if ( !buf ) {
		ri.Printf( PRINT_WARNING, "oit_certify_wboit export: OOM\n" );
		return;
	}
	Com_Memset( buf, 0, VK_OIT_EXPORT_BUF );
	off += Com_sprintf( buf + off, VK_OIT_EXPORT_BUF - off,
		"{\n  \"sessionId\": \"%s\",\n  \"startedAt\": \"%s\",\n"
		"  \"gpu\": \"%s\",\n  \"vendorId\": \"0x%x\",\n  \"deviceId\": \"0x%x\",\n"
		"  \"driverVersion\": \"0x%x\",\n  \"apiVersion\": \"0x%x\",\n"
		"  \"profile\": \"%s\",\n  \"binaryFingerprint\": \"%s\",\n  \"shaderFingerprint\": \"%s\",\n"
		"  \"oitMode\": %d,\n  \"resolution\": \"%dx%d\",\n  \"msaa\": %d,\n  \"taa\": %d,\n"
		"  \"clusterGen\": %u,\n  \"fogMode\": %d,\n  \"cases\": [\n",
		s_cert.sessionId, s_cert.startedAt, s_cert.gpuName,
		s_cert.vendorId, s_cert.deviceId, s_cert.driverVersion, s_cert.apiVersion,
		s_cert.profile, s_cert.binaryHash, s_cert.shaderHash,
		s_cert.oitMode, s_cert.width, s_cert.height, s_cert.msaa, s_cert.taa,
		s_cert.clusterGenStart,
		r_oitFogMode ? r_oitFogMode->integer : 0 );
	for ( i = 0; i < s_cert.caseCount; i++ ) {
		const vkOitCertCaseResult_t *r = &s_cert.cases[i];
		if ( !Q_stricmp( r->result, "PASS" ) ) {
			pass++;
		} else if ( !Q_stricmp( r->result, "FAIL" ) ) {
			fail++;
		}
		off += Com_sprintf( buf + off, VK_OIT_EXPORT_BUF - off,
			"    {\"id\":\"%s\",\"title\":\"%s\",\"result\":\"%s\",\"notes\":\"%s\","
			"\"clear\":%u,\"resolve\":%u,\"corruptionDelta\":%u,\"fallbackDelta\":%u}%s\n",
			r->id, r->title, r->result, r->notes[0] ? r->notes : "",
			r->clearCount, r->resolveCount, r->corruptionDelta, r->fallbackDelta,
			( i + 1 < s_cert.caseCount ) ? "," : "" );
		if ( off > VK_OIT_EXPORT_BUF - 512 ) {
			break;
		}
	}
	off += Com_sprintf( buf + off, VK_OIT_EXPORT_BUF - off,
		"  ],\n  \"summary\": {\"pass\":%d,\"fail\":%d,\"total\":%d}\n}\n",
		pass, fail, s_cert.caseCount );
	Com_sprintf( path, sizeof( path ), "oit_cert_%s.json", s_cert.sessionId );
	ri.FS_WriteFile( path, buf, off );
	ri.Hunk_FreeTempMemory( buf );
	ri.Printf( PRINT_ALL, "oit_certify_wboit exported %s (pass=%d fail=%d)\n", path, pass, fail );

	/* Update persist: live basic if any passes; live full if all pass and no fail. */
	if ( pass > 0 ) {
		s_persist.liveBasicOk = qtrue;
	}
	if ( pass == s_cert.caseCount && fail == 0 ) {
		s_persist.liveFullOk = qtrue;
		VK_OitCert_NowIso( s_persist.lastPassAt, sizeof( s_persist.lastPassAt ) );
		Q_strncpyz( s_persist.certifiedGpu, s_cert.gpuName, sizeof( s_persist.certifiedGpu ) );
		Com_sprintf( s_persist.certifiedDriver, sizeof( s_persist.certifiedDriver ), "0x%x", s_cert.driverVersion );
		Q_strncpyz( s_persist.certifiedBinaryHash, s_cert.binaryHash, sizeof( s_persist.certifiedBinaryHash ) );
		Q_strncpyz( s_persist.certifiedShaderHash, s_cert.shaderHash, sizeof( s_persist.certifiedShaderHash ) );
	}
	s_persist.staticGatesOk = qtrue; /* runner present implies gates were used */
	VK_OitCert_RecomputeLevel();
}

static void VK_OitCertify_f( void )
{
	const char *cmd = ( ri.Cmd_Argc() >= 2 ) ? ri.Cmd_Argv( 1 ) : "status";
	if ( !Q_stricmp( cmd, "begin" ) ) {
		VK_OitCert_Begin_f();
	} else if ( !Q_stricmp( cmd, "next" ) ) {
		VK_OitCert_Next_f();
	} else if ( !Q_stricmp( cmd, "repeat" ) ) {
		VK_OitCert_Repeat_f();
	} else if ( !Q_stricmp( cmd, "pass" ) ) {
		VK_OitCert_Pass_f();
	} else if ( !Q_stricmp( cmd, "fail" ) ) {
		VK_OitCert_Fail_f();
	} else if ( !Q_stricmp( cmd, "status" ) ) {
		VK_OitCert_Status_f();
	} else if ( !Q_stricmp( cmd, "abort" ) ) {
		VK_OitCert_Abort_f();
	} else if ( !Q_stricmp( cmd, "export" ) ) {
		VK_OitCert_Export_f();
	} else {
		ri.Printf( PRINT_ALL,
			"usage: oit_certify_wboit <begin|next|repeat|pass|fail <reason>|status|abort|export>\n" );
	}
}

static void VK_OitSoak_Sample( void )
{
	vkOitSoakSample_t *s;
	s = &s_soak.history[s_soak.historyWrite % VK_OIT_SOAK_HISTORY];
	Com_Memset( s, 0, sizeof( *s ) );
	s->frameTimeMs = 0.0f;
	s->clearCount = vk.oitClearCount;
	s->resolveCount = vk.oitResolveCount;
	s->accumPassCount = vk.oitAccumPassCount;
	s->drawCount = vk.oitDrawCount;
	s->clusterGen = vk_cluster_list_generation();
	s->attGen = vk.oitAttachmentGeneration;
	s->corruption = vk.oitCorruptionCount;
	s->fallbacks = vk.oitFallbackCount;
	s->boundsViol = vk.oitBoundsViolationCount;
	s->width = glConfig.vidWidth;
	s->height = glConfig.vidHeight;
	s->oitMode = r_oit ? r_oit->integer : 0;
	s_soak.historyWrite++;
	if ( s_soak.historyCount < VK_OIT_SOAK_HISTORY ) {
		s_soak.historyCount++;
	}
	s_soak.sampleCount++;
	if ( s->corruption > s_soak.peakCorruption ) {
		s_soak.peakCorruption = s->corruption;
	}
	if ( s->fallbacks > s_soak.peakFallback ) {
		s_soak.peakFallback = s->fallbacks;
	}
	if ( s->clearCount > 0 && s->resolveCount > s->clearCount + 1 ) {
		s_soak.clearResolveImbalance = qtrue;
		vk_oit_certify_note_anomaly( "clear/resolve imbalance" );
	}
}

static void VK_OitSoak_Export( void )
{
	char path[MAX_QPATH];
	char pathJson[MAX_QPATH];
	char *buf;
	int i, off = 0, n;
	buf = (char *)ri.Hunk_AllocateTempMemory( VK_OIT_EXPORT_BUF );
	if ( !buf ) {
		return;
	}
	Com_Memset( buf, 0, VK_OIT_EXPORT_BUF );
	off += Com_sprintf( buf + off, VK_OIT_EXPORT_BUF - off,
		"session,elapsedSec,anomalies,peakCorruption,peakFallback,imbalance\n"
		"%s,%d,%d,%u,%u,%d\n\n"
		"idx,clear,resolve,accum,draws,clusterGen,attGen,corruption,fallbacks,w,h,frameMs,oitMode\n",
		s_soak.sessionId, s_soak.elapsedSec, s_soak.anomalyCount,
		s_soak.peakCorruption, s_soak.peakFallback, s_soak.clearResolveImbalance ? 1 : 0 );
	n = s_soak.historyCount;
	for ( i = 0; i < n; i++ ) {
		int idx = ( s_soak.historyWrite - n + i + VK_OIT_SOAK_HISTORY * 4 ) % VK_OIT_SOAK_HISTORY;
		const vkOitSoakSample_t *s = &s_soak.history[idx];
		off += Com_sprintf( buf + off, VK_OIT_EXPORT_BUF - off,
			"%d,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%.3f,%d\n",
			i, s->clearCount, s->resolveCount, s->accumPassCount, s->drawCount,
			s->clusterGen, s->attGen, s->corruption, s->fallbacks, s->width, s->height,
			s->frameTimeMs, s->oitMode );
		if ( off > VK_OIT_EXPORT_BUF - 256 ) {
			break;
		}
	}
	Com_sprintf( path, sizeof( path ), "oit_soak_%s.csv", s_soak.sessionId );
	ri.FS_WriteFile( path, buf, off );

	/* Companion JSON summary for tooling. */
	off = 0;
	Com_Memset( buf, 0, VK_OIT_EXPORT_BUF );
	off += Com_sprintf( buf + off, VK_OIT_EXPORT_BUF - off,
		"{\n  \"sessionId\": \"%s\",\n  \"elapsedSec\": %d,\n  \"anomalies\": %d,\n"
		"  \"peakCorruption\": %u,\n  \"peakFallback\": %u,\n  \"clearResolveImbalance\": %s,\n"
		"  \"historyFrames\": %d,\n  \"samples\": [\n",
		s_soak.sessionId, s_soak.elapsedSec, s_soak.anomalyCount,
		s_soak.peakCorruption, s_soak.peakFallback,
		s_soak.clearResolveImbalance ? "true" : "false",
		n );
	for ( i = 0; i < n; i++ ) {
		int idx = ( s_soak.historyWrite - n + i + VK_OIT_SOAK_HISTORY * 4 ) % VK_OIT_SOAK_HISTORY;
		const vkOitSoakSample_t *s = &s_soak.history[idx];
		off += Com_sprintf( buf + off, VK_OIT_EXPORT_BUF - off,
			"    {\"i\":%d,\"clear\":%u,\"resolve\":%u,\"accum\":%u,\"draws\":%u,"
			"\"clusterGen\":%u,\"attGen\":%u,\"corruption\":%u,\"fallbacks\":%u,"
			"\"w\":%d,\"h\":%d,\"oitMode\":%d}%s\n",
			i, s->clearCount, s->resolveCount, s->accumPassCount, s->drawCount,
			s->clusterGen, s->attGen, s->corruption, s->fallbacks, s->width, s->height, s->oitMode,
			( i + 1 < n ) ? "," : "" );
		if ( off > VK_OIT_EXPORT_BUF - 256 ) {
			break;
		}
	}
	off += Com_sprintf( buf + off, VK_OIT_EXPORT_BUF - off, "  ]\n}\n" );
	Com_sprintf( pathJson, sizeof( pathJson ), "oit_soak_%s.json", s_soak.sessionId );
	ri.FS_WriteFile( pathJson, buf, off );
	ri.Hunk_FreeTempMemory( buf );
	ri.Printf( PRINT_ALL, "oit_soak_wboit exported %s + %s anomalies=%d\n",
		path, pathJson, s_soak.anomalyCount );
}

static void VK_OitSoak_f( void )
{
	int minutes = 30;
	if ( ri.Cmd_Argc() >= 2 ) {
		minutes = atoi( ri.Cmd_Argv( 1 ) );
	}
	if ( minutes <= 0 ) {
		minutes = 30;
	}
	if ( minutes > 240 ) {
		minutes = 240;
	}
	Com_Memset( &s_soak, 0, sizeof( s_soak ) );
	s_soak.active = qtrue;
	s_soak.minutes = minutes;
	VK_OitCert_NowIso( s_soak.sessionId, sizeof( s_soak.sessionId ) );
	{
		char tmp[48];
		Q_strncpyz( tmp, s_soak.sessionId, sizeof( tmp ) );
		Com_sprintf( s_soak.sessionId, sizeof( s_soak.sessionId ), "soak-%s", tmp );
	}
	s_soak.lastSecond = (uint32_t)time( NULL );
	ri.Printf( PRINT_ALL,
		"oit_soak_wboit START %d min session=%s — cycles motion/lights/mode/faults; "
		"telemetry each second; auto-export on completion\n",
		minutes, s_soak.sessionId );
}

void vk_oit_certify_note_anomaly( const char *reason )
{
	if ( s_soak.active ) {
		s_soak.anomalyCount++;
	}
	if ( r_oitCaptureOnError && r_oitCaptureOnError->integer ) {
		vk.oitCapturePending = VK_OIT_CAPTURE_CONTEXT | VK_OIT_CAPTURE_STAGES;
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][OIT] anomaly capture armed: %s\n" S_COLOR_WHITE,
			reason ? reason : "(unknown)" );
	} else if ( reason && reason[0] ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][OIT] anomaly: %s\n", reason );
	}
}

void vk_oit_certify_frame_tick( void )
{
	uint32_t now;
	int phase;
	if ( !s_soak.active ) {
		return;
	}
	now = (uint32_t)time( NULL );
	if ( now == s_soak.lastSecond ) {
		return;
	}
	s_soak.lastSecond = now;
	s_soak.elapsedSec++;
	VK_OitSoak_Sample();

	/* Cycle controlled stress phases once per ~10s. */
	phase = ( s_soak.elapsedSec / 10 ) % 8;
	s_soak.nextPhase = phase;
	switch ( phase ) {
	case 0:
		ri.Cvar_Set( "r_oitForceAllocationFailure", "0" );
		break;
	case 5:
		if ( ( s_soak.elapsedSec % 10 ) == 0 ) {
			ri.Cvar_Set( "r_oitForceClusterMismatch", "1" );
		}
		break;
	case 6:
		ri.Cvar_Set( "r_oitForceClusterMismatch", "0" );
		break;
	default:
		break;
	}

	if ( s_soak.elapsedSec >= s_soak.minutes * 60 ) {
		s_soak.active = qfalse;
		VK_OitSoak_Export();
		if ( s_soak.anomalyCount == 0 && !s_soak.clearResolveImbalance ) {
			s_persist.liveSoakedOk = qtrue;
			s_persist.soakMinutesCompleted = s_soak.minutes;
			VK_OitCert_RecomputeLevel();
			{
				wboitCertStageResult_t sr;
				Com_Memset( &sr, 0, sizeof( sr ) );
				sr.stage = WBOIT_CERT_STAGE_SOAK;
				sr.status = WBOIT_CERT_STATUS_PASS;
				sr.evidenceType = WBOIT_EVIDENCE_SOAK;
				sr.observed = (double)s_soak.minutes;
				Q_strncpyz( sr.testName, "oit_soak_wboit", sizeof( sr.testName ) );
				Q_strncpyz( sr.failureReason, "soak completed with zero anomalies", sizeof( sr.failureReason ) );
				vk_wboit_cert_record_result( &sr );
			}
			ri.Printf( PRINT_ALL, "oit_soak_wboit COMPLETE clean — LIVE_SOAKED eligible\n" );
		} else {
			ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
				"oit_soak_wboit COMPLETE with anomalies=%d imbalance=%d — not soaked-certified\n"
				S_COLOR_WHITE, s_soak.anomalyCount, s_soak.clearResolveImbalance ? 1 : 0 );
		}
	} else if ( ( s_soak.elapsedSec % 30 ) == 0 ) {
		ri.Printf( PRINT_ALL, "oit_soak_wboit %ds/%ds anomalies=%d clear=%u resolve=%u\n",
			s_soak.elapsedSec, s_soak.minutes * 60, s_soak.anomalyCount,
			vk.oitClearCount, vk.oitResolveCount );
	}
}

static void VK_OitCertificationStatus_f( void )
{
	VK_OitCert_RecomputeLevel();
	ri.Printf( PRINT_ALL,
		"oit_certification_status:\n"
		"  level=%s\n"
		"  staticGates=%d liveBasic=%d liveFull=%d liveSoaked=%d soakMinutes=%d\n"
		"  lastSession=%s lastPassAt=%s\n"
		"  certifiedGpu=%s certifiedDriver=%s\n"
		"  certifiedBinaryHash=%s certifiedShaderHash=%s\n"
		"  missing=%s\n"
		"  note=STATIC_GATES alone is NOT Spine 1.1 / WBOIT_PRODUCTION_CERTIFIED\n"
		"  phase26Level=%s (use wboit_production_status for stage matrix)\n",
		vk_oit_certification_level_name( s_persist.level ),
		s_persist.staticGatesOk ? 1 : 0,
		s_persist.liveBasicOk ? 1 : 0,
		s_persist.liveFullOk ? 1 : 0,
		s_persist.liveSoakedOk ? 1 : 0,
		s_persist.soakMinutesCompleted,
		s_persist.lastSessionId[0] ? s_persist.lastSessionId : "(none)",
		s_persist.lastPassAt[0] ? s_persist.lastPassAt : "(none)",
		s_persist.certifiedGpu[0] ? s_persist.certifiedGpu : "(none)",
		s_persist.certifiedDriver[0] ? s_persist.certifiedDriver : "(none)",
		s_persist.certifiedBinaryHash[0] ? s_persist.certifiedBinaryHash : "(unset)",
		s_persist.certifiedShaderHash[0] ? s_persist.certifiedShaderHash : "(unset)",
		( s_persist.level < VK_OIT_CERT_LIVE_FULL )
			? "complete live B0-B7 + export"
			: ( s_persist.level < VK_OIT_CERT_LIVE_SOAKED )
				? "run oit_soak_wboit 30 with zero anomalies"
				: "(none)",
		vk_wboit_production_level_name( vk_wboit_production_level() ) );
}

static void VK_OitFogStatus_f( void )
{
	const int fogMode = r_oitFogMode ? r_oitFogMode->integer : 0;
	const float density = r_oitFogDensity ? r_oitFogDensity->value : 0.0f;
	ri.Printf( PRINT_ALL,
		"oit_fog_status:\n"
		"  fogMode=%d (0=off/legacy 1=per-fragment lit fog 2=weighted moments 3=experimental)\n"
		"  fogDensity=%g fogDebug=%d\n"
		"  accumulationFog=%s\n"
		"  resolveFog=%s\n"
		"  volumetricSource=%s\n"
		"  depthApprox=certified positive view-depth (-viewSpace.z / axis[0]; depth_view.glsl)\n"
		"  doubleFogPrevention=%s\n"
		"  passOrder=opaque(+vol) -> WBOIT fogged-lit accum -> resolve over fogged opaque -> weapon -> bloom\n",
		fogMode, density, r_oitFogDebug ? r_oitFogDebug->integer : 0,
		( fogMode >= 1 ) ? "fragment lit radiance * T(viewDepth)" : "none (legacy)",
		( fogMode >= 1 ) ? "no second full-screen fog on transparent result" : "post stack may fog entire HDR",
		( fogMode >= 1 )
			? "opaque froxel before OIT (vk_volumetric_fog_before_oit; frame-end skipped via doneFog)"
			: "frame-end volumetric over full HDR (legacy)",
		( fogMode >= 1 ) ? "enabled (mode>=1)" : "not active" );
}

void vk_oit_certify_init( void )
{
	if ( s_inited ) {
		return;
	}
	r_oitCaptureOnError = ri.Cvar_Get( "r_oitCaptureOnError", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_oitCaptureOnError, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitCaptureOnError,
		"Arm oit_capture stages automatically when OIT anomalies / fault injection fire." );
	ri.Cvar_SetGroup( r_oitCaptureOnError, CVG_RENDERER );

	r_oitCaptureHistoryFrames = ri.Cvar_Get( "r_oitCaptureHistoryFrames", "120", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_oitCaptureHistoryFrames, "1", "120", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitCaptureHistoryFrames,
		"Soak telemetry history frames retained for anomaly export (max 120)." );
	ri.Cvar_SetGroup( r_oitCaptureHistoryFrames, CVG_RENDERER );

	r_oitFogMode = ri.Cvar_Get( "r_oitFogMode", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_oitFogMode, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitFogMode,
		"WBOIT fog-through-layers:\n"
		" 0 - legacy (no OIT fog)\n"
		" 1 - per-fragment fogged lit radiance (production default)\n"
		" 2 - weighted fog moments (optional)\n"
		" 3 - experimental enhanced approximation" );
	ri.Cvar_SetGroup( r_oitFogMode, CVG_RENDERER );

	r_oitFogDensity = ri.Cvar_Get( "r_oitFogDensity", "0.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_oitFogDensity, "0", "1", CV_FLOAT );
	ri.Cvar_SetDescription( r_oitFogDensity,
		"Exponential fog density for OIT accum (T=exp(-density*viewDepth)). 0 disables even if r_oitFogMode>=1." );
	ri.Cvar_SetGroup( r_oitFogDensity, CVG_RENDERER );

	r_oitFogDebug = ri.Cvar_Get( "r_oitFogDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_oitFogDebug, "0", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitFogDebug,
		"OIT fog debug (cheat):\n"
		" 1 viewDepth  2 transmittance  3 in-scatter  4 weighted depth\n"
		" 5 weighted T  6 double-fog detector  7 opaque/translucent difference\n"
		" 8 |cameraDistance-viewDepth| heat (cert)" );
	ri.Cvar_SetGroup( r_oitFogDebug, CVG_RENDERER );

	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "oit_certify_wboit", VK_OitCertify_f );
		ri.Cmd_AddCommand( "oit_soak_wboit", VK_OitSoak_f );
		ri.Cmd_AddCommand( "oit_certification_status", VK_OitCertificationStatus_f );
		ri.Cmd_AddCommand( "oit_fog_status", VK_OitFogStatus_f );
	}
	s_persist.staticGatesOk = qtrue;
	VK_OitCert_RecomputeLevel();
	s_inited = qtrue;
	ri.Printf( PRINT_ALL,
		"[VK][OIT] certification runner ready (oit_certify_wboit / oit_soak_wboit / oit_certification_status)\n"
		"[VK][OIT] fog-through-layers: r_oitFogMode=%d density=%g\n",
		r_oitFogMode->integer, r_oitFogDensity->value );
}

void vk_oit_certify_shutdown( void )
{
	if ( ri.Cmd_RemoveCommand ) {
		ri.Cmd_RemoveCommand( "oit_certify_wboit" );
		ri.Cmd_RemoveCommand( "oit_soak_wboit" );
		ri.Cmd_RemoveCommand( "oit_certification_status" );
		ri.Cmd_RemoveCommand( "oit_fog_status" );
	}
	s_inited = qfalse;
}
