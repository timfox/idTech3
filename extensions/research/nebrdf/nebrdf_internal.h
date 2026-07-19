#pragma once

/*
===========================================================================
Internal tables for NEBRDF scaffold (arXiv:2604.24081).
===========================================================================
*/

#include "nebrdf/nebrdf.h"

extern const nebrdf_node_t nebrdf_nodes[NEBRDF_NODE_COUNT];
extern const int nebrdf_enhance_order[NEBRDF_ENHANCE_ORDER_LEN];
extern const nebrdf_compare_row_t nebrdf_compare_rows[NEBRDF_COMPARE_COUNT];
