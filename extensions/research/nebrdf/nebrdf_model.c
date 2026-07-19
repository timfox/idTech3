/*
===========================================================================
NEBRDF — GGX graph, final hats, param sizes, comparison rows (Shen et al.).
===========================================================================
*/

#include "nebrdf/nebrdf_internal.h"

#include <ctype.h>
#include <string.h>

/*
 * Paper Fig. 2 / §7: 11 nodes/operators in the GGX computational subgraph.
 * Final enhanced: M+S·D·F̂·Ĝ·(1/E)̂ with mul of F×G also neural.
 * Ids used in enhancement order: 1/E=5, G=4, mul_FG=9, F=3.
 */
const nebrdf_node_t nebrdf_nodes[NEBRDF_NODE_COUNT] = {
	{ 0, "M",   "Lambertian M",           NEBRDF_KIND_NODE, 0 },
	{ 1, "S",   "specular albedo S",      NEBRDF_KIND_NODE, 0 },
	{ 2, "D",   "microfacet D (GGX)",     NEBRDF_KIND_NODE, 0 },
	{ 3, "F",   "Fresnel F",              NEBRDF_KIND_NODE, 1 },
	{ 4, "G",   "geometric G",            NEBRDF_KIND_NODE, 1 },
	{ 5, "1/E", "reciprocal 1/E",         NEBRDF_KIND_NODE, 1 },
	{ 6, "+",   "add (diffuse+spec)",     NEBRDF_KIND_OP,   0 },
	{ 7, "*",   "mul S·D",                NEBRDF_KIND_OP,   0 },
	{ 8, "*",   "mul ·D·",                NEBRDF_KIND_OP,   0 },
	{ 9, "*",   "mul F×G",                NEBRDF_KIND_OP,   1 },
	{ 10, "*",  "mul ·(1/E)",             NEBRDF_KIND_OP,   0 },
};

/* Fig. 1 boosting order: E → G → mul(F×G) → F */
const int nebrdf_enhance_order[NEBRDF_ENHANCE_ORDER_LEN] = { 5, 4, 9, 3 };

/*
 * Subset of Fig. 5 published SSIM / ΔE_ITP (×10^3) for unit ordering checks.
 */
const nebrdf_compare_row_t nebrdf_compare_rows[NEBRDF_COMPARE_COUNT] = {
	{ "CHM LIGHT BLUE",     0.984f, 2.37f, 0.914f, 3.88f },
	{ "VCH ULTRA PINK",     0.980f, 4.96f, 0.969f, 5.51f },
	{ "ILM L3 37 METALLIC", 0.952f, 1.55f, 0.901f, 2.34f },
	{ "M002 CAR PAINT01",   0.995f, 0.70f, 0.992f, 0.72f },
	{ "M003 CARPET01",      0.996f, 0.67f, 0.958f, 1.73f },
};

static void nebrdf_tolower_copy( char *dst, size_t dstSize, const char *src )
{
	size_t i;

	if ( !dst || dstSize == 0 ) {
		return;
	}
	for ( i = 0; src && src[i] && i + 1 < dstSize; i++ ) {
		dst[i] = (char)tolower( (unsigned char)src[i] );
	}
	dst[i] = '\0';
}

static int nebrdf_streq_ci( const char *a, const char *b )
{
	char aa[32];
	char bb[32];

	nebrdf_tolower_copy( aa, sizeof( aa ), a );
	nebrdf_tolower_copy( bb, sizeof( bb ), b );
	return strcmp( aa, bb ) == 0;
}

int NeBrdf_NodeCount( void )
{
	return NEBRDF_NODE_COUNT;
}

const nebrdf_node_t *NeBrdf_GetNode( int id )
{
	if ( id < 0 || id >= NEBRDF_NODE_COUNT ) {
		return NULL;
	}
	return &nebrdf_nodes[id];
}

unsigned int NeBrdf_FinalStateMask( void )
{
	unsigned int mask = 0;
	int i;

	for ( i = 0; i < NEBRDF_NODE_COUNT; i++ ) {
		if ( nebrdf_nodes[i].neuralInFinal ) {
			mask |= ( 1u << i );
		}
	}
	return mask;
}

int NeBrdf_IsNeural( int nodeId )
{
	const nebrdf_node_t *n = NeBrdf_GetNode( nodeId );
	return n ? n->neuralInFinal : 0;
}

void NeBrdf_ParamCounts( nebrdf_param_counts_t *out )
{
	if ( !out ) {
		return;
	}
	out->analyticalParams = 12;
	out->neuralParams = 27;
	out->totalParams = 39;
	out->weightApprox = 7000;
	out->sizeKB = 26.45f;
	out->mlpH0 = 16;
	out->mlpH1 = 32;
	out->mlpH2 = 16;
	out->mlpLayers = 4;
}

const char *NeBrdf_FinalFormula( void )
{
	return "M+S·D·F̂·Ĝ·(1/E)̂  (mul F×G also neural)";
}

int NeBrdf_EpochsBetweenStateChanges( void )
{
	return 30; /* main experiments §4.2 */
}

int NeBrdf_CompareCount( void )
{
	return NEBRDF_COMPARE_COUNT;
}

const nebrdf_compare_row_t *NeBrdf_CompareRow( int id )
{
	if ( id < 0 || id >= NEBRDF_COMPARE_COUNT ) {
		return NULL;
	}
	return &nebrdf_compare_rows[id];
}

const char *NeBrdf_SelectAdvice( const char *useCase )
{
	char key[32];

	if ( !useCase || !useCase[0] ) {
		useCase = "fit";
	}
	nebrdf_tolower_copy( key, sizeof( key ), useCase );

	if ( nebrdf_streq_ci( key, "render" ) || nebrdf_streq_ci( key, "shader" ) ) {
		return "Translate hats to small MLPs in existing GGX shader code; "
			   "importance sampling still uses refit to analytical GGX (§5.2). "
			   "Engine shipping path remains analytic GGX (docs/NEBRDF.md).";
	}
	if ( nebrdf_streq_ci( key, "edit" ) || nebrdf_streq_ci( key, "albedo" ) ) {
		return "Diffuse/specular albedos ρd/ρs remain analytical after enhancement, "
			   "so color/intensity edits need no analytical refit for those terms (Fig. 12).";
	}
	if ( nebrdf_streq_ci( key, "limit" ) || nebrdf_streq_ci( key, "ood" ) ) {
		return "Fails when the BRDF cannot be represented by the input analytical "
			   "model or differs substantially from training data (e.g. color-changing "
			   "malachite, Fig. 14). Prefer per-material NBRDF in those cases.";
	}
	/* fit / default */
	return "Prefer neural-enhanced GGX for unified fitting of measured isotropic/"
		   "anisotropic BRDFs with fixed network + per-material params (39 total). "
		   "Use vanilla GGX for cheapest eval; NBRDF when dedicating a net per material.";
}
