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
#include "cl_cin_colors.h"

#ifdef USE_VVDEC

#include <vvdec/vvdec.h>

typedef struct {
	vvdecDecoder    *vvdecCtx;
	vvdecParams      params;

	int              fileSize;
	byte            *fileData;

	byte            *frameBuffer;
	int              frameBufferSize;

	qboolean         bitstreamSent;
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

static void cin_vvdec_to_rgba(const vvdecFrame *src, byte *rgba) {
	int chromaShiftX = 0;
	int chromaShiftY = 0;

	switch (src->colorFormat) {
		case VVDEC_CF_YUV420_PLANAR:
			chromaShiftX = 1;
			chromaShiftY = 1;
			break;
		case VVDEC_CF_YUV422_PLANAR:
			chromaShiftX = 1;
			break;
		case VVDEC_CF_YUV444_PLANAR:
		case VVDEC_CF_YUV400_PLANAR:
		default:
			break;
	}

	if (src->bitDepth <= 8) {
		CIN_ConvertPlanarYUV8ToRGBA(
			src->planes[0].ptr, (int)src->planes[0].stride,
			src->colorFormat == VVDEC_CF_YUV400_PLANAR ? NULL : src->planes[1].ptr, (int)src->planes[1].stride,
			src->colorFormat == VVDEC_CF_YUV400_PLANAR ? NULL : src->planes[2].ptr, (int)src->planes[2].stride,
			chromaShiftX, chromaShiftY, rgba, (int)src->width, (int)src->height );
		return;
	}

	CIN_ConvertPlanarYUV16ToRGBA(
		(const uint16_t *)src->planes[0].ptr, (int)src->planes[0].stride,
		src->colorFormat == VVDEC_CF_YUV400_PLANAR ? NULL : (const uint16_t *)src->planes[1].ptr, (int)src->planes[1].stride,
		src->colorFormat == VVDEC_CF_YUV400_PLANAR ? NULL : (const uint16_t *)src->planes[2].ptr, (int)src->planes[2].stride,
		chromaShiftX, chromaShiftY, (1u << src->bitDepth) - 1u, rgba, (int)src->width, (int)src->height );
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

		if (!ctx->bitstreamSent) {
			vvdecAccessUnit accessUnit;

			vvdec_accessUnit_default(&accessUnit);
			accessUnit.payload = (unsigned char *)ctx->fileData;
			accessUnit.payloadSize = ctx->fileSize;
			accessUnit.payloadUsedSize = ctx->fileSize;

			/* Feed the full in-memory Annex B stream once, then drain pictures. */
			ret = vvdec_decode(ctx->vvdecCtx, &accessUnit, &decodedFrame);
			ctx->bitstreamSent = qtrue;
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

	ctx->bitstreamSent = qfalse;
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
