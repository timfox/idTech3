#pragma once

/*
===========================================================================
Real-Time FEM scaffold — Parker & O'Brien, SCA 2009.

Corotational tet FEM + fracture design notes (Force Unleashed / Pixelux DMM).
Not a shipping FEM solver; documents paper constants and gaps vs engine DMM.
===========================================================================
*/

#ifdef __cplusplus
extern "C" {
#endif

#define RTFEM_STAGE_COUNT 11
#define RTFEM_GAP_COUNT   8

typedef enum {
	RTFEM_STATUS_PRESENT = 0,
	RTFEM_STATUS_PARTIAL,
	RTFEM_STATUS_ABSENT
} rtfem_status_t;

typedef struct {
	int id;
	const char *name;
	const char *summary;
} rtfem_stage_t;

typedef struct {
	int id;
	const char *feature;
	rtfem_status_t status;
	const char *engineNote;
} rtfem_gap_t;

int RtFem_StageCount( void );
const rtfem_stage_t *RtFem_GetStage( int id );

float RtFem_InvertVolumeThreshold( void );   /* 0.06 */
float RtFem_CgRelError( void );              /* 0.001 */
int RtFem_FractureMinTets( void );           /* 3 */
float RtFem_FastObjectMoveFraction( void );  /* 1/6 */

/* Paper §4: large if nodes>=60 AND nodes > liveTotal/4. */
int RtFem_LargeIslandHeuristic( int islandNodes, int liveTotalNodes );

int RtFem_GapCount( void );
const rtfem_gap_t *RtFem_GetGap( int id );

/* useCase: design | fracture | perf | limit */
const char *RtFem_SelectAdvice( const char *useCase );
const char *RtFem_PaperCite( void );

void RtFem_ConsoleInit( void );
void RtFem_ConsoleShutdown( void );

#ifdef __cplusplus
}
#endif
