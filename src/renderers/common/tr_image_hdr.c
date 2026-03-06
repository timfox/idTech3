#include "tr_image_loaders.h"
/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Radiance HDR (.hdr) image loader.
Reads RGBE format HDR images commonly used for environment maps
and IBL. Converts to RGBA8 with gamma correction for standard
texture use, or provides float4 for HDR skybox/IBL.
===========================================================================
*/

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include <math.h>

static void RGBE_to_float(unsigned char r, unsigned char g, unsigned char b, unsigned char e, float *out) {
	if (e == 0) {
		out[0] = out[1] = out[2] = 0.0f;
	} else {
		float scale = ldexpf(1.0f, (int)e - 128 - 8);
		out[0] = r * scale;
		out[1] = g * scale;
		out[2] = b * scale;
	}
}

static qboolean HDR_ReadHeader(const byte *data, int size, int *width, int *height, int *dataOffset) {
	const char *p = (const char *)data;
	const char *end = p + (size < 4096 ? size : 4096);
	int w = 0, h = 0;

	if (size < 11) return qfalse;
	if (p[0] != '#' || p[1] != '?') return qfalse;

	while (p < end - 1) {
		if (p[0] == '\n' && p[1] == '\n') { p += 2; break; }
		if (p[0] == '\n' && p[1] == '-') { p += 1; break; }
		p++;
	}

	if (sscanf(p, "-Y %d +X %d", &h, &w) != 2) {
		if (sscanf(p, "+X %d +Y %d", &w, &h) != 2) {
			return qfalse;
		}
	}

	while (p < end && *p != '\n') p++;
	p++;

	*width = w;
	*height = h;
	*dataOffset = (int)(p - (const char *)data);
	return (w > 0 && h > 0 && w <= 16384 && h <= 16384);
}

static qboolean HDR_DecodeRLE(const byte *data, int dataSize, int offset, int width, int height, float *out) {
	const byte *p = data + offset;
	const byte *pEnd = data + dataSize;
	int y, x, i;

	for (y = 0; y < height; y++) {
		if (p + 4 > pEnd) return qfalse;

		if (width >= 8 && width <= 0x7fff && p[0] == 2 && p[1] == 2) {
			int scanW = ((int)p[2] << 8) | p[3];
			if (scanW != width) return qfalse;
			p += 4;

			byte *scanline = (byte *)Z_Malloc(width * 4);
			for (i = 0; i < 4; i++) {
				int j = 0;
				while (j < width) {
					if (p >= pEnd) { Z_Free(scanline); return qfalse; }
					if (*p > 128) {
						int count = *p++ - 128;
						if (p >= pEnd || j + count > width) { Z_Free(scanline); return qfalse; }
						byte val = *p++;
						while (count-- > 0) scanline[j++ * 4 + i] = val;
					} else {
						int count = *p++;
						if (p + count > pEnd || j + count > width) { Z_Free(scanline); return qfalse; }
						while (count-- > 0) scanline[j++ * 4 + i] = *p++;
					}
				}
			}

			for (x = 0; x < width; x++) {
				int idx = (y * width + x) * 4;
				float rgb[3];
				RGBE_to_float(scanline[x*4+0], scanline[x*4+1], scanline[x*4+2], scanline[x*4+3], rgb);
				out[idx+0] = rgb[0];
				out[idx+1] = rgb[1];
				out[idx+2] = rgb[2];
				out[idx+3] = 1.0f;
			}
			Z_Free(scanline);
		} else {
			for (x = 0; x < width; x++) {
				if (p + 4 > pEnd) return qfalse;
				int idx = (y * width + x) * 4;
				float rgb[3];
				RGBE_to_float(p[0], p[1], p[2], p[3], rgb);
				out[idx+0] = rgb[0];
				out[idx+1] = rgb[1];
				out[idx+2] = rgb[2];
				out[idx+3] = 1.0f;
				p += 4;
			}
		}
	}
	return qtrue;
}

void R_LoadHDR(const char *filename, byte **pic, int *width, int *height) {
	void *fileData;
	int fileSize, w, h, dataOff, numPixels, i;
	float *hdrData;
	byte *out;

	*pic = NULL; *width = 0; *height = 0;

	fileSize = FS_ReadFile(filename, &fileData);
	if (fileSize <= 0 || !fileData) return;

	if (!HDR_ReadHeader((const byte *)fileData, fileSize, &w, &h, &dataOff)) {
		Com_Printf(S_COLOR_YELLOW "HDR: invalid header in %s\n", filename);
		FS_FreeFile(fileData);
		return;
	}

	numPixels = w * h;
	hdrData = (float *)Z_Malloc(numPixels * 4 * sizeof(float));

	if (!HDR_DecodeRLE((const byte *)fileData, fileSize, dataOff, w, h, hdrData)) {
		Com_Printf(S_COLOR_YELLOW "HDR: decode error in %s\n", filename);
		Z_Free(hdrData);
		FS_FreeFile(fileData);
		return;
	}

	FS_FreeFile(fileData);

	out = (byte *)Z_Malloc(numPixels * 4);
	for (i = 0; i < numPixels; i++) {
		float r = hdrData[i*4+0], g = hdrData[i*4+1], b = hdrData[i*4+2];
		r = r > 1.0f ? 1.0f : (r < 0 ? 0 : r);
		g = g > 1.0f ? 1.0f : (g < 0 ? 0 : g);
		b = b > 1.0f ? 1.0f : (b < 0 ? 0 : b);
		float inv = 1.0f / 2.2f;
		out[i*4+0] = (byte)(powf(r, inv) * 255.0f + 0.5f);
		out[i*4+1] = (byte)(powf(g, inv) * 255.0f + 0.5f);
		out[i*4+2] = (byte)(powf(b, inv) * 255.0f + 0.5f);
		out[i*4+3] = 255;
	}

	Z_Free(hdrData);
	*pic = out; *width = w; *height = h;
}

void R_LoadHDR_Float(const char *filename, float **pic, int *width, int *height) {
	void *fileData;
	int fileSize, w, h, dataOff, numPixels;
	float *hdrData;

	*pic = NULL;
	*width = 0;
	*height = 0;

	fileSize = FS_ReadFile(filename, &fileData);
	if (fileSize <= 0 || !fileData) {
		return;
	}

	if (!HDR_ReadHeader((const byte *)fileData, fileSize, &w, &h, &dataOff)) {
		Com_Printf(S_COLOR_YELLOW "HDR: invalid header in %s\n", filename);
		FS_FreeFile(fileData);
		return;
	}

	numPixels = w * h;
	hdrData = (float *)Z_Malloc(numPixels * 4 * sizeof(float));

	if (!HDR_DecodeRLE((const byte *)fileData, fileSize, dataOff, w, h, hdrData)) {
		Com_Printf(S_COLOR_YELLOW "HDR: decode error in %s\n", filename);
		Z_Free(hdrData);
		FS_FreeFile(fileData);
		return;
	}

	FS_FreeFile(fileData);

	*pic = hdrData;
	*width = w;
	*height = h;
}
