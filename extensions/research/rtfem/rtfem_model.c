/*
===========================================================================
RTFEM — SCA 2009 pipeline stages + engine DMM gap table.
===========================================================================
*/

#include "rtfem/rtfem_internal.h"

#include <ctype.h>
#include <string.h>

const rtfem_stage_t rtfem_stages[RTFEM_STAGE_COUNT] = {
	{ 0, "embed",      "Tet mesh embeds high-res graphics via barycentric weights" },
	{ 1, "corotate",   "Per-element polar decomp F=QA; corotational infinitesimal strain" },
	{ 2, "invert",     "If volume <6% of reference, switch polar→QR to survive inversion" },
	{ 3, "plasticity", "Additive plastic strain with yield/creep/clamp (§3)" },
	{ 4, "assemble_K", "Parallel element Jij then noderow assembly; full sparse K" },
	{ 5, "integrate",  "Linearized backward Euler; Raleigh damping; CG solve" },
	{ 6, "islands",    "Live/asleep/kinematic islands; METIS partition; composite contacts" },
	{ 7, "fracture",   "Separation tensor; node replicate; no tet splitting (framerate)" },
	{ 8, "splinters",  "Artist splinters mask coarse fracture faces" },
	{ 9, "contact",    "Sweep-prune + damping-only overlap response (anti-jitter)" },
	{ 10, "friction",  "Implicit dynamic friction + explicit PID static friction" },
};

/*
 * Honest map vs shipping engine DMM / XPBD (docs/PHYSICS.md).
 */
const rtfem_gap_t rtfem_gaps[RTFEM_GAP_COUNT] = {
	{ 0, "tetrahedral mesh",           RTFEM_STATUS_ABSENT,
	  "Engine DMM uses axis-aligned stress grid / rigid proxy (phys_dmm / Box3D)" },
	{ 1, "corotational linear FEM",    RTFEM_STATUS_ABSENT,
	  "No element stiffness / polar corotation; softblob is XPBD lattice" },
	{ 2, "implicit CG solver",         RTFEM_STATUS_ABSENT,
	  "Cloth/softblob use XPBD Gauss–Seidel; unused DmmStruct_* is explicit+GS" },
	{ 3, "tet-boundary fracture",      RTFEM_STATUS_ABSENT,
	  "DMM fractures to rigid octant/Voronoi debris boxes, not FEM islands" },
	{ 4, "FEM connected islands",      RTFEM_STATUS_ABSENT,
	  "Box3D islandCount is rigid sleep islands, not tet components" },
	{ 5, "deform mesh → renderer",     RTFEM_STATUS_PARTIAL,
	  "Dmm_GetDeformMesh exists but indices/positions not driven by FEM" },
	{ 6, "damping contact (anti-chatter)", RTFEM_STATUS_PARTIAL,
	  "Rigid Soft Step contacts; soft companions use raycast penalties" },
	{ 7, "XPBD soft companions",       RTFEM_STATUS_PRESENT,
	  "phys_softblob / xpbd_cloth POST solvers for deformable gameplay" },
};

static void rtfem_tolower_copy( char *dst, size_t dstSize, const char *src )
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

static int rtfem_streq_ci( const char *a, const char *b )
{
	char aa[32];
	char bb[32];

	rtfem_tolower_copy( aa, sizeof( aa ), a );
	rtfem_tolower_copy( bb, sizeof( bb ), b );
	return strcmp( aa, bb ) == 0;
}

int RtFem_StageCount( void )
{
	return RTFEM_STAGE_COUNT;
}

const rtfem_stage_t *RtFem_GetStage( int id )
{
	if ( id < 0 || id >= RTFEM_STAGE_COUNT ) {
		return NULL;
	}
	return &rtfem_stages[id];
}

float RtFem_InvertVolumeThreshold( void )
{
	return 0.06f;
}

float RtFem_CgRelError( void )
{
	return 0.001f;
}

int RtFem_FractureMinTets( void )
{
	return 3;
}

float RtFem_FastObjectMoveFraction( void )
{
	return 1.0f / 6.0f;
}

int RtFem_GapCount( void )
{
	return RTFEM_GAP_COUNT;
}

const rtfem_gap_t *RtFem_GetGap( int id )
{
	if ( id < 0 || id >= RTFEM_GAP_COUNT ) {
		return NULL;
	}
	return &rtfem_gaps[id];
}

const char *RtFem_PaperCite( void )
{
	return "Parker & O'Brien, Real-Time Deformation and Fracture in a Game "
		   "Environment, SCA 2009 (Pixelux DMM / Star Wars: The Force Unleashed)";
}

const char *RtFem_SelectAdvice( const char *useCase )
{
	char key[32];

	if ( !useCase || !useCase[0] ) {
		useCase = "design";
	}
	rtfem_tolower_copy( key, sizeof( key ), useCase );

	if ( rtfem_streq_ci( key, "fracture" ) || rtfem_streq_ci( key, "break" ) ) {
		return "SCA09 uses fully dynamic FEM so stored elastic energy drives vibrant "
			   "fracture (Fig. 11). Engine DMM debris lacks that energy release; "
			   "prefer planned phys_rtfem for energetic breaks, current DMM for cheap props.";
	}
	if ( rtfem_streq_ci( key, "perf" ) || rtfem_streq_ci( key, "budget" ) ) {
		return "Paper avoids tet splitting so tet counts stay predictable for framerate. "
			   "Large islands (≥60 nodes and >¼ of live) use parallel CG; sleep islands "
			   "aggressively. Cap CG iterations as a material/content parameter.";
	}
	if ( rtfem_streq_ci( key, "limit" ) || rtfem_streq_ci( key, "scope" ) ) {
		return "This scaffold does not solve FEM. Shipping deformables today: XPBD "
			   "softblob/cloth; destructibles: stress-grid DMM → rigid fragments "
			   "(docs/PHYSICS.md). Follow-up: modules/physics/phys_rtfem POST solver.";
	}
	/* design / default */
	return "For soft continuous deformation use xpbd_cloth / softblob. For breakable "
		   "props use phys_dmm (not FEM). For SCA09-faithful corotational tets + "
		   "splinters, wait for phys_rtfem — see docs/RTFEM.md.";
}
