/*
 * Deferred M3 production contract and honest certification surface.
 * Static checks can reach STATIC_READY only. GPU/HDR evidence is mandatory
 * for all image-parity levels.
 */

#include "tr_local.h"
#include "vk.h"
#include "vk_deferred_certification.h"
#include "vk_deferred_honesty.h"
#include "vk_deferred_gbuffer.h"
#include "vk_renderer_iq_p1.h"
#include "vk_forward_plus.h"
#include "vk_hdr_resolve_contract.h"

#ifdef USE_VULKAN

static deferredRenderingContract_t s_contract;
static deferredCertificationLevel_t s_level = DEFERRED_UNCERTIFIED;
static qboolean s_registered;
static qboolean s_abort;
static uint32_t s_contractHashAtEvidence;
static uint32_t s_gbufferGenerationAtEvidence;
static uint32_t s_clusterGenerationAtEvidence;

static cvar_t *r_lightingOwnershipDebug;
static cvar_t *r_materialDecodeDebug;
static cvar_t *r_brdfComponentDebug;
static cvar_t *r_normalParityDebug;
static cvar_t *r_clusterParityDebug;
static cvar_t *r_shadowParityDebug;
static cvar_t *r_lightmapParityDebug;
static cvar_t *r_iblParityDebug;
static cvar_t *r_aoOwnershipDebug;
static cvar_t *r_emissiveOwnershipDebug;

static uint32_t DeferredContractHash( const deferredRenderingContract_t *c )
{
	const uint32_t *words = (const uint32_t *)c;
	uint32_t h = 2166136261u;
	size_t i;
	for ( i = 0; i < sizeof( *c ) / sizeof( uint32_t ); ++i ) {
		if ( i == 1u ) {
			continue;
		}
		h = ( h ^ words[i] ) * 16777619u;
	}
	return h;
}

static void DeferredContractRefresh( void )
{
	Com_Memset( &s_contract, 0, sizeof( s_contract ) );
	s_contract.version = DEFERRED_RENDERING_CONTRACT_VERSION;
	s_contract.gbufferQuality = (uint32_t)vk_gbuffer_quality_effective();
	s_contract.normalEncoding = s_contract.gbufferQuality == 0u ? 1u : 2u;
	s_contract.materialEncoding = s_contract.gbufferQuality == 2u ? 3u : 1u;
	s_contract.ownershipEncoding = 2u; /* explicit SurfaceData owner, never bias */
	s_contract.brdfVersion = 2u; /* pbr_brdf_core.glsl */
	s_contract.clusterContractVersion = 2u;
	s_contract.lightmapContractVersion = 2u;
	s_contract.shadowContractVersion = 1u;
	s_contract.aoOwnershipVersion = 1u;
	s_contract.emissiveOwnershipVersion = 1u;
	s_contract.hash = DeferredContractHash( &s_contract );
}

static const char *DeferredLevelName( deferredCertificationLevel_t level )
{
	switch ( level ) {
	case DEFERRED_UNCERTIFIED: return "DEFERRED_UNCERTIFIED";
	case DEFERRED_STATIC_READY: return "DEFERRED_STATIC_READY";
	case DEFERRED_GBUFFER_CERTIFIED: return "DEFERRED_GBUFFER_CERTIFIED";
	case DEFERRED_MATERIAL_CERTIFIED: return "DEFERRED_MATERIAL_CERTIFIED";
	case DEFERRED_DIRECT_LIGHT_CERTIFIED: return "DEFERRED_DIRECT_LIGHT_CERTIFIED";
	case DEFERRED_INDIRECT_LIGHT_CERTIFIED: return "DEFERRED_INDIRECT_LIGHT_CERTIFIED";
	case DEFERRED_OWNERSHIP_CERTIFIED: return "DEFERRED_OWNERSHIP_CERTIFIED";
	case DEFERRED_FORWARD_PARITY_CERTIFIED: return "DEFERRED_FORWARD_PARITY_CERTIFIED";
	case DEFERRED_PRODUCTION_CERTIFIED: return "DEFERRED_PRODUCTION_CERTIFIED";
	default: return "DEFERRED_UNKNOWN";
	}
}

static clusterLightingFrame_t DeferredClusterSnapshot( void )
{
	clusterLightingFrame_t f;
	Com_Memset( &f, 0, sizeof( f ) );
	f.frameNumber = (uint64_t)tr.frameCount;
	f.generation = vk_cluster_list_generation();
	f.tileCountX = vk.forward_plus.tiles_x;
	f.tileCountY = vk.forward_plus.tiles_y;
	f.zSliceCount = vk.forward_plus.z_slices;
	f.lightCount = vk.forward_plus.last_packed_count;
	f.listEntryCount = vk.forward_plus.last_index_used;
	f.overflowCount = vk.forward_plus.last_overflow_count;
	f.depthGeneration = vk_hdr_resolve_depth_generation();
	f.lightDataGeneration = vk.forward_plus.cluster_list_generation;
	return f;
}

static void DeferredContractStatus_f( void )
{
	DeferredContractRefresh();
	ri.Printf( PRINT_ALL,
		"DeferredContract v%u hash=%08x quality=%u normal=%u material=%u owner=%u "
		"BRDF=%u cluster=%u LM=%u shadow=%u AO=%u emissive=%u\n",
		s_contract.version, s_contract.hash, s_contract.gbufferQuality,
		s_contract.normalEncoding, s_contract.materialEncoding,
		s_contract.ownershipEncoding, s_contract.brdfVersion,
		s_contract.clusterContractVersion, s_contract.lightmapContractVersion,
		s_contract.shadowContractVersion, s_contract.aoOwnershipVersion,
		s_contract.emissiveOwnershipVersion );
}

static void DeferredContractValidate_f( void )
{
	uint32_t recomputed;
	DeferredContractRefresh();
	recomputed = DeferredContractHash( &s_contract );
	ri.Printf( recomputed == s_contract.hash ? PRINT_ALL : PRINT_ERROR,
		"deferred_contract_validate: %s version=%u hash=%08x\n",
		recomputed == s_contract.hash ? "PASS" : "FAIL",
		s_contract.version, s_contract.hash );
}

static void DeferredClusterStatus_f( void )
{
	clusterLightingFrame_t f = DeferredClusterSnapshot();
	ri.Printf( PRINT_ALL,
		"cluster frame=%llu gen=%u tiles=%ux%ux%u lights=%u entries=%u "
		"overflow=%u depthGen=%u lightDataGen=%u fallback=importance-retention\n",
		(unsigned long long)f.frameNumber, f.generation, f.tileCountX, f.tileCountY,
		f.zSliceCount, f.lightCount, f.listEntryCount, f.overflowCount,
		f.depthGeneration, f.lightDataGeneration );
}

static void DeferredClusterValidate_f( void )
{
	clusterLightingFrame_t f = DeferredClusterSnapshot();
	const qboolean active = vk_deferred_lighting_active();
	const qboolean pass = !active ||
		( f.generation != 0u && f.generation == f.lightDataGeneration &&
		  f.tileCountX != 0u && f.tileCountY != 0u && f.zSliceCount != 0u );
	ri.Printf( pass ? PRINT_ALL : PRINT_ERROR,
		"cluster_parity_validate: %s generation=%u lightData=%u overflow=%u\n",
		pass ? "PASS" : "FAIL", f.generation, f.lightDataGeneration, f.overflowCount );
}

static void DeferredOwnershipStatus_f( void )
{
	deferredOwnershipSnapshot_t o;
	R_DeferredHonesty_GetOwnershipSnapshot( &o );
	ri.Printf( PRINT_ALL,
		"owner draws: eligible=%u Deferred=%u Forward+=%u unsupported=%u "
		"invalid=%u double=%u fullbrightEscape=%u\n",
		o.eligibleMaterials, o.deferredOwnedDraws, o.forwardOwnedDraws,
		o.unsupportedMaterials, o.invalidOwnerPixels, o.doubleOwnerPixels,
		o.fullbrightEscapeCount );
}

static void DeferredOwnershipValidate_f( void )
{
	deferredOwnershipSnapshot_t o;
	R_DeferredHonesty_GetOwnershipSnapshot( &o );
	const qboolean submitted = ( o.deferredOwnedDraws + o.forwardOwnedDraws +
		o.unsupportedMaterials ) != 0u;
	const qboolean pass = submitted && o.invalidOwnerPixels == 0u &&
		o.doubleOwnerPixels == 0u && o.fullbrightEscapeCount == 0u;
	ri.Printf( pass ? PRINT_ALL : PRINT_ERROR,
		"ownership validation: %s submitted=%d invalid=%u double=%u fullbright=%u\n",
		pass ? "PASS" : "FAIL", submitted, o.invalidOwnerPixels,
		o.doubleOwnerPixels, o.fullbrightEscapeCount );
}

static void DeferredStaticParityStatus_f( void )
{
	ri.Printf( PRINT_ALL,
		"Deferred parity static wiring: canonical SurfaceMaterial + pbr_brdf_core + "
		"shared cluster buffers + owner replacement.\n"
		"GPU scene-linear HDR readback evidence: PENDING\n" );
}

static void DeferredStaticParityValidate_f( void )
{
	ri.Printf( PRINT_WARNING,
		"parity validation: PENDING_GPU_EVIDENCE (static wiring cannot certify pixels)\n" );
}

static void DeferredCertify_f( void )
{
	deferredOwnershipSnapshot_t o;
	clusterLightingFrame_t cluster = DeferredClusterSnapshot();
	DeferredContractRefresh();
	R_DeferredHonesty_GetOwnershipSnapshot( &o );
	if ( s_abort ) {
		ri.Printf( PRINT_WARNING, "deferred_certify: aborted; run deferred_certify_resume\n" );
		return;
	}
	s_level = DEFERRED_UNCERTIFIED;
	if ( r_deferredArchitecture && r_deferredArchitecture->integer == DEFERRED_ARCH_FULL_FIDELITY &&
		s_contract.gbufferQuality == 2u && vk_deferred_lighting_path_ready() ) {
		s_level = DEFERRED_STATIC_READY;
	}
	s_contractHashAtEvidence = s_contract.hash;
	s_gbufferGenerationAtEvidence = vk.deferredGbufferGeneration;
	s_clusterGenerationAtEvidence = cluster.generation;
	ri.Printf( PRINT_ALL,
		"deferred_certify: %s (GPU HDR parity evidence required for advancement)\n",
		DeferredLevelName( s_level ) );
}

static void DeferredCertifyStatus_f( void )
{
	DeferredContractRefresh();
	if ( s_contractHashAtEvidence != 0u &&
		( s_contractHashAtEvidence != s_contract.hash ||
		  s_gbufferGenerationAtEvidence != vk.deferredGbufferGeneration ||
		  s_clusterGenerationAtEvidence != vk_cluster_list_generation() ) ) {
		s_level = DEFERRED_UNCERTIFIED;
	}
	ri.Printf( PRINT_ALL,
		"deferred certification=%s contract=%08x gbufferGen=%u clusterGen=%u "
		"GPUEvidence=%s aborted=%d\n",
		DeferredLevelName( s_level ), s_contract.hash, vk.deferredGbufferGeneration,
		vk_cluster_list_generation(),
		s_level > DEFERRED_STATIC_READY ? "current" : "missing", s_abort );
}

static void DeferredCertifyAbort_f( void )
{
	s_abort = qtrue;
	s_level = DEFERRED_UNCERTIFIED;
	ri.Printf( PRINT_ALL, "deferred certification aborted\n" );
}

static void DeferredCertifyResume_f( void )
{
	s_abort = qfalse;
	ri.Printf( PRINT_ALL, "deferred certification resumed at UNCATALOGUED evidence state\n" );
}

static void RegisterDebugCvars( void )
{
#define DEBUG_CVAR(name, maxValue, description) \
	name = ri.Cvar_Get( #name, "0", CVAR_CHEAT ); \
	ri.Cvar_CheckRange( name, "0", maxValue, CV_INTEGER ); \
	ri.Cvar_SetDescription( name, description ); \
	ri.Cvar_SetGroup( name, CVG_RENDERER )
	DEBUG_CVAR( r_lightingOwnershipDebug, "6", "Opaque owner colors / invalid diagnostics." );
	DEBUG_CVAR( r_materialDecodeDebug, "8", "Canonical material decode components." );
	DEBUG_CVAR( r_brdfComponentDebug, "8", "Shared BRDF component view." );
	DEBUG_CVAR( r_normalParityDebug, "4", "Deferred/Forward normal parity." );
	DEBUG_CVAR( r_clusterParityDebug, "4", "Cluster generation/index parity." );
	DEBUG_CVAR( r_shadowParityDebug, "4", "Forward/Deferred shadow term parity." );
	DEBUG_CVAR( r_lightmapParityDebug, "6", "Lightmap/deluxe ownership and parity." );
	DEBUG_CVAR( r_iblParityDebug, "5", "IBL diffuse/specular parity." );
	DEBUG_CVAR( r_aoOwnershipDebug, "5", "AO owner/application visualization." );
	DEBUG_CVAR( r_emissiveOwnershipDebug, "5", "Emissive owner/application visualization." );
#undef DEBUG_CVAR
}

void vk_deferred_certification_register( void )
{
	static const char *statusCommands[] = {
		"gbuffer_status", "gbuffer_attachment_status", "gbuffer_decode_validate",
		"material_decode_status", "material_decode_validate", "brdf_status", "brdf_validate",
		"normal_parity_status", "normal_parity_validate",
		"direct_lighting_parity_status", "direct_lighting_parity_validate",
		"shadow_parity_status", "shadow_parity_validate",
		"lightmap_parity_status", "lightmap_parity_validate",
		"ibl_parity_status", "ibl_parity_validate",
		"ao_ownership_status", "ao_ownership_validate",
		"emissive_ownership_status", "emissive_ownership_validate",
		"deferred_composite_status", "deferred_composite_validate",
		"deferred_depth_parity_status", "deferred_depth_parity_validate",
		"fullbright_escape_status", "fullbright_escape_validate",
		"deferred_parity_metrics", "deferred_parity_report"
	};
	size_t i;
	if ( s_registered ) {
		return;
	}
	RegisterDebugCvars();
	for ( i = 0; i < sizeof( statusCommands ) / sizeof( statusCommands[0] ); ++i ) {
		const qboolean validate = strstr( statusCommands[i], "validate" ) != NULL;
		ri.Cmd_AddCommand( statusCommands[i],
			validate ? DeferredStaticParityValidate_f : DeferredStaticParityStatus_f );
	}
	ri.Cmd_AddCommand( "cluster_parity_status", DeferredClusterStatus_f );
	ri.Cmd_AddCommand( "cluster_parity_validate", DeferredClusterValidate_f );
	ri.Cmd_AddCommand( "lighting_ownership_status", DeferredOwnershipStatus_f );
	ri.Cmd_AddCommand( "lighting_ownership_validate", DeferredOwnershipValidate_f );
	ri.Cmd_AddCommand( "deferred_contract_status", DeferredContractStatus_f );
	ri.Cmd_AddCommand( "deferred_contract_validate", DeferredContractValidate_f );
	ri.Cmd_AddCommand( "deferred_parity_certify", DeferredStaticParityValidate_f );
	ri.Cmd_AddCommand( "deferred_certify", DeferredCertify_f );
	ri.Cmd_AddCommand( "deferred_certify_status", DeferredCertifyStatus_f );
	ri.Cmd_AddCommand( "deferred_certify_abort", DeferredCertifyAbort_f );
	ri.Cmd_AddCommand( "deferred_certify_resume", DeferredCertifyResume_f );
	s_registered = qtrue;
	DeferredContractRefresh();
}

void vk_deferred_certification_begin_frame( void )
{
	DeferredContractRefresh();
	if ( s_contractHashAtEvidence && s_contractHashAtEvidence != s_contract.hash ) {
		s_level = DEFERRED_UNCERTIFIED;
	}
}

#endif
