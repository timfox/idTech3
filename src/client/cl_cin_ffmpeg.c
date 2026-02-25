/*
===========================================================================
Copyright (C) 2026 id Tech 3 Contributors

FFmpeg video decoder backend for id Tech 3 engine.
Provides H.264, H.265, VP8, VP9, AV1, Theora, and all FFmpeg-supported
video codecs through libavcodec/libavformat/libswscale.

Compile with USE_FFMPEG=ON and link against libavcodec, libavformat,
libavutil, and libswscale.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "cl_cin_modern.h"

#ifdef USE_FFMPEG

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

typedef struct {
	AVFormatContext    *formatCtx;
	AVCodecContext     *videoCodecCtx;
	AVCodecContext     *audioCodecCtx;
	struct SwsContext  *swsCtx;
	AVFrame            *frame;
	AVFrame            *rgbaFrame;
	AVPacket           *packet;

	int                 videoStreamIdx;
	int                 audioStreamIdx;

	byte               *frameBuffer;
	int                 frameBufferSize;

	byte               *audioBuffer;
	int                 audioBufferSize;

	qboolean            eof;
	double              timeBase;
	int64_t             startPts;
} ffmpegContext_t;

static qboolean ffmpeg_open(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize);
static qboolean ffmpeg_decodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio);
static void     ffmpeg_seek(cinModernDecoder_t *dec, int timeMs);
static void     ffmpeg_close(cinModernDecoder_t *dec);
static qboolean ffmpeg_isEof(cinModernDecoder_t *dec);

/*
===============
CIN_FFmpeg_CreateDecoder
===============
*/
qboolean CIN_FFmpeg_CreateDecoder(cinModernDecoder_t *dec) {
	dec->open = ffmpeg_open;
	dec->decodeFrame = ffmpeg_decodeFrame;
	dec->seek = ffmpeg_seek;
	dec->close = ffmpeg_close;
	dec->isEof = ffmpeg_isEof;
	dec->type = CODEC_FFMPEG;
	return qtrue;
}

/*
===============
ffmpeg_open
===============
*/
static qboolean ffmpeg_open(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize) {
	ffmpegContext_t *ctx;
	const AVCodec *videoCodec = NULL;
	const AVCodec *audioCodec = NULL;
	int ret;

	(void)file;
	(void)fileSize;

	ctx = (ffmpegContext_t *)Z_Malloc(sizeof(ffmpegContext_t));
	Com_Memset(ctx, 0, sizeof(*ctx));
	ctx->videoStreamIdx = -1;
	ctx->audioStreamIdx = -1;

	ret = avformat_open_input(&ctx->formatCtx, filename, NULL, NULL);
	if (ret < 0) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not open %s\n", filename);
		Z_Free(ctx);
		return qfalse;
	}

	ret = avformat_find_stream_info(ctx->formatCtx, NULL);
	if (ret < 0) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not find stream info in %s\n", filename);
		avformat_close_input(&ctx->formatCtx);
		Z_Free(ctx);
		return qfalse;
	}

	ctx->videoStreamIdx = av_find_best_stream(ctx->formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &videoCodec, 0);
	ctx->audioStreamIdx = av_find_best_stream(ctx->formatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &audioCodec, 0);

	if (ctx->videoStreamIdx < 0) {
		Com_Printf(S_COLOR_RED "FFmpeg: No video stream found in %s\n", filename);
		avformat_close_input(&ctx->formatCtx);
		Z_Free(ctx);
		return qfalse;
	}

	ctx->videoCodecCtx = avcodec_alloc_context3(videoCodec);
	avcodec_parameters_to_context(ctx->videoCodecCtx, ctx->formatCtx->streams[ctx->videoStreamIdx]->codecpar);

	ret = avcodec_open2(ctx->videoCodecCtx, videoCodec, NULL);
	if (ret < 0) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not open video codec for %s\n", filename);
		avcodec_free_context(&ctx->videoCodecCtx);
		avformat_close_input(&ctx->formatCtx);
		Z_Free(ctx);
		return qfalse;
	}

	if (ctx->audioStreamIdx >= 0 && audioCodec) {
		ctx->audioCodecCtx = avcodec_alloc_context3(audioCodec);
		avcodec_parameters_to_context(ctx->audioCodecCtx, ctx->formatCtx->streams[ctx->audioStreamIdx]->codecpar);
		if (avcodec_open2(ctx->audioCodecCtx, audioCodec, NULL) < 0) {
			avcodec_free_context(&ctx->audioCodecCtx);
			ctx->audioCodecCtx = NULL;
			ctx->audioStreamIdx = -1;
		}
	}

	dec->width = ctx->videoCodecCtx->width;
	dec->height = ctx->videoCodecCtx->height;

	AVRational tb = ctx->formatCtx->streams[ctx->videoStreamIdx]->time_base;
	ctx->timeBase = av_q2d(tb);

	AVRational fr = ctx->formatCtx->streams[ctx->videoStreamIdx]->avg_frame_rate;
	if (fr.num > 0 && fr.den > 0) {
		dec->fps = (float)av_q2d(fr);
	} else {
		dec->fps = 30.0f;
	}

	if (ctx->formatCtx->duration > 0) {
		dec->durationMs = (int)(ctx->formatCtx->duration / (AV_TIME_BASE / 1000));
	}

	ctx->frame = av_frame_alloc();
	ctx->rgbaFrame = av_frame_alloc();
	ctx->packet = av_packet_alloc();

	ctx->frameBufferSize = dec->width * dec->height * 4;
	ctx->frameBuffer = (byte *)Z_Malloc(ctx->frameBufferSize);

	ctx->rgbaFrame->format = AV_PIX_FMT_RGBA;
	ctx->rgbaFrame->width = dec->width;
	ctx->rgbaFrame->height = dec->height;
	av_image_alloc(ctx->rgbaFrame->data, ctx->rgbaFrame->linesize,
		dec->width, dec->height, AV_PIX_FMT_RGBA, 32);

	ctx->swsCtx = sws_getContext(
		dec->width, dec->height, ctx->videoCodecCtx->pix_fmt,
		dec->width, dec->height, AV_PIX_FMT_RGBA,
		SWS_BILINEAR, NULL, NULL, NULL);

	if (!ctx->swsCtx) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not create scaler for %s\n", filename);
		ffmpeg_close(dec);
		return qfalse;
	}

	dec->context = ctx;
	ctx->eof = qfalse;

	Com_Printf("FFmpeg: Opened %s (%dx%d, %.1f fps, codec: %s)\n",
		filename, dec->width, dec->height, (double)dec->fps,
		videoCodec->name);

	return qtrue;
}

/*
===============
ffmpeg_decodeFrame
===============
*/
static qboolean ffmpeg_decodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio) {
	ffmpegContext_t *ctx;
	int ret;

	if (!dec || !dec->context) {
		return qfalse;
	}

	ctx = (ffmpegContext_t *)dec->context;

	if (ctx->eof) {
		return qfalse;
	}

	while (1) {
		ret = av_read_frame(ctx->formatCtx, ctx->packet);
		if (ret < 0) {
			ctx->eof = qtrue;
			return qfalse;
		}

		if (ctx->packet->stream_index == ctx->videoStreamIdx) {
			ret = avcodec_send_packet(ctx->videoCodecCtx, ctx->packet);
			if (ret < 0) {
				av_packet_unref(ctx->packet);
				continue;
			}

			ret = avcodec_receive_frame(ctx->videoCodecCtx, ctx->frame);
			if (ret == AVERROR(EAGAIN)) {
				av_packet_unref(ctx->packet);
				continue;
			}
			if (ret < 0) {
				av_packet_unref(ctx->packet);
				ctx->eof = qtrue;
				return qfalse;
			}

			sws_scale(ctx->swsCtx,
				(const uint8_t *const *)ctx->frame->data, ctx->frame->linesize,
				0, ctx->videoCodecCtx->height,
				ctx->rgbaFrame->data, ctx->rgbaFrame->linesize);

			if (frame) {
				int copySize = dec->width * dec->height * 4;
				if (copySize > ctx->frameBufferSize) {
					copySize = ctx->frameBufferSize;
				}
				Com_Memcpy(ctx->frameBuffer, ctx->rgbaFrame->data[0], copySize);

				frame->data = ctx->frameBuffer;
				frame->width = dec->width;
				frame->height = dec->height;
				frame->stride = dec->width * 4;
				frame->format = CIN_FRAME_RGBA;
				frame->valid = qtrue;
			}

			av_packet_unref(ctx->packet);
			return qtrue;
		}

		if (ctx->packet->stream_index == ctx->audioStreamIdx && ctx->audioCodecCtx && audio) {
			ret = avcodec_send_packet(ctx->audioCodecCtx, ctx->packet);
			if (ret >= 0) {
				AVFrame *audioFrame = av_frame_alloc();
				ret = avcodec_receive_frame(ctx->audioCodecCtx, audioFrame);
				if (ret >= 0) {
					audio->sampleCount = audioFrame->nb_samples;
					audio->sampleRate = audioFrame->sample_rate;
					audio->channels = audioFrame->ch_layout.nb_channels;
					audio->bytesPerSample = 2;
				}
				av_frame_free(&audioFrame);
			}
		}

		av_packet_unref(ctx->packet);
	}
}

/*
===============
ffmpeg_seek
===============
*/
static void ffmpeg_seek(cinModernDecoder_t *dec, int timeMs) {
	ffmpegContext_t *ctx;
	int64_t timestamp;

	if (!dec || !dec->context) {
		return;
	}

	ctx = (ffmpegContext_t *)dec->context;

	timestamp = (int64_t)timeMs * AV_TIME_BASE / 1000;
	av_seek_frame(ctx->formatCtx, -1, timestamp, AVSEEK_FLAG_BACKWARD);

	if (ctx->videoCodecCtx) {
		avcodec_flush_buffers(ctx->videoCodecCtx);
	}
	if (ctx->audioCodecCtx) {
		avcodec_flush_buffers(ctx->audioCodecCtx);
	}

	ctx->eof = qfalse;
}

/*
===============
ffmpeg_close
===============
*/
static void ffmpeg_close(cinModernDecoder_t *dec) {
	ffmpegContext_t *ctx;

	if (!dec || !dec->context) {
		return;
	}

	ctx = (ffmpegContext_t *)dec->context;

	if (ctx->swsCtx) {
		sws_freeContext(ctx->swsCtx);
	}
	if (ctx->rgbaFrame) {
		if (ctx->rgbaFrame->data[0]) {
			av_freep(&ctx->rgbaFrame->data[0]);
		}
		av_frame_free(&ctx->rgbaFrame);
	}
	if (ctx->frame) {
		av_frame_free(&ctx->frame);
	}
	if (ctx->packet) {
		av_packet_free(&ctx->packet);
	}
	if (ctx->videoCodecCtx) {
		avcodec_free_context(&ctx->videoCodecCtx);
	}
	if (ctx->audioCodecCtx) {
		avcodec_free_context(&ctx->audioCodecCtx);
	}
	if (ctx->formatCtx) {
		avformat_close_input(&ctx->formatCtx);
	}
	if (ctx->frameBuffer) {
		Z_Free(ctx->frameBuffer);
	}
	if (ctx->audioBuffer) {
		Z_Free(ctx->audioBuffer);
	}

	Z_Free(ctx);
	dec->context = NULL;
}

/*
===============
ffmpeg_isEof
===============
*/
static qboolean ffmpeg_isEof(cinModernDecoder_t *dec) {
	ffmpegContext_t *ctx;

	if (!dec || !dec->context) {
		return qtrue;
	}

	ctx = (ffmpegContext_t *)dec->context;
	return ctx->eof;
}

#endif /* USE_FFMPEG */
