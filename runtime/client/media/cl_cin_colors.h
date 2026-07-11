/*
===========================================================================
Shared video color conversion helpers for native cinematic decoders.
===========================================================================
*/

#pragma once

#include "../../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

void CIN_ConvertYUV420Planar8ToRGBA(const byte *yPlane, int yStride,
	const byte *uPlane, int uStride,
	const byte *vPlane, int vStride,
	byte *rgba, int width, int height);

void CIN_ConvertPlanarYUV8ToRGBA(const byte *yPlane, int yStride,
	const byte *uPlane, int uStride,
	const byte *vPlane, int vStride,
	int chromaShiftX, int chromaShiftY,
	byte *rgba, int width, int height);

void CIN_ConvertPlanarYUV16ToRGBA(const uint16_t *yPlane, int yStrideBytes,
	const uint16_t *uPlane, int uStrideBytes,
	const uint16_t *vPlane, int vStrideBytes,
	int chromaShiftX, int chromaShiftY,
	unsigned maxValue,
	byte *rgba, int width, int height);

#ifdef __cplusplus
}
#endif
