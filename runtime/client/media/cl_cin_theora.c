/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Theora decoder backend for id Tech 3 engine.
Provides Ogg Theora video decoding via libtheora and libogg.
===========================================================================
*/

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "cl_cin_modern.h"
#include "cl_cin_colors.h"

#ifdef USE_THEORA

#include <theora/theoradec.h>
#include <ogg/ogg.h>

typedef struct {
	ogg_sync_state      syncState;
	ogg_stream_state    streamState;
	th_info             theoraInfo;
	th_comment          theoraComment;
	th_setup_info      *theoraSetup;
	th_dec_ctx         *theoraDecCtx;

	byte               *fileData;
	int                 fileSize;
	int                 fileOffset;

	byte               *frameBuffer;
	int                 frameBufferSize;

	qboolean            headersParsed;
	qboolean            streamInitialized;
	qboolean            eof;
} theoraContext_t;

static qboolean theora_open(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize);
static qboolean theora_decodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio);
static void     theora_seek(cinModernDecoder_t *dec, int timeMs);
static void     theora_close(cinModernDecoder_t *dec);
static qboolean theora_isEof(cinModernDecoder_t *dec);

/*
===============
CIN_Theora_CreateDecoder
===============
*/
qboolean CIN_Theora_CreateDecoder(cinModernDecoder_t *dec) {
	dec->open = theora_open;
	dec->decodeFrame = theora_decodeFrame;
	dec->seek = theora_seek;
	dec->close = theora_close;
	dec->isEof = theora_isEof;
	dec->type = CODEC_THEORA;
	return qtrue;
}

static int theora_feedPage(theoraContext_t *ctx) {
	ogg_page page;
	char *buffer;
	int remaining;
	int chunkSize;

	remaining = ctx->fileSize - ctx->fileOffset;
	if (remaining <= 0) {
		return -1;
	}

	chunkSize = remaining < 8192 ? remaining : 8192;
	buffer = ogg_sync_buffer(&ctx->syncState, chunkSize);
	if (!buffer) {
		return -1;
	}

	Com_Memcpy(buffer, ctx->fileData + ctx->fileOffset, chunkSize);
	ctx->fileOffset += chunkSize;
	ogg_sync_wrote(&ctx->syncState, chunkSize);

	if (ogg_sync_pageout(&ctx->syncState, &page) != 1) {
		return 0;
	}

	if (!ctx->streamInitialized) {
		ogg_stream_init(&ctx->streamState, ogg_page_serialno(&page));
		ctx->streamInitialized = qtrue;
	}

	ogg_stream_pagein(&ctx->streamState, &page);
	return 1;
}

/*
===============
theora_open
===============
*/
static qboolean theora_open(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize) {
	theoraContext_t *ctx;
	ogg_packet packet;
	int headersNeeded = 3;
	int headersRead = 0;

	(void)file;
	(void)fileSize;

	ctx = (theoraContext_t *)Z_Malloc(sizeof(theoraContext_t));
	Com_Memset(ctx, 0, sizeof(*ctx));

	ctx->fileSize = FS_ReadFile(filename, (void **)&ctx->fileData);
	if (ctx->fileSize <= 0 || !ctx->fileData) {
		Com_Printf(S_COLOR_RED "Theora: Could not read %s\n", filename);
		Z_Free(ctx);
		return qfalse;
	}
	ctx->fileOffset = 0;

	ogg_sync_init(&ctx->syncState);
	th_info_init(&ctx->theoraInfo);
	th_comment_init(&ctx->theoraComment);

	while (headersRead < headersNeeded) {
		int feedRet = theora_feedPage(ctx);
		if (feedRet < 0) {
			break;
		}
		if (feedRet == 0) {
			continue;
		}

		while (ogg_stream_packetout(&ctx->streamState, &packet) == 1) {
			int ret = th_decode_headerin(&ctx->theoraInfo, &ctx->theoraComment, &ctx->theoraSetup, &packet);
			if (ret < 0) {
				break;
			}
			if (ret > 0) {
				headersRead++;
			}
			if (ret == 0) {
				headersRead = headersNeeded;
				break;
			}
		}
	}

	if (headersRead < headersNeeded) {
		Com_Printf(S_COLOR_RED "Theora: Could not parse headers in %s\n", filename);
		theora_close(dec);
		Z_Free(ctx);
		return qfalse;
	}

	ctx->theoraDecCtx = th_decode_alloc(&ctx->theoraInfo, ctx->theoraSetup);
	if (!ctx->theoraDecCtx) {
		Com_Printf(S_COLOR_RED "Theora: Could not create decoder for %s\n", filename);
		theora_close(dec);
		Z_Free(ctx);
		return qfalse;
	}

	dec->width = ctx->theoraInfo.pic_width;
	dec->height = ctx->theoraInfo.pic_height;
	dec->fps = (float)ctx->theoraInfo.fps_numerator / (float)ctx->theoraInfo.fps_denominator;
	ctx->headersParsed = qtrue;
	ctx->eof = qfalse;
	dec->context = ctx;

	Com_Printf("Theora: Opened %s (%dx%d, %.1f fps)\n",
		filename, dec->width, dec->height, (double)dec->fps);

	return qtrue;
}

/*
===============
theora_decodeFrame
===============
*/
static qboolean theora_decodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio) {
	theoraContext_t *ctx;
	ogg_packet packet;
	th_ycbcr_buffer ycbcr;

	(void)audio;

	if (!dec || !dec->context) {
		return qfalse;
	}

	ctx = (theoraContext_t *)dec->context;

	if (ctx->eof) {
		return qfalse;
	}

	while (1) {
		if (ogg_stream_packetout(&ctx->streamState, &packet) == 1) {
			if (th_decode_packetin(ctx->theoraDecCtx, &packet, NULL) == 0) {
				if (th_decode_ycbcr_out(ctx->theoraDecCtx, ycbcr) == 0) {
					int w = dec->width;
					int h = dec->height;

					if (!ctx->frameBuffer || ctx->frameBufferSize < w * h * 4) {
						if (ctx->frameBuffer) {
							Z_Free(ctx->frameBuffer);
						}
						ctx->frameBufferSize = w * h * 4;
						ctx->frameBuffer = (byte *)Z_Malloc(ctx->frameBufferSize);
					}

						CIN_ConvertYUV420Planar8ToRGBA(
							ycbcr[0].data, ycbcr[0].stride,
							ycbcr[1].data, ycbcr[1].stride,
							ycbcr[2].data, ycbcr[2].stride,
							ctx->frameBuffer, w, h );

					if (frame) {
						frame->data = ctx->frameBuffer;
						frame->width = w;
						frame->height = h;
						frame->stride = w * 4;
						frame->format = CIN_FRAME_RGBA;
						frame->valid = qtrue;
					}

					return qtrue;
				}
			}
		}

		if (theora_feedPage(ctx) < 0) {
			ctx->eof = qtrue;
			return qfalse;
		}
	}
}

/*
===============
theora_seek
===============
*/
static void theora_seek(cinModernDecoder_t *dec, int timeMs) {
	(void)dec;
	(void)timeMs;
}

/*
===============
theora_close
===============
*/
static void theora_close(cinModernDecoder_t *dec) {
	theoraContext_t *ctx;

	if (!dec || !dec->context) {
		return;
	}

	ctx = (theoraContext_t *)dec->context;

	if (ctx->theoraDecCtx) {
		th_decode_free(ctx->theoraDecCtx);
	}
	if (ctx->theoraSetup) {
		th_setup_free(ctx->theoraSetup);
	}
	th_comment_clear(&ctx->theoraComment);
	th_info_clear(&ctx->theoraInfo);

	if (ctx->streamInitialized) {
		ogg_stream_clear(&ctx->streamState);
	}
	ogg_sync_clear(&ctx->syncState);

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
theora_isEof
===============
*/
static qboolean theora_isEof(cinModernDecoder_t *dec) {
	theoraContext_t *ctx;

	if (!dec || !dec->context) {
		return qtrue;
	}

	ctx = (theoraContext_t *)dec->context;
	return ctx->eof;
}

#endif /* USE_THEORA */
