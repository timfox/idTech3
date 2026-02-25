/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

libvpx VP8/VP9 decoder backend for id Tech 3 engine.
Provides VP8 and VP9 video decoding via libvpx, typically used
with WebM containers (already supported via Nestegg).
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "cl_cin_modern.h"

#ifdef USE_VPX

#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>
#include <nestegg/nestegg.h>

typedef struct {
	vpx_codec_ctx_t     codec;
	nestegg            *nesteggCtx;
	nestegg_io          nesteggIo;

	byte               *fileData;
	int                 fileSize;
	int                 fileOffset;

	int                 videoTrack;
	unsigned int        trackCount;

	byte               *frameBuffer;
	int                 frameBufferSize;

	qboolean            codecInitialized;
	qboolean            eof;
	int                 width;
	int                 height;
} vpxContext_t;

static qboolean vpx_open(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize);
static qboolean vpx_decodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio);
static void     vpx_seek(cinModernDecoder_t *dec, int timeMs);
static void     vpx_close(cinModernDecoder_t *dec);
static qboolean vpx_isEof(cinModernDecoder_t *dec);

static void vpx_yuv_to_rgba(vpx_image_t *img, byte *rgba, int width, int height);

static int nestegg_io_read(void *buffer, size_t length, void *userdata);
static int nestegg_io_seek(int64_t offset, int whence, void *userdata);
static int64_t nestegg_io_tell(void *userdata);

/*
===============
CIN_Vpx_CreateDecoder
===============
*/
qboolean CIN_Vpx_CreateDecoder(cinModernDecoder_t *dec) {
	dec->open = vpx_open;
	dec->decodeFrame = vpx_decodeFrame;
	dec->seek = vpx_seek;
	dec->close = vpx_close;
	dec->isEof = vpx_isEof;
	dec->type = CODEC_VPX;
	return qtrue;
}

static int nestegg_io_read(void *buffer, size_t length, void *userdata) {
	vpxContext_t *ctx = (vpxContext_t *)userdata;
	int remaining = ctx->fileSize - ctx->fileOffset;
	int toRead = (int)length;

	if (toRead > remaining) {
		toRead = remaining;
	}
	if (toRead <= 0) {
		return 0;
	}

	Com_Memcpy(buffer, ctx->fileData + ctx->fileOffset, toRead);
	ctx->fileOffset += toRead;

	return (toRead == (int)length) ? 1 : 0;
}

static int nestegg_io_seek(int64_t offset, int whence, void *userdata) {
	vpxContext_t *ctx = (vpxContext_t *)userdata;

	switch (whence) {
		case NESTEGG_SEEK_SET:
			ctx->fileOffset = (int)offset;
			break;
		case NESTEGG_SEEK_CUR:
			ctx->fileOffset += (int)offset;
			break;
		case NESTEGG_SEEK_END:
			ctx->fileOffset = ctx->fileSize + (int)offset;
			break;
	}

	if (ctx->fileOffset < 0) ctx->fileOffset = 0;
	if (ctx->fileOffset > ctx->fileSize) ctx->fileOffset = ctx->fileSize;

	return 0;
}

static int64_t nestegg_io_tell(void *userdata) {
	vpxContext_t *ctx = (vpxContext_t *)userdata;
	return (int64_t)ctx->fileOffset;
}

/*
===============
vpx_open
===============
*/
static qboolean vpx_open(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize) {
	vpxContext_t *ctx;
	vpx_codec_dec_cfg_t cfg;
	const vpx_codec_iface_t *iface = NULL;
	int ret;
	unsigned int i;
	int codecId;

	(void)file;
	(void)fileSize;

	ctx = (vpxContext_t *)Z_Malloc(sizeof(vpxContext_t));
	Com_Memset(ctx, 0, sizeof(*ctx));

	ctx->fileSize = FS_ReadFile(filename, (void **)&ctx->fileData);
	if (ctx->fileSize <= 0 || !ctx->fileData) {
		Com_Printf(S_COLOR_RED "VPX: Could not read %s\n", filename);
		Z_Free(ctx);
		return qfalse;
	}
	ctx->fileOffset = 0;

	ctx->nesteggIo.read = nestegg_io_read;
	ctx->nesteggIo.seek = nestegg_io_seek;
	ctx->nesteggIo.tell = nestegg_io_tell;
	ctx->nesteggIo.userdata = ctx;

	ret = nestegg_init(&ctx->nesteggCtx, ctx->nesteggIo, NULL, -1);
	if (ret != 0) {
		Com_Printf(S_COLOR_RED "VPX: Could not parse WebM container %s\n", filename);
		FS_FreeFile(ctx->fileData);
		Z_Free(ctx);
		return qfalse;
	}

	nestegg_track_count(ctx->nesteggCtx, &ctx->trackCount);
	ctx->videoTrack = -1;

	for (i = 0; i < ctx->trackCount; i++) {
		int type = nestegg_track_type(ctx->nesteggCtx, i);
		if (type == NESTEGG_TRACK_VIDEO) {
			ctx->videoTrack = (int)i;
			break;
		}
	}

	if (ctx->videoTrack < 0) {
		Com_Printf(S_COLOR_RED "VPX: No video track found in %s\n", filename);
		nestegg_destroy(ctx->nesteggCtx);
		FS_FreeFile(ctx->fileData);
		Z_Free(ctx);
		return qfalse;
	}

	codecId = nestegg_track_codec_id(ctx->nesteggCtx, ctx->videoTrack);
	if (codecId == NESTEGG_CODEC_VP9) {
		iface = vpx_codec_vp9_dx();
		Com_Printf("VPX: Using VP9 decoder\n");
	} else {
		iface = vpx_codec_vp8_dx();
		Com_Printf("VPX: Using VP8 decoder\n");
	}

	Com_Memset(&cfg, 0, sizeof(cfg));
	cfg.threads = 4;

	if (vpx_codec_dec_init(&ctx->codec, iface, &cfg, 0) != VPX_CODEC_OK) {
		Com_Printf(S_COLOR_RED "VPX: Could not initialize codec\n");
		nestegg_destroy(ctx->nesteggCtx);
		FS_FreeFile(ctx->fileData);
		Z_Free(ctx);
		return qfalse;
	}
	ctx->codecInitialized = qtrue;

	{
		nestegg_video_params params;
		nestegg_track_video_params(ctx->nesteggCtx, ctx->videoTrack, &params);
		dec->width = (int)params.width;
		dec->height = (int)params.height;
		ctx->width = dec->width;
		ctx->height = dec->height;
	}

	dec->fps = 30.0f;
	ctx->eof = qfalse;
	dec->context = ctx;

	Com_Printf("VPX: Opened %s (%dx%d)\n", filename, dec->width, dec->height);
	return qtrue;
}

/*
===============
vpx_yuv_to_rgba
===============
*/
static void vpx_yuv_to_rgba(vpx_image_t *img, byte *rgba, int width, int height) {
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int yVal = img->planes[VPX_PLANE_Y][y * img->stride[VPX_PLANE_Y] + x];
			int uVal = img->planes[VPX_PLANE_U][(y >> 1) * img->stride[VPX_PLANE_U] + (x >> 1)];
			int vVal = img->planes[VPX_PLANE_V][(y >> 1) * img->stride[VPX_PLANE_V] + (x >> 1)];

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
vpx_decodeFrame
===============
*/
static qboolean vpx_decodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio) {
	vpxContext_t *ctx;
	nestegg_packet *pkt = NULL;
	int ret;

	(void)audio;

	if (!dec || !dec->context) {
		return qfalse;
	}

	ctx = (vpxContext_t *)dec->context;

	if (ctx->eof) {
		return qfalse;
	}

	while (1) {
		ret = nestegg_read_packet(ctx->nesteggCtx, &pkt);
		if (ret <= 0 || !pkt) {
			ctx->eof = qtrue;
			return qfalse;
		}

		unsigned int track;
		nestegg_packet_track(pkt, &track);

		if ((int)track == ctx->videoTrack) {
			unsigned int chunks;
			nestegg_packet_count(pkt, &chunks);

			unsigned int ci;
			for (ci = 0; ci < chunks; ci++) {
				unsigned char *data;
				size_t dataLen;
				nestegg_packet_data(pkt, ci, &data, &dataLen);

				if (vpx_codec_decode(&ctx->codec, data, (unsigned int)dataLen, NULL, 0) != VPX_CODEC_OK) {
					continue;
				}

				vpx_codec_iter_t iter = NULL;
				vpx_image_t *img = vpx_codec_get_frame(&ctx->codec, &iter);

				if (img) {
					int w = (int)img->d_w;
					int h = (int)img->d_h;

					if (!ctx->frameBuffer || ctx->frameBufferSize < w * h * 4) {
						if (ctx->frameBuffer) {
							Z_Free(ctx->frameBuffer);
						}
						ctx->frameBufferSize = w * h * 4;
						ctx->frameBuffer = (byte *)Z_Malloc(ctx->frameBufferSize);
					}

					vpx_yuv_to_rgba(img, ctx->frameBuffer, w, h);

					if (frame) {
						frame->data = ctx->frameBuffer;
						frame->width = w;
						frame->height = h;
						frame->stride = w * 4;
						frame->format = CIN_FRAME_RGBA;
						frame->valid = qtrue;
					}

					nestegg_free_packet(pkt);
					return qtrue;
				}
			}
		}

		nestegg_free_packet(pkt);
	}
}

/*
===============
vpx_seek
===============
*/
static void vpx_seek(cinModernDecoder_t *dec, int timeMs) {
	vpxContext_t *ctx;

	if (!dec || !dec->context) {
		return;
	}

	ctx = (vpxContext_t *)dec->context;

	if (ctx->nesteggCtx && ctx->videoTrack >= 0) {
		uint64_t timestampNs = (uint64_t)timeMs * 1000000ULL;
		nestegg_track_seek(ctx->nesteggCtx, ctx->videoTrack, timestampNs);
	}

	ctx->eof = qfalse;
}

/*
===============
vpx_close
===============
*/
static void vpx_close(cinModernDecoder_t *dec) {
	vpxContext_t *ctx;

	if (!dec || !dec->context) {
		return;
	}

	ctx = (vpxContext_t *)dec->context;

	if (ctx->codecInitialized) {
		vpx_codec_destroy(&ctx->codec);
	}
	if (ctx->nesteggCtx) {
		nestegg_destroy(ctx->nesteggCtx);
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
vpx_isEof
===============
*/
static qboolean vpx_isEof(cinModernDecoder_t *dec) {
	vpxContext_t *ctx;

	if (!dec || !dec->context) {
		return qtrue;
	}

	ctx = (vpxContext_t *)dec->context;
	return ctx->eof;
}

#endif /* USE_VPX */
