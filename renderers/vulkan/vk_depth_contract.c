/*
===========================================================================
Color Pipeline Phase 2.3.1 — authoritative depth contract freeze.
Reversed-Z, 0..1 clip, GREATER_OR_EQUAL, clear=0, positive view-depth policy.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_depth_contract.h"
#include "vk_hiz.h"

static cvar_t *r_depthDebug;
static char s_depthFirstWriter[48];
static char s_depthLastWriter[48];
static char s_depthReaders[8][48];
static uint32_t s_depthReaderCount;
static qboolean s_depthWritten;
static qboolean s_cmdsRegistered;
static depthContract_t s_frozen;

uint32_t vk_depth_contract_compute_hash( const depthContract_t *c )
{
	uint32_t h = 2166136261u;
	const unsigned char *p;
	size_t n;
	size_t i;

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

const char *vk_depth_projection_mode_name( depthProjectionMode_t m )
{
	switch ( m ) {
	case DEPTH_PROJECTION_PERSPECTIVE: return "perspective";
	case DEPTH_PROJECTION_ORTHOGRAPHIC: return "orthographic";
	case DEPTH_PROJECTION_PERSPECTIVE_INFINITE: return "perspective_infinite";
	default: return "unknown";
	}
}

const char *vk_depth_view_depth_mode_name( positiveViewDepthMode_t m )
{
	switch ( m ) {
	case VIEW_DEPTH_NEG_VIEW_Z: return "neg_view_z (-viewSpace.z)";
	case VIEW_DEPTH_CAMERA_DISTANCE: return "camera_distance |world-viewOrg|";
	case VIEW_DEPTH_DEVICE_RAW: return "device_raw (FORBIDDEN for fog/weight)";
	default: return "unknown";
	}
}

const char *vk_depth_far_plane_mode_name( depthFarPlaneMode_t m )
{
	switch ( m ) {
	case DEPTH_FAR_FINITE: return "finite";
	case DEPTH_FAR_INFINITE: return "infinite";
	default: return "unknown";
	}
}

static void VK_Depth_Contract_FillFrozen( depthContract_t *c )
{
	Com_Memset( c, 0, sizeof( *c ) );

	c->reversedZ = qtrue;
	c->zeroToOneClipDepth = qtrue;
	c->infiniteFarPlane = qfalse;

	c->projectionMode = DEPTH_PROJECTION_PERSPECTIVE;
	c->farPlaneMode = DEPTH_FAR_FINITE;
	/* Certified metric: -viewSpace.z (Q3 axis[0] / Depth_PositiveViewFromWorld). */
	c->positiveViewDepthMode = VIEW_DEPTH_NEG_VIEW_Z;

	c->nearPlane = 8.0f; /* r_znear default */
	c->farPlane = 0.0f;  /* dynamic from viewParms.zFar */
	c->clearDepth = 0.0f;

	c->depthCompareOp = (uint32_t)VK_COMPARE_OP_GREATER_OR_EQUAL;
	c->deviceDepthFormatHint = (uint32_t)VK_FORMAT_D32_SFLOAT;

	c->currentDepthOwnedByScene = qtrue;
	c->previousDepthOwnedByTemporal = qtrue;

	c->contractVersion = DEPTH_CONTRACT_VERSION;
	c->contractHash = 0u;
	c->contractHash = vk_depth_contract_compute_hash( c );
}

const depthContract_t *vk_depth_contract_get( void )
{
	static qboolean inited;
	if ( !inited ) {
		VK_Depth_Contract_FillFrozen( &s_frozen );
		inited = qtrue;
	}
	return &s_frozen;
}

void vk_depth_contract_refresh_planes( depthContract_t *dst )
{
	const depthContract_t *f = vk_depth_contract_get();

	if ( !dst ) {
		return;
	}
	*dst = *f;
	if ( r_znear && r_znear->value > 0.0f ) {
		dst->nearPlane = r_znear->value;
	}
	if ( backEnd.viewParms.zFar > 0.0f ) {
		dst->farPlane = backEnd.viewParms.zFar;
	}
	/* Hash stays the frozen policy hash — live planes are runtime overlays. */
	dst->contractHash = f->contractHash;
}

qboolean vk_depth_contract_validate( const depthContract_t *c, char *errBuf, int errBufSize )
{
	depthContract_t rebuilt;
	const depthContract_t *frozen;

	if ( errBuf && errBufSize > 0 ) {
		errBuf[0] = '\0';
	}
	if ( !c ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "null contract", errBufSize );
		}
		return qfalse;
	}

	frozen = vk_depth_contract_get();
	if ( c->contractVersion != DEPTH_CONTRACT_VERSION ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "version %u != %u",
				c->contractVersion, DEPTH_CONTRACT_VERSION );
		}
		return qfalse;
	}

	if ( !c->reversedZ || !c->zeroToOneClipDepth ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "must be reversed-Z with 0..1 clip depth", errBufSize );
		}
		return qfalse;
	}
	if ( c->clearDepth != 0.0f ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "clearDepth must be 0 (reversed-Z)", errBufSize );
		}
		return qfalse;
	}
	if ( c->depthCompareOp != (uint32_t)VK_COMPARE_OP_GREATER_OR_EQUAL ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "compare must be GREATER_OR_EQUAL", errBufSize );
		}
		return qfalse;
	}
	if ( c->positiveViewDepthMode == VIEW_DEPTH_DEVICE_RAW ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "device_raw view-depth forbidden for production", errBufSize );
		}
		return qfalse;
	}
	if ( c->infiniteFarPlane || c->farPlaneMode == DEPTH_FAR_INFINITE ||
		c->projectionMode == DEPTH_PROJECTION_PERSPECTIVE_INFINITE ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "infinite-far not production yet", errBufSize );
		}
		return qfalse;
	}

	VK_Depth_Contract_FillFrozen( &rebuilt );
	if ( rebuilt.contractHash != frozen->contractHash ||
		c->contractHash != frozen->contractHash ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "frozen hash mismatch", errBufSize );
		}
		return qfalse;
	}
	if ( rebuilt.reversedZ != c->reversedZ ||
		rebuilt.clearDepth != c->clearDepth ||
		rebuilt.depthCompareOp != c->depthCompareOp ||
		rebuilt.positiveViewDepthMode != c->positiveViewDepthMode ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "policy field drift vs freeze", errBufSize );
		}
		return qfalse;
	}
	return qtrue;
}

void vk_depth_contract_print( const depthContract_t *c )
{
	char err[128];
	depthContract_t live;
	const qboolean ok = vk_depth_contract_validate( c ? c : vk_depth_contract_get(), err, sizeof( err ) );
	const depthContract_t *base = c ? c : vk_depth_contract_get();
	uint32_t i;

	vk_depth_contract_refresh_planes( &live );

	ri.Printf( PRINT_ALL, "======== Depth Contract (Phase 2.3.1 freeze) ========\n" );
	ri.Printf( PRINT_ALL, "version=%u hash=0x%08x validate=%s%s%s\n",
		base->contractVersion, base->contractHash,
		ok ? "PASS" : "FAIL",
		ok ? "" : " reason=",
		ok ? "" : err );
	ri.Printf( PRINT_ALL, "device: reversedZ=%d clipDepth=0..1 clear=%.0f compare=GREATER_OR_EQUAL\n",
		base->reversedZ ? 1 : 0, base->clearDepth );
	ri.Printf( PRINT_ALL, "projection: %s farMode=%s infiniteFar=%d\n",
		vk_depth_projection_mode_name( base->projectionMode ),
		vk_depth_far_plane_mode_name( base->farPlaneMode ),
		base->infiniteFarPlane ? 1 : 0 );
	ri.Printf( PRINT_ALL, "planes: near=%.3f (live=%.3f) far=%s (live=%.1f)\n",
		base->nearPlane, live.nearPlane,
		( base->farPlane > 0.0f ) ? "fixed" : "dynamic",
		live.farPlane );
	ri.Printf( PRINT_ALL, "positiveViewDepth: certified=%s (WBOIT fog+weight migrated)\n",
		vk_depth_view_depth_mode_name( base->positiveViewDepthMode ) );
	ri.Printf( PRINT_ALL,
		"  helper: depth_view.glsl / vk_depth_linearize_reversed_z / vk_depth_positive_view_from_world\n" );
	ri.Printf( PRINT_ALL, "ownership: currentScene=%d previousTemporal=%d\n",
		base->currentDepthOwnedByScene ? 1 : 0,
		base->previousDepthOwnedByTemporal ? 1 : 0 );
	ri.Printf( PRINT_ALL, "r_depthDebug=%d written=%s first=%s last=%s\n",
		r_depthDebug ? r_depthDebug->integer : 0,
		s_depthWritten ? "yes" : "no",
		s_depthFirstWriter[0] ? s_depthFirstWriter : "(none)",
		s_depthLastWriter[0] ? s_depthLastWriter : "(none)" );
	ri.Printf( PRINT_ALL, "extent=%ux%u format=%u image=%p\n",
		vk.renderWidth, vk.renderHeight, (unsigned)vk.depth_format,
		(void *)(uintptr_t)vk.depth_image );
	for ( i = 0; i < s_depthReaderCount; i++ ) {
		ri.Printf( PRINT_ALL, "  reader[%u]=%s\n", i, s_depthReaders[i] );
	}
	ri.Printf( PRINT_ALL, "docs: docs/DEPTH_CONTRACT.md\n" );
	ri.Printf( PRINT_ALL, "====================================================\n" );
}

static void VK_Depth_Status_f( void )
{
	vk_depth_contract_print( vk_depth_contract_get() );
	vk_hiz_status_f();
}

static void VK_Depth_ContractValidate_f( void )
{
	char err[160];
	if ( vk_depth_contract_validate( vk_depth_contract_get(), err, sizeof( err ) ) ) {
		ri.Printf( PRINT_ALL, "depth_contract_validate: PASS (v%u hash=0x%08x)\n",
			DEPTH_CONTRACT_VERSION, vk_depth_contract_get()->contractHash );
	} else {
		ri.Printf( PRINT_ALL, "depth_contract_validate: FAIL (%s)\n", err[0] ? err : "unknown" );
	}
}

void vk_depth_contract_register( void )
{
	(void)vk_depth_contract_get();

	r_depthDebug = ri.Cvar_Get( "r_depthDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_depthDebug, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_depthDebug,
		"Depth contract debug: 0 off, 1 writers, 2 previous-depth, 3 Hi-Z link, 4 full status." );
	ri.Cvar_SetGroup( r_depthDebug, CVG_RENDERER );

	if ( !s_cmdsRegistered ) {
		ri.Cmd_AddCommand( "depth_status", VK_Depth_Status_f );
		ri.Cmd_AddCommand( "depth_contract_status", VK_Depth_Status_f );
		ri.Cmd_AddCommand( "depth_contract_validate", VK_Depth_ContractValidate_f );
		s_cmdsRegistered = qtrue;
	}
	ri.Printf( PRINT_ALL,
		"[VK][depth] Phase 2.3.1 contract frozen v%u hash=0x%08x "
		"(depth_contract_status / depth_contract_validate)\n",
		DEPTH_CONTRACT_VERSION, s_frozen.contractHash );
}

void vk_depth_contract_begin_frame( void )
{
	s_depthFirstWriter[0] = '\0';
	s_depthLastWriter[0] = '\0';
	s_depthWritten = qfalse;
	s_depthReaderCount = 0u;
	Com_Memset( s_depthReaders, 0, sizeof( s_depthReaders ) );
}

void vk_depth_contract_note_writer( const char *passName )
{
	if ( !passName || !passName[0] ) {
		return;
	}
	if ( !s_depthWritten ) {
		Q_strncpyz( s_depthFirstWriter, passName, sizeof( s_depthFirstWriter ) );
		s_depthWritten = qtrue;
	}
	Q_strncpyz( s_depthLastWriter, passName, sizeof( s_depthLastWriter ) );
}

void vk_depth_contract_note_reader( const char *passName )
{
	if ( !passName || !passName[0] ) {
		return;
	}
	if ( s_depthReaderCount >= 8u ) {
		return;
	}
	Q_strncpyz( s_depthReaders[s_depthReaderCount], passName, sizeof( s_depthReaders[0] ) );
	s_depthReaderCount++;
}

float vk_depth_linearize_reversed_z( float deviceDepth, float zNear, float zFar )
{
	float zn = zNear > 1e-4f ? zNear : 1e-4f;
	float zf = ( zFar > zn + 1e-3f ) ? zFar : ( zn + 1e-3f );
	float d = deviceDepth;
	float denom;

	if ( d < 0.0f ) {
		d = 0.0f;
	} else if ( d > 1.0f ) {
		d = 1.0f;
	}
	denom = zn + d * ( zf - zn );
	if ( denom < 1e-6f ) {
		denom = 1e-6f;
	}
	return ( zn * zf ) / denom;
}

float vk_depth_positive_view_from_world( const vec3_t worldPos, const vec3_t viewOrg,
	const vec3_t viewForward )
{
	vec3_t delta, fwd;
	float len;
	float d;

	VectorSubtract( worldPos, viewOrg, delta );
	VectorCopy( viewForward, fwd );
	len = VectorNormalize( fwd );
	if ( len < 1e-8f ) {
		return 0.0f;
	}
	d = DotProduct( delta, fwd );
	return d > 0.0f ? d : 0.0f;
}

float vk_depth_camera_distance( const vec3_t worldPos, const vec3_t viewOrg )
{
	vec3_t delta;
	VectorSubtract( worldPos, viewOrg, delta );
	return VectorLength( delta );
}

float vk_depth_view_depth_to_traditional01( float viewDepth, float zNear, float zFar )
{
	float zn = zNear > 1e-4f ? zNear : 1e-4f;
	float zf = ( zFar > zn + 1e-3f ) ? zFar : ( zn + 1e-3f );
	float t = ( viewDepth - zn ) / ( zf - zn );

	if ( t < 0.0f ) {
		return 0.0f;
	}
	if ( t > 1.0f ) {
		return 1.0f;
	}
	return t;
}
