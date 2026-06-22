#pragma once

/*
===========================================================================
Separable-Field Cellular Automaton (SFCA).

Shi & Huang — normalized rank-one row–column field CA; survival/birth intervals.
===========================================================================
*/

#include "qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SFCA_GRID_LEVELS 18
#define SFCA_FINE_LEVELS 180

typedef enum {
	SFCA_OUTCOME_EXTINCTION = 0,
	SFCA_OUTCOME_FIXED_POINT,
	SFCA_OUTCOME_CYCLE,
	SFCA_OUTCOME_LONG_TRANSIENT
} sfca_outcome_t;

typedef enum {
	SFCA_GEOM_B_IN_S = 0,
	SFCA_GEOM_PARTIAL_OVERLAP,
	SFCA_GEOM_NO_OVERLAP
} sfca_interval_geom_t;

typedef struct {
	float sLow;
	float sHigh;
	float bLow;
	float bHigh;
} sfca_intervals_t;

typedef struct {
	int height;
	int width;
	float rho0;
	int maxGenerations;
	unsigned seed;
	sfca_intervals_t intervals;
} sfca_run_params_t;

typedef struct {
	sfca_outcome_t outcome;
	int period;
	int terminalGeneration;
	float finalDensity;
	float meanDensity;
	float meanChangeRate;
	float stripeScore;
} sfca_run_result_t;

typedef struct {
	float extinction;
	float fixedPoint;
	float cycle;
	float longTransient;
	int numRuns;
} sfca_outcome_stats_t;

typedef struct {
	int wSLvl;
	sfca_interval_geom_t geometry;
	sfca_outcome_stats_t outcomes;
} sfca_transition_scan_t;

typedef struct {
	int count;
	float meanDensity;
	float meanChangeRate;
	float meanStripe;
} sfca_cycle_fingerprint_t;

typedef struct {
	int wSLvl;
	float plateauHamming;
} sfca_damage_scan_t;

void SFCA_DefaultRepresentativeRule( sfca_intervals_t *iv );
void SFCA_CanonicalTransitionRule( float sWidth, sfca_intervals_t *iv );

sfca_interval_geom_t SFCA_IntervalGeometry( const sfca_intervals_t *iv );

void SFCA_InitRandom( byte *grid, int height, int width, float rho0, unsigned *rng );
int SFCA_CountAlive( const byte *grid, int height, int width );
float SFCA_Density( const byte *grid, int height, int width );
float SFCA_StripeScore( const byte *grid, int height, int width );

void SFCA_Run( const sfca_run_params_t *params, sfca_run_result_t *result );
void SFCA_RunBatch( const sfca_run_params_t *params, int numRuns,
	sfca_outcome_stats_t *stats, sfca_run_result_t *lastCycle );

float SFCA_DamageSpreadFinal( const sfca_run_params_t *params, unsigned seed );
float SFCA_DamageSpreadMean( const sfca_run_params_t *params, int numPairs, unsigned seed );

void SFCA_IntervalsFromLevels( int sLowLvl, int sHighLvl, int bLowLvl, int bHighLvl,
	sfca_intervals_t *iv );
void SFCA_GeometryWidthRule( int wSLvl, int wBLvl, int deltaLowLvl, int sLowLvl,
	sfca_intervals_t *iv );
int SFCA_EnumerateWidthRules( int wSLvl, int wBLvl, sfca_intervals_t *rules, int maxRules );

void SFCA_ScanWidthCellBatch( const sfca_run_params_t *base, int wSLvl, int wBLvl,
	int runsPerRule, sfca_outcome_stats_t *stats );
void SFCA_ScanTransitionAxis( const sfca_run_params_t *base, int wSLvlMin, int wSLvlMax,
	int wSLvlStep, int runsPerPoint, sfca_transition_scan_t *out, int maxPoints );
void SFCA_CycleFingerprints( const sfca_run_params_t *base, int numRuns,
	sfca_cycle_fingerprint_t *fp );
void SFCA_KaplanMeier( const sfca_run_params_t *base, int numRuns,
	const int *generations, int numGenerations, float *survival );
void SFCA_ScanDamageAxis( const sfca_run_params_t *base, int wSLvlMin, int wSLvlMax,
	int wSLvlStep, int pairsPerPoint, sfca_damage_scan_t *out, int maxPoints );
float SFCA_FieldStdDevMean( const sfca_run_params_t *base, int numRuns, int sampleGenerations );

void SFCA_ConsoleInit( void );
void SFCA_ConsoleShutdown( void );

#ifdef __cplusplus
}
#endif
