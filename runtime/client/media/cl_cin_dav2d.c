/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

dav2d AV2 decoder backend for id Tech 3 engine.
Provides native AV2 elementary stream decoding via the dav2d library.
===========================================================================
*/

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "cl_cin_modern.h"

#ifdef USE_DAV2D

#include <dav2d/dav2d.h>

typedef struct {
	Dav2dContext    *dav2dCtx;
	Dav2dSettings    settings;

	int              fileSize;
	int              fileOffset;
	byte            *fileData;

	byte            *frameBuffer;
	int              frameBufferSize;

	qboolean         sentDrain;
	qboolean         eof;
	int              width;
	int              height;
} dav2dContext_t;

static qboolean cin_dav2d_open(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize);
static qboolean cin_dav2d_decodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio);
static void     cin_dav2d_seek(cinModernDecoder_t *dec, int timeMs);
static void     cin_dav2d_close(cinModernDecoder_t *dec);
static qboolean cin_dav2d_isEof(cinModernDecoder_t *dec);

static byte cin_dav2d_rescale_sample(unsigned sample, unsigned maxValue) {
	if (maxValue == 0) {
		return 0;
	}
	return (byte)((sample * 255u + (maxValue / 2u)) / maxValue);
}

static int cin_dav2d_clip_byte(int value) {
	if (value < 0) {
		return 0;
	}
	if (value > 255) {
		return 255;
	}
	return value;
}

static unsigned cin_dav2d_plane_sample(const Dav2dPicture *pic, int plane, int x, int y) {
	const int bytesPerSample = pic->p.bpc > 8 ? 2 : 1;
	const int stride = (int)pic->stride[plane == 0 ? 0 : 1];
	const byte *row = (const byte *)pic->data[plane] + (y * stride);

	if (bytesPerSample == 1) {
		return row[x];
	}

	return ((const uint16_t *)row)[x];
}

static void cin_dav2d_to_rgba(const Dav2dPicture *pic, byte *rgba) {
	const int width = pic->p.w;
	const int height = pic->p.h;
	const unsigned maxValue = (1u << pic->p.bpc) - 1u;
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			unsigned yRaw = cin_dav2d_plane_sample(pic, 0, x, y);
			unsigned uRaw = maxValue / 2u;
			unsigned vRaw = maxValue / 2u;
			int uvx = x;
			int uvy = y;
			byte yVal, uVal, vVal;
			int c, d, e;
			int idx;

			switch (pic->p.layout) {
				case DAV2D_PIXEL_LAYOUT_I420:
					uvx >>= 1;
					uvy >>= 1;
					uRaw = cin_dav2d_plane_sample(pic, 1, uvx, uvy);
					vRaw = cin_dav2d_plane_sample(pic, 2, uvx, uvy);
					break;
				case DAV2D_PIXEL_LAYOUT_I422:
					uvx >>= 1;
					uRaw = cin_dav2d_plane_sample(pic, 1, uvx, uvy);
					vRaw = cin_dav2d_plane_sample(pic, 2, uvx, uvy);
					break;
				case DAV2D_PIXEL_LAYOUT_I444:
					uRaw = cin_dav2d_plane_sample(pic, 1, uvx, uvy);
					vRaw = cin_dav2d_plane_sample(pic, 2, uvx, uvy);
					break;
				case DAV2D_PIXEL_LAYOUT_I400:
				default:
					break;
			}

			yVal = cin_dav2d_rescale_sample(yRaw, maxValue);
			uVal = cin_dav2d_rescale_sample(uRaw, maxValue);
			vVal = cin_dav2d_rescale_sample(vRaw, maxValue);

			c = (int)yVal - 16;
			d = (int)uVal - 128;
			e = (int)vVal - 128;

			idx = (y * width + x) * 4;
			rgba[idx + 0] = (byte)cin_dav2d_clip_byte((298 * c + 409 * e + 128) >> 8);
			rgba[idx + 1] = (byte)cin_dav2d_clip_byte((298 * c - 100 * d - 208 * e + 128) >> 8);
			rgba[idx + 2] = (byte)cin_dav2d_clip_byte((298 * c + 516 * d + 128) >> 8);
			rgba[idx + 3] = 255;
		}
	}
}

qboolean CIN_Dav2d_CreateDecoder(cinModernDecoder_t *dec) {
	dec->open = cin_dav2d_open;
	dec->decodeFrame = cin_dav2d_decodeFrame;
	dec->seek = cin_dav2d_seek;
	dec->close = cin_dav2d_close;
	dec->isEof = cin_dav2d_isEof;
	dec->type = CODEC_DAV2D;
	return qtrue;
}

static qboolean cin_dav2d_open(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize) {
	dav2dContext_t *ctx;
	int ret;

	(void)file;
	(void)fileSize;

	ctx = (dav2dContext_t *)Z_Malloc(sizeof(*ctx));
	Com_Memset(ctx, 0, sizeof(*ctx));

	dav2d_default_settings(&ctx->settings);
	ctx->settings.n_threads = 4;
	ctx->settings.max_frame_delay = 1;

	ret = dav2d_open(&ctx->dav2dCtx, &ctx->settings);
	if (ret < 0) {
		Com_Printf(S_COLOR_RED "dav2d: Could not create decoder context for %s\n", filename);
		Z_Free(ctx);
		return qfalse;
	}

	ctx->fileSize = FS_ReadFile(filename, (void **)&ctx->fileData);
	if (ctx->fileSize <= 0 || !ctx->fileData) {
		Com_Printf(S_COLOR_RED "dav2d: Could not read %s\n", filename);
		dav2d_close(&ctx->dav2dCtx);
		Z_Free(ctx);
		return qfalse;
	}

	dec->context = ctx;
	dec->fps = 30.0f;

	Com_Printf("dav2d: Opened AV2 file %s (%d bytes)\n", filename, ctx->fileSize);
	return qtrue;
}

static qboolean cin_dav2d_decodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio) {
	dav2dContext_t *ctx;
	Dav2dPicture pic;
	int ret;

	(void)audio;

	if (!dec || !dec->context) {
		return qfalse;
	}

	ctx = (dav2dContext_t *)dec->context;
	Com_Memset(&pic, 0, sizeof(pic));

	for (;;) {
		ret = dav2d_get_picture(ctx->dav2dCtx, &pic);
		if (ret == 0) {
			int neededSize = pic.p.w * pic.p.h * 4;

			dec->width = pic.p.w;
			dec->height = pic.p.h;
			ctx->width = pic.p.w;
			ctx->height = pic.p.h;

			if (!ctx->frameBuffer || ctx->frameBufferSize < neededSize) {
				if (ctx->frameBuffer) {
					Z_Free(ctx->frameBuffer);
				}
				ctx->frameBufferSize = neededSize;
				ctx->frameBuffer = (byte *)Z_Malloc(ctx->frameBufferSize);
			}

			cin_dav2d_to_rgba(&pic, ctx->frameBuffer);
			dav2d_picture_unref(&pic);

			if (frame) {
				frame->data = ctx->frameBuffer;
				frame->width = ctx->width;
				frame->height = ctx->height;
				frame->stride = ctx->width * 4;
				frame->format = CIN_FRAME_RGBA;
				frame->valid = qtrue;
			}

			return qtrue;
		}

		if (ret != DAV2D_ERR(EAGAIN)) {
			if (ctx->sentDrain) {
				ctx->eof = qtrue;
			}
			return qfalse;
		}

		if (ctx->fileOffset < ctx->fileSize) {
			Dav2dData data;
			int remaining = ctx->fileSize - ctx->fileOffset;
			int chunkSize = remaining < 65536 ? remaining : 65536;
			uint8_t *buf;

			Com_Memset(&data, 0, sizeof(data));
			buf = dav2d_data_create(&data, chunkSize);
			if (!buf) {
				Com_Printf(S_COLOR_RED "dav2d: Out of memory while decoding\n");
				ctx->eof = qtrue;
				return qfalse;
			}

			Com_Memcpy(buf, ctx->fileData + ctx->fileOffset, chunkSize);
			ret = dav2d_send_data(ctx->dav2dCtx, &data);
			if (ret == 0) {
				ctx->fileOffset += chunkSize;
			} else if (ret == DAV2D_ERR(EAGAIN)) {
				dav2d_data_unref(&data);
			} else {
				dav2d_data_unref(&data);
				Com_Printf(S_COLOR_RED "dav2d: Failed to send AV2 bitstream data\n");
				ctx->eof = qtrue;
				return qfalse;
			}
			continue;
		}

		if (!ctx->sentDrain) {
			ret = dav2d_send_data(ctx->dav2dCtx, NULL);
			if (ret < 0 && ret != DAV2D_EOF) {
				Com_Printf(S_COLOR_RED "dav2d: Failed to drain decoder\n");
				ctx->eof = qtrue;
				return qfalse;
			}
			ctx->sentDrain = qtrue;
			continue;
		}

		ctx->eof = qtrue;
		return qfalse;
	}
}

static void cin_dav2d_seek(cinModernDecoder_t *dec, int timeMs) {
	dav2dContext_t *ctx;

	(void)timeMs;

	if (!dec || !dec->context) {
		return;
	}

	ctx = (dav2dContext_t *)dec->context;
	ctx->fileOffset = 0;
	ctx->sentDrain = qfalse;
	ctx->eof = qfalse;
	dav2d_flush(ctx->dav2dCtx);
}

static void cin_dav2d_close(cinModernDecoder_t *dec) {
	dav2dContext_t *ctx;

	if (!dec || !dec->context) {
		return;
	}

	ctx = (dav2dContext_t *)dec->context;

	if (ctx->dav2dCtx) {
		dav2d_close(&ctx->dav2dCtx);
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

static qboolean cin_dav2d_isEof(cinModernDecoder_t *dec) {
	dav2dContext_t *ctx;

	if (!dec || !dec->context) {
		return qtrue;
	}

	ctx = (dav2dContext_t *)dec->context;
	return ctx->eof;
}

#endif /* USE_DAV2D */
