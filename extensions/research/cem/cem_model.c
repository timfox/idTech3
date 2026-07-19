/*
===========================================================================
CEM — pipeline stages, patterns I–VI, gaps (Xie et al. arXiv:2508.04076).
===========================================================================
*/

#include "cem/cem_internal.h"

#include <ctype.h>
#include <string.h>

const cem_stage_t cem_stages[CEM_STAGE_COUNT] = {
	{ 0, "esfem",   "ES-T-FEM / ES-H-FEM edge-smoothed discretization of variational fracture" },
	{ 1, "stretch", "Edge tensile stretch δ via Heaviside lengthening (Eq. 16)" },
	{ 2, "release", "Evaluate G_I…G_VI from projected stretch × max principal stress" },
	{ 3, "split",   "If G ≥ Gc: deactivate element or remesh remainder (prism → tets)" },
	{ 4, "newmark", "Explicit Newmark β=0 time integration (paper Eqs. 13–15)" },
};

const cem_pattern_t cem_patterns[CEM_PATTERN_COUNT] = {
	{ 0, "I",   "tet", "Quad plane G1-G2-G4-G3; G_I = ½(σ̂_G3·δ̂1 + σ̂_G4·δ̂2)" },
	{ 1, "II",  "tet", "Triangle G1-G2-G6; G_II = ½(σ̂_G6·δ̂1 + σ̂_G6·δ̂2)" },
	{ 2, "III", "hex", "Parallelepiped opposite edges; G_III follows G_I form" },
	{ 3, "IV",  "hex", "Opposite quadrature crack; may fully deactivate element" },
	{ 4, "V",   "hex", "Neighboring quadrature; remainder → triangular prism/tets" },
	{ 5, "VI",  "hex", "Neighboring quadrature; G_VI follows G_II form" },
};

const cem_gap_t cem_gaps[CEM_GAP_COUNT] = {
	{ 0, "ES-FEM topology-based G", CEM_STATUS_ABSENT,
	  "phys_dmm is stress-grid → rigid debris; no edge-smoothed G_I/G_II" },
	{ 1, "element split / deactivate", CEM_STATUS_PARTIAL,
	  "DMM deletes proxy and spawns boxes; not CEM prism remesh after split" },
	{ 2, "tet FEM (SCA09) path", CEM_STATUS_PARTIAL,
	  "RTFEM scaffold only (docs/RTFEM.md); different fracture energy path" },
	{ 3, "voxel PBD soft bodies", CEM_STATUS_ABSENT,
	  "VGS scaffold (docs/VGS.md) is stylized soft-body PBD, not CEM" },
	{ 4, "GPU Newmark CEM loop", CEM_STATUS_ABSENT,
	  "Paper uses NVIDIA GPU; engine has no CEM kernels (follow-up phys_cem)" },
};

static void cem_tolower_copy( char *dst, size_t dstSize, const char *src )
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

static int cem_streq_ci( const char *a, const char *b )
{
	char aa[32];
	char bb[32];

	cem_tolower_copy( aa, sizeof( aa ), a );
	cem_tolower_copy( bb, sizeof( bb ), b );
	return strcmp( aa, bb ) == 0;
}

int Cem_StageCount( void )
{
	return CEM_STAGE_COUNT;
}

const cem_stage_t *Cem_GetStage( int id )
{
	if ( id < 0 || id >= CEM_STAGE_COUNT ) {
		return NULL;
	}
	return &cem_stages[id];
}

int Cem_PatternCount( void )
{
	return CEM_PATTERN_COUNT;
}

const cem_pattern_t *Cem_GetPattern( int id )
{
	if ( id < 0 || id >= CEM_PATTERN_COUNT ) {
		return NULL;
	}
	return &cem_patterns[id];
}

float Cem_KalthoffGc( void )
{
	return 2.213e4f;
}

float Cem_BranchingPlateGc( void )
{
	return 3.0f;
}

float Cem_KalthoffYoungGPa( void )
{
	return 190.0f;
}

int Cem_NeumannBranchMeshFloor( void )
{
	return 70000;
}

int Cem_GapCount( void )
{
	return CEM_GAP_COUNT;
}

const cem_gap_t *Cem_GetGap( int id )
{
	if ( id < 0 || id >= CEM_GAP_COUNT ) {
		return NULL;
	}
	return &cem_gaps[id];
}

const char *Cem_PaperCite( void )
{
	return "Xie, Wu, Hu, Xu, Bui, Li, A GPU-accelerated 3D crack element method "
		   "for transient dynamic fracture, arXiv:2508.04076";
}

const char *Cem_SelectAdvice( const char *useCase )
{
	char key[32];

	if ( !useCase || !useCase[0] ) {
		useCase = "branch";
	}
	cem_tolower_copy( key, sizeof( key ), useCase );

	if ( cem_streq_ci( key, "mesh" ) || cem_streq_ci( key, "resolution" ) ) {
		return "Neumann branching needs ~70k+ tets; coarse meshes miss the branch point. "
			   "Dirichlet compact-tension branching is more robust even on coarser meshes.";
	}
	if ( cem_streq_ci( key, "limit" ) || cem_streq_ci( key, "scope" ) ) {
		return "Scaffold + CPU Cem_EvalGI/GII only. Shipping destructibles: phys_dmm. "
			   "No ES-FEM / GPU Newmark — follow-up phys_cem POST solver.";
	}
	if ( cem_streq_ci( key, "gpu" ) || cem_streq_ci( key, "hpc" ) ) {
		return "Paper runs all 3D benchmarks on NVIDIA GPUs. Engine scaffold has no CEM "
			   "kernels; use research profile docs only until phys_cem ships.";
	}
	/* branch / default */
	return "G ≥ Gc deactivates the element — branching emerges without a local criterion. "
		   "Prefer tet patterns I/II first; hex III–VI need remainder prism handling. "
		   "Today use phys_dmm for gameplay fracture (docs/PHYSICS.md).";
}
