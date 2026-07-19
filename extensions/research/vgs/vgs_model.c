/*
===========================================================================
VGS — pipeline stages, constants, gaps (McGraw MIG 2024).
===========================================================================
*/

#include "vgs/vgs_internal.h"

#include <ctype.h>
#include <string.h>

const vgs_stage_t vgs_stages[VGS_STAGE_COUNT] = {
	{ 0, "voxelize",  "Solid voxelization of watertight mesh; 8 particles/voxel, no sharing" },
	{ 1, "vgs",       "Voxel Gram-Schmidt project to volume-preserving parallelepiped" },
	{ 2, "faces",     "Breakable face-to-face constraints (3 partitions, 8 bytes each)" },
	{ 3, "collide",   "Particle collide + voxel-center augmented normals" },
	{ 4, "strain",    "Break faces when tensile/compressive strain limits exceeded" },
	{ 5, "embed",     "High-res mesh vertices in local voxel basis after deformation" },
};

const vgs_gap_t vgs_gaps[VGS_GAP_COUNT] = {
	{ 0, "VGS parallelepiped constraint", VGS_STATUS_ABSENT,
	  "softblob is XPBD distance lattice — no per-voxel Gram-Schmidt (phys_softblob)" },
	{ 1, "breakable face-to-face voxels", VGS_STATUS_ABSENT,
	  "DMM breaks to rigid debris boxes; softblob has no anisotropic face strain" },
	{ 2, "4 GS partitions (1 VGS + 3 face)", VGS_STATUS_ABSENT,
	  "No VGS/face compute shaders; cloth/softblob use serial XPBD iterations" },
	{ 3, "XPBD soft companions", VGS_STATUS_PRESENT,
	  "phys_softblob / xpbd_cloth POST solvers for deformable gameplay" },
	{ 4, "tet FEM (SCA09) path", VGS_STATUS_PARTIAL,
	  "RTFEM scaffold only (docs/RTFEM.md); VGS is stylized PBD alternative" },
};

static void vgs_tolower_copy( char *dst, size_t dstSize, const char *src )
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

static int vgs_streq_ci( const char *a, const char *b )
{
	char aa[32];
	char bb[32];

	vgs_tolower_copy( aa, sizeof( aa ), a );
	vgs_tolower_copy( bb, sizeof( bb ), b );
	return strcmp( aa, bb ) == 0;
}

int Vgs_StageCount( void )
{
	return VGS_STAGE_COUNT;
}

const vgs_stage_t *Vgs_GetStage( int id )
{
	if ( id < 0 || id >= VGS_STAGE_COUNT ) {
		return NULL;
	}
	return &vgs_stages[id];
}

float Vgs_DefaultAlpha( void )
{
	return 0.5f;
}

float Vgs_DefaultBeta( void )
{
	return 1.0f;
}

int Vgs_DefaultIters( void )
{
	return 3;
}

int Vgs_FacePartitionCount( void )
{
	return 3;
}

int Vgs_VgsPartitionCount( void )
{
	return 1;
}

int Vgs_FaceConstraintBytes( void )
{
	return 8;
}

int Vgs_GapCount( void )
{
	return VGS_GAP_COUNT;
}

const vgs_gap_t *Vgs_GetGap( int id )
{
	if ( id < 0 || id >= VGS_GAP_COUNT ) {
		return NULL;
	}
	return &vgs_gaps[id];
}

const char *Vgs_PaperCite( void )
{
	return "McGraw, Gram-Schmidt voxel constraints for real-time destructible "
		   "soft bodies, MIG '24, https://doi.org/10.1145/3677388.3696322";
}

const char *Vgs_SelectAdvice( const char *useCase )
{
	char key[32];

	if ( !useCase || !useCase[0] ) {
		useCase = "soft";
	}
	vgs_tolower_copy( key, sizeof( key ), useCase );

	if ( vgs_streq_ci( key, "fracture" ) || vgs_streq_ci( key, "break" ) ) {
		return "Use face strain limits {ε̂_c, ε̂_t} per axis for tear locations; "
			   "randomized ε̂ encourages chunks; infinite |ε̂| on skeleton keeps hangable core.";
	}
	if ( vgs_streq_ci( key, "aniso" ) || vgs_streq_ci( key, "sheets" ) ||
		 vgs_streq_ci( key, "sticks" ) ) {
		return "Delaminate sheets: |ε̂_x|≈|ε̂_y| ≫ |ε̂_z|. Matchsticks: |ε̂_x| ≫ |ε̂_y|≈|ε̂_z|. "
			   "Distance-transform shells: high |ε̂| when dist(vox_a)==dist(vox_b).";
	}
	if ( vgs_streq_ci( key, "limit" ) || vgs_streq_ci( key, "scope" ) ) {
		return "Scaffold + CPU Vgs_ProjectVoxel only. Shipping deformables: softblob/cloth. "
			   "No GPU PBD / mesh shaders yet — follow-up phys_vgs POST solver.";
	}
	/* soft / default */
	return "α≈0.5, vgs_it=3 for stiff ortho; lower α for compliant soft body. "
		   "β=1 equal edges (cube-like); β≪1 allows nonuniform edge lengths. "
		   "Today use SoftBlob_CreateLattice for jelly props (docs/PHYSICS.md).";
}
