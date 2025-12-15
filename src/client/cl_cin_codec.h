#ifndef CL_CIN_CODEC_H
#define CL_CIN_CODEC_H

#include "../qcommon/q_shared.h"
#include "../renderers/renderercommon/tr_types.h"  // For MAX_VIDEO_HANDLES

// Video codec types (must be defined before cin_cache_s)
typedef enum {
	CODEC_NONE = 0,
	CODEC_ROQ,      // ROQ format (original Quake 3 codec)
	CODEC_THEORA,   // Ogg Theora (GPL 2 compatible)
	CODEC_VP8,      // VP8/WebM (BSD license, GPL 2 compatible)
	CODEC_VP9,      // VP9/WebM (BSD license, GPL 2 compatible)
	CODEC_AV1       // AV1/WebM (BSD license, GPL 2 compatible)
} video_codec_t;

// Full definition of cin_cache structure (needed for array declarations)
struct cin_cache_s {
	char				fileName[MAX_OSPATH];
	int					CIN_WIDTH, CIN_HEIGHT;
	int					xpos, ypos, width, height;
	qboolean			looping, holdAtEnd, dirty, alterGameState, silent, shader;
	fileHandle_t		iFile;
	e_status			status;
	int					startTime;
	int					lastTime;
	long				tfps;
	long				RoQPlayed;
	long				ROQSize;
	unsigned int		RoQFrameSize;
	long				onQuad;
	long				numQuads;
	long				samplesPerLine;
	unsigned int		roq_id;
	long				screenDelta;

	void ( *VQ0)(byte *status, void *qdata );
	void ( *VQ1)(byte *status, void *qdata );
	void ( *VQNormal)(byte *status, void *qdata );
	void ( *VQBuffer)(byte *status, void *qdata );

	long				samplesPerPixel;				// defaults to 2
	byte*				gray;
	unsigned int		xsize, ysize, maxsize, minsize;

	qboolean			half, smootheddouble;
	long				inMemory;
	long				normalBuffer0;
	long				roq_flags;
	long				roqF0;
	long				roqF1;
	long				t[2];
	long				roqFPS;
	int					playonwalls;
	byte*				buf;
	long				drawX, drawY;
	video_codec_t		codec;			// Video codec type
	void*				codecData;		// Codec-specific data
};

typedef struct cin_cache_s cin_cache;

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

// Codec registration functions
#ifdef USE_THEORA
void Theora_RegisterCodec(void);
#endif
#ifdef USE_VPX
void VPX_RegisterCodec(void);
#endif
#ifdef USE_DAV1D
void AV1_RegisterCodec(void);
#endif

// ROQ magic number
#define ROQ_MAGIC_NUMBER 0x1084

// Theora magic (OggS header)
#define THEORA_MAGIC_OGGS 0x5367674F  // "OggS" in little-endian

// WebM magic numbers
#define WEBM_MAGIC_EBML 0x1A45DFA3    // EBML header
#define WEBM_MAGIC_WEBM 0x4286         // WebM segment

// Default cinematic dimensions (from cl_cin.c)
#define DEFAULT_CIN_WIDTH	512
#define DEFAULT_CIN_HEIGHT	512

// External declarations (defined in cl_cin.c)
// MAX_VIDEO_HANDLES is defined in renderercommon/tr_types.h as 16
// Using incomplete array type for extern declaration (valid C)
extern cin_cache cinTable[];
extern int currentHandle;

#endif // CL_CIN_CODEC_H

