/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

SqueezeMe asset layout (arXiv:2412.15171v4 — mobile Gaussian avatars).
===========================================================================
*/

#ifndef SQZ_FORMAT_H
#define SQZ_FORMAT_H

#include "q_shared.h"

#define SQZ_MAGIC           "SQZ1"
#define SQZ_VERSION         1

#define SQZ_GCS_GRID        64
#define SQZ_GCS_CELLS       ( SQZ_GCS_GRID * SQZ_GCS_GRID )
#define SQZ_CHANNELS        16
#define SQZ_POSE_DIM        128
#define SQZ_POSE_BASIS_COLS 64
#define SQZ_MAX_GAUSSIANS   65536
#define SQZ_MAX_JOINTS      24
#define SQZ_MAX_AVATARS     3

/* Matches mgs_prepare.comp GaussianPrim (48 bytes). */
typedef struct {
	float position[3];
	float opacity;
	float scale[3];
	float sigmaScale;
	float rotation[4];
	float color[3];
	float pad;
} sqzMgsGaussian_t;

typedef struct {
	float poseMean[SQZ_POSE_DIM];
	float poseBasis[SQZ_POSE_DIM * SQZ_POSE_BASIS_COLS];
	float correctivesBasis[( SQZ_POSE_BASIS_COLS + 1 ) * SQZ_GCS_CELLS * SQZ_CHANNELS];
	float templateMean[SQZ_GCS_CELLS * SQZ_CHANNELS];
	uint8_t mask[SQZ_GCS_CELLS];
	float jointWeights[SQZ_GCS_CELLS * SQZ_MAX_JOINTS];
	float bindPos[SQZ_GCS_CELLS * 3];
	uint32_t numJoints;
	uint32_t gaussianCount;
	qboolean hasLinear;
	qboolean gcs64;
} sqzAvatarAsset_t;

#endif /* SQZ_FORMAT_H */
