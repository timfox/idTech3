#pragma once

#include "qcommon/q_shared.h"

#define BUBBLESH_DEFAULT_ORDER   14
#define BUBBLESH_BUBBLE_COUNT    32
#define BUBBLESH_MESH_NODES_MIN  1100
#define BUBBLESH_MESH_NODES_MAX  1500

typedef struct {
	int diameter_mm;
	int void_fraction_pct;
	int bubble_count;
	int sh_order;
	int sh_coeff_count;
	int state_dim;
	float dt_seconds;
	float domain_size_mm;
	float coord_compression_ratio;
} bubblesh_config_t;

int BubbleSH_CoefficientCount( int sh_order );
float BubbleSH_DomainSizeMm( int bubble_count, float diameter_mm, float void_fraction_pct );
qboolean BubbleSH_FillConfig( int diameter_mm, int void_fraction_pct, int sh_order, bubblesh_config_t *out );

float BubbleSH_RelativeAverageDisplacementError( float mean_displacement_error, float arc_length );
float BubbleSH_RelativeFinalDisplacementError( float final_displacement_error, float arc_length );
float BubbleSH_RelativeAverageChamferDistance( float mean_chamfer_distance, float total_surface_change );

float BubbleSH_Wasserstein1( const float *predicted, const float *truth, int count );
float BubbleSH_InterquartileRange( const float *values, int count );
float BubbleSH_NormalizedWasserstein1( const float *predicted, const float *truth, int count );
