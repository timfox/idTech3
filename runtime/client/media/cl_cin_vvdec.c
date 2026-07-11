/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

vvdec VVC decoder backend for id Tech 3 engine.
Provides native H.266/VVC elementary stream decoding via the vvdec library.
===========================================================================
*/

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "cl_cin_modern.h"

#ifdef USE_VVDEC

#include <vvdec/vvdec.h>

typedef struct {
	vvdecDecoder    *vvdecCtx;
	vvdecParams      params;

	int              fileSize;
	int              fileOffset;
	byte            *fileData;

	byte            *frameBuffer;
	int              frameBufferSize;

	qboolean         sentDrain;
	qboolean         eof;
	int              width;
	int              height;
} vvdecContext_t;

static qboolean cin_vvdec_open(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize);
static qboolean cin_vvdec_decodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio);
static void     cin_vvdec_seek(cinModernDecoder_t *dec, int timeMs);
static void     cin_vvdec_close(cinModernDecoder_t *dec);
static qboolean cin_vvdec_isEof(cinModernDecoder_t *dec);

static byte cin_vvdec_rescale_sample(unsigned sample, unsigned maxValue) {
	if (maxValue == 0) {
		return 0;
	}
	return (byte)((sample * 255u + (maxValue / 2u)) / maxValue);
}

static int cin_vvdec_clip_byte(int value) {
	if (value < 0) {
		return 0;
	}
	if (value > 255) {
		return 255;
	}
	return value;
}

static qboolean cin_vvdec_is_start_code3(const byte *buf) {
	return buf[0] == 0 && buf[1] == 0 && buf[2] == 1;
}

static qboolean cin_vvdec_is_start_code4(const byte *buf) {
	return buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1;
}

static int cin_vvdec_next_nal(const vvdecContext_t *ctx, int offset, const byte **nalData, int *nalSize) {
	int pos;
	int start = -1;
	int next = -1;

	if (!ctx || !nalData || !nalSize || offset >= ctx->fileSize) {
		return -1;
	}

	for (pos = offset; pos + 3 < ctx->fileSize; pos++) {
		if (cin_vvdec_is_start_code4(ctx->fileData + pos) || cin_vvdec_is_start_code3(ctx->fileData + pos)) {
			start = pos;
			break;
		}
	}

	if (start < 0) {
		return -1;
	}

	for (pos = start + 3; pos + 3 < ctx->fileSize; pos++) {
		if (cin_vvdec_is_start_code4(ctx->fileData + pos) || cin_vvdec_is_start_code3(ctx->fileData + pos)) {
			next = pos;
			break;
		}
	}

	if (next < 0) {
		next = ctx->fileSize;
	}

	*nalData = ctx->fileData + start;
	*nalSize = next - start;
	return next;
}

static unsigned cin_vvdec_plane_sample(const vvdecPlane *plane, int x, int y) {
	const byte *row = plane->ptr + (y * plane->stride);

	if (plane->bytesPerSample <= 1) {
		return row[x];
	}

	return ((const uint16_t *)row)[x];
}

static void cin_vvdec_to_rgba(const vvdecFrame *src, byte *rgba) {
	const unsigned maxValue = (1u << src->bitDepth) - 1u;
	int x, y;

	for (y = 0; y < (int)src->height; y++) {
		for (x = 0; x < (int)src->width; x++) {
			unsigned yRaw = cin_vvdec_plane_sample(&src->planes[0], x, y);
			unsigned uRaw = maxValue / 2u;
			unsigned vRaw = maxValue / 2u;
			int uvx = x;
			int uvy = y;
			byte yVal, uVal, vVal;
			int c, d, e;
			int idx;

			switch (src->colorFormat) {
				case VVDEC_CF_YUV420_PLANAR:
					uvx >>= 1;
					uvy >>= 1;
					uRaw = cin_vvdec_plane_sample(&src->planes[1], uvx, uvy);
					vRaw = cin_vvdec_plane_sample(&src->planes[2], uvx, uvy);
					break;
				case VVDEC_CF_YUV422_PLANAR:
					uvx >>= 1;
					uRaw = cin_vvdec_plane_sample(&src->planes[1], uvx, uvy);
					vRaw = cin_vvdec_plane_sample(&src->planes[2], uvx, uvy);
					break;
				case VVDEC_CF_YUV444_PLANAR:
					uRaw = cin_vvdec_plane_sample(&src->planes[1], uvx, uvy);
					vRaw = cin_vvdec_plane_sample(&src->planes[2], uvx, uvy);
					break;
				case VVDEC_CF_YUV400_PLANAR:
				default:
					break;
			}

			yVal = cin_vvdec_rescale_sample(yRaw, maxValue);
			uVal = cin_vvdec_rescale_sample(uRaw, maxValue);
			vVal = cin_vvdec_rescale_sample(vRaw, maxValue);

			c = (int)yVal - 16;
			d = (int)uVal - 128;
			e = (int)vVal - 128;

			idx = (y * (int)src->width + x) * 4;
			rgba[idx + 0] = (byte)cin_vvdec_clip_byte((298 * c + 409 * e + 128) >> 8);
			rgba[idx + 1] = (byte)cin_vvdec_clip_byte((298 * c - 100 * d - 208 * e + 128) >> 8);
			rgba[idx + 2] = (byte)cin_vvdec_clip_byte((298 * c + 516 * d + 128) >> 8);
			rgba[idx + 3] = 255;
		}
	}
}

qboolean CIN_Vvdec_CreateDecoder(cinModernDecoder_t *dec) {
	dec->open = cin_vvdec_open;
	dec->decodeFrame = cin_vvdec_decodeFrame;
	dec->seek = cin_vvdec_seek;
	dec->close = cin_vvdec_close;
	dec->isEof = cin_vvdec_isEof;
	dec->type = CODEC_VVDEC;
	return qtrue;
}

static qboolean cin_vvdec_open(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize) {
	vvdecContext_t *ctx;

	(void)file;
	(void)fileSize;

	ctx = (vvdecContext_t *)Z_Malloc(sizeof(*ctx));
	Com_Memset(ctx, 0, sizeof(*ctx));

	vvdec_params_default(&ctx->params);
	ctx->params.threads = 4;
	ctx->params.logLevel = VVDEC_WARNING;

	ctx->vvdecCtx = vvdec_decoder_open(&ctx->params);
	if (!ctx->vvdecCtx) {
		Com_Printf(S_COLOR_RED "vvdec: Could not create decoder context for %s\n", filename);
		Z_Free(ctx);
		return qfalse;
	}

	ctx->fileSize = FS_ReadFile(filename, (void **)&ctx->fileData);
	if (ctx->fileSize <= 0 || !ctx->fileData) {
		Com_Printf(S_COLOR_RED "vvdec: Could not read %s\n", filename);
		vvdec_decoder_close(ctx->vvdecCtx);
		Z_Free(ctx);
		return qfalse;
	}

	dec->context = ctx;
	dec->fps = 30.0f;

	Com_Printf("vvdec: Opened VVC file %s (%d bytes)\n", filename, ctx->fileSize);
	return qtrue;
}

static qboolean cin_vvdec_decodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio) {
	vvdecContext_t *ctx;

	(void)audio;

	if (!dec || !dec->context) {
		return qfalse;
	}

	ctx = (vvdecContext_t *)dec->context;

	for (;;) {
		vvdecFrame *decodedFrame = NULL;
		int ret;

		if (ctx->fileOffset < ctx->fileSize) {
			const byte *nalData = NULL;
			int nalSize = 0;
			int nextOffset = cin_vvdec_next_nal(ctx, ctx->fileOffset, &nalData, &nalSize);

			if (nextOffset < 0 || !nalData || nalSize <= 0) {
				ctx->fileOffset = ctx->fileSize;
				continue;
			}

			ctx->fileOffset = nextOffset;

			{
				vvdecAccessUnit accessUnit;

				vvdec_accessUnit_default(&accessUnit);
				accessUnit.payload = (unsigned char *)nalData;
				accessUnit.payloadSize = nalSize;
				accessUnit.payloadUsedSize = nalSize;

				ret = vvdec_decode(ctx->vvdecCtx, &accessUnit, &decodedFrame);
			}
		} else if (!ctx->sentDrain) {
			ret = vvdec_flush(ctx->vvdecCtx, &decodedFrame);
			ctx->sentDrain = qtrue;
		} else {
			ctx->eof = qtrue;
			return qfalse;
		}

		if (ret == VVDEC_EOF) {
			ctx->eof = qtrue;
			return qfalse;
		}

		if (ret != VVDEC_OK && ret != VVDEC_TRY_AGAIN) {
			Com_Printf(S_COLOR_RED "vvdec: %s\n", vvdec_get_error_msg(ret));
			ctx->eof = qtrue;
			return qfalse;
		}

		if (decodedFrame) {
			int neededSize = (int)(decodedFrame->width * decodedFrame->height * 4);

			dec->width = (int)decodedFrame->width;
			dec->height = (int)decodedFrame->height;
			ctx->width = dec->width;
			ctx->height = dec->height;

			if (!ctx->frameBuffer || ctx->frameBufferSize < neededSize) {
				if (ctx->frameBuffer) {
					Z_Free(ctx->frameBuffer);
				}
				ctx->frameBufferSize = neededSize;
				ctx->frameBuffer = (byte *)Z_Malloc(ctx->frameBufferSize);
			}

			cin_vvdec_to_rgba(decodedFrame, ctx->frameBuffer);
			vvdec_frame_unref(ctx->vvdecCtx, decodedFrame);

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
	}
}

static void cin_vvdec_seek(cinModernDecoder_t *dec, int timeMs) {
	vvdecContext_t *ctx;

	(void)timeMs;

	if (!dec || !dec->context) {
		return;
	}

	ctx = (vvdecContext_t *)dec->context;

	if (ctx->vvdecCtx) {
		vvdec_decoder_close(ctx->vvdecCtx);
		ctx->vvdecCtx = vvdec_decoder_open(&ctx->params);
	}

	ctx->fileOffset = 0;
	ctx->sentDrain = qfalse;
	ctx->eof = qfalse;
}

static void cin_vvdec_close(cinModernDecoder_t *dec) {
	vvdecContext_t *ctx;

	if (!dec || !dec->context) {
		return;
	}

	ctx = (vvdecContext_t *)dec->context;

	if (ctx->vvdecCtx) {
		vvdec_decoder_close(ctx->vvdecCtx);
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

static qboolean cin_vvdec_isEof(cinModernDecoder_t *dec) {
	vvdecContext_t *ctx;

	if (!dec || !dec->context) {
		return qtrue;
	}

	ctx = (vvdecContext_t *)dec->context;
	return ctx->eof;
}

#endif /* USE_VVDEC */
