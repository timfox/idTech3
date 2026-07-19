#pragma once

/*
===========================================================================
Gram-Schmidt Voxel Constraints scaffold — McGraw, MIG 2024.

CPU VGS projector (paper Alg. 1) + PBD pipeline notes. Not a world solver;
engine softblob remains XPBD distance lattice (docs/PHYSICS.md).
===========================================================================
*/

#ifdef __cplusplus
extern "C" {
#endif

#define VGS_STAGE_COUNT 6
#define VGS_GAP_COUNT   5
#define VGS_CORNERS     8

typedef enum {
	VGS_STATUS_PRESENT = 0,
	VGS_STATUS_PARTIAL,
	VGS_STATUS_ABSENT
} vgs_status_t;

typedef struct {
	int id;
	const char *name;
	const char *summary;
} vgs_stage_t;

typedef struct {
	int id;
	const char *feature;
	vgs_status_t status;
	const char *engineNote;
} vgs_gap_t;

int Vgs_StageCount( void );
const vgs_stage_t *Vgs_GetStage( int id );

float Vgs_DefaultAlpha( void );   /* 0.5 */
float Vgs_DefaultBeta( void );    /* 1.0 rigid-ish; soft uses lower */
int Vgs_DefaultIters( void );     /* 3 */
int Vgs_FacePartitionCount( void ); /* 3 */
int Vgs_VgsPartitionCount( void );  /* 1 */
int Vgs_FaceConstraintBytes( void ); /* 8 */

/*
 * Project 8 corner positions (xyz interleaved: p[0..2]=corner0, …).
 * w[i]==0 leaves corner i fixed. v0Volume = rest parallelepiped volume.
 * r = particle radius (paper uses L/4). Returns 0 on success.
 */
int Vgs_ProjectVoxel( float alpha, float beta, int vgsIt,
	float *p /* 24 floats */, const float *w /* 8 */, float r, float v0Volume );

float Vgs_ParallelepipedVolume( const float *p /* 24 */ );

int Vgs_GapCount( void );
const vgs_gap_t *Vgs_GetGap( int id );

/* useCase: soft | fracture | aniso | limit */
const char *Vgs_SelectAdvice( const char *useCase );
const char *Vgs_PaperCite( void );

void Vgs_ConsoleInit( void );
void Vgs_ConsoleShutdown( void );

#ifdef __cplusplus
}
#endif
