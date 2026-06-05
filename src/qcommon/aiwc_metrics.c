/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Architecture-independent workload characterization metrics (AIWC-style).
Parallel spatial locality per Chilukuri, Milthorpe & Johnston, IWOCL 2020.
===========================================================================
*/

#include "aiwc_metrics.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define AIWC_TS_MAX_ACCESSES 65536
#define AIWC_GROW 4096

typedef struct {
	uint64_t key;
	uint64_t count;
} aiwc_freq_t;

struct aiwc_recorder_s {
	aiwc_mem_access_t *flat;
	uint32_t flat_count;
	uint32_t flat_cap;

	uint32_t cur_wg;
	uint32_t wg_count;

	uint64_t *ts_addrs;
	qboolean *ts_local;
	uint32_t ts_count;

	double wg_psl_sum[AIWC_BITS_LEVELS];
	uint32_t wg_ts_count;

	double *psl_sum;
	uint32_t psl_wg_count;

	uint64_t total_accesses;
};

static int AIWC_U64Compare( const void *a, const void *b )
{
	const uint64_t va = *(const uint64_t *)a;
	const uint64_t vb = *(const uint64_t *)b;

	if ( va < vb ) {
		return -1;
	}
	if ( va > vb ) {
		return 1;
	}
	return 0;
}

static int AIWC_FreqCompareDesc( const void *a, const void *b )
{
	const aiwc_freq_t *fa = (const aiwc_freq_t *)a;
	const aiwc_freq_t *fb = (const aiwc_freq_t *)b;

	if ( fa->count < fb->count ) {
		return 1;
	}
	if ( fa->count > fb->count ) {
		return -1;
	}
	return 0;
}

double AIWC_EntropyBitsDropped( const uint64_t *addrs, uint32_t count, int bits_drop )
{
	uint64_t *scratch;
	uint32_t i;
	uint32_t unique;
	double entropy;
	uint64_t total;

	if ( !addrs || count == 0 ) {
		return 0.0;
	}

	scratch = (uint64_t *)malloc( (size_t)count * sizeof( *scratch ) );
	if ( !scratch ) {
		return 0.0;
	}

	for ( i = 0; i < count; i++ ) {
		scratch[i] = addrs[i] >> (unsigned)bits_drop;
	}

	qsort( scratch, count, sizeof( scratch[0] ), AIWC_U64Compare );

	unique = 0;
	total = count;
	entropy = 0.0;
	i = 0;
	while ( i < count ) {
		uint32_t j = i + 1;
		uint64_t run;

		while ( j < count && scratch[j] == scratch[i] ) {
			j++;
		}
		run = j - i;
		{
			double p = (double)run / (double)total;
			entropy -= p * ( log( p ) / log( 2.0 ) );
		}
		unique++;
		i = j;
	}

	(void)unique;
	free( scratch );
	return entropy;
}

static void AIWC_FinalizeTimestep( aiwc_recorder_t *rec )
{
	int n;

	if ( !rec || rec->ts_count == 0 ) {
		return;
	}

	for ( n = 0; n < AIWC_BITS_LEVELS; n++ ) {
		double e = AIWC_EntropyBitsDropped( rec->ts_addrs, rec->ts_count, n );
		rec->wg_psl_sum[n] += e;
	}
	rec->wg_ts_count++;
	rec->ts_count = 0;
}

static void AIWC_FinalizeWorkGroup( aiwc_recorder_t *rec )
{
	int n;

	if ( !rec || rec->wg_ts_count == 0 ) {
		return;
	}

	for ( n = 0; n < AIWC_BITS_LEVELS; n++ ) {
		rec->psl_sum[n] += rec->wg_psl_sum[n] / (double)rec->wg_ts_count;
	}
	rec->psl_wg_count++;

	memset( rec->wg_psl_sum, 0, sizeof( rec->wg_psl_sum ) );
	rec->wg_ts_count = 0;
}

static void AIWC_GrowFlat( aiwc_recorder_t *rec )
{
	uint32_t new_cap;
	aiwc_mem_access_t *n;

	if ( rec->flat_count < rec->flat_cap ) {
		return;
	}
	new_cap = rec->flat_cap ? rec->flat_cap + AIWC_GROW : AIWC_GROW;
	n = (aiwc_mem_access_t *)realloc( rec->flat, (size_t)new_cap * sizeof( *n ) );
	if ( !n ) {
		return;
	}
	rec->flat = n;
	rec->flat_cap = new_cap;
}

static void AIWC_AppendAccess( aiwc_recorder_t *rec, uint64_t virt_addr, qboolean is_local )
{
	if ( rec->ts_count >= AIWC_TS_MAX_ACCESSES ) {
		return;
	}
	rec->ts_addrs[rec->ts_count] = virt_addr;
	rec->ts_local[rec->ts_count] = is_local;
	rec->ts_count++;

	AIWC_GrowFlat( rec );
	if ( rec->flat_count < rec->flat_cap ) {
		rec->flat[rec->flat_count].virt_addr = virt_addr;
		rec->flat[rec->flat_count].is_local = is_local;
		rec->flat_count++;
	}
	rec->total_accesses++;
}

void AIWC_MetricsClear( aiwc_metrics_t *m )
{
	if ( !m ) {
		return;
	}
	memset( m, 0, sizeof( *m ) );
}

aiwc_recorder_t *AIWC_RecorderCreate( void )
{
	aiwc_recorder_t *rec;

	rec = (aiwc_recorder_t *)calloc( 1, sizeof( *rec ) );
	if ( !rec ) {
		return NULL;
	}
	rec->psl_sum = (double *)calloc( AIWC_BITS_LEVELS, sizeof( double ) );
	rec->ts_addrs = (uint64_t *)malloc( (size_t)AIWC_TS_MAX_ACCESSES * sizeof( uint64_t ) );
	rec->ts_local = (qboolean *)malloc( (size_t)AIWC_TS_MAX_ACCESSES * sizeof( qboolean ) );
	if ( !rec->psl_sum || !rec->ts_addrs || !rec->ts_local ) {
		AIWC_RecorderDestroy( rec );
		return NULL;
	}
	return rec;
}

void AIWC_RecorderDestroy( aiwc_recorder_t *rec )
{
	if ( !rec ) {
		return;
	}
	free( rec->flat );
	free( rec->ts_addrs );
	free( rec->ts_local );
	free( rec->psl_sum );
	free( rec );
}

void AIWC_RecorderBeginWorkGroup( aiwc_recorder_t *rec, uint32_t wg_id )
{
	(void)wg_id;
	if ( !rec ) {
		return;
	}
	AIWC_FinalizeWorkGroup( rec );
	rec->cur_wg = wg_id;
	rec->ts_count = 0;
	memset( rec->wg_psl_sum, 0, sizeof( rec->wg_psl_sum ) );
	rec->wg_ts_count = 0;
}

void AIWC_RecorderBeginTimestep( aiwc_recorder_t *rec )
{
	if ( !rec ) {
		return;
	}
	AIWC_FinalizeTimestep( rec );
}

void AIWC_RecorderRecord( aiwc_recorder_t *rec, uint64_t virt_addr, qboolean is_local )
{
	if ( !rec ) {
		return;
	}
	AIWC_AppendAccess( rec, virt_addr, is_local );
}

void AIWC_RecorderEndWorkGroup( aiwc_recorder_t *rec )
{
	if ( !rec ) {
		return;
	}
	AIWC_FinalizeTimestep( rec );
	AIWC_FinalizeWorkGroup( rec );
	rec->wg_count++;
}

static void AIWC_ComputeFootprint( const aiwc_mem_access_t *flat, uint32_t count,
	uint32_t *total_out, uint32_t *pct90_out )
{
	uint64_t *keys;
	aiwc_freq_t *freq;
	uint32_t i;
	uint32_t unique;
	uint64_t target;
	uint64_t accum;

	if ( !flat || count == 0 || !total_out || !pct90_out ) {
		if ( total_out ) {
			*total_out = 0;
		}
		if ( pct90_out ) {
			*pct90_out = 0;
		}
		return;
	}

	keys = (uint64_t *)malloc( (size_t)count * sizeof( *keys ) );
	freq = (aiwc_freq_t *)malloc( (size_t)count * sizeof( *freq ) );
	if ( !keys || !freq ) {
		free( keys );
		free( freq );
		return;
	}

	for ( i = 0; i < count; i++ ) {
		keys[i] = flat[i].virt_addr;
	}
	qsort( keys, count, sizeof( keys[0] ), AIWC_U64Compare );

	unique = 0;
	i = 0;
	while ( i < count ) {
		uint32_t j = i + 1;
		while ( j < count && keys[j] == keys[i] ) {
			j++;
		}
		freq[unique].key = keys[i];
		freq[unique].count = j - i;
		unique++;
		i = j;
	}

	qsort( freq, unique, sizeof( freq[0] ), AIWC_FreqCompareDesc );

	target = (uint64_t)( (double)count * 0.9 );
	accum = 0;
	*pct90_out = unique;
	for ( i = 0; i < unique; i++ ) {
		accum += freq[i].count;
		if ( accum >= target ) {
			*pct90_out = i + 1;
			break;
		}
	}
	*total_out = unique;

	free( keys );
	free( freq );
}

static double AIWC_EntropyFiltered( const aiwc_mem_access_t *flat, uint32_t count, qboolean want_local, int bits_drop )
{
	uint64_t *addrs;
	uint32_t i;
	uint32_t n;
	double e;

	if ( !flat || count == 0 ) {
		return 0.0;
	}

	addrs = (uint64_t *)malloc( (size_t)count * sizeof( *addrs ) );
	if ( !addrs ) {
		return 0.0;
	}

	n = 0;
	for ( i = 0; i < count; i++ ) {
		if ( flat[i].is_local == want_local ) {
			addrs[n++] = flat[i].virt_addr;
		}
	}
	e = AIWC_EntropyBitsDropped( addrs, n, bits_drop );
	free( addrs );
	return e;
}

void AIWC_RecorderFinalize( aiwc_recorder_t *rec, aiwc_metrics_t *out )
{
	uint32_t i;
	uint64_t local_count;
	int n;

	if ( !rec || !out ) {
		return;
	}

	AIWC_FinalizeTimestep( rec );
	AIWC_FinalizeWorkGroup( rec );
	AIWC_MetricsClear( out );

	out->total_accesses = rec->total_accesses;
	if ( rec->flat_count == 0 ) {
		return;
	}

	AIWC_ComputeFootprint( rec->flat, rec->flat_count, &out->total_footprint, &out->footprint_90pct );

	out->global_mae = AIWC_EntropyFiltered( rec->flat, rec->flat_count, qfalse, 0 );
	out->local_mae = AIWC_EntropyFiltered( rec->flat, rec->flat_count, qtrue, 0 );

	for ( n = 0; n < AIWC_BITS_LEVELS; n++ ) {
		uint64_t *addrs;
		uint32_t ac;
		uint32_t j;

		addrs = (uint64_t *)malloc( (size_t)rec->flat_count * sizeof( *addrs ) );
		if ( !addrs ) {
			continue;
		}
		for ( j = 0, ac = 0; j < rec->flat_count; j++ ) {
			addrs[ac++] = rec->flat[j].virt_addr;
		}
		out->lmae[n] = AIWC_EntropyBitsDropped( addrs, ac, n );
		free( addrs );
	}

	local_count = 0;
	for ( i = 0; i < rec->flat_count; i++ ) {
		if ( rec->flat[i].is_local ) {
			local_count++;
		}
	}
	out->relative_local_usage = (double)local_count / (double)rec->flat_count;

	if ( rec->psl_wg_count > 0 ) {
		for ( n = 0; n < AIWC_BITS_LEVELS; n++ ) {
			out->psl[n] = rec->psl_sum[n] / (double)rec->psl_wg_count;
		}
	}
}
