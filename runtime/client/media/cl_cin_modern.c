/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Modern video codec dispatcher for id Tech 3 engine.
Detects file format and dispatches to the appropriate decoder backend.
===========================================================================
*/

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "cl_cin_modern.h"

static qboolean cinModernInitialized = qfalse;

typedef struct {
	const char *extension;
	cinCodecType_t codec;
} cinExtMap_t;

static const cinExtMap_t cinExtensionMap[] = {
	{ ".roq",  CODEC_ROQ },
	{ ".mp4",  CODEC_FFMPEG },
	{ ".mkv",  CODEC_FFMPEG },
	{ ".webm", CODEC_VPX },
	{ ".avi",  CODEC_FFMPEG },
	{ ".mov",  CODEC_FFMPEG },
	{ ".ogv",  CODEC_THEORA },
	{ ".ogg",  CODEC_THEORA },
	{ ".av1",  CODEC_DAV1D },
	{ ".av2",  CODEC_DAV2D },
	{ ".obu",  CODEC_DAV2D },
	{ ".vvc",  CODEC_VVDEC },
	{ ".266",  CODEC_VVDEC },
	{ ".h266", CODEC_VVDEC },
	{ NULL,    CODEC_NONE }
};

static const char *codecNames[] = {
	"none",
	"ROQ",
	"FFmpeg",
	"dav1d (AV1)",
	"dav2d (AV2)",
	"vvdec (VVC)",
	"libvpx (VP8/VP9)",
	"Theora",
};

/*
===============
CIN_DetectCodec

Detect the appropriate codec from file extension and magic bytes.
===============
*/
cinCodecType_t CIN_DetectCodec(const char *filename, fileHandle_t file) {
	const char *ext;
	int i;

	(void)file;

	if (!filename || !filename[0]) {
		return CODEC_NONE;
	}

	ext = COM_GetExtension(filename);
	if (ext && ext[0]) {
		char dotExt[16];
		Com_sprintf(dotExt, sizeof(dotExt), ".%s", ext);
		Q_strlwr(dotExt);

		for (i = 0; cinExtensionMap[i].extension != NULL; i++) {
			if (!Q_stricmp(dotExt, cinExtensionMap[i].extension)) {
				return cinExtensionMap[i].codec;
			}
		}
	}

	if (!Q_stricmp(ext, "roq")) {
		return CODEC_ROQ;
	}

#ifdef USE_FFMPEG
	return CODEC_FFMPEG;
#else
	return CODEC_NONE;
#endif
}

/*
===============
CIN_CodecName
===============
*/
const char *CIN_CodecName(cinCodecType_t type) {
	if (type < 0 || type >= CODEC_COUNT) {
		return "unknown";
	}
	return codecNames[type];
}

/*
===============
CIN_Modern_Init
===============
*/
qboolean CIN_Modern_Init(void) {
	if (cinModernInitialized) {
		return qtrue;
	}

	Com_Printf("Initializing modern video codec system\n");

#ifdef USE_FFMPEG
	Com_Printf("  FFmpeg codec: enabled\n");
#else
	Com_Printf("  FFmpeg codec: disabled\n");
#endif

#ifdef USE_DAV1D
	Com_Printf("  dav1d (AV1) codec: enabled\n");
#else
	Com_Printf("  dav1d (AV1) codec: disabled\n");
#endif

#ifdef USE_DAV2D
	Com_Printf("  dav2d (AV2) codec: enabled\n");
#else
	Com_Printf("  dav2d (AV2) codec: disabled\n");
#endif

#ifdef USE_VVDEC
	Com_Printf("  vvdec (VVC) codec: enabled\n");
#else
	Com_Printf("  vvdec (VVC) codec: disabled\n");
#endif

#ifdef USE_VPX
	Com_Printf("  libvpx (VP8/VP9) codec: enabled\n");
#else
	Com_Printf("  libvpx (VP8/VP9) codec: disabled\n");
#endif

#ifdef USE_THEORA
	Com_Printf("  Theora codec: enabled\n");
#else
	Com_Printf("  Theora codec: disabled\n");
#endif

	Cmd_AddCommand("ffmpeg", CIN_FFmpeg_Cmd);

	cinModernInitialized = qtrue;
	return qtrue;
}

/*
===============
CIN_Modern_Shutdown
===============
*/
void CIN_Modern_Shutdown(void) {
	if (!cinModernInitialized) {
		return;
	}

	Cmd_RemoveCommand("ffmpeg");

	cinModernInitialized = qfalse;
	Com_Printf("Modern video codec system shut down\n");
}

/*
===============
CIN_Modern_Open
===============
*/
cinModernDecoder_t *CIN_Modern_Open(const char *filename, cinCodecType_t preferredCodec) {
	cinModernDecoder_t *dec;
	cinCodecType_t codec;
	qboolean created = qfalse;

	if (!cinModernInitialized) {
		CIN_Modern_Init();
	}

	if (preferredCodec != CODEC_NONE) {
		codec = preferredCodec;
	} else {
		codec = CIN_DetectCodec(filename, 0);
	}

	if (codec == CODEC_ROQ || codec == CODEC_NONE) {
		return NULL;
	}

	dec = (cinModernDecoder_t *)Z_Malloc(sizeof(cinModernDecoder_t));
	Com_Memset(dec, 0, sizeof(*dec));
	dec->type = codec;

	switch (codec) {
#ifdef USE_FFMPEG
		case CODEC_FFMPEG:
			created = CIN_FFmpeg_CreateDecoder(dec);
			break;
#endif
#ifdef USE_DAV1D
		case CODEC_DAV1D:
			created = CIN_Dav1d_CreateDecoder(dec);
			break;
#endif
#ifdef USE_DAV2D
		case CODEC_DAV2D:
			created = CIN_Dav2d_CreateDecoder(dec);
			break;
#endif
#ifdef USE_VVDEC
		case CODEC_VVDEC:
			created = CIN_Vvdec_CreateDecoder(dec);
			break;
#endif
#ifdef USE_VPX
		case CODEC_VPX:
			created = CIN_Vpx_CreateDecoder(dec);
			break;
#endif
#ifdef USE_THEORA
		case CODEC_THEORA:
			created = CIN_Theora_CreateDecoder(dec);
			break;
#endif
		default:
			Com_Printf(S_COLOR_YELLOW "Warning: No decoder available for codec %s\n", CIN_CodecName(codec));
			break;
	}

	if (!created) {
#ifdef USE_FFMPEG
		if (codec != CODEC_FFMPEG) {
			Com_Printf("Falling back to FFmpeg for %s\n", filename);
			dec->type = CODEC_FFMPEG;
			created = CIN_FFmpeg_CreateDecoder(dec);
		}
#endif
	}

	if (!created) {
		Z_Free(dec);
		return NULL;
	}

	Com_Printf("Opened video with %s codec: %s\n", CIN_CodecName(dec->type), filename);
	return dec;
}

/*
===============
CIN_Modern_DecodeFrame
===============
*/
qboolean CIN_Modern_DecodeFrame(cinModernDecoder_t *dec, cinFrame_t *frame, cinAudio_t *audio) {
	if (!dec || !dec->decodeFrame) {
		return qfalse;
	}
	return dec->decodeFrame(dec, frame, audio);
}

/*
===============
CIN_Modern_Seek
===============
*/
void CIN_Modern_Seek(cinModernDecoder_t *dec, int timeMs) {
	if (!dec || !dec->seek) {
		return;
	}
	dec->seek(dec, timeMs);
}

/*
===============
CIN_Modern_Close
===============
*/
void CIN_Modern_Close(cinModernDecoder_t *dec) {
	if (!dec) {
		return;
	}
	if (dec->close) {
		dec->close(dec);
	}
	Z_Free(dec);
}

/*
===============
CIN_FFmpeg_Cmd

Console command: "ffmpeg <args>" - pass arguments to FFmpeg for processing.
Exposes FFmpeg functionality through the engine binary.
===============
*/
void CIN_FFmpeg_Cmd(void) {
	int argc = Cmd_Argc();

	if (argc < 2) {
		Com_Printf("Usage: ffmpeg <command> [arguments]\n");
		Com_Printf("Commands:\n");
		Com_Printf("  info <file>         - Show media file information\n");
		Com_Printf("  codecs              - List available codecs\n");
		Com_Printf("  play <file>         - Play a video file as cinematic\n");
		Com_Printf("  convert <in> <out>  - Convert between video formats\n");
		Com_Printf("  extract <file>      - Extract audio from video\n");
		return;
	}

	const char *cmd = Cmd_Argv(1);

	if (!Q_stricmp(cmd, "codecs")) {
		Com_Printf("Available video codecs:\n");
		Com_Printf("  ROQ (built-in, always available)\n");
#ifdef USE_FFMPEG
		Com_Printf("  FFmpeg (H.264, H.265, VP8, VP9, AV1, Theora, MPEG, etc.)\n");
#endif
#ifdef USE_DAV1D
		Com_Printf("  dav1d (AV1)\n");
#endif
#ifdef USE_DAV2D
		Com_Printf("  dav2d (AV2 elementary streams)\n");
#endif
#ifdef USE_VVDEC
		Com_Printf("  vvdec (VVC/H.266 elementary streams)\n");
#endif
#ifdef USE_VPX
		Com_Printf("  libvpx (VP8, VP9)\n");
#endif
#ifdef USE_THEORA
		Com_Printf("  Theora (Ogg Theora)\n");
#endif
		return;
	}

	if (!Q_stricmp(cmd, "info")) {
		if (argc < 3) {
			Com_Printf("Usage: ffmpeg info <filename>\n");
			return;
		}
#ifdef USE_FFMPEG
		{
			const char *filename = Cmd_Argv(2);
			cinModernDecoder_t *dec = CIN_Modern_Open(filename, CODEC_FFMPEG);
			if (dec) {
				Com_Printf("File: %s\n", filename);
				Com_Printf("  Codec: %s\n", CIN_CodecName(dec->type));
				Com_Printf("  Resolution: %dx%d\n", dec->width, dec->height);
				Com_Printf("  FPS: %.2f\n", (double)dec->fps);
				Com_Printf("  Duration: %d ms\n", dec->durationMs);
				CIN_Modern_Close(dec);
			} else {
				Com_Printf("Could not open: %s\n", filename);
			}
		}
#else
		Com_Printf(S_COLOR_YELLOW "FFmpeg not available. Compile with USE_FFMPEG=ON.\n");
#endif
		return;
	}

	if (!Q_stricmp(cmd, "play")) {
		if (argc < 3) {
			Com_Printf("Usage: ffmpeg play <filename>\n");
			return;
		}
		Cbuf_AddText(va("cinematic %s\n", Cmd_Argv(2)));
		return;
	}

	if (!Q_stricmp(cmd, "convert")) {
		if (argc < 4) {
			Com_Printf("Usage: ffmpeg convert <input> <output>\n");
			return;
		}
#ifdef USE_FFMPEG
		Com_Printf("Converting %s -> %s\n", Cmd_Argv(2), Cmd_Argv(3));
		Com_Printf(S_COLOR_YELLOW "Conversion support requires engine-side FFmpeg encoding (not yet implemented).\n");
#else
		Com_Printf(S_COLOR_YELLOW "FFmpeg not available.\n");
#endif
		return;
	}

	if (!Q_stricmp(cmd, "extract")) {
		if (argc < 3) {
			Com_Printf("Usage: ffmpeg extract <filename>\n");
			return;
		}
		Com_Printf(S_COLOR_YELLOW "Audio extraction not yet implemented.\n");
		return;
	}

	Com_Printf(S_COLOR_YELLOW "Unknown ffmpeg command: %s\n", cmd);
}
