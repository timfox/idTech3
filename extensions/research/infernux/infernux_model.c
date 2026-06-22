/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Infernux benchmark tables (Tables I, II, IV — arXiv:2604.10263).
===========================================================================
*/

#include "infernux/infernux_model.h"

static const infernux_row_t spawn_single[] = {
	{ 10, 803.0f, 1000.0f, 714.0f, 1000.0f },
	{ 30, 625.0f, 865.0f, 404.0f, 1000.0f },
	{ 50, 414.0f, 647.0f, 228.0f, 651.0f },
	{ 70, 265.0f, 437.0f, 125.0f, 347.0f },
	{ 100, 127.0f, 171.0f, 61.0f, 187.0f },
};

static const infernux_row_t spawn_multi10[] = {
	{ 10, 778.0f, 968.0f, 711.0f, 1000.0f },
	{ 30, 588.0f, 812.0f, 346.0f, 1000.0f },
	{ 50, 389.0f, 591.0f, 194.0f, 651.0f },
	{ 70, 229.0f, 397.0f, 107.0f, 339.0f },
	{ 100, 114.0f, 162.0f, 65.0f, 182.0f },
};

static const infernux_row_t spawn_multi100[] = {
	{ 10, 572.0f, 723.0f, 678.0f, 1000.0f },
	{ 30, 458.0f, 631.0f, 436.0f, 1000.0f },
	{ 50, 331.0f, 484.0f, 227.0f, 623.0f },
	{ 70, 201.0f, 324.0f, 119.0f, 333.0f },
	{ 100, 109.0f, 152.0f, 40.0f, 178.0f },
};

static const infernux_row_t pure_compute[] = {
	{ 100, 766.0f, 1000.0f, 557.0f, 1000.0f },
	{ 300, 755.0f, 1000.0f, 172.0f, 994.0f },
	{ 500, 724.0f, 989.0f, 72.0f, 422.0f },
	{ 700, 694.0f, 949.0f, 38.0f, 241.0f },
	{ 1000, 624.0f, 848.0f, 19.0f, 123.0f },
};

const infernux_row_t *Infernux_Table( infernux_bench_t bench, int *count )
{
	if ( count ) {
		switch ( bench ) {
		case INFERNUX_BENCH_SPAWN_MULTI10:
			*count = (int)( sizeof( spawn_multi10 ) / sizeof( spawn_multi10[0] ) );
			break;
		case INFERNUX_BENCH_SPAWN_MULTI100:
			*count = (int)( sizeof( spawn_multi100 ) / sizeof( spawn_multi100[0] ) );
			break;
		case INFERNUX_BENCH_PURE_COMPUTE:
			*count = (int)( sizeof( pure_compute ) / sizeof( pure_compute[0] ) );
			break;
		default:
			*count = (int)( sizeof( spawn_single ) / sizeof( spawn_single[0] ) );
			break;
		}
	}

	switch ( bench ) {
	case INFERNUX_BENCH_SPAWN_MULTI10:
		return spawn_multi10;
	case INFERNUX_BENCH_SPAWN_MULTI100:
		return spawn_multi100;
	case INFERNUX_BENCH_PURE_COMPUTE:
		return pure_compute;
	default:
		return spawn_single;
	}
}

float Infernux_JitSpeedup( int grid_n )
{
	/* Table IV: N=1000 runtime JIT 848 vs No-JIT 81 => ~10.5x; scale roughly for demo */
	if ( grid_n >= 1000 ) {
		return 10.5f;
	}
	if ( grid_n >= 700 ) {
		return 6.0f;
	}
	if ( grid_n >= 500 ) {
		return 1.7f;
	}
	return 1.1f;
}
