/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

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
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>

typedef struct {
	AVFormatContext    *formatCtx;
	AVCodecContext     *videoCodecCtx;
	AVCodecContext     *audioCodecCtx;
	AVIOContext        *avioCtx;
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

	byte               *fileData;
	int                 fileSize;
	int                 fileOffset;

	byte               *avioBuffer;
	int                 avioBufferSize;

	qboolean            eof;
	double              timeBase;
	int64_t             startPts;
} ffmpegContext_t;

static qboolean ffmpeg_open(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize);
static qboolean ffmpeg_decodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio);
static void     ffmpeg_seek(cinModernDecoder_t *dec, int timeMs);
static void     ffmpeg_close(cinModernDecoder_t *dec);
static qboolean ffmpeg_isEof(cinModernDecoder_t *dec);

static int ffmpeg_io_read(void *opaque, uint8_t *buf, int bufSize);
static int64_t ffmpeg_io_seek(void *opaque, int64_t offset, int whence);
static qboolean ffmpeg_decode_audio_frame(ffmpegContext_t *ctx, cinAudio_t *audio);
static int ffmpeg_convert_audio_sample(enum AVSampleFormat format, const uint8_t *sampleData);
static const uint8_t *ffmpeg_audio_sample_ptr(const AVFrame *frame, enum AVSampleFormat format, int channels, int sampleIndex, int channelIndex, int bytesPerSample);

static int ffmpeg_io_read(void *opaque, uint8_t *buf, int bufSize) {
	ffmpegContext_t *ctx = (ffmpegContext_t *)opaque;
	int remaining;
	int toRead;

	if (!ctx || !ctx->fileData || bufSize <= 0) {
		return AVERROR_EOF;
	}

	remaining = ctx->fileSize - ctx->fileOffset;
	if (remaining <= 0) {
		return AVERROR_EOF;
	}

	toRead = bufSize;
	if (toRead > remaining) {
		toRead = remaining;
	}

	Com_Memcpy(buf, ctx->fileData + ctx->fileOffset, toRead);
	ctx->fileOffset += toRead;

	return toRead;
}

static int64_t ffmpeg_io_seek(void *opaque, int64_t offset, int whence) {
	ffmpegContext_t *ctx = (ffmpegContext_t *)opaque;
	int newOffset;

	if (!ctx) {
		return -1;
	}

	if (whence == AVSEEK_SIZE) {
		return ctx->fileSize;
	}

	switch (whence & ~AVSEEK_FORCE) {
		case SEEK_SET:
			newOffset = (int)offset;
			break;
		case SEEK_CUR:
			newOffset = ctx->fileOffset + (int)offset;
			break;
		case SEEK_END:
			newOffset = ctx->fileSize + (int)offset;
			break;
		default:
			return -1;
	}

	if (newOffset < 0) {
		newOffset = 0;
	}
	if (newOffset > ctx->fileSize) {
		newOffset = ctx->fileSize;
	}

	ctx->fileOffset = newOffset;
	return ctx->fileOffset;
}

static int ffmpeg_convert_audio_sample(enum AVSampleFormat format, const uint8_t *sampleData) {
	switch ( format ) {
		case AV_SAMPLE_FMT_U8:
		case AV_SAMPLE_FMT_U8P:
			return ( (int)( *sampleData ) - 128 ) << 8;

		case AV_SAMPLE_FMT_S16:
		case AV_SAMPLE_FMT_S16P:
			return *(const int16_t *)sampleData;

		case AV_SAMPLE_FMT_S32:
		case AV_SAMPLE_FMT_S32P:
			return *(const int32_t *)sampleData >> 16;

		case AV_SAMPLE_FMT_FLT:
		case AV_SAMPLE_FMT_FLTP: {
			float value = *(const float *)sampleData;
			int sample;
			if ( value > 1.0f ) {
				value = 1.0f;
			} else if ( value < -1.0f ) {
				value = -1.0f;
			}
			sample = (int)( value * 32767.0f );
			if ( sample < -32768 ) {
				sample = -32768;
			} else if ( sample > 32767 ) {
				sample = 32767;
			}
			return sample;
		}

		case AV_SAMPLE_FMT_DBL:
		case AV_SAMPLE_FMT_DBLP: {
			double value = *(const double *)sampleData;
			int sample;
			if ( value > 1.0 ) {
				value = 1.0;
			} else if ( value < -1.0 ) {
				value = -1.0;
			}
			sample = (int)( value * 32767.0 );
			if ( sample < -32768 ) {
				sample = -32768;
			} else if ( sample > 32767 ) {
				sample = 32767;
			}
			return sample;
		}

		default:
			return 0;
	}
}

static const uint8_t *ffmpeg_audio_sample_ptr(const AVFrame *frame, enum AVSampleFormat format, int channels, int sampleIndex, int channelIndex, int bytesPerSample) {
	if ( av_sample_fmt_is_planar( format ) ) {
		return frame->extended_data[channelIndex] + ( sampleIndex * bytesPerSample );
	}

	return frame->extended_data[0] + ( ( sampleIndex * channels + channelIndex ) * bytesPerSample );
}

static qboolean ffmpeg_decode_audio_frame(ffmpegContext_t *ctx, cinAudio_t *audio) {
	AVFrame *audioFrame;
	enum AVSampleFormat format;
	int inputChannels;
	int outputChannels;
	int bytesPerSample;
	int outputBytes;
	int sampleIndex;
	int channelIndex;
	int16_t *dst;
	int ret;

	if ( !ctx || !ctx->audioCodecCtx || !audio ) {
		return qfalse;
	}

	audioFrame = av_frame_alloc();
	if ( !audioFrame ) {
		return qfalse;
	}

	ret = avcodec_receive_frame( ctx->audioCodecCtx, audioFrame );
	if ( ret < 0 ) {
		av_frame_free( &audioFrame );
		return qfalse;
	}

	format = (enum AVSampleFormat)audioFrame->format;
	inputChannels = audioFrame->ch_layout.nb_channels;
	if ( inputChannels <= 0 && ctx->audioCodecCtx ) {
		inputChannels = ctx->audioCodecCtx->ch_layout.nb_channels;
	}
	if ( inputChannels <= 0 ) {
		inputChannels = 1;
	}

	bytesPerSample = av_get_bytes_per_sample( format );
	if ( bytesPerSample <= 0 || audioFrame->nb_samples <= 0 ) {
		av_frame_free( &audioFrame );
		return qfalse;
	}

	outputChannels = inputChannels >= 2 ? 2 : 1;
	outputBytes = audioFrame->nb_samples * outputChannels * (int)sizeof( int16_t );
	if ( outputBytes > ctx->audioBufferSize ) {
		if ( ctx->audioBuffer ) {
			Z_Free( ctx->audioBuffer );
		}
		ctx->audioBuffer = (byte *)Z_Malloc( outputBytes );
		if ( !ctx->audioBuffer ) {
			ctx->audioBufferSize = 0;
			av_frame_free( &audioFrame );
			return qfalse;
		}
		ctx->audioBufferSize = outputBytes;
	}

	dst = (int16_t *)ctx->audioBuffer;
	for ( sampleIndex = 0; sampleIndex < audioFrame->nb_samples; sampleIndex++ ) {
		if ( outputChannels == 1 ) {
			const uint8_t *src = ffmpeg_audio_sample_ptr( audioFrame, format, inputChannels, sampleIndex, 0, bytesPerSample );
			dst[sampleIndex] = (int16_t)ffmpeg_convert_audio_sample( format, src );
			continue;
		}

		for ( channelIndex = 0; channelIndex < outputChannels; channelIndex++ ) {
			const uint8_t *src = ffmpeg_audio_sample_ptr( audioFrame, format, inputChannels, sampleIndex, channelIndex, bytesPerSample );
			dst[sampleIndex * outputChannels + channelIndex] = (int16_t)ffmpeg_convert_audio_sample( format, src );
		}
	}

	audio->samples = ctx->audioBuffer;
	audio->sampleCount = audioFrame->nb_samples;
	audio->sampleRate = audioFrame->sample_rate > 0 ? audioFrame->sample_rate : ctx->audioCodecCtx->sample_rate;
	audio->channels = outputChannels;
	audio->bytesPerSample = (int)sizeof( int16_t );

	av_frame_free( &audioFrame );
	return qtrue;
}

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
	int openedInput = 0;
	int ret;

	ctx = (ffmpegContext_t *)Z_Malloc(sizeof(ffmpegContext_t));
	Com_Memset(ctx, 0, sizeof(*ctx));
	ctx->videoStreamIdx = -1;
	ctx->audioStreamIdx = -1;
	ctx->avioBufferSize = 64 * 1024;

	if (file == FS_INVALID_HANDLE || fileSize <= 0) {
		Com_Printf(S_COLOR_RED "FFmpeg: Invalid file handle for %s\n", filename);
		goto fail;
	}

	ctx->fileSize = fileSize;
	ctx->fileData = (byte *)Z_Malloc(ctx->fileSize);
	if (!ctx->fileData) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not allocate %d bytes for %s\n", ctx->fileSize, filename);
		goto fail;
	}

	if (FS_Read(ctx->fileData, ctx->fileSize, file) != ctx->fileSize) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not read %s from virtual filesystem\n", filename);
		goto fail;
	}
	ctx->fileOffset = 0;

	ctx->avioBuffer = (byte *)av_malloc(ctx->avioBufferSize);
	if (!ctx->avioBuffer) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not allocate AVIO buffer for %s\n", filename);
		goto fail;
	}

	ctx->avioCtx = avio_alloc_context(
		ctx->avioBuffer,
		ctx->avioBufferSize,
		0,
		ctx,
		ffmpeg_io_read,
		NULL,
		ffmpeg_io_seek);
	if (!ctx->avioCtx) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not create custom IO for %s\n", filename);
		goto fail;
	}

	ctx->formatCtx = avformat_alloc_context();
	if (!ctx->formatCtx) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not allocate format context for %s\n", filename);
		goto fail;
	}

	ctx->formatCtx->pb = ctx->avioCtx;
	ctx->formatCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

	ret = avformat_open_input(&ctx->formatCtx, NULL, NULL, NULL);
	if (ret < 0) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not open %s\n", filename);
		goto fail;
	}
	openedInput = 1;

	ret = avformat_find_stream_info(ctx->formatCtx, NULL);
	if (ret < 0) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not find stream info in %s\n", filename);
		goto fail;
	}

	ctx->videoStreamIdx = av_find_best_stream(ctx->formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &videoCodec, 0);
	ctx->audioStreamIdx = av_find_best_stream(ctx->formatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, &audioCodec, 0);

	if (ctx->videoStreamIdx < 0) {
		Com_Printf(S_COLOR_RED "FFmpeg: No video stream found in %s\n", filename);
		goto fail;
	}

	ctx->videoCodecCtx = avcodec_alloc_context3(videoCodec);
	if (!ctx->videoCodecCtx) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not allocate video codec context for %s\n", filename);
		goto fail;
	}
	avcodec_parameters_to_context(ctx->videoCodecCtx, ctx->formatCtx->streams[ctx->videoStreamIdx]->codecpar);

	ret = avcodec_open2(ctx->videoCodecCtx, videoCodec, NULL);
	if (ret < 0) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not open video codec for %s\n", filename);
		goto fail;
	}

	if (ctx->audioStreamIdx >= 0 && audioCodec) {
		ctx->audioCodecCtx = avcodec_alloc_context3(audioCodec);
		if (ctx->audioCodecCtx) {
			avcodec_parameters_to_context(ctx->audioCodecCtx, ctx->formatCtx->streams[ctx->audioStreamIdx]->codecpar);
			if (avcodec_open2(ctx->audioCodecCtx, audioCodec, NULL) < 0) {
				avcodec_free_context(&ctx->audioCodecCtx);
				ctx->audioCodecCtx = NULL;
				ctx->audioStreamIdx = -1;
			}
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
	if (!ctx->frame || !ctx->rgbaFrame || !ctx->packet) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not allocate decode buffers for %s\n", filename);
		goto fail;
	}

	ctx->frameBufferSize = dec->width * dec->height * 4;
	ctx->frameBuffer = (byte *)Z_Malloc(ctx->frameBufferSize);
	if (!ctx->frameBuffer) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not allocate frame buffer for %s\n", filename);
		goto fail;
	}

	ctx->rgbaFrame->format = AV_PIX_FMT_RGBA;
	ctx->rgbaFrame->width = dec->width;
	ctx->rgbaFrame->height = dec->height;
	ret = av_image_alloc(ctx->rgbaFrame->data, ctx->rgbaFrame->linesize,
		dec->width, dec->height, AV_PIX_FMT_RGBA, 32);
	if (ret < 0) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not allocate RGBA image for %s\n", filename);
		goto fail;
	}

	ctx->swsCtx = sws_getContext(
		dec->width, dec->height, ctx->videoCodecCtx->pix_fmt,
		dec->width, dec->height, AV_PIX_FMT_RGBA,
		SWS_BILINEAR, NULL, NULL, NULL);

	if (!ctx->swsCtx) {
		Com_Printf(S_COLOR_RED "FFmpeg: Could not create scaler for %s\n", filename);
		goto fail;
	}

	dec->context = ctx;
	ctx->eof = qfalse;

	Com_Printf("FFmpeg: Opened %s (%dx%d, %.1f fps, codec: %s)\n",
		filename, dec->width, dec->height, (double)dec->fps,
		videoCodec->name);

	return qtrue;

fail:
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
	if (openedInput && ctx->formatCtx) {
		avformat_close_input(&ctx->formatCtx);
	} else if (ctx->formatCtx) {
		avformat_free_context(ctx->formatCtx);
		ctx->formatCtx = NULL;
	}
	if (ctx->avioCtx) {
		av_freep(&ctx->avioCtx->buffer);
		avio_context_free(&ctx->avioCtx);
		ctx->avioBuffer = NULL;
	}
	if (ctx->fileData) {
		Z_Free(ctx->fileData);
	}
	if (ctx->frameBuffer) {
		Z_Free(ctx->frameBuffer);
	}
	Z_Free(ctx);
	return qfalse;
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
	if ( frame ) {
		Com_Memset( frame, 0, sizeof( *frame ) );
	}
	if ( audio ) {
		Com_Memset( audio, 0, sizeof( *audio ) );
	}

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
				if (ret >= 0 && ffmpeg_decode_audio_frame(ctx, audio)) {
					av_packet_unref(ctx->packet);
					return qtrue;
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
	if (ctx->avioCtx) {
		av_freep(&ctx->avioCtx->buffer);
		avio_context_free(&ctx->avioCtx);
		ctx->avioBuffer = NULL;
	}
	if (ctx->frameBuffer) {
		Z_Free(ctx->frameBuffer);
	}
	if (ctx->audioBuffer) {
		Z_Free(ctx->audioBuffer);
	}
	if (ctx->fileData) {
		Z_Free(ctx->fileData);
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
