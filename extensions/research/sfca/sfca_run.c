/*
===========================================================================
SFCA trajectory simulation, outcome classification, batch statistics.
===========================================================================
*/

#include "sfca/sfca_internal.h"

#include <stdlib.h>
#include <string.h>

static int SFCA_GridBytes( int height, int width )
{
	return height * width;
}

static qboolean SFCA_GridEqual( const byte *a, const byte *b, int n )
{
	int i;
	for ( i = 0; i < n; i++ ) {
		if ( ( a[i] & 1 ) != ( b[i] & 1 ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

void SFCA_Run( const sfca_run_params_t *params, sfca_run_result_t *result )
{
	sfca_workspace_t ws;
	byte *history = NULL;
	const int cells = SFCA_GridBytes( params->height, params->width );
	unsigned rng = params->seed ? params->seed : 1u;
	int t;
	int hLen = 0;
	float densAcc = 0.0f;
	float chiAcc = 0.0f;
	byte prev[SFCA_MAX_CELLS];

	memset( result, 0, sizeof( *result ) );

	if ( cells <= 0 || cells > SFCA_MAX_CELLS || params->maxGenerations <= 0 ) {
		result->outcome = SFCA_OUTCOME_LONG_TRANSIENT;
		return;
	}

	history = (byte *)malloc( (size_t)cells * (size_t)SFCA_MAX_HISTORY );
	if ( !history ) {
		result->outcome = SFCA_OUTCOME_LONG_TRANSIENT;
		return;
	}

	ws.height = params->height;
	ws.width = params->width;
	memset( ws.cur, 0, sizeof( ws.cur ) );
	SFCA_InitRandom( ws.cur, params->height, params->width, params->rho0, &rng );
	memcpy( prev, ws.cur, (size_t)cells );

	for ( t = 0; t <= params->maxGenerations; t++ ) {
		const float rho = SFCA_Density( ws.cur, params->height, params->width );
		int h;

		if ( SFCA_CountAlive( ws.cur, params->height, params->width ) == 0 ) {
			result->outcome = SFCA_OUTCOME_EXTINCTION;
			result->terminalGeneration = t;
			result->finalDensity = 0.0f;
			result->meanDensity = densAcc / (float)( t > 0 ? t : 1 );
			result->meanChangeRate = chiAcc / (float)( t > 0 ? t : 1 );
			result->stripeScore = 0.0f;
			free( history );
			return;
		}

		for ( h = 0; h < hLen; h++ ) {
			const byte *snap = history + (size_t)h * (size_t)cells;
			if ( SFCA_GridEqual( snap, ws.cur, cells ) ) {
				const int period = t - h;
				result->outcome = ( period <= 1 ) ? SFCA_OUTCOME_FIXED_POINT : SFCA_OUTCOME_CYCLE;
				result->period = period;
				result->terminalGeneration = t;
				result->finalDensity = rho;
				result->meanDensity = densAcc / (float)( t > 0 ? t : 1 );
				result->meanChangeRate = chiAcc / (float)( t > 0 ? t : 1 );
				result->stripeScore = SFCA_StripeScore( ws.cur, params->height, params->width );
				free( history );
				return;
			}
		}

		if ( hLen < SFCA_MAX_HISTORY ) {
			memcpy( history + (size_t)hLen * (size_t)cells, ws.cur, (size_t)cells );
			hLen++;
		}

		densAcc += rho;

		if ( t < params->maxGenerations ) {
			SFCA_Step( &ws, &params->intervals );
			chiAcc += SFCA_ChangeRate( prev, ws.cur, params->height, params->width );
			memcpy( prev, ws.cur, (size_t)cells );
		}
	}

	result->outcome = SFCA_OUTCOME_LONG_TRANSIENT;
	result->terminalGeneration = params->maxGenerations;
	result->finalDensity = SFCA_Density( ws.cur, params->height, params->width );
	result->meanDensity = densAcc / (float)params->maxGenerations;
	result->meanChangeRate = chiAcc / (float)params->maxGenerations;
	result->stripeScore = SFCA_StripeScore( ws.cur, params->height, params->width );
	free( history );
}

void SFCA_RunBatch( const sfca_run_params_t *params, int numRuns,
	sfca_outcome_stats_t *stats, sfca_run_result_t *lastCycle )
{
	sfca_run_params_t p;
	int r;
	unsigned rng = params->seed ? params->seed : 1u;

	memset( stats, 0, sizeof( *stats ) );
	p = *params;

	for ( r = 0; r < numRuns; r++ ) {
		sfca_run_result_t res;
		p.seed = rng;
		rng = rng * 1664525u + 1013904223u;
		SFCA_Run( &p, &res );
		stats->numRuns++;
		switch ( res.outcome ) {
		case SFCA_OUTCOME_EXTINCTION:
			stats->extinction += 1.0f;
			break;
		case SFCA_OUTCOME_FIXED_POINT:
			stats->fixedPoint += 1.0f;
			break;
		case SFCA_OUTCOME_CYCLE:
			stats->cycle += 1.0f;
			if ( lastCycle ) {
				*lastCycle = res;
			}
			break;
		case SFCA_OUTCOME_LONG_TRANSIENT:
			stats->longTransient += 1.0f;
			break;
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

float SFCA_DamageSpreadFinal( const sfca_run_params_t *params, unsigned seed )
{
	sfca_workspace_t wsA;
	sfca_workspace_t wsB;
	const int cells = params->height * params->width;
	unsigned rng = seed ? seed : 1u;
	int t;
	int flipIdx;
	int diff;

	if ( cells <= 0 || cells > SFCA_MAX_CELLS ) {
		return 0.0f;
	}

	wsA.height = wsB.height = params->height;
	wsA.width = wsB.width = params->width;
	SFCA_InitRandom( wsA.cur, params->height, params->width, params->rho0, &rng );
	memcpy( wsB.cur, wsA.cur, (size_t)cells );

	flipIdx = (int)( rng % (unsigned)cells );
	rng = rng * 1664525u + 1013904223u;
	wsB.cur[flipIdx] ^= 1;

	for ( t = 0; t < params->maxGenerations; t++ ) {
		if ( SFCA_CountAlive( wsA.cur, params->height, params->width ) == 0 &&
			SFCA_CountAlive( wsB.cur, params->height, params->width ) == 0 ) {
			break;
		}
		SFCA_Step( &wsA, &params->intervals );
		SFCA_Step( &wsB, &params->intervals );
	}

	diff = 0;
	for ( t = 0; t < cells; t++ ) {
		if ( ( wsA.cur[t] & 1 ) != ( wsB.cur[t] & 1 ) ) {
			diff++;
		}
	}
	return (float)diff / (float)cells;
}
