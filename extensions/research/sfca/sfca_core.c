/*
===========================================================================
SFCA field construction and synchronous update (eqs. 1–5).
===========================================================================
*/

#include "sfca/sfca_internal.h"

#include <math.h>
#include <string.h>

static int SFCA_Wrap( int i, int n )
{
	while ( i < 0 ) {
		i += n;
	}
	while ( i >= n ) {
		i -= n;
	}
	return i;
}

int SFCA_InInterval( float q, float lo, float hi )
{
	return ( q >= lo && q <= hi ) ? 1 : 0;
}

void SFCA_ComputeField( sfca_workspace_t *ws, const sfca_intervals_t *iv )
{
	int j;
	int i;
	float nMax = 0.0f;
	const int H = ws->height;
	const int W = ws->width;

	(void)iv;

	memset( ws->colSum, 0, sizeof( int ) * (size_t)W );
	memset( ws->rowSum, 0, sizeof( int ) * (size_t)H );

	for ( j = 0; j < H; j++ ) {
		for ( i = 0; i < W; i++ ) {
			const byte v = ws->cur[j * W + i];
			ws->rowSum[j] += v;
			ws->colSum[i] += v;
		}
	}

	for ( i = 0; i < W; i++ ) {
		const int im = SFCA_Wrap( i - 1, W );
		const int ip = SFCA_Wrap( i + 1, W );
		ws->colBlur[i] = ws->colSum[im] + ws->colSum[i] + ws->colSum[ip];
	}
	for ( j = 0; j < H; j++ ) {
		const int jm = SFCA_Wrap( j - 1, H );
		const int jp = SFCA_Wrap( j + 1, H );
		ws->rowBlur[j] = ws->rowSum[jm] + ws->rowSum[j] + ws->rowSum[jp];
	}

	for ( j = 0; j < H; j++ ) {
		for ( i = 0; i < W; i++ ) {
			const int idx = j * W + i;
			const float n = (float)( ws->rowBlur[j] * ws->colBlur[i] );
			ws->field[idx] = n;
			if ( n > nMax ) {
				nMax = n;
			}
		}
	}

	if ( nMax <= 0.0f ) {
		memset( ws->qField, 0, sizeof( float ) * (size_t)H * (size_t)W );
		return;
	}

	for ( j = 0; j < H; j++ ) {
		for ( i = 0; i < W; i++ ) {
			const int idx = j * W + i;
			ws->qField[idx] = ws->field[idx] / nMax;
		}
	}
}

void SFCA_Step( sfca_workspace_t *ws, const sfca_intervals_t *iv )
{
	int j;
	int i;
	const int H = ws->height;
	const int W = ws->width;

	SFCA_ComputeField( ws, iv );

	for ( j = 0; j < H; j++ ) {
		for ( i = 0; i < W; i++ ) {
			const int idx = j * W + i;
			const float q = ws->qField[idx];
			if ( ws->cur[idx] ) {
				ws->nxt[idx] = SFCA_InInterval( q, iv->sLow, iv->sHigh ) ? 1 : 0;
			} else {
				ws->nxt[idx] = SFCA_InInterval( q, iv->bLow, iv->bHigh ) ? 1 : 0;
			}
		}
	}

	memcpy( ws->cur, ws->nxt, (size_t)H * (size_t)W );
}

float SFCA_ChangeRate( const byte *a, const byte *b, int height, int width )
{
	int diff = 0;
	int j;
	const int n = height * width;

	for ( j = 0; j < n; j++ ) {
		if ( ( a[j] & 1 ) != ( b[j] & 1 ) ) {
			diff++;
		}
	}
	return ( n > 0 ) ? ( (float)diff / (float)n ) : 0.0f;
}

void SFCA_InitRandom( byte *grid, int height, int width, float rho0, unsigned *rng )
{
	unsigned r = rng ? *rng : 1u;
	int j;
	int i;

	for ( j = 0; j < height; j++ ) {
		for ( i = 0; i < width; i++ ) {
			r = r * 1664525u + 1013904223u;
			grid[j * width + i] = ( (float)( r % 10000u ) / 10000.0f < rho0 ) ? 1 : 0;
		}
	}

	if ( rng ) {
		*rng = r;
	}
}

int SFCA_CountAlive( const byte *grid, int height, int width )
{
	int n = 0;
	int j;
	int i;

	for ( j = 0; j < height; j++ ) {
		for ( i = 0; i < width; i++ ) {
			n += grid[j * width + i] & 1;
		}
	}
	return n;
}

float SFCA_Density( const byte *grid, int height, int width )
{
	const int n = height * width;
	if ( n <= 0 ) {
		return 0.0f;
	}
	return (float)SFCA_CountAlive( grid, height, width ) / (float)n;
}

float SFCA_StripeScore( const byte *grid, int height, int width )
{
	float rowMean[256];
	float colMean[256];
	float rowVar = 0.0f;
	float colVar = 0.0f;
	float rowAvg = 0.0f;
	float colAvg = 0.0f;
	int j;
	int i;

	if ( height <= 0 || width <= 0 || height > 256 || width > 256 ) {
		return 0.0f;
	}

	memset( rowMean, 0, sizeof( rowMean ) );
	memset( colMean, 0, sizeof( colMean ) );

	for ( j = 0; j < height; j++ ) {
		for ( i = 0; i < width; i++ ) {
			const float v = (float)( grid[j * width + i] & 1 );
			rowMean[j] += v;
			colMean[i] += v;
		}
	}

	for ( j = 0; j < height; j++ ) {
		rowMean[j] /= (float)width;
		rowAvg += rowMean[j];
	}
	rowAvg /= (float)height;
	for ( j = 0; j < height; j++ ) {
		const float d = rowMean[j] - rowAvg;
		rowVar += d * d;
	}
	rowVar /= (float)height;

	for ( i = 0; i < width; i++ ) {
		colMean[i] /= (float)height;
		colAvg += colMean[i];
	}
	colAvg /= (float)width;
	for ( i = 0; i < width; i++ ) {
		const float d = colMean[i] - colAvg;
		colVar += d * d;
	}
	colVar /= (float)width;

	return rowVar + colVar;
}

void SFCA_DefaultRepresentativeRule( sfca_intervals_t *iv )
{
	iv->sLow = 3.0f / (float)SFCA_GRID_LEVELS;
	iv->sHigh = 11.0f / (float)SFCA_GRID_LEVELS;
	iv->bLow = 7.0f / (float)SFCA_GRID_LEVELS;
	iv->bHigh = 9.0f / (float)SFCA_GRID_LEVELS;
}

void SFCA_CanonicalTransitionRule( float sWidth, sfca_intervals_t *iv )
{
	const float inv = 1.0f / (float)SFCA_FINE_LEVELS;
	iv->sLow = 10.0f * inv;
	iv->sHigh = iv->sLow + sWidth;
	iv->bLow = 60.0f * inv;
	iv->bHigh = 160.0f * inv;
}

sfca_interval_geom_t SFCA_IntervalGeometry( const sfca_intervals_t *iv )
{
	if ( iv->bLow >= iv->sLow && iv->bHigh <= iv->sHigh ) {
	 return SFCA_GEOM_B_IN_S;
	}
	if ( iv->sHigh <= iv->bLow || iv->bHigh <= iv->sLow ) {
	 return SFCA_GEOM_NO_OVERLAP;
	}
	return SFCA_GEOM_PARTIAL_OVERLAP;
}
