/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Clean-room TIKI (.tik) text parser + little-endian .tan header decode.
FAKK2 / Elite Force II dialect notes only — not Ritual SDK source.
===========================================================================
*/

#ifndef TR_MODEL_TIKI_H
#define TR_MODEL_TIKI_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIKI_MAX_ANIMS       128
#define TIKI_MAX_LODS        8
#define TIKI_MAX_SURFACE     64
#define TIKI_MAX_FRAMECMDS   64
#define TIKI_NAME_SIZE       64
#define TIKI_PATH_SIZE       MAX_QPATH

typedef struct tikiFrameCmd_s {
	int frame;
	char cmd[32];
	char arg[MAX_QPATH];
} tikiFrameCmd_t;

typedef struct tikiAnim_s {
	char name[TIKI_NAME_SIZE];
	char alias[TIKI_NAME_SIZE];
	char path[TIKI_PATH_SIZE];
	int firstFrame;
	int numFrames;
	float frameRate;
	tikiFrameCmd_t cmds[TIKI_MAX_FRAMECMDS];
	int numCmds;
	qboolean used;
} tikiAnim_t;

typedef struct tikiLod_s {
	char path[TIKI_PATH_SIZE];
	float distance;
} tikiLod_t;

typedef struct tikiSurface_s {
	char name[TIKI_NAME_SIZE];
	char shader[MAX_QPATH];
} tikiSurface_t;

typedef struct tikiDef_s {
	char name[MAX_QPATH];
	char skelmodel[TIKI_PATH_SIZE];
	char mesh[TIKI_PATH_SIZE];
	float scale;
	vec3_t origin;
	tikiAnim_t anims[TIKI_MAX_ANIMS];
	int numAnims;
	tikiLod_t lods[TIKI_MAX_LODS];
	int numLods;
	tikiSurface_t surfaces[TIKI_MAX_SURFACE];
	int numSurfaces;
	qboolean used;
} tikiDef_t;

typedef struct tikiTanHeader_s {
	char ident[4]; /* "TAN " */
	int version;
	int numFrames;
	int numBones;
	int frameRate;
	int ofsFrames;
} tikiTanHeader_t;

/* Parse .tik text. Returns qtrue on success. */
qboolean R_Tiki_Parse( const char *buf, int bufLen, tikiDef_t *out, char *err, int errSize );

/* Decode little-endian .tan header; returns qtrue if ident/version plausible. */
qboolean R_Tiki_ParseTanHeader( const byte *data, int size, tikiTanHeader_t *out );

/* Frame-command allowlist (sound, effect, shout, footstep, …). */
qboolean R_Tiki_IsAllowedFrameCmd( const char *cmd );

#ifdef __cplusplus
}
#endif

#endif /* TR_MODEL_TIKI_H */
