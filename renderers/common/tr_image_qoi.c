#include "tr_image_loaders.h"
/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

QOI (Quite OK Image) loader using phoboslab/qoi (MIT license).
20-50x faster decode than PNG, lossless, RGBA.
===========================================================================
*/

#define QOI_IMPLEMENTATION
#define QOI_MALLOC(sz) malloc(sz)
#define QOI_FREE(p)    free(p)
#include "../../external/include/qoi/qoi.h"

#include "q_shared.h"
#include "qcommon.h"
#include <stdlib.h>

void R_LoadQOI(const char *filename, byte **pic, int *width, int *height) {
	void *fileData;
	int fileSize;
	qoi_desc desc;
	void *pixels;

	*pic = NULL; *width = 0; *height = 0;

	fileSize = FS_ReadFile(filename, &fileData);
	if (fileSize <= 0 || !fileData) return;

	pixels = qoi_decode(fileData, fileSize, &desc, 4);
	FS_FreeFile(fileData);

	if (!pixels) {
		Com_Printf(S_COLOR_YELLOW "QOI: decode failed for %s\n", filename);
		return;
	}

	if (desc.width <= 0 || desc.height <= 0 || desc.width > 16384 || desc.height > 16384) {
		Com_Printf(S_COLOR_YELLOW "QOI: invalid dimensions in %s (%ux%u)\n", filename, desc.width, desc.height);
		QOI_FREE(pixels);
		return;
	}

	int numPixels = (int)(desc.width * desc.height);
	byte *out = (byte *)Z_Malloc(numPixels * 4);
	Com_Memcpy(out, pixels, numPixels * 4);
	QOI_FREE(pixels);

	*pic = out;
	*width = (int)desc.width;
	*height = (int)desc.height;
}
