/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Modern video codec support for id Tech 3 engine.
Provides FFmpeg, dav1d (AV1), dav2d (AV2), vvdec (VVC),
libvpx (VP8/VP9), and Theora backends
while maintaining full backward compatibility with ROQ.
===========================================================================
*/

#pragma once

#include "../../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	CODEC_NONE = 0,
	CODEC_ROQ,
	CODEC_FFMPEG,
	CODEC_DAV1D,
	CODEC_DAV2D,
	CODEC_VVDEC,
	CODEC_VPX,
	CODEC_THEORA,
	CODEC_COUNT
} cinCodecType_t;

typedef enum {
	CIN_FRAME_NONE = 0,
	CIN_FRAME_RGBA,
	CIN_FRAME_YUV420,
} cinFrameFormat_t;

typedef struct cinFrame_s {
	byte            *data;
	int              width;
	int              height;
	int              stride;
	cinFrameFormat_t format;
	qboolean         valid;
} cinFrame_t;

typedef struct cinAudio_s {
	byte    *samples;
	int      sampleCount;
	int      sampleRate;
	int      channels;
	int      bytesPerSample;
} cinAudio_t;

typedef struct cinModernDecoder_s cinModernDecoder_t;

struct cinModernDecoder_s {
	cinCodecType_t  type;
	void           *context;

	qboolean (*open)(cinModernDecoder_t *dec, const char *filename, fileHandle_t file, int fileSize);
	qboolean (*decodeFrame)(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio);
	void     (*seek)(cinModernDecoder_t *dec, int timeMs);
	void     (*close)(cinModernDecoder_t *dec);
	qboolean (*isEof)(cinModernDecoder_t *dec);

	int     width;
	int     height;
	float   fps;
	int     durationMs;
};

cinCodecType_t  CIN_DetectCodec(const char *filename, fileHandle_t file);
const char     *CIN_CodecName(cinCodecType_t type);

qboolean CIN_Modern_Init(void);
void     CIN_Modern_Shutdown(void);

cinModernDecoder_t *CIN_Modern_Open(const char *filename, cinCodecType_t preferredCodec);
qboolean            CIN_Modern_DecodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio);
void                CIN_Modern_Seek(cinModernDecoder_t *dec, int timeMs);
void                CIN_Modern_Close(cinModernDecoder_t *dec);

#ifdef USE_FFMPEG
qboolean CIN_FFmpeg_CreateDecoder(cinModernDecoder_t *dec);
#endif

#ifdef USE_DAV1D
qboolean CIN_Dav1d_CreateDecoder(cinModernDecoder_t *dec);
#endif

#ifdef USE_DAV2D
qboolean CIN_Dav2d_CreateDecoder(cinModernDecoder_t *dec);
#endif

#ifdef USE_VVDEC
qboolean CIN_Vvdec_CreateDecoder(cinModernDecoder_t *dec);
#endif

#ifdef USE_VPX
qboolean CIN_Vpx_CreateDecoder(cinModernDecoder_t *dec);
#endif

#ifdef USE_THEORA
qboolean CIN_Theora_CreateDecoder(cinModernDecoder_t *dec);
#endif

void CIN_FFmpeg_Cmd(void);

#ifdef __cplusplus
}
#endif
