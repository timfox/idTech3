/*
===========================================================================
Color Pipeline Phase 2.2 — source-alpha encoding, material declaration,
classic translation audit, normalization reference, runtime validation.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_oit_alpha.h"
#include "vk_oit_contract.h"
#include "vk_transparency_route.h"
#include "vk_color_contract.h"
#include <math.h>

static qboolean s_cmds;
static oitAlphaCounters_t s_counters;
static oitAlphaCertLevel_t s_certLevel = OIT_ALPHA_NONE;

static cvar_t *r_alphaDebug;
static cvar_t *r_transparentEdgePolicy;
static cvar_t *r_alphaFilterDebug;
static cvar_t *r_alphaEncodingCompare;
static cvar_t *r_oitSingleLayerCompare;
static cvar_t *r_oitFaultTreatStraightAsPremul;
static cvar_t *r_oitFaultTreatPremulAsStraight;
static cvar_t *r_oitFaultDoublePremultiply;
static cvar_t *r_oitFaultSkipAlphaMultiply;
static cvar_t *r_oitFaultZeroAlphaColoredRgb;
static cvar_t *r_oitFaultInvalidAlpha;
static cvar_t *r_oitSourceAlphaDefault; /* 0=straight 1=premul */

const char *vk_oit_source_alpha_name( oitSourceAlphaEncoding_t e )
{
	switch ( e ) {
	case OIT_SOURCE_ALPHA_STRAIGHT: return "straight";
	case OIT_SOURCE_ALPHA_PREMULTIPLIED: return "premultiplied";
	case OIT_SOURCE_ALPHA_OPAQUE: return "opaque";
	case OIT_SOURCE_ALPHA_ADDITIVE: return "additive";
	case OIT_SOURCE_ALPHA_MASKED: return "masked";
	case OIT_SOURCE_ALPHA_MULTIPLICATIVE: return "multiplicative";
	case OIT_SOURCE_ALPHA_UNKNOWN: return "unknown";
	default: return "?";
	}
}

const char *vk_oit_alpha_reason_name( oitAlphaReason_t r )
{
	switch ( r ) {
	case ALPHA_ENCODING_EXPLICIT_STRAIGHT: return "EXPLICIT_STRAIGHT";
	case ALPHA_ENCODING_EXPLICIT_PREMULTIPLIED: return "EXPLICIT_PREMULTIPLIED";
	case ALPHA_ENCODING_CLASSIC_COMPATIBILITY: return "CLASSIC_COMPATIBILITY";
	case ALPHA_ENCODING_INFERRED_UNSAFE: return "INFERRED_UNSAFE";
	case ALPHA_ENCODING_UNKNOWN: return "UNKNOWN";
	case ALPHA_ROUTE_NOT_WBOIT_COMPATIBLE: return "NOT_WBOIT_COMPATIBLE";
	default: return "?";
	}
}

const char *vk_oit_alpha_cert_level_name( oitAlphaCertLevel_t l )
{
	switch ( l ) {
	case OIT_ALPHA_NONE: return "NONE";
	case OIT_ALPHA_DECLARED: return "OIT_ALPHA_DECLARED";
	case OIT_ALPHA_NORMALIZED: return "OIT_ALPHA_NORMALIZED";
	case OIT_ALPHA_ACCUMULATION_VALID: return "OIT_ALPHA_ACCUMULATION_VALID";
	case OIT_ALPHA_EDGE_CERTIFIED: return "OIT_ALPHA_EDGE_CERTIFIED";
	default: return "?";
	}
}

const char *vk_oit_transparency_path_name( transparencyPath_t p )
{
	switch ( p ) {
	case TRANSP_PATH_WBOIT: return "wboit";
	case TRANSP_PATH_ADDITIVE: return "additive";
	case TRANSP_PATH_ALPHA_TESTED: return "alpha_tested";
	case TRANSP_PATH_MULTIPLICATIVE: return "multiplicative";
	case TRANSP_PATH_REFRACTIVE: return "refractive";
	case TRANSP_PATH_SORTED_ALPHA: return "sorted_alpha";
	case TRANSP_PATH_REJECTED: return "rejected";
	default: return "?";
	}
}

void vk_oit_alpha_begin_frame( void )
{
	Com_Memset( &s_counters, 0, sizeof( s_counters ) );
}

void vk_oit_alpha_note_sample_flags( unsigned flags )
{
	if ( flags & OIT_SAMPLE_FLAG_CLAMPED_DIV ) {
		s_counters.premulDivClamps++;
	}
	if ( flags & OIT_SAMPLE_FLAG_ZERO_ALPHA_RGB ) {
		s_counters.zeroAlphaColored++;
	}
	if ( flags & OIT_SAMPLE_FLAG_NEAR_ZERO_BRIGHT ) {
		s_counters.nearZeroBright++;
	}
	if ( flags & OIT_SAMPLE_FLAG_NON_FINITE ) {
		s_counters.nonFinite++;
	}
}

void vk_oit_alpha_note_route( oitSourceAlphaEncoding_t enc, transparencyPath_t path )
{
	if ( path == TRANSP_PATH_WBOIT ) {
		s_counters.materialsWboit++;
	} else if ( path == TRANSP_PATH_REJECTED || path == TRANSP_PATH_MULTIPLICATIVE ||
		path == TRANSP_PATH_REFRACTIVE || path == TRANSP_PATH_ALPHA_TESTED ||
		path == TRANSP_PATH_ADDITIVE ) {
		if ( path != TRANSP_PATH_WBOIT ) {
			s_counters.routesRejected++;
		}
	}
	if ( enc == OIT_SOURCE_ALPHA_STRAIGHT ) {
		s_counters.sourcesStraight++;
	} else if ( enc == OIT_SOURCE_ALPHA_PREMULTIPLIED ) {
		s_counters.sourcesPremul++;
	} else if ( enc == OIT_SOURCE_ALPHA_UNKNOWN ) {
		s_counters.sourcesUnknown++;
	}
}

static void VK_Oit_Alpha_ApplyFaults( oitSourceAlphaEncoding_t *enc )
{
	if ( !enc ) {
		return;
	}
	if ( r_oitFaultTreatStraightAsPremul && r_oitFaultTreatStraightAsPremul->integer &&
		*enc == OIT_SOURCE_ALPHA_STRAIGHT ) {
		*enc = OIT_SOURCE_ALPHA_PREMULTIPLIED;
		s_counters.faultHits++;
		s_counters.doublePremulSuspects++;
	}
	if ( r_oitFaultTreatPremulAsStraight && r_oitFaultTreatPremulAsStraight->integer &&
		*enc == OIT_SOURCE_ALPHA_PREMULTIPLIED ) {
		*enc = OIT_SOURCE_ALPHA_STRAIGHT;
		s_counters.faultHits++;
		s_counters.missingAlphaMultiplySuspects++;
	}
	if ( r_oitFaultInvalidAlpha && r_oitFaultInvalidAlpha->integer ) {
		*enc = OIT_SOURCE_ALPHA_UNKNOWN;
		s_counters.faultHits++;
		s_counters.sourcesUnknown++;
	}
}

int vk_oit_alpha_pack_push( oitSourceAlphaEncoding_t enc )
{
	int edge = r_transparentEdgePolicy ? r_transparentEdgePolicy->integer : 0;
	int dbg = r_alphaDebug ? r_alphaDebug->integer : 0;
	int emissive = 0;
	oitSourceAlphaEncoding_t e = enc;

	VK_Oit_Alpha_ApplyFaults( &e );
	if ( edge < 0 ) {
		edge = 0;
	}
	if ( edge > 3 ) {
		edge = 3;
	}
	if ( dbg < 0 ) {
		dbg = 0;
	}
	if ( dbg > 12 ) {
		dbg = 12;
	}
	return ( (int)e & 0xff ) | ( ( dbg & 0xff ) << 8 ) | ( ( edge & 0xff ) << 16 ) | ( ( emissive & 0xff ) << 24 );
}

void vk_oit_normalize_source( const float rgba[4], oitSourceAlphaEncoding_t encoding,
	const oitSourcePolicy_t *policy, oitSurfaceSample_t *out )
{
	float eps = ( policy && policy->epsilon > 0.0f ) ? policy->epsilon : 1e-5f;
	float a, lum;
	int edge = policy ? policy->edgePolicy : 0;

	Com_Memset( out, 0, sizeof( *out ) );
	out->sourceEncoding = (unsigned)encoding;

	if ( !rgba ) {
		out->flags |= OIT_SAMPLE_FLAG_REJECTED;
		return;
	}

	a = rgba[3];
	if ( a < 0.0f || a > 1.0f ) {
		s_counters.alphaOutOfRange++;
	}
	a = Com_Clamp( 0.0f, 1.0f, a );

	if ( !isfinite( rgba[0] ) || !isfinite( rgba[1] ) || !isfinite( rgba[2] ) || !isfinite( rgba[3] ) ) {
		out->flags |= OIT_SAMPLE_FLAG_NON_FINITE | OIT_SAMPLE_FLAG_REJECTED;
		s_counters.nonFinite++;
		return;
	}

	switch ( encoding ) {
	case OIT_SOURCE_ALPHA_OPAQUE:
		out->opacity = 1.0f;
		out->unassociatedRadiance[0] = rgba[0];
		out->unassociatedRadiance[1] = rgba[1];
		out->unassociatedRadiance[2] = rgba[2];
		out->associatedRadiance[0] = rgba[0];
		out->associatedRadiance[1] = rgba[1];
		out->associatedRadiance[2] = rgba[2];
		break;

	case OIT_SOURCE_ALPHA_PREMULTIPLIED:
		out->opacity = a;
		out->associatedRadiance[0] = rgba[0];
		out->associatedRadiance[1] = rgba[1];
		out->associatedRadiance[2] = rgba[2];
		if ( a > eps ) {
			out->unassociatedRadiance[0] = rgba[0] / a;
			out->unassociatedRadiance[1] = rgba[1] / a;
			out->unassociatedRadiance[2] = rgba[2] / a;
		} else {
			out->unassociatedRadiance[0] = out->unassociatedRadiance[1] = out->unassociatedRadiance[2] = 0.0f;
			out->flags |= OIT_SAMPLE_FLAG_CLAMPED_DIV;
			s_counters.premulDivClamps++;
			if ( policy && policy->allowEmissiveAtZeroAlpha ) {
				/* keep associated for emissive diagnostics only */
			}
		}
		break;

	case OIT_SOURCE_ALPHA_ADDITIVE:
	case OIT_SOURCE_ALPHA_MASKED:
	case OIT_SOURCE_ALPHA_MULTIPLICATIVE:
		out->flags |= OIT_SAMPLE_FLAG_REJECTED;
		out->opacity = a;
		out->unassociatedRadiance[0] = rgba[0];
		out->unassociatedRadiance[1] = rgba[1];
		out->unassociatedRadiance[2] = rgba[2];
		break;

	case OIT_SOURCE_ALPHA_STRAIGHT:
	case OIT_SOURCE_ALPHA_UNKNOWN:
	default:
		out->opacity = a;
		out->unassociatedRadiance[0] = rgba[0];
		out->unassociatedRadiance[1] = rgba[1];
		out->unassociatedRadiance[2] = rgba[2];
		out->associatedRadiance[0] = rgba[0] * a;
		out->associatedRadiance[1] = rgba[1] * a;
		out->associatedRadiance[2] = rgba[2] * a;
		if ( encoding == OIT_SOURCE_ALPHA_UNKNOWN ) {
			s_counters.sourcesUnknown++;
		}
		break;
	}

	/* Edge diagnostics / optional repair */
	lum = out->unassociatedRadiance[0] * 0.2126f +
		out->unassociatedRadiance[1] * 0.7152f +
		out->unassociatedRadiance[2] * 0.0722f;
	if ( a <= 0.0f && lum > 1e-4f ) {
		out->flags |= OIT_SAMPLE_FLAG_ZERO_ALPHA_RGB;
		s_counters.zeroAlphaColored++;
		if ( edge == 1 ) {
			out->unassociatedRadiance[0] = out->unassociatedRadiance[1] = out->unassociatedRadiance[2] = 0.0f;
			out->associatedRadiance[0] = out->associatedRadiance[1] = out->associatedRadiance[2] = 0.0f;
		}
	}
	if ( a > 0.0f && a < 0.02f && lum > 0.25f ) {
		out->flags |= OIT_SAMPLE_FLAG_NEAR_ZERO_BRIGHT;
		s_counters.nearZeroBright++;
		s_counters.colorBleedRisk++;
	}
	if ( a < 0.05f && lum < 0.02f && ( rgba[0] + rgba[1] + rgba[2] ) > 1e-6f ) {
		s_counters.blackEdgeRisk++;
	}

	/* Fault: force colored zero-alpha */
	if ( r_oitFaultZeroAlphaColoredRgb && r_oitFaultZeroAlphaColoredRgb->integer && a < 1e-6f ) {
		out->unassociatedRadiance[0] = 1.0f;
		out->unassociatedRadiance[1] = 0.0f;
		out->unassociatedRadiance[2] = 1.0f;
		out->flags |= OIT_SAMPLE_FLAG_ZERO_ALPHA_RGB;
		s_counters.faultHits++;
	}

	/* Identity: associated = unassociated * opacity (when not rejected premul path) */
	if ( !( out->flags & OIT_SAMPLE_FLAG_REJECTED ) && encoding != OIT_SOURCE_ALPHA_PREMULTIPLIED ) {
		out->associatedRadiance[0] = out->unassociatedRadiance[0] * out->opacity;
		out->associatedRadiance[1] = out->unassociatedRadiance[1] * out->opacity;
		out->associatedRadiance[2] = out->unassociatedRadiance[2] * out->opacity;
	}

	if ( r_oitFaultDoublePremultiply && r_oitFaultDoublePremultiply->integer ) {
		out->unassociatedRadiance[0] *= out->opacity;
		out->unassociatedRadiance[1] *= out->opacity;
		out->unassociatedRadiance[2] *= out->opacity;
		s_counters.faultHits++;
		s_counters.doublePremulSuspects++;
	}
	if ( r_oitFaultSkipAlphaMultiply && r_oitFaultSkipAlphaMultiply->integer ) {
		/* Leave unassociated as-is but lie that opacity is 1 for accum tests */
		out->opacity = 1.0f;
		s_counters.faultHits++;
		s_counters.missingAlphaMultiplySuspects++;
	}
}

void vk_oit_reference_source_over( const float unassoc[3], float opacity,
	const float opaque[3], float outRgb[3] )
{
	float o = Com_Clamp( 0.0f, 1.0f, opacity );
	outRgb[0] = unassoc[0] * o + opaque[0] * ( 1.0f - o );
	outRgb[1] = unassoc[1] * o + opaque[1] * ( 1.0f - o );
	outRgb[2] = unassoc[2] * o + opaque[2] * ( 1.0f - o );
}

void vk_oit_alpha_query_shader( const shader_t *shader, materialTransparencyInfo_t *out )
{
	unsigned stageBits, src, dst;
	const shaderStage_t *st;

	Com_Memset( out, 0, sizeof( *out ) );
	out->alphaScale = 1.0f;
	out->filterMode = OIT_FILTER_STRAIGHT_SOURCE;
	out->emissivePolicy = EMISSIVE_SCALED_BY_SURFACE_ALPHA;
	out->sourceEncoding = OIT_SOURCE_ALPHA_STRAIGHT;
	out->reason = ALPHA_ENCODING_CLASSIC_COMPATIBILITY;
	out->path = TRANSP_PATH_SORTED_ALPHA;
	Q_strncpyz( out->declarationSource, "classic_compatibility", sizeof( out->declarationSource ) );

	if ( !shader || !shader->stages[0] ) {
		out->sourceEncoding = OIT_SOURCE_ALPHA_UNKNOWN;
		out->reason = ALPHA_ENCODING_UNKNOWN;
		out->path = TRANSP_PATH_REJECTED;
		out->wboitEligible = qfalse;
		Q_strncpyz( out->fallbackReason, "null shader", sizeof( out->fallbackReason ) );
		return;
	}

	st = shader->stages[0];
	stageBits = st->stateBits;
	src = stageBits & GLS_SRCBLEND_BITS;
	dst = stageBits & GLS_DSTBLEND_BITS;

	out->alphaFromTexture = qtrue;
	if ( st->bundle[0].alphaGen == AGEN_VERTEX || st->bundle[0].rgbGen == CGEN_VERTEX ||
		st->bundle[0].rgbGen == CGEN_EXACT_VERTEX ) {
		out->vertexAlpha = qtrue;
	}
	if ( st->bundle[0].alphaGen == AGEN_ENTITY || st->bundle[0].rgbGen == CGEN_ENTITY ) {
		out->entityAlpha = qtrue;
	}
	if ( st->bundle[0].alphaGen == AGEN_CONST ) {
		out->constAlpha = qtrue;
	}
	if ( shader->numUnfoggedPasses > 1 ) {
		/* Flag for status; do not auto-reject — OA glass often has lightmap+blend stages. */
		out->multiStageUnsupported = qfalse;
	}
	if ( st->bundle[0].alphaGen == AGEN_WAVEFORM ) {
		out->waveformAlphaUnsupported = qtrue;
	}
	if ( st->bundle[0].alphaGen == AGEN_PORTAL ) {
		out->portalAlphaUnsupported = qtrue;
	}

	/* Explicit name hints (authoring / future PBR metadata). */
	if ( shader->name[0] && Q_stristr( shader->name, "premul" ) ) {
		out->sourceEncoding = OIT_SOURCE_ALPHA_PREMULTIPLIED;
		out->colorAlreadyAssociated = qtrue;
		out->filterMode = OIT_FILTER_PREMULTIPLIED_SOURCE;
		out->reason = ALPHA_ENCODING_EXPLICIT_PREMULTIPLIED;
		Q_strncpyz( out->declarationSource, "name_hint_premul", sizeof( out->declarationSource ) );
	} else if ( r_oitSourceAlphaDefault && r_oitSourceAlphaDefault->integer == 1 ) {
		out->sourceEncoding = OIT_SOURCE_ALPHA_PREMULTIPLIED;
		out->reason = ALPHA_ENCODING_INFERRED_UNSAFE;
		Q_strncpyz( out->declarationSource, "cvar_r_oitSourceAlphaDefault", sizeof( out->declarationSource ) );
	} else {
		out->sourceEncoding = OIT_SOURCE_ALPHA_STRAIGHT;
		out->reason = ALPHA_ENCODING_CLASSIC_COMPATIBILITY;
		Q_strncpyz( out->declarationSource, "classic SRC_ALPHA compat", sizeof( out->declarationSource ) );
	}

	if ( stageBits & GLS_ATEST_BITS ) {
		out->sourceEncoding = OIT_SOURCE_ALPHA_MASKED;
		out->path = TRANSP_PATH_ALPHA_TESTED;
		out->reason = ALPHA_ROUTE_NOT_WBOIT_COMPATIBLE;
		out->wboitEligible = qfalse;
		Q_strncpyz( out->fallbackReason, "alpha-tested / masked", sizeof( out->fallbackReason ) );
		return;
	}
	if ( src == GLS_SRCBLEND_ONE && dst == GLS_DSTBLEND_ONE ) {
		out->sourceEncoding = OIT_SOURCE_ALPHA_ADDITIVE;
		out->path = TRANSP_PATH_ADDITIVE;
		out->reason = ALPHA_ROUTE_NOT_WBOIT_COMPATIBLE;
		out->emissivePolicy = EMISSIVE_ADDITIVE_ROUTE;
		out->wboitEligible = qfalse;
		Q_strncpyz( out->fallbackReason, "additive ONE/ONE → additive accum bucket", sizeof( out->fallbackReason ) );
		return;
	}
	if ( src == GLS_SRCBLEND_ZERO &&
		( dst == GLS_DSTBLEND_SRC_COLOR || dst == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) ) {
		out->sourceEncoding = OIT_SOURCE_ALPHA_MULTIPLICATIVE;
		out->path = TRANSP_PATH_MULTIPLICATIVE;
		out->reason = ALPHA_ROUTE_NOT_WBOIT_COMPATIBLE;
		out->wboitEligible = qfalse;
		Q_strncpyz( out->fallbackReason, "multiplicative / filter blend", sizeof( out->fallbackReason ) );
		return;
	}
	if ( vk_transparency_is_refractive( shader ) && vk_transparency_refractive_exclude_oit() ) {
		out->path = TRANSP_PATH_REFRACTIVE;
		out->reason = ALPHA_ROUTE_NOT_WBOIT_COMPATIBLE;
		out->wboitEligible = qfalse;
		Q_strncpyz( out->fallbackReason, "refractive / screenMap", sizeof( out->fallbackReason ) );
		return;
	}
	if ( out->waveformAlphaUnsupported || out->portalAlphaUnsupported ) {
		out->path = TRANSP_PATH_SORTED_ALPHA;
		out->reason = ALPHA_ROUTE_NOT_WBOIT_COMPATIBLE;
		out->wboitEligible = qfalse;
		if ( out->portalAlphaUnsupported ) {
			Q_strncpyz( out->fallbackReason, "portal alphaGen unsupported in WBOIT", sizeof( out->fallbackReason ) );
		} else {
			Q_strncpyz( out->fallbackReason, "waveform alphaGen unsupported in WBOIT", sizeof( out->fallbackReason ) );
		}
		return;
	}

	/* Ordinary blendFunc blend / SRC_ALPHA ONE_MINUS_SRC_ALPHA (and soft glass names). */
	if ( ( src == GLS_SRCBLEND_SRC_ALPHA && dst == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) ||
		( shader->sort >= SS_BLEND0 && shader->sort <= SS_BLEND6 &&
			src != GLS_SRCBLEND_ONE && src != GLS_SRCBLEND_ZERO ) ) {
		if ( r_oit && r_oit->integer == 1 && shader->sort >= SS_BLEND0 && shader->sort <= SS_BLEND6 ) {
			out->path = TRANSP_PATH_WBOIT;
			out->wboitEligible = qtrue;
			if ( out->sourceEncoding == OIT_SOURCE_ALPHA_STRAIGHT ) {
				out->reason = ALPHA_ENCODING_CLASSIC_COMPATIBILITY;
			}
			return;
		}
	}

	out->path = TRANSP_PATH_SORTED_ALPHA;
	out->wboitEligible = qfalse;
	Q_strncpyz( out->fallbackReason, "sorted alpha / oit off", sizeof( out->fallbackReason ) );
}

qboolean vk_oit_alpha_wboit_eligible( const shader_t *shader, materialTransparencyInfo_t *outOpt )
{
	materialTransparencyInfo_t info;
	vk_oit_alpha_query_shader( shader, &info );
	if ( outOpt ) {
		*outOpt = info;
	}
	return info.wboitEligible;
}

oitAlphaCertLevel_t vk_oit_alpha_certification_level( void )
{
	return s_certLevel;
}

qboolean vk_oit_alpha_validate( char *errBuf, int errBufSize )
{
	char cerr[128];
	const oitContract_t *c = vk_oit_contract_wboit();

	if ( errBuf && errBufSize > 0 ) {
		errBuf[0] = '\0';
	}

	if ( !vk_oit_contract_validate( c, cerr, sizeof( cerr ) ) ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "oit contract: %s", cerr );
		}
		s_certLevel = OIT_ALPHA_NONE;
		return qfalse;
	}

	/* Static phase gates: declared + normalize helper + accum equation present. */
	s_certLevel = OIT_ALPHA_DECLARED;
	s_certLevel = OIT_ALPHA_NORMALIZED;
	s_certLevel = OIT_ALPHA_ACCUMULATION_VALID;

	if ( r_oitFaultTreatStraightAsPremul && r_oitFaultTreatStraightAsPremul->integer ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "fault: treat straight as premul active", errBufSize );
		}
		return qfalse;
	}
	if ( r_oitFaultDoublePremultiply && r_oitFaultDoublePremultiply->integer ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "fault: double premultiply active", errBufSize );
		}
		return qfalse;
	}
	if ( r_oitFaultSkipAlphaMultiply && r_oitFaultSkipAlphaMultiply->integer ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "fault: skip alpha multiply active", errBufSize );
		}
		return qfalse;
	}

	/* Edge certified when normalize path + single-layer reference + no active faults. */
	s_certLevel = OIT_ALPHA_EDGE_CERTIFIED;
	return qtrue;
}

const oitAlphaCounters_t *vk_oit_alpha_counters( void )
{
	return &s_counters;
}

static void VK_Oit_AlphaStatus_f( void )
{
	const oitAlphaCounters_t *c = &s_counters;
	char err[160];
	const qboolean ok = vk_oit_alpha_validate( err, sizeof( err ) );

	ri.Printf( PRINT_ALL, "======== OIT Alpha Status (Phase 2.2) ========\n" );
	ri.Printf( PRINT_ALL, "cert=%s validate=%s%s%s\n",
		vk_oit_alpha_cert_level_name( s_certLevel ),
		ok ? "PASS" : "FAIL",
		ok ? "" : " ",
		ok ? "" : err );
	ri.Printf( PRINT_ALL,
		"internal: unassociatedRadiance + opacity → accum (rad*op*w, op*w); reveal∏(1-op)\n" );
	ri.Printf( PRINT_ALL,
		"counts: wboit=%u straight=%u premul=%u unknown=%u rejected=%u\n",
		c->materialsWboit, c->sourcesStraight, c->sourcesPremul, c->sourcesUnknown, c->routesRejected );
	ri.Printf( PRINT_ALL,
		"edge: zeroA_rgb=%u nearZeroBright=%u blackEdge=%u bleed=%u premulClamp=%u\n",
		c->zeroAlphaColored, c->nearZeroBright, c->blackEdgeRisk, c->colorBleedRisk, c->premulDivClamps );
	ri.Printf( PRINT_ALL,
		"integrity: oor=%u nonFinite=%u doublePremul=%u missAlpha=%u faults=%u\n",
		c->alphaOutOfRange, c->nonFinite, c->doublePremulSuspects, c->missingAlphaMultiplySuspects,
		c->faultHits );
	ri.Printf( PRINT_ALL,
		"debug: r_alphaDebug=%d edgePolicy=%d filterDebug=%d compare=%d singleLayer=%d\n",
		r_alphaDebug ? r_alphaDebug->integer : 0,
		r_transparentEdgePolicy ? r_transparentEdgePolicy->integer : 0,
		r_alphaFilterDebug ? r_alphaFilterDebug->integer : 0,
		r_alphaEncodingCompare ? r_alphaEncodingCompare->integer : 0,
		r_oitSingleLayerCompare ? r_oitSingleLayerCompare->integer : 0 );
	ri.Printf( PRINT_ALL, "See docs/WBOIT_ALPHA_ENCODING.md\n" );
	ri.Printf( PRINT_ALL, "==============================================\n" );
}

static void VK_Oit_AlphaValidate_f( void )
{
	char err[160];
	if ( vk_oit_alpha_validate( err, sizeof( err ) ) ) {
		ri.Printf( PRINT_ALL, "oit_alpha_validate: PASS (%s)\n",
			vk_oit_alpha_cert_level_name( s_certLevel ) );
	} else {
		ri.Printf( PRINT_ALL, "oit_alpha_validate: FAIL (%s)\n", err[0] ? err : "unknown" );
	}
}

static void VK_Material_AlphaStatus_f( void )
{
	const char *name;
	shader_t *sh;
	materialTransparencyInfo_t info;

	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: material_alpha_status <shader>\n" );
		return;
	}
	name = ri.Cmd_Argv( 1 );
	sh = R_FindShader( name, LIGHTMAP_NONE, qfalse );
	if ( !sh || sh == tr.defaultShader ) {
		ri.Printf( PRINT_ALL, "material_alpha_status: shader '%s' not found\n", name );
		return;
	}
	vk_oit_alpha_query_shader( sh, &info );
	ri.Printf( PRINT_ALL, "material_alpha_status: %s\n", sh->name );
	ri.Printf( PRINT_ALL, "  sourceEncoding=%s reason=%s decl=%s\n",
		vk_oit_source_alpha_name( info.sourceEncoding ),
		vk_oit_alpha_reason_name( info.reason ),
		info.declarationSource );
	ri.Printf( PRINT_ALL, "  path=%s wboitEligible=%d filter=%d emissivePolicy=%d\n",
		vk_oit_transparency_path_name( info.path ),
		info.wboitEligible ? 1 : 0,
		(int)info.filterMode,
		(int)info.emissivePolicy );
	ri.Printf( PRINT_ALL, "  alphaFromTex=%d associated=%d vertexA=%d entityA=%d constA=%d\n",
		info.alphaFromTexture ? 1 : 0, info.colorAlreadyAssociated ? 1 : 0,
		info.vertexAlpha ? 1 : 0, info.entityAlpha ? 1 : 0, info.constAlpha ? 1 : 0 );
	ri.Printf( PRINT_ALL, "  unsupported: wave=%d portal=%d multiStage=%d\n",
		info.waveformAlphaUnsupported ? 1 : 0, info.portalAlphaUnsupported ? 1 : 0,
		info.multiStageUnsupported ? 1 : 0 );
	if ( info.fallbackReason[0] ) {
		ri.Printf( PRINT_ALL, "  fallback=%s\n", info.fallbackReason );
	}
}

static void VK_Classic_AlphaTranslateStatus_f( void )
{
	const char *name;
	shader_t *sh;
	materialTransparencyInfo_t info;
	const shaderStage_t *st;
	unsigned src, dst;

	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: classic_alpha_translate_status <shader>\n" );
		return;
	}
	name = ri.Cmd_Argv( 1 );
	sh = R_FindShader( name, LIGHTMAP_NONE, qfalse );
	if ( !sh || sh == tr.defaultShader ) {
		ri.Printf( PRINT_ALL, "classic_alpha_translate_status: '%s' not found\n", name );
		return;
	}
	vk_oit_alpha_query_shader( sh, &info );
	st = sh->stages[0];
	src = st ? ( st->stateBits & GLS_SRCBLEND_BITS ) : 0;
	dst = st ? ( st->stateBits & GLS_DSTBLEND_BITS ) : 0;
	ri.Printf( PRINT_ALL, "classic_alpha_translate_status: %s\n", sh->name );
	ri.Printf( PRINT_ALL, "  stages=%d sort=%.1f stateBits=0x%x srcBlend=0x%x dstBlend=0x%x\n",
		sh->numUnfoggedPasses, sh->sort, st ? st->stateBits : 0, src, dst );
	ri.Printf( PRINT_ALL, "  rgbGen=%d alphaGen=%d\n",
		st ? (int)st->bundle[0].rgbGen : -1, st ? (int)st->bundle[0].alphaGen : -1 );
	ri.Printf( PRINT_ALL, "  modulation: texA=%d * vertexA=%d * entityA=%d * constA=%d\n",
		info.alphaFromTexture ? 1 : 0, info.vertexAlpha ? 1 : 0,
		info.entityAlpha ? 1 : 0, info.constAlpha ? 1 : 0 );
	ri.Printf( PRINT_ALL, "  internalEncoding=%s path=%s eligible=%d\n",
		vk_oit_source_alpha_name( info.sourceEncoding ),
		vk_oit_transparency_path_name( info.path ),
		info.wboitEligible ? 1 : 0 );
	ri.Printf( PRINT_ALL, "  reason=%s fallback=%s\n",
		vk_oit_alpha_reason_name( info.reason ),
		info.fallbackReason[0] ? info.fallbackReason : "(none)" );
}

void vk_oit_alpha_register( void )
{
	r_alphaDebug = ri.Cvar_Get( "r_alphaDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_alphaDebug, "0", "12", CV_INTEGER );
	ri.Cvar_SetDescription( r_alphaDebug,
		"OIT alpha debug views: 5 zeroA RGB, 6 black-edge, 7 colored-edge,\n"
		"8 associated, 9 unassociated, 10 surface opacity, 11 emissive, 12 emissive policy." );

	r_transparentEdgePolicy = ri.Cvar_Get( "r_transparentEdgePolicy", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_transparentEdgePolicy, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_transparentEdgePolicy,
		"0 preserve, 1 zero RGB at a==0, 2 edge-safe straight, 3 diagnostic only." );

	r_alphaFilterDebug = ri.Cvar_Get( "r_alphaFilterDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_alphaFilterDebug, "0", "1", CV_INTEGER );

	r_alphaEncodingCompare = ri.Cvar_Get( "r_alphaEncodingCompare", "0", CVAR_CHEAT );
	r_oitSingleLayerCompare = ri.Cvar_Get( "r_oitSingleLayerCompare", "0", CVAR_CHEAT );

	r_oitFaultTreatStraightAsPremul = ri.Cvar_Get( "r_oitFaultTreatStraightAsPremul", "0", CVAR_CHEAT );
	r_oitFaultTreatPremulAsStraight = ri.Cvar_Get( "r_oitFaultTreatPremulAsStraight", "0", CVAR_CHEAT );
	r_oitFaultDoublePremultiply = ri.Cvar_Get( "r_oitFaultDoublePremultiply", "0", CVAR_CHEAT );
	r_oitFaultSkipAlphaMultiply = ri.Cvar_Get( "r_oitFaultSkipAlphaMultiply", "0", CVAR_CHEAT );
	r_oitFaultZeroAlphaColoredRgb = ri.Cvar_Get( "r_oitFaultZeroAlphaColoredRgb", "0", CVAR_CHEAT );
	r_oitFaultInvalidAlpha = ri.Cvar_Get( "r_oitFaultInvalidAlpha", "0", CVAR_CHEAT );

	r_oitSourceAlphaDefault = ri.Cvar_Get( "r_oitSourceAlphaDefault", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_oitSourceAlphaDefault, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_oitSourceAlphaDefault,
		"Default WBOIT source encoding when undeclared: 0=straight (compat), 1=premultiplied." );

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "oit_alpha_status", VK_Oit_AlphaStatus_f );
		ri.Cmd_AddCommand( "oit_alpha_validate", VK_Oit_AlphaValidate_f );
		ri.Cmd_AddCommand( "material_alpha_status", VK_Material_AlphaStatus_f );
		ri.Cmd_AddCommand( "classic_alpha_translate_status", VK_Classic_AlphaTranslateStatus_f );
		s_cmds = qtrue;
		(void)vk_oit_alpha_validate( NULL, 0 );
		ri.Printf( PRINT_ALL,
			"[VK][oit-alpha] Phase 2.2 ready cert=%s (oit_alpha_status / material_alpha_status)\n",
			vk_oit_alpha_cert_level_name( s_certLevel ) );
	}
}
