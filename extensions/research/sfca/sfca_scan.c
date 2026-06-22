/*
===========================================================================
SFCA parameter scans: phase diagram, transition ridge, fingerprints, KM, damage.
===========================================================================
*/

#include "sfca/sfca_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void SFCA_IntervalsFromLevels( int sLowLvl, int sHighLvl, int bLowLvl, int bHighLvl,
	sfca_intervals_t *iv )
{
	const float inv = 1.0f / (float)SFCA_GRID_LEVELS;
	iv->sLow = (float)sLowLvl * inv;
	iv->sHigh = (float)sHighLvl * inv;
	iv->bLow = (float)bLowLvl * inv;
	iv->bHigh = (float)bHighLvl * inv;
}

void SFCA_GeometryWidthRule( int wSLvl, int wBLvl, int deltaLowLvl, int sLowLvl,
	sfca_intervals_t *iv )
{
	SFCA_IntervalsFromLevels( sLowLvl, sLowLvl + wSLvl, sLowLvl + deltaLowLvl,
		sLowLvl + deltaLowLvl + wBLvl, iv );
}

int SFCA_EnumerateWidthRules( int wSLvl, int wBLvl, sfca_intervals_t *rules, int maxRules )
{
	int sLow;
	int bLow;
	int count = 0;

	if ( wSLvl < 1 || wBLvl < 1 || !rules || maxRules <= 0 ) {
		return 0;
	}

	for ( sLow = 0; sLow + wSLvl <= SFCA_GRID_LEVELS; sLow++ ) {
		for ( bLow = 0; bLow + wBLvl <= SFCA_GRID_LEVELS; bLow++ ) {
			if ( sLow >= bLow ) {
				continue;
			}
			if ( count >= maxRules ) {
				return count;
			}
			SFCA_IntervalsFromLevels( sLow, sLow + wSLvl, bLow, bLow + wBLvl, &rules[count] );
			count++;
		}
	}
	return count;
}

void SFCA_ScanWidthCellBatch( const sfca_run_params_t *base, int wSLvl, int wBLvl,
	int runsPerRule, sfca_outcome_stats_t *stats )
{
	sfca_intervals_t rules[512];
	const int nRules = SFCA_EnumerateWidthRules( wSLvl, wBLvl, rules, 512 );
	sfca_run_params_t p;
	int i;
	int r;
	unsigned rng = base->seed ? base->seed : 1u;

	memset( stats, 0, sizeof( *stats ) );
	if ( nRules <= 0 || runsPerRule <= 0 ) {
		return;
	}

	p = *base;

	for ( i = 0; i < nRules; i++ ) {
		for ( r = 0; r < runsPerRule; r++ ) {
			sfca_run_result_t res;
			p.intervals = rules[i];
			p.seed = rng;
			rng = rng * 1664525u + 1013904223u;
			SFCA_Run( &p, &res );
			stats->numRuns++;
			switch ( res.outcome ) {
			case SFCA_OUTCOME_EXTINCTION: stats->extinction += 1.0f; break;
			case SFCA_OUTCOME_FIXED_POINT: stats->fixedPoint += 1.0f; break;
			case SFCA_OUTCOME_CYCLE: stats->cycle += 1.0f; break;
			default: stats->longTransient += 1.0f; break;
			}
		}
	}

	if ( stats->numRuns > 0 ) {
		const float inv = 1.0f / (float)stats->numRuns;
		stats->extinction *= inv;
		stats->fixedPoint *= inv;
		stats->cycle *= inv;
		stats->longTransient *= inv;
	}
}

void SFCA_ScanTransitionAxis( const sfca_run_params_t *base, int wSLvlMin, int wSLvlMax,
	int wSLvlStep, int runsPerPoint, sfca_transition_scan_t *out, int maxPoints )
{
	sfca_run_params_t p;
	int idx = 0;
	int w;

	if ( !out || maxPoints <= 0 || runsPerPoint <= 0 || wSLvlStep <= 0 ) {
		return;
	}

	p = *base;

	for ( w = wSLvlMin; w <= wSLvlMax && idx < maxPoints; w += wSLvlStep ) {
		sfca_transition_scan_t *pt = &out[idx];
		const float sWidth = (float)w / (float)SFCA_FINE_LEVELS;

		memset( pt, 0, sizeof( *pt ) );
		pt->wSLvl = w;
		SFCA_CanonicalTransitionRule( sWidth, &p.intervals );
		pt->geometry = SFCA_IntervalGeometry( &p.intervals );
		p.seed = (unsigned)( base->seed + (unsigned)w );
		SFCA_RunBatch( &p, runsPerPoint, &pt->outcomes, NULL );
		idx++;
	}
}

void SFCA_CycleFingerprints( const sfca_run_params_t *base, int numRuns,
	sfca_cycle_fingerprint_t *fp )
{
	sfca_run_params_t p;
	unsigned rng = base->seed ? base->seed : 1u;
	int r;

	memset( fp, 0, sizeof( *fp ) );
	p = *base;

	for ( r = 0; r < numRuns; r++ ) {
		sfca_run_result_t res;
		p.seed = rng;
		rng = rng * 1664525u + 1013904223u;
		SFCA_Run( &p, &res );
		if ( res.outcome != SFCA_OUTCOME_CYCLE ) {
			continue;
		}
		fp->count++;
		fp->meanDensity += res.finalDensity;
		fp->meanChangeRate += res.meanChangeRate;
		fp->meanStripe += res.stripeScore;
	}

	if ( fp->count > 0 ) {
		const float inv = 1.0f / (float)fp->count;
		fp->meanDensity *= inv;
		fp->meanChangeRate *= inv;
		fp->meanStripe *= inv;
	}
}

void SFCA_KaplanMeier( const sfca_run_params_t *base, int numRuns,
	const int *generations, int numGenerations, float *survival )
{
	sfca_run_params_t p;
	unsigned rng = base->seed ? base->seed : 1u;
	int *tau;
	int r;
	int g;

	if ( !generations || !survival || numRuns <= 0 || numGenerations <= 0 ) {
		return;
	}

	tau = (int *)malloc( (size_t)numRuns * sizeof( int ) );
	if ( !tau ) {
		return;
	}

	p = *base;
	for ( r = 0; r < numRuns; r++ ) {
		sfca_run_result_t res;
		p.seed = rng;
		rng = rng * 1664525u + 1013904223u;
		SFCA_Run( &p, &res );
		if ( res.outcome == SFCA_OUTCOME_LONG_TRANSIENT ) {
			tau[r] = p.maxGenerations + 1;
		} else {
			tau[r] = res.terminalGeneration;
		}
	}

	for ( g = 0; g < numGenerations; g++ ) {
		const int tCheck = generations[g];
		int alive = 0;
		for ( r = 0; r < numRuns; r++ ) {
			if ( tau[r] > tCheck ) {
				alive++;
			}
		}
		survival[g] = (float)alive / (float)numRuns;
	}

	free( tau );
}

float SFCA_DamageSpreadMean( const sfca_run_params_t *params, int numPairs, unsigned seed )
{
	unsigned rng = seed ? seed : 1u;
	float sum = 0.0f;
	int i;

	if ( numPairs <= 0 ) {
		return 0.0f;
	}

	for ( i = 0; i < numPairs; i++ ) {
		sum += SFCA_DamageSpreadFinal( params, rng );
		rng = rng * 1664525u + 1013904223u;
	}
	return sum / (float)numPairs;
}

void SFCA_ScanDamageAxis( const sfca_run_params_t *base, int wSLvlMin, int wSLvlMax,
	int wSLvlStep, int pairsPerPoint, sfca_damage_scan_t *out, int maxPoints )
{
	sfca_run_params_t p;
	int idx = 0;
	int w;

	if ( !out || maxPoints <= 0 || pairsPerPoint <= 0 || wSLvlStep <= 0 ) {
		return;
	}

	p = *base;

	for ( w = wSLvlMin; w <= wSLvlMax && idx < maxPoints; w += wSLvlStep ) {
		sfca_damage_scan_t *pt = &out[idx];
		const float sWidth = (float)w / (float)SFCA_FINE_LEVELS;

		pt->wSLvl = w;
		SFCA_CanonicalTransitionRule( sWidth, &p.intervals );
		pt->plateauHamming = SFCA_DamageSpreadMean( &p, pairsPerPoint,
			(unsigned)( base->seed + (unsigned)w * 17u ) );
		idx++;
	}
}

float SFCA_FieldStdDevMean( const sfca_run_params_t *base, int numRuns, int sampleGenerations )
{
	sfca_workspace_t ws;
	sfca_run_params_t p;
	unsigned rng = base->seed ? base->seed : 1u;
	float sumStd = 0.0f;
	int valid = 0;
	int r;

	if ( numRuns <= 0 || sampleGenerations <= 0 ) {
		return 0.0f;
	}

	p = *base;
	ws.height = p.height;
	ws.width = p.width;

	for ( r = 0; r < numRuns; r++ ) {
		const int cells = p.height * p.width;
		int t;
		int i;

		if ( cells <= 0 || cells > SFCA_MAX_CELLS ) {
			continue;
		}

		SFCA_InitRandom( ws.cur, p.height, p.width, p.rho0, &rng );

		for ( t = 0; t < sampleGenerations; t++ ) {
			float meanQ = 0.0f;
			float varQ = 0.0f;

			SFCA_ComputeField( &ws, &p.intervals );
			for ( i = 0; i < cells; i++ ) {
				meanQ += ws.qField[i];
			}
			meanQ /= (float)cells;
			for ( i = 0; i < cells; i++ ) {
				const float d = ws.qField[i] - meanQ;
				varQ += d * d;
			}
			varQ /= (float)cells;
			sumStd += sqrtf( varQ );
			valid++;
			SFCA_Step( &ws, &p.intervals );
		}
	}

	return ( valid > 0 ) ? ( sumStd / (float)valid ) : 0.0f;
}
