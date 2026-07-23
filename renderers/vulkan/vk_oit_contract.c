/*
===========================================================================
Production WBOIT contract freeze — Color Pipeline Phase 2.1.
Mirrors shaders (oit_accum.frag / oit_resolve.frag) + pipeline blend state.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_oit_contract.h"
#include "vk_color_contract.h"

static qboolean s_cmds;
static oitContract_t s_wboit;

/*
 * Hash covers every field that defines compositing math. Bump
 * OIT_CONTRACT_VERSION and docs/WBOIT_CONTRACT.md when this changes.
 */
uint32_t vk_oit_contract_compute_hash( const oitContract_t *c )
{
	uint32_t h = 2166136261u;
	const unsigned char *p;
	size_t n;
	size_t i;

	if ( !c ) {
		return 0u;
	}

	/* FNV-1a over a packed POD prefix (exclude nothing — full struct). */
	p = (const unsigned char *)c;
	n = sizeof( *c ) - sizeof( c->contractHash );
	for ( i = 0; i < n; i++ ) {
		h ^= p[i];
		h *= 16777619u;
	}
	return h;
}

static void VK_Oit_Contract_FillWboit( oitContract_t *c )
{
	Com_Memset( c, 0, sizeof( *c ) );

	c->sourceAlphaEncoding = OIT_ALPHA_STRAIGHT;
	c->accumulationMode = OIT_ACCUM_WEIGHTED_COLOR;
	c->revealageMode = OIT_REVEAL_PRODUCT_ONE_MINUS_ALPHA;
	c->weightMode = OIT_WEIGHT_MCGUIRE_MODERATED;
	c->depthConvention = OIT_DEPTH_REVERSED_Z_GREATER_OR_EQUAL;
	c->resolveMode = OIT_RESOLVE_MCGUIRE_BAVOIL;

	c->sceneLinear = qtrue;
	c->preExposed = qfalse;
	c->premultipliedRadiance = qtrue;
	c->fogAppliedPerFragment = qtrue; /* production default r_oitFogMode 1 */
	c->emptyPixelPreservesOpaque = qtrue;
	c->additiveSkipsRevealage = qtrue;

	c->accumFormat = (uint32_t)VK_FORMAT_R16G16B16A16_SFLOAT;
	c->revealageFormat = (uint32_t)VK_FORMAT_R16_SFLOAT;

	c->accumClear[0] = 0.0f;
	c->accumClear[1] = 0.0f;
	c->accumClear[2] = 0.0f;
	c->accumClear[3] = 0.0f;
	c->revealageClear = 1.0f;

	c->accumSrcColorBlend = (uint32_t)VK_BLEND_FACTOR_ONE;
	c->accumDstColorBlend = (uint32_t)VK_BLEND_FACTOR_ONE;
	c->revealSrcColorBlend = (uint32_t)VK_BLEND_FACTOR_ZERO;
	c->revealDstColorBlend = (uint32_t)VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	c->depthCompareOp = (uint32_t)VK_COMPARE_OP_GREATER_OR_EQUAL;
	c->depthWrite = qfalse;

	c->contractVersion = OIT_CONTRACT_VERSION;
	c->contractHash = 0u;
	c->contractHash = vk_oit_contract_compute_hash( c );
}

const oitContract_t *vk_oit_contract_wboit( void )
{
	static qboolean inited;
	if ( !inited ) {
		VK_Oit_Contract_FillWboit( &s_wboit );
		inited = qtrue;
	}
	return &s_wboit;
}

const char *vk_oit_alpha_encoding_name( oitAlphaEncoding_t e )
{
	switch ( e ) {
	case OIT_ALPHA_STRAIGHT: return "straight";
	case OIT_ALPHA_PREMULTIPLIED: return "premultiplied";
	default: return "unknown";
	}
}

const char *vk_oit_accum_mode_name( oitAccumulationMode_t m )
{
	switch ( m ) {
	case OIT_ACCUM_WEIGHTED_COLOR: return "weighted_color_(C*a*w,a*w)";
	default: return "unknown";
	}
}

const char *vk_oit_revealage_mode_name( oitRevealageMode_t m )
{
	switch ( m ) {
	case OIT_REVEAL_PRODUCT_ONE_MINUS_ALPHA: return "product(1-alpha)";
	default: return "unknown";
	}
}

const char *vk_oit_weight_mode_name( oitWeightMode_t m )
{
	switch ( m ) {
	case OIT_WEIGHT_MCGUIRE_MODERATED:
		return "mcguire_moderated aFactor*1e3*zFactor clamp[1e-2,3e3]";
	default: return "unknown";
	}
}

const char *vk_oit_resolve_mode_name( oitResolveMode_t m )
{
	switch ( m ) {
	case OIT_RESOLVE_MCGUIRE_BAVOIL:
		return "C_avg=accum.rgb/max(a,eps); C=C_avg*(1-R)+bg*R";
	default: return "unknown";
	}
}

qboolean vk_oit_contract_validate( const oitContract_t *c, char *errBuf, int errBufSize )
{
	uint32_t h;
	const oitContract_t *frozen;

	if ( errBuf && errBufSize > 0 ) {
		errBuf[0] = '\0';
	}
	if ( !c ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "null contract", errBufSize );
		}
		return qfalse;
	}

	frozen = vk_oit_contract_wboit();
	if ( c->contractVersion != OIT_CONTRACT_VERSION ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "version %u != %u",
				c->contractVersion, OIT_CONTRACT_VERSION );
		}
		return qfalse;
	}

	h = vk_oit_contract_compute_hash( c );
	if ( h != c->contractHash ) {
		if ( errBuf && errBufSize > 0 ) {
			Com_sprintf( errBuf, errBufSize, "hash mismatch got=0x%08x stored=0x%08x",
				h, c->contractHash );
		}
		return qfalse;
	}

	if ( !c->sceneLinear || c->preExposed ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "must be scene-linear and not pre-exposed", errBufSize );
		}
		return qfalse;
	}
	if ( c->sourceAlphaEncoding != OIT_ALPHA_STRAIGHT ||
		!c->premultipliedRadiance ||
		c->revealageMode != OIT_REVEAL_PRODUCT_ONE_MINUS_ALPHA ||
		c->resolveMode != OIT_RESOLVE_MCGUIRE_BAVOIL ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "WBOIT alpha/accum/reveal/resolve mode drift", errBufSize );
		}
		return qfalse;
	}
	if ( c->accumFormat != (uint32_t)VK_FORMAT_R16G16B16A16_SFLOAT ||
		c->revealageFormat != (uint32_t)VK_FORMAT_R16_SFLOAT ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "accum/reveal format drift", errBufSize );
		}
		return qfalse;
	}
	if ( c->accumClear[0] != 0.0f || c->accumClear[1] != 0.0f ||
		c->accumClear[2] != 0.0f || c->accumClear[3] != 0.0f ||
		c->revealageClear != 1.0f ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "clear value drift", errBufSize );
		}
		return qfalse;
	}
	if ( c->accumSrcColorBlend != (uint32_t)VK_BLEND_FACTOR_ONE ||
		c->accumDstColorBlend != (uint32_t)VK_BLEND_FACTOR_ONE ||
		c->revealSrcColorBlend != (uint32_t)VK_BLEND_FACTOR_ZERO ||
		c->revealDstColorBlend != (uint32_t)VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "blend factor drift", errBufSize );
		}
		return qfalse;
	}
	if ( c->contractHash != frozen->contractHash ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "frozen hash mismatch", errBufSize );
		}
		return qfalse;
	}
	return qtrue;
}

void vk_oit_contract_print( const oitContract_t *c )
{
	char err[128];
	const qboolean ok = vk_oit_contract_validate( c, err, sizeof( err ) );
	const int fogMode = ri.Cvar_VariableIntegerValue( "r_oitFogMode" );
	const int oit = r_oit ? r_oit->integer : 0;

	if ( !c ) {
		ri.Printf( PRINT_ALL, "oit_contract: null\n" );
		return;
	}

	ri.Printf( PRINT_ALL, "======== WBOIT Contract (Phase 2.1 freeze) ========\n" );
	ri.Printf( PRINT_ALL, "version=%u hash=0x%08x validate=%s%s%s\n",
		c->contractVersion, c->contractHash,
		ok ? "PASS" : "FAIL",
		ok ? "" : " reason=",
		ok ? "" : err );
	ri.Printf( PRINT_ALL, "policy: r_oit=%d (%s) MBOIT experimental until same cert\n",
		oit, ( oit == 1 ) ? "WBOIT production" : ( oit == 2 ) ? "MBOIT experimental" : "off" );
	ri.Printf( PRINT_ALL, "space: sceneLinear=%d preExposed=%d (expect SCENE_LINEAR_HDR, not pre-exposed)\n",
		c->sceneLinear ? 1 : 0, c->preExposed ? 1 : 0 );
	ri.Printf( PRINT_ALL, "alpha: source=%s accumPremulRadiance=%d\n",
		vk_oit_alpha_encoding_name( c->sourceAlphaEncoding ),
		c->premultipliedRadiance ? 1 : 0 );
	ri.Printf( PRINT_ALL, "accum: mode=%s format=R16G16B16A16_SFLOAT clear=(%.0f,%.0f,%.0f,%.0f)\n",
		vk_oit_accum_mode_name( c->accumulationMode ),
		c->accumClear[0], c->accumClear[1], c->accumClear[2], c->accumClear[3] );
	ri.Printf( PRINT_ALL, "  blend RT0: src=ONE dst=ONE (additive weighted sum)\n" );
	ri.Printf( PRINT_ALL, "reveal: mode=%s format=R16_SFLOAT clear=%.0f\n",
		vk_oit_revealage_mode_name( c->revealageMode ), c->revealageClear );
	ri.Printf( PRINT_ALL, "  blend RT1: src=ZERO dst=ONE_MINUS_SRC_COLOR (shader out=alpha)\n" );
	ri.Printf( PRINT_ALL, "weight: %s\n", vk_oit_weight_mode_name( c->weightMode ) );
	ri.Printf( PRINT_ALL, "depth: reversed-Z GREATER_OR_EQUAL write=%d\n", c->depthWrite ? 1 : 0 );
	ri.Printf( PRINT_ALL,
		"fog: contractPerFragment=%d runtime r_oitFogMode=%d "
		"(T=exp(-density*viewDepth); lit*=T; no resolve fog)\n",
		c->fogAppliedPerFragment ? 1 : 0, fogMode );
	ri.Printf( PRINT_ALL, "resolve: %s\n", vk_oit_resolve_mode_name( c->resolveMode ) );
	ri.Printf( PRINT_ALL, "  emptyPixelPreservesOpaque=%d additiveSkipsRevealage=%d\n",
		c->emptyPixelPreservesOpaque ? 1 : 0, c->additiveSkipsRevealage ? 1 : 0 );
	ri.Printf( PRINT_ALL,
		"equations:\n"
		"  accum.rgb = lit * alpha * w;  accum.a = alpha * w\n"
		"  reveal'   = reveal * (1 - alpha)   [via ZERO / ONE_MINUS_SRC_COLOR]\n"
		"  C_avg     = accum.rgb / max(accum.a, eps)\n"
		"  C_out     = C_avg * (1 - reveal) + C_opaque * reveal\n"
		"  empty     -> C_opaque (never black)\n" );
	ri.Printf( PRINT_ALL, "color stage: OIT_ACCUM/RESOLVE → %s (see color_pipeline_status)\n",
		vk_color_space_name( VK_COLOR_SPACE_SCENE_LINEAR_HDR ) );
	ri.Printf( PRINT_ALL, "docs: docs/WBOIT_CONTRACT.md · docs/COLOR_PIPELINE.md\n" );
	ri.Printf( PRINT_ALL, "===================================================\n" );
}

static void VK_Oit_ContractStatus_f( void )
{
	vk_oit_contract_print( vk_oit_contract_wboit() );
}

static void VK_Oit_ContractValidate_f( void )
{
	char err[160];
	if ( vk_oit_contract_validate( vk_oit_contract_wboit(), err, sizeof( err ) ) ) {
		ri.Printf( PRINT_ALL, "oit_contract_validate: PASS (v%u hash=0x%08x)\n",
			OIT_CONTRACT_VERSION, vk_oit_contract_wboit()->contractHash );
	} else {
		ri.Printf( PRINT_ALL, "oit_contract_validate: FAIL (%s)\n", err[0] ? err : "unknown" );
	}
}

void vk_oit_contract_register( void )
{
	(void)vk_oit_contract_wboit();
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "oit_contract_status", VK_Oit_ContractStatus_f );
		ri.Cmd_AddCommand( "oit_contract_validate", VK_Oit_ContractValidate_f );
		s_cmds = qtrue;
		ri.Printf( PRINT_ALL,
			"[VK][oit] Phase 2.1 WBOIT contract frozen v%u hash=0x%08x "
			"(oit_contract_status / oit_contract_validate)\n",
			OIT_CONTRACT_VERSION, s_wboit.contractHash );
	}
}
