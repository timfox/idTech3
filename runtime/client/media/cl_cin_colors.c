/*
===========================================================================
Shared YUV conversion helpers used by native cinematic decoders.
Uses lookup tables for the 8-bit fast path to reduce per-pixel multiplies.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "cl_cin_colors.h"

static qboolean cinColorTablesInited = qfalse;
static int cinYTable[256];
static int cinRvTable[256];
static int cinGuTable[256];
static int cinGvTable[256];
static int cinBuTable[256];

static ID_INLINE int CIN_ClampByte(int value) {
	if (value < 0) {
		return 0;
	}
	if (value > 255) {
		return 255;
	}
	return value;
}

static void CIN_InitColorTables(void) {
	int i;

	if (cinColorTablesInited) {
		return;
	}

	for (i = 0; i < 256; i++) {
		cinYTable[i] = 298 * (i - 16);
		cinRvTable[i] = 409 * (i - 128);
		cinGuTable[i] = -100 * (i - 128);
		cinGvTable[i] = -208 * (i - 128);
		cinBuTable[i] = 516 * (i - 128);
	}

	cinColorTablesInited = qtrue;
}

void CIN_ConvertYUV420Planar8ToRGBA(const byte *yPlane, int yStride,
	const byte *uPlane, int uStride,
	const byte *vPlane, int vStride,
	byte *rgba, int width, int height) {
	CIN_ConvertPlanarYUV8ToRGBA(yPlane, yStride, uPlane, uStride, vPlane, vStride, 1, 1, rgba, width, height);
}

void CIN_ConvertPlanarYUV8ToRGBA(const byte *yPlane, int yStride,
	const byte *uPlane, int uStride,
	const byte *vPlane, int vStride,
	int chromaShiftX, int chromaShiftY,
	byte *rgba, int width, int height) {
	int x, y;

	CIN_InitColorTables();

	for (y = 0; y < height; y++) {
		const byte *yRow = yPlane + (y * yStride);
		const byte *uRow = uPlane ? (uPlane + ((y >> chromaShiftY) * uStride)) : NULL;
		const byte *vRow = vPlane ? (vPlane + ((y >> chromaShiftY) * vStride)) : NULL;
		byte *rgbaRow = rgba + (y * width * 4);

		for (x = 0; x < width; x++) {
			int yTerm = cinYTable[yRow[x]];
			int uVal = uRow ? uRow[x >> chromaShiftX] : 128;
			int vVal = vRow ? vRow[x >> chromaShiftX] : 128;
			int r = (yTerm + cinRvTable[vVal] + 128) >> 8;
			int g = (yTerm + cinGuTable[uVal] + cinGvTable[vVal] + 128) >> 8;
			int b = (yTerm + cinBuTable[uVal] + 128) >> 8;
			int idx = x * 4;

			rgbaRow[idx + 0] = (byte)CIN_ClampByte(r);
			rgbaRow[idx + 1] = (byte)CIN_ClampByte(g);
			rgbaRow[idx + 2] = (byte)CIN_ClampByte(b);
			rgbaRow[idx + 3] = 255;
		}
	}
}

void CIN_ConvertPlanarYUV16ToRGBA(const uint16_t *yPlane, int yStrideBytes,
	const uint16_t *uPlane, int uStrideBytes,
	const uint16_t *vPlane, int vStrideBytes,
	int chromaShiftX, int chromaShiftY,
	unsigned maxValue,
	byte *rgba, int width, int height) {
	int x, y;

	if (maxValue == 0) {
		maxValue = 1023;
	}

	for (y = 0; y < height; y++) {
		const uint16_t *yRow = (const uint16_t *)((const byte *)yPlane + (y * yStrideBytes));
		const uint16_t *uRow = uPlane ? (const uint16_t *)((const byte *)uPlane + ((y >> chromaShiftY) * uStrideBytes)) : NULL;
		const uint16_t *vRow = vPlane ? (const uint16_t *)((const byte *)vPlane + ((y >> chromaShiftY) * vStrideBytes)) : NULL;
		byte *rgbaRow = rgba + (y * width * 4);

		for (x = 0; x < width; x++) {
			unsigned yRaw = yRow[x];
			unsigned uRaw = uRow ? uRow[x >> chromaShiftX] : (maxValue / 2u);
			unsigned vRaw = vRow ? vRow[x >> chromaShiftX] : (maxValue / 2u);
			int yVal = (int)((yRaw * 255u + (maxValue / 2u)) / maxValue);
			int uVal = (int)((uRaw * 255u + (maxValue / 2u)) / maxValue);
			int vVal = (int)((vRaw * 255u + (maxValue / 2u)) / maxValue);
			int c = yVal - 16;
			int d = uVal - 128;
			int e = vVal - 128;
			int idx = x * 4;

			rgbaRow[idx + 0] = (byte)CIN_ClampByte((298 * c + 409 * e + 128) >> 8);
			rgbaRow[idx + 1] = (byte)CIN_ClampByte((298 * c - 100 * d - 208 * e + 128) >> 8);
			rgbaRow[idx + 2] = (byte)CIN_ClampByte((298 * c + 516 * d + 128) >> 8);
			rgbaRow[idx + 3] = 255;
		}
	}
}
