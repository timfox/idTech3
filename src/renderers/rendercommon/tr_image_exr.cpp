/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

OpenEXR image loader/saver using tinyexr (BSD license).
Supports HDR image loading for environment maps, lightmaps,
and texture authoring workflows. Converts EXR float data
to engine-native RGBA8 or RGBA16F formats.
===========================================================================
*/

#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_MINIZ (1)
#include "../../external/include/tinyexr/tinyexr.h"

extern "C" {
#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
}

extern "C" void R_LoadEXR(const char *filename, byte **pic, int *width, int *height) {
	float *rgba = NULL;
	int w, h;
	const char *err = NULL;
	void *fileData;
	int fileSize;
	int ret;
	int i, numPixels;
	byte *out;

	*pic = NULL;
	*width = 0;
	*height = 0;

	fileSize = FS_ReadFile(filename, &fileData);
	if (fileSize <= 0 || !fileData) {
		return;
	}

	ret = LoadEXRFromMemory(&rgba, &w, &h, (const unsigned char *)fileData, (size_t)fileSize, &err);
	FS_FreeFile(fileData);

	if (ret != TINYEXR_SUCCESS || !rgba) {
		if (err) {
			Com_Printf(S_COLOR_YELLOW "EXR: %s: %s\n", filename, err);
			FreeEXRErrorMessage(err);
		}
		return;
	}

	if (w <= 0 || h <= 0 || w > 16384 || h > 16384) {
		Com_Printf(S_COLOR_YELLOW "EXR: %s: invalid dimensions %dx%d\n", filename, w, h);
		free(rgba);
		return;
	}

	numPixels = w * h;
	out = (byte *)Z_Malloc(numPixels * 4);
	if (!out) {
		free(rgba);
		return;
	}

	for (i = 0; i < numPixels; i++) {
		float r = rgba[i * 4 + 0];
		float g = rgba[i * 4 + 1];
		float b = rgba[i * 4 + 2];
		float a = rgba[i * 4 + 3];

		r = r < 0 ? 0 : (r > 1 ? 1 : r);
		g = g < 0 ? 0 : (g > 1 ? 1 : g);
		b = b < 0 ? 0 : (b > 1 ? 1 : b);
		a = a < 0 ? 0 : (a > 1 ? 1 : a);

		float invGamma = 1.0f / 2.2f;
		out[i * 4 + 0] = (byte)(powf(r, invGamma) * 255.0f + 0.5f);
		out[i * 4 + 1] = (byte)(powf(g, invGamma) * 255.0f + 0.5f);
		out[i * 4 + 2] = (byte)(powf(b, invGamma) * 255.0f + 0.5f);
		out[i * 4 + 3] = (byte)(a * 255.0f + 0.5f);
	}

	free(rgba);

	*pic = out;
	*width = w;
	*height = h;
}

extern "C" void R_LoadEXR_HDR(const char *filename, float **pic, int *width, int *height) {
	void *fileData;
	int fileSize;
	const char *err = NULL;
	int ret;

	*pic = NULL;
	*width = 0;
	*height = 0;

	fileSize = FS_ReadFile(filename, &fileData);
	if (fileSize <= 0 || !fileData) {
		return;
	}

	ret = LoadEXRFromMemory(pic, width, height, (const unsigned char *)fileData, (size_t)fileSize, &err);
	FS_FreeFile(fileData);

	if (ret != TINYEXR_SUCCESS) {
		if (err) {
			Com_Printf(S_COLOR_YELLOW "EXR HDR: %s: %s\n", filename, err);
			FreeEXRErrorMessage(err);
		}
		*pic = NULL;
	}
}

extern "C" qboolean R_SaveEXR(const char *filename, const float *rgba, int width, int height) {
	const char *err = NULL;
	int ret;

	if (!rgba || width <= 0 || height <= 0) {
		return qfalse;
	}

	ret = SaveEXR(rgba, width, height, 4, 0, filename, &err);
	if (ret != TINYEXR_SUCCESS) {
		if (err) {
			Com_Printf(S_COLOR_RED "EXR save: %s: %s\n", filename, err);
			FreeEXRErrorMessage(err);
		}
		return qfalse;
	}

	Com_Printf("EXR: saved %s (%dx%d)\n", filename, width, height);
	return qtrue;
}
