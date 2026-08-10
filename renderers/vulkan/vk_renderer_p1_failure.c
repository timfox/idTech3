/*
===========================================================================
Phase 1.6 — failure bundles.
===========================================================================
*/

#include "tr_local.h"
#include "vk_renderer_p1_failure.h"
#include "vk_renderer_p1_cert.h"
#include "vk_renderer_p1_live.h"
#include "vk_renderer_p1_thresholds.h"
#include <stdio.h>
#include <time.h>


static p1FailureBundle_t s_last;
static qboolean s_cmds;

const p1FailureBundle_t *vk_renderer_p1_last_failure( void )
{
	return s_last.valid ? &s_last : NULL;
}

static const char *P1_FailClassName( p1FailClass_t k )
{
	switch ( k ) {
	case P1_FAIL_CLASS_RENDERER_BUG: return "RENDERER_BUG";
	case P1_FAIL_CLASS_FIXTURE_BUG: return "FIXTURE_BUG";
	case P1_FAIL_CLASS_READBACK_BUG: return "READBACK_BUG";
	case P1_FAIL_CLASS_METRIC_BUG: return "METRIC_BUG";
	case P1_FAIL_CLASS_THRESHOLD_BUG: return "THRESHOLD_BUG";
	case P1_FAIL_CLASS_RESOURCE_LIFETIME_BUG: return "RESOURCE_LIFETIME_BUG";
	case P1_FAIL_CLASS_SYNC_BUG: return "SYNC_BUG";
	case P1_FAIL_CLASS_PREFLIGHT: return "PREFLIGHT";
	default: return "UNKNOWN";
	}
}

qboolean vk_renderer_p1_failure_capture( p1CertStage_t stage, uint32_t caseId,
	p1FailClass_t klass, const char *reason, const char *extraMeta )
{
	char meta[2048];
	char console[512];
	time_t now = time( NULL );
	int n;
	const rendererP1LiveStamp_t *st = vk_renderer_p1_live_stamp();

	Com_Memset( &s_last, 0, sizeof( s_last ) );
	s_last.stage = stage;
	s_last.caseId = caseId;
	s_last.klass = klass;
	s_last.timestamp = (uint64_t)now;
	Q_strncpyz( s_last.reason, reason ? reason : "", sizeof( s_last.reason ) );
	Com_sprintf( s_last.path, sizeof( s_last.path ),
		"render_cert/failures/p1_%s_case%u_%llu",
		vk_renderer_p1_cert_stage_name( stage ), caseId, (unsigned long long)now );
	s_last.valid = qtrue;

	n = Com_sprintf( meta, sizeof( meta ),
		"{\n"
		"  \"stage\": \"%s\",\n"
		"  \"caseId\": %u,\n"
		"  \"class\": \"%s\",\n"
		"  \"reason\": \"%s\",\n"
		"  \"extra\": \"%s\",\n"
		"  \"fixtureFrame\": %llu,\n"
		"  \"snapshotFrame\": %llu,\n"
		"  \"generation\": %u,\n"
		"  \"expectedGeneration\": %u,\n"
		"  \"profileHash\": %u,\n"
		"  \"thresholdHash\": %u,\n"
		"  \"liveState\": \"%s\"\n"
		"}\n",
		vk_renderer_p1_cert_stage_name( stage ), caseId, P1_FailClassName( klass ),
		reason ? reason : "", extraMeta ? extraMeta : "",
		(unsigned long long)( st ? st->fixtureFrame : 0 ),
		(unsigned long long)( st ? st->snapshotFrame : 0 ),
		st ? st->resourceGeneration : 0,
		st ? st->expectedGeneration : 0,
		st ? st->profileHash : 0,
		st ? st->thresholdHash : 0,
		vk_renderer_p1_live_state_name( vk_renderer_p1_live_state() ) );

	{
		char path[MAX_OSPATH];
		Com_sprintf( path, sizeof( path ), "%s/metadata.json", s_last.path );
		ri.FS_WriteFile( path, meta, n );
	}
	n = Com_sprintf( console, sizeof( console ),
		"P1 failure: %s case=%u class=%s\n%s\n",
		vk_renderer_p1_cert_stage_name( stage ), caseId, P1_FailClassName( klass ),
		reason ? reason : "" );
	{
		char path[MAX_OSPATH];
		char thrPath[MAX_OSPATH];
		Com_sprintf( path, sizeof( path ), "%s/console.txt", s_last.path );
		ri.FS_WriteFile( path, console, n );
		Com_sprintf( thrPath, sizeof( thrPath ), "%s/thresholds.json", s_last.path );
		vk_renderer_p1_thresholds_export_json( thrPath );
	}

	ri.Printf( PRINT_ALL, "renderer_p1_failure: wrote bundle %s\n", s_last.path );
	return qtrue;
}

static void P1_LastFailure_f( void )
{
	if ( !s_last.valid ) {
		ri.Printf( PRINT_ALL, "renderer_p1_last_failure: none\n" );
		return;
	}
	ri.Printf( PRINT_ALL,
		"renderer_p1_last_failure:\n"
		"  path=%s\n  stage=%s case=%u class=%s\n  reason=%s\n",
		s_last.path, vk_renderer_p1_cert_stage_name( s_last.stage ),
		s_last.caseId, P1_FailClassName( s_last.klass ), s_last.reason );
}

static void P1_OpenLastFailure_f( void )
{
	P1_LastFailure_f();
	if ( s_last.valid ) {
		ri.Printf( PRINT_ALL, "Open directory: %s\n", s_last.path );
	}
}

void vk_renderer_p1_failure_register( void )
{
	if ( s_cmds ) {
		return;
	}
	s_cmds = qtrue;
	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "renderer_p1_last_failure", P1_LastFailure_f );
		ri.Cmd_AddCommand( "renderer_p1_open_last_failure", P1_OpenLastFailure_f );
	}
}

