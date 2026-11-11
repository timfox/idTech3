/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Native vs emulated double-precision point visualization (arXiv:2408.09699).
===========================================================================
*/
#pragma once

#include "tr_local.h"


typedef enum {
	VK_FP64_POINTS_MODE_NATIVE = 0,
	VK_FP64_POINTS_MODE_EMULATED = 1,
	VK_FP64_POINTS_MODE_SINGLE = 2,
	VK_FP64_POINTS_MODE_COUNT
} vk_fp64_points_mode_t;

void VK_FP64_PointsInit( void );
void VK_FP64_PointsShutdown( void );
void VK_FP64_PointsCreatePipelines( void );

qboolean VK_FP64_PointsLoadCsv( const char *path, int dimensions );
qboolean VK_FP64_PointsGenerate( int count, int dimensions );
void VK_FP64_PointsClear( void );
void VK_FP64_PointsBenchmark( int frames );
void VK_FP64_PointsDraw( void );

qboolean VK_FP64_PointsReady( void );
uint32_t VK_FP64_PointsVertexCount( void );

