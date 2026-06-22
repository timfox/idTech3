#pragma once

#include "dlm/dlm.h"

#include <stddef.h>

typedef unsigned int dlm_truth_t;

int DLM_HammingWeight( dlm_truth_t tt, int ell );
dlm_truth_t DLM_FunctionApply( dlm_truth_t f, dlm_truth_t inputs, int k );
void DLM_EvalNetwork( int k, int depth, const dlm_truth_t *nodeFuncs,
	dlm_truth_t *outTruth );

void DLM_MatrixVector( int dim, const double *A, const float *x, float *y );
void DLM_MatrixPowerApply( int k, int depth, const float *q0, float *qOut );
