#include "client.h"
#include "cl_cin_codec.h"

// ROQ detection
static qboolean DetectROQ(byte *header, int headerSize) {
	if (headerSize < 2) return qfalse;
	unsigned short magic = (unsigned short)(header[0]) + (unsigned short)(header[1])*256;
	return (magic == ROQ_MAGIC_NUMBER);
}

// Theora detection (OggS header)
// Note: This detects Ogg container, but actual Theora streams need to be verified
// by checking for Theora identification header in the Ogg stream
static qboolean DetectTheora(byte *header, int headerSize) {
	if (headerSize < 4) return qfalse;
	// Check for OggS magic number ("OggS")
	// Full Theora detection would require parsing the Ogg stream to find
	// the Theora identification header, but for now we detect by extension
	return (header[0] == 'O' && header[1] == 'g' && header[2] == 'g' && header[3] == 'S');
}

// WebM/VP8/VP9/AV1 detection
static qboolean DetectWebM(byte *header, int headerSize) {
	if (headerSize < 4) return qfalse;
	// Check for EBML header (WebM files start with EBML: 0x1A 0x45 0xDF 0xA3)
	return (header[0] == 0x1A && header[1] == 0x45 && header[2] == 0xDF && header[3] == 0xA3);
}

// AV1 detection (can be in WebM container or raw OBU format)
// For WebM, we rely on container detection; for raw OBU, check for OBU header
static qboolean DetectAV1(byte *header, int headerSize) {
	if (headerSize < 4) return qfalse;
	
	// Check for WebM container (AV1 can be in WebM)
	if (DetectWebM(header, headerSize)) {
		return qtrue; // Will need to check codec ID in container
	}
	
	// Check for raw AV1 OBU (Obsolete Unit) format
	// OBU header: first bit is obu_forbidden_bit, next 4 bits are obu_type
	// Common OBU types: 1=OBU_SEQUENCE_HEADER, 2=OBU_FRAME, etc.
	// For now, we'll rely on extension-based detection for raw AV1
	return qfalse;
}

// Codec information table
static const video_codec_info_t codec_info[] = {
	{
		CODEC_ROQ,
		"ROQ",
		".roq",
		DetectROQ,
		NULL,  // Will be set by ROQ init
		NULL,  // Will be set by ROQ shutdown
		NULL,  // Will be set by ROQ run
		NULL   // Will be set by ROQ reset
	},
#ifdef USE_THEORA
	{
		CODEC_THEORA,
		"Theora",
		".ogv,.ogg",
		DetectTheora,
		NULL,  // Will be implemented in cl_cin_theora.c
		NULL,
		NULL,
		NULL
	},
#endif
#ifdef USE_VPX
	{
		CODEC_VP8,
		"VP8",
		".webm,.vp8",
		DetectWebM,
		NULL,  // Will be implemented in cl_cin_vpx.c
		NULL,
		NULL,
		NULL
	},
	{
		CODEC_VP9,
		"VP9",
		".webm,.vp9",
		DetectWebM,
		NULL,  // Will be implemented in cl_cin_vpx.c
		NULL,
		NULL,
		NULL
	},
#endif
#ifdef USE_DAV1D
	{
		CODEC_AV1,
		"AV1",
		".webm,.av1,.mkv",
		DetectAV1,
		NULL,  // Will be implemented in cl_cin_av1.c
		NULL,
		NULL,
		NULL
	},
#endif
	{ CODEC_NONE, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

/*
==================
CIN_DetectCodec

Detects video codec from file header and/or extension
==================
*/
video_codec_t CIN_DetectCodec(const char *filename, byte *header, int headerSize) {
	int i;
	const char *ext;
	
	// First try magic number detection
	for (i = 0; codec_info[i].codec != CODEC_NONE; i++) {
		if (codec_info[i].detect && codec_info[i].detect(header, headerSize)) {
			return codec_info[i].codec;
		}
	}
	
	// Fallback to extension-based detection
	if (filename) {
		ext = strrchr(filename, '.');
		if (ext) {
			for (i = 0; codec_info[i].codec != CODEC_NONE; i++) {
				if (codec_info[i].extensions && strstr(codec_info[i].extensions, ext)) {
					return codec_info[i].codec;
				}
			}
		}
	}
	
	return CODEC_NONE;
}

/*
==================
CIN_GetCodecInfo

Returns codec information structure
==================
*/
const video_codec_info_t *CIN_GetCodecInfo(video_codec_t codec) {
	int i;
	
	for (i = 0; codec_info[i].codec != CODEC_NONE; i++) {
		if (codec_info[i].codec == codec) {
			return &codec_info[i];
		}
	}
	
	return NULL;
}

