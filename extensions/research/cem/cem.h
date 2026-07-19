#pragma once

/*
===========================================================================
3D Crack Element Method scaffold — Xie et al., arXiv:2508.04076.

CPU G_I / G_II evaluators (paper Eqs. 17–18) + ES-FEM CEM pipeline notes.
Not a world solver; engine DMM remains stress-grid debris (docs/PHYSICS.md).
===========================================================================
*/

#ifdef __cplusplus
extern "C" {
#endif

#define CEM_STAGE_COUNT   5
#define CEM_PATTERN_COUNT 6
#define CEM_GAP_COUNT     5

typedef enum {
	CEM_STATUS_PRESENT = 0,
	CEM_STATUS_PARTIAL,
	CEM_STATUS_ABSENT
} cem_status_t;

typedef struct {
	int id;
	const char *name;
	const char *summary;
} cem_stage_t;

typedef struct {
	int id;
	const char *name;       /* I .. VI */
	const char *element;    /* tet | hex */
	const char *summary;
} cem_pattern_t;

typedef struct {
	int id;
	const char *feature;
	cem_status_t status;
	const char *engineNote;
} cem_gap_t;

int Cem_StageCount( void );
const cem_stage_t *Cem_GetStage( int id );

int Cem_PatternCount( void );
const cem_pattern_t *Cem_GetPattern( int id );

/* Paper Eq. 17: G_I = 1/2 (σ̂_G3·δ̂1 + σ̂_G4·δ̂2) with projections on unit n. */
float Cem_EvalGI( const float n[3],
	const float delta1[3], const float delta2[3],
	const float sigmaG3[3], const float sigmaG4[3] );

/* Paper Eq. 18: G_II = 1/2 (σ̂_G6·δ̂1 + σ̂_G6·δ̂2). */
float Cem_EvalGII( const float n[3],
	const float delta1[3], const float delta2[3],
	const float sigmaG6[3] );

/* Tensile edge stretch (Eq. 16 simplified): (u_i - u_j) if lengthened, else 0. */
void Cem_EdgeStretch( float outDelta[3],
	const float uI[3], const float uJ[3],
	const float xI[3], const float xJ[3],
	const float XI[3], const float XJ[3] );

int Cem_ShouldFail( float G, float Gc ); /* 1 if G >= Gc */

float Cem_KalthoffGc( void );          /* 2.213e4 J/m^2 */
float Cem_BranchingPlateGc( void );   /* 3 J/m^2 */
float Cem_KalthoffYoungGPa( void );   /* 190 */
int Cem_NeumannBranchMeshFloor( void ); /* ~70000 elems */

int Cem_GapCount( void );
const cem_gap_t *Cem_GetGap( int id );

/* useCase: branch | mesh | limit | gpu */
const char *Cem_SelectAdvice( const char *useCase );
const char *Cem_PaperCite( void );

void Cem_ConsoleInit( void );
void Cem_ConsoleShutdown( void );

#ifdef __cplusplus
}
#endif
