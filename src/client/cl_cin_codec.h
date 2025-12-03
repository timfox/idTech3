#ifndef CL_CIN_CODEC_H
#define CL_CIN_CODEC_H

#include "../qcommon/q_shared.h"

// Video codec types
typedef enum {
	CODEC_NONE = 0,
	CODEC_ROQ,      // ROQ format (original Quake 3 codec)
	CODEC_THEORA,   // Ogg Theora (GPL 2 compatible)
	CODEC_VP8,      // VP8/WebM (BSD license, GPL 2 compatible)
	CODEC_VP9       // VP9/WebM (BSD license, GPL 2 compatible)
} video_codec_t;

// Codec detection structure
typedef struct {
	video_codec_t codec;
	const char *name;
	const char *extensions;  // Comma-separated list of extensions
	qboolean (*detect)(byte *header, int headerSize);  // Magic number detection
	qboolean (*init)(int handle);
	void (*shutdown)(int handle);
	e_status (*run)(int handle);
	void (*reset)(int handle);
} video_codec_info_t;

// Codec detection functions
video_codec_t CIN_DetectCodec(const char *filename, byte *header, int headerSize);
const video_codec_info_t *CIN_GetCodecInfo(video_codec_t codec);

// ROQ magic number
#define ROQ_MAGIC_NUMBER 0x1084

// Theora magic (OggS header)
#define THEORA_MAGIC_OGGS 0x5367674F  // "OggS" in little-endian

// WebM magic numbers
#define WEBM_MAGIC_EBML 0x1A45DFA3    // EBML header
#define WEBM_MAGIC_WEBM 0x4286         // WebM segment

#endif // CL_CIN_CODEC_H

