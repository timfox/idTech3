/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

dav1d AV1 decoder backend for id Tech 3 engine.
Provides high-performance AV1 video decoding via the dav1d library.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "cl_cin_modern.h"

#ifdef USE_DAV1D

#include <dav1d/dav1d.h>

typedef struct {
	Dav1dContext    *dav1dCtx;
	Dav1dSettings    settings;

	fileHandle_t     file;
	int              fileSize;
	int              fileOffset;

	byte            *fileData;

	byte            *frameBuffer;
	int              frameBufferSize;

	qboolean         eof;
	int              width;
	int              height;
} dav1dContext_t;

static qboolean dav1d_open(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize);
static qboolean dav1d_decodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio);
static void     dav1d_seek(cinModernDecoder_t *dec, int timeMs);
static void     dav1d_close(cinModernDecoder_t *dec);
static qboolean dav1d_isEof(cinModernDecoder_t *dec);

static void dav1d_yuv420_to_rgba(const Dav1dPicture *pic, byte *rgba, int width, int height);

/*
===============
CIN_Dav1d_CreateDecoder
===============
*/
qboolean CIN_Dav1d_CreateDecoder(cinModernDecoder_t *dec) {
	dec->open = dav1d_open;
	dec->decodeFrame = dav1d_decodeFrame;
	dec->seek = dav1d_seek;
	dec->close = dav1d_close;
	dec->isEof = dav1d_isEof;
	dec->type = CODEC_DAV1D;
	return qtrue;
}

/*
===============
dav1d_open
===============
*/
static qboolean dav1d_open(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize) {
	dav1dContext_t *ctx;
	int ret;

	(void)file;

	ctx = (dav1dContext_t *)Z_Malloc(sizeof(dav1dContext_t));
	Com_Memset(ctx, 0, sizeof(*ctx));

	dav1d_default_settings(&ctx->settings);
	ctx->settings.n_threads = 4;
	ctx->settings.max_frame_delay = 1;

	ret = dav1d_open(&ctx->dav1dCtx, &ctx->settings);
	if (ret < 0) {
		Com_Printf(S_COLOR_RED "dav1d: Could not create decoder context\n");
		Z_Free(ctx);
		return qfalse;
	}

	ctx->fileSize = FS_ReadFile(filename, (void **)&ctx->fileData);
	if (ctx->fileSize <= 0 || !ctx->fileData) {
		Com_Printf(S_COLOR_RED "dav1d: Could not read %s\n", filename);
		dav1d_close_internal(ctx->dav1dCtx);
		Z_Free(ctx);
		return qfalse;
	}

	ctx->fileOffset = 0;
	ctx->eof = qfalse;

	dec->context = ctx;
	dec->fps = 30.0f;

	Com_Printf("dav1d: Opened AV1 file %s (%d bytes)\n", filename, ctx->fileSize);
	return qtrue;
}

/*
===============
dav1d_yuv420_to_rgba

Convert YUV420 planar to RGBA.
===============
*/
static void dav1d_yuv420_to_rgba(const Dav1dPicture *pic, byte *rgba, int width, int height) {
	const uint8_t *yPlane = (const uint8_t *)pic->data[0];
	const uint8_t *uPlane = (const uint8_t *)pic->data[1];
	const uint8_t *vPlane = (const uint8_t *)pic->data[2];
	int yStride = (int)pic->stride[0];
	int uvStride = (int)pic->stride[1];
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int yVal = yPlane[y * yStride + x];
			int uVal = uPlane[(y >> 1) * uvStride + (x >> 1)];
			int vVal = vPlane[(y >> 1) * uvStride + (x >> 1)];

			int c = yVal - 16;
			int d = uVal - 128;
			int e = vVal - 128;

			int r = (298 * c + 409 * e + 128) >> 8;
			int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
			int b = (298 * c + 516 * d + 128) >> 8;

			int idx = (y * width + x) * 4;
			rgba[idx + 0] = (byte)(r < 0 ? 0 : (r > 255 ? 255 : r));
			rgba[idx + 1] = (byte)(g < 0 ? 0 : (g > 255 ? 255 : g));
			rgba[idx + 2] = (byte)(b < 0 ? 0 : (b > 255 ? 255 : b));
			rgba[idx + 3] = 255;
		}
	}
}

/*
===============
dav1d_decodeFrame
===============
*/
static qboolean dav1d_decodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio) {
	dav1dContext_t *ctx;
	Dav1dData data;
	Dav1dPicture pic;
	int ret;

	(void)audio;

	if (!dec || !dec->context) {
		return qfalse;
	}

	ctx = (dav1dContext_t *)dec->context;

	if (ctx->eof) {
		return qfalse;
	}

	Com_Memset(&pic, 0, sizeof(pic));

	if (ctx->fileOffset < ctx->fileSize) {
		int remaining = ctx->fileSize - ctx->fileOffset;
		int chunkSize = remaining < 65536 ? remaining : 65536;

		Com_Memset(&data, 0, sizeof(data));
		uint8_t *buf = dav1d_data_create(&data, chunkSize);
		if (buf) {
			Com_Memcpy(buf, ctx->fileData + ctx->fileOffset, chunkSize);
			ctx->fileOffset += chunkSize;

			ret = dav1d_send_data(ctx->dav1dCtx, &data);
			if (ret < 0 && ret != DAV1D_ERR(EAGAIN)) {
				dav1d_data_unref(&data);
			}
		}
	}

	ret = dav1d_get_picture(ctx->dav1dCtx, &pic);
	if (ret < 0) {
		if (ctx->fileOffset >= ctx->fileSize) {
			ctx->eof = qtrue;
		}
		return qfalse;
	}

	dec->width = pic.p.w;
	dec->height = pic.p.h;
	ctx->width = pic.p.w;
	ctx->height = pic.p.h;

	if (!ctx->frameBuffer || ctx->frameBufferSize < pic.p.w * pic.p.h * 4) {
		if (ctx->frameBuffer) {
			Z_Free(ctx->frameBuffer);
		}
		ctx->frameBufferSize = pic.p.w * pic.p.h * 4;
		ctx->frameBuffer = (byte *)Z_Malloc(ctx->frameBufferSize);
	}

	dav1d_yuv420_to_rgba(&pic, ctx->frameBuffer, pic.p.w, pic.p.h);

	if (frame) {
		frame->data = ctx->frameBuffer;
		frame->width = pic.p.w;
		frame->height = pic.p.h;
		frame->stride = pic.p.w * 4;
		frame->format = CIN_FRAME_RGBA;
		frame->valid = qtrue;
	}

	dav1d_picture_unref(&pic);
	return qtrue;
}

/*
===============
dav1d_seek
===============
*/
static void dav1d_seek(cinModernDecoder_t *dec, int timeMs) {
	dav1dContext_t *ctx;

	(void)timeMs;

	if (!dec || !dec->context) {
		return;
	}

	ctx = (dav1dContext_t *)dec->context;
	ctx->fileOffset = 0;
	ctx->eof = qfalse;
	dav1d_flush(ctx->dav1dCtx);
}

/*
===============
dav1d_close
===============
*/
static void dav1d_close(cinModernDecoder_t *dec) {
	dav1dContext_t *ctx;

	if (!dec || !dec->context) {
		return;
	}

	ctx = (dav1dContext_t *)dec->context;

	if (ctx->dav1dCtx) {
		dav1d_close(&ctx->dav1dCtx);
	}
	if (ctx->fileData) {
		FS_FreeFile(ctx->fileData);
	}
	if (ctx->frameBuffer) {
		Z_Free(ctx->frameBuffer);
	}

	Z_Free(ctx);
	dec->context = NULL;
}

/*
===============
dav1d_isEof
===============
*/
static qboolean dav1d_isEof(cinModernDecoder_t *dec) {
	dav1dContext_t *ctx;

	if (!dec || !dec->context) {
		return qtrue;
	}

	ctx = (dav1dContext_t *)dec->context;
	return ctx->eof;
}

#endif /* USE_DAV1D */
