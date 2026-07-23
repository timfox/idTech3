/*
===========================================================================
Color Pipeline Phase 2.5.1 — bounded WBOIT weight-function contract freeze.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_oit_weight_contract.h"

static qboolean s_cmds;
static oitWeightContract_t s_frozen;

uint32_t vk_oit_weight_contract_compute_hash( const oitWeightContract_t *c )
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

const char *vk_oit_weight_mode_name( oitWeightMode_t m )
{
	switch ( m ) {
	case OIT_WEIGHT_ALPHA_REFERENCE: return "alpha_reference";
	case OIT_WEIGHT_LEGACY_DEPTH: return "legacy_depth";
	case OIT_WEIGHT_BOUNDED_PRODUCTION: return "bounded_production";
	case OIT_WEIGHT_MATERIAL_RESEARCH: return "material_research";
	default: return "unknown";
	}
}

static void VK_OitWeight_FillFrozen( oitWeightContract_t *c )
{
	Com_Memset( c, 0, sizeof( *c ) );

	/*
	 * Matches live oit_accum.frag McGuire moderated form after Phase 2.3.2
	 * view-depth migration — now named and hash-frozen as BOUNDED_PRODUCTION:
	 *   aFactor = pow(min(1, alpha*10) + minimumOpacityContribution, alphaExponent)
	 *   zFactor = pow(1 - zTrad*depthScale, depthExponent)
	 *   w = clamp(aFactor * 1e3 * zFactor, minWeight, maxWeight)
	 * Implicit frozen scales: alphaGain=10, luminanceScale=1e3 (see docs).
	 */
	c->mode = OIT_WEIGHT_BOUNDED_PRODUCTION;
	c->minWeight = 1e-2f;
	c->maxWeight = 3e3f;
	c->alphaExponent = 3.0f;
	c->depthExponent = 3.0f;
	c->depthScale = 0.9f;
	c->minimumOpacityContribution = 0.01f;
	c->nearClamp = 8.0f;     /* r_znear default */
	c->farClamp = 8192.0f;   /* typical viewParms.zFar floor */
	c->usesPositiveViewDepth = 1u;
	c->contractVersion = OIT_WEIGHT_CONTRACT_VERSION;
	c->contractHash = 0u;
	c->contractHash = vk_oit_weight_contract_compute_hash( c );
}

const oitWeightContract_t *vk_oit_weight_contract_get( void )
{
	static qboolean inited;
	if ( !inited ) {
		VK_OitWeight_FillFrozen( &s_frozen );
		inited = qtrue;
	}
	return &s_frozen;
}

qboolean vk_oit_weight_contract_validate( const oitWeightContract_t *c, char *errBuf, int errBufSize )
{
	oitWeightContract_t rebuilt;

	if ( errBuf && errBufSize > 0 ) {
		errBuf[0] = '\0';
	}
	if ( !c ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "null contract", errBufSize );
		}
		return qfalse;
	}
	if ( c->contractVersion != OIT_WEIGHT_CONTRACT_VERSION ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "version %u != %u",
				c->contractVersion, OIT_WEIGHT_CONTRACT_VERSION );
		}
		return qfalse;
	}
	if ( c->mode != OIT_WEIGHT_BOUNDED_PRODUCTION ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "production mode must be BOUNDED_PRODUCTION", errBufSize );
		}
		return qfalse;
	}
	if ( c->usesPositiveViewDepth == 0u ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "must use positive view-depth", errBufSize );
		}
		return qfalse;
	}
	if ( !( c->minWeight > 0.0f ) || !( c->maxWeight > c->minWeight ) ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "invalid weight bounds", errBufSize );
		}
		return qfalse;
	}
	if ( !( c->nearClamp > 0.0f ) || !( c->farClamp > c->nearClamp ) ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "invalid near/far clamps", errBufSize );
		}
		return qfalse;
	}

	VK_OitWeight_FillFrozen( &rebuilt );
	if ( rebuilt.contractHash != c->contractHash ||
		rebuilt.minWeight != c->minWeight ||
		rebuilt.maxWeight != c->maxWeight ||
		rebuilt.alphaExponent != c->alphaExponent ||
		rebuilt.depthExponent != c->depthExponent ||
		rebuilt.depthScale != c->depthScale ||
		rebuilt.minimumOpacityContribution != c->minimumOpacityContribution ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "frozen hash / coeff drift", errBufSize );
		}
		return qfalse;
	}
	return qtrue;
}

float vk_oit_weight_evaluate( const oitWeightContract_t *c, float opacity, float positiveViewDepth )
{
	float a, zTrad, aFactor, zFactor, w;
	float zn, zf;

	if ( !c ) {
		c = vk_oit_weight_contract_get();
	}
	a = opacity;
	if ( a < 0.0f ) {
		a = 0.0f;
	} else if ( a > 1.0f ) {
		a = 1.0f;
	}

	if ( c->mode == OIT_WEIGHT_ALPHA_REFERENCE ) {
		w = a;
		if ( w < c->minWeight ) {
			w = c->minWeight;
		}
		if ( w > c->maxWeight ) {
			w = c->maxWeight;
		}
		return w;
	}

	/* BOUNDED_PRODUCTION (+ LEGACY_DEPTH shares coeffs; depth already view-linear). */
	zn = c->nearClamp;
	zf = c->farClamp;
	if ( !( zf > zn ) ) {
		zn = 8.0f;
		zf = 8192.0f;
	}
	zTrad = ( positiveViewDepth - zn ) / ( zf - zn );
	if ( zTrad < 0.0f ) {
		zTrad = 0.0f;
	} else if ( zTrad > 1.0f ) {
		zTrad = 1.0f;
	}

	{
		float t = a * 10.0f;
		if ( t > 1.0f ) {
			t = 1.0f;
		}
		aFactor = powf( t + c->minimumOpacityContribution, c->alphaExponent );
	}
	zFactor = powf( 1.0f - zTrad * c->depthScale, c->depthExponent );
	w = aFactor * 1e3f * zFactor;
	if ( w < c->minWeight ) {
		w = c->minWeight;
	} else if ( w > c->maxWeight ) {
		w = c->maxWeight;
	}
	return w;
}

void vk_oit_weight_contract_print( const oitWeightContract_t *c )
{
	char err[160];
	const oitWeightContract_t *base = c ? c : vk_oit_weight_contract_get();
	const qboolean ok = vk_oit_weight_contract_validate( base, err, sizeof( err ) );
	float wNear, wFar, wMid;

	wNear = vk_oit_weight_evaluate( base, 0.5f, base->nearClamp );
	wFar = vk_oit_weight_evaluate( base, 0.5f, base->farClamp );
	wMid = vk_oit_weight_evaluate( base, 0.5f, 0.5f * ( base->nearClamp + base->farClamp ) );

	ri.Printf( PRINT_ALL, "======== OIT Weight Contract (Phase 2.5.1) ========\n" );
	ri.Printf( PRINT_ALL, "version=%u hash=0x%08x validate=%s%s%s\n",
		base->contractVersion, base->contractHash,
		ok ? "PASS" : "FAIL", ok ? "" : " reason=", ok ? "" : err );
	ri.Printf( PRINT_ALL, "mode=%s usesPositiveViewDepth=%u\n",
		vk_oit_weight_mode_name( base->mode ), base->usesPositiveViewDepth );
	ri.Printf( PRINT_ALL, "bounds: min=%g max=%g\n", base->minWeight, base->maxWeight );
	ri.Printf( PRINT_ALL, "exponents: alpha=%g depth=%g depthScale=%g minOpacityContrib=%g\n",
		base->alphaExponent, base->depthExponent, base->depthScale,
		base->minimumOpacityContribution );
	ri.Printf( PRINT_ALL, "depthClamps: near=%g far=%g\n", base->nearClamp, base->farClamp );
	ri.Printf( PRINT_ALL, "sample(alpha=0.5): w(near)=%g w(mid)=%g w(far)=%g\n",
		wNear, wMid, wFar );
	ri.Printf( PRINT_ALL,
		"equation:\n"
		"  zTrad  = saturate((viewDepth-near)/(far-near))\n"
		"  aFactor= pow(min(1, alpha*10)+minOpacity, alphaExp)\n"
		"  zFactor= pow(1 - zTrad*depthScale, depthExp)\n"
		"  w      = clamp(aFactor*1e3*zFactor, min, max)\n" );
	ri.Printf( PRINT_ALL, "docs: docs/WBOIT_WEIGHT_CONTRACT.md\n" );
	ri.Printf( PRINT_ALL, "===================================================\n" );
}

static void VK_OitWeight_Status_f( void )
{
	vk_oit_weight_contract_print( vk_oit_weight_contract_get() );
}

static void VK_OitWeight_Validate_f( void )
{
	char err[160];
	if ( vk_oit_weight_contract_validate( vk_oit_weight_contract_get(), err, sizeof( err ) ) ) {
		ri.Printf( PRINT_ALL, "oit_weight_validate: PASS (v%u hash=0x%08x)\n",
			OIT_WEIGHT_CONTRACT_VERSION, vk_oit_weight_contract_get()->contractHash );
	} else {
		ri.Printf( PRINT_ALL, "oit_weight_validate: FAIL (%s)\n", err[0] ? err : "unknown" );
	}
}

void vk_oit_weight_contract_register( void )
{
	(void)vk_oit_weight_contract_get();
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "oit_weight_status", VK_OitWeight_Status_f );
		ri.Cmd_AddCommand( "oit_weight_validate", VK_OitWeight_Validate_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL,
		"[VK][OIT] Phase 2.5.1 weight contract frozen v%u hash=0x%08x mode=%s "
		"(oit_weight_status / oit_weight_validate)\n",
		OIT_WEIGHT_CONTRACT_VERSION, s_frozen.contractHash,
		vk_oit_weight_mode_name( s_frozen.mode ) );
}
