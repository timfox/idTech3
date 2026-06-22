/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

KTX2 (Khronos Texture 2.0) loader for BC7 compressed textures.
Supports non-supercompressed BC7 only. No external dependencies.
===========================================================================
*/

#include "tr_image_loaders.h"
#include <string.h>
#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"

/* KTX2 identifier */
static const byte KTX2_ID[12] = {
	0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

#define KTX_SS_NONE 0
#define VK_FORMAT_BC7_UNORM_BLOCK 145
#define VK_FORMAT_BC7_SRGB_BLOCK  146

qboolean R_LoadKTX2(const char *filename, byte **data, int *width, int *height, int *format, int *size) {
	void *fileData;
	int fileSize;
	const byte *p;
	uint32_t vkFormat, levelCount, pixelWidth, pixelHeight;
	uint32_t supercompressionScheme, dfdByteLength, kvdByteLength;
	uint64_t levelIndexOffset;
	uint64_t byteOffset, byteLength;
	int totalSize;
	byte *out;
	int i;

	*data = NULL; *width = 0; *height = 0; *format = 0; *size = 0;

	fileSize = FS_ReadFile(filename, &fileData);
	if (fileSize <= 0 || !fileData) return qfalse;

	if (fileSize < 80) {
		FS_FreeFile(fileData);
		return qfalse;
	}

	p = (const byte *)fileData;
	if ( memcmp( p, KTX2_ID, 12 ) != 0 ) {
		FS_FreeFile(fileData);
		return qfalse;
	}

	vkFormat = p[12] | (p[13]<<8) | (p[14]<<16) | (p[15]<<24);
	pixelWidth  = p[20] | (p[21]<<8) | (p[22]<<16) | (p[23]<<24);
	pixelHeight = p[24] | (p[25]<<8) | (p[26]<<16) | (p[27]<<24);
	levelCount = p[36] | (p[37]<<8) | (p[38]<<16) | (p[39]<<24);
	supercompressionScheme = p[40] | (p[41]<<8) | (p[42]<<16) | (p[43]<<24);
	dfdByteLength = p[44] | (p[45]<<8) | (p[46]<<16) | (p[47]<<24);
	kvdByteLength = p[48] | (p[49]<<8) | (p[50]<<16) | (p[51]<<24);

	if (supercompressionScheme != KTX_SS_NONE) {
		Com_Printf(S_COLOR_YELLOW "KTX2: supercompressed %s not supported\n", filename);
		FS_FreeFile(fileData);
		return qfalse;
	}

	if (vkFormat != VK_FORMAT_BC7_UNORM_BLOCK && vkFormat != VK_FORMAT_BC7_SRGB_BLOCK) {
		FS_FreeFile(fileData);
		return qfalse;
	}

	if (pixelWidth <= 0 || pixelHeight <= 0 || pixelWidth > 16384 || pixelHeight > 16384) {
		FS_FreeFile(fileData);
		return qfalse;
	}

	if (levelCount < 1) levelCount = 1;

	levelIndexOffset = 80 + (uint64_t)dfdByteLength + (uint64_t)kvdByteLength;

	totalSize = 0;
	for (i = 0; i < (int)levelCount; i++) {
		uint64_t off = levelIndexOffset + (uint64_t)i * 16;
		if (off + 16 > (uint64_t)fileSize) break;
		byteOffset = (uint64_t)p[off] | ((uint64_t)p[off+1]<<8) | ((uint64_t)p[off+2]<<16) | ((uint64_t)p[off+3]<<24)
			| ((uint64_t)p[off+4]<<32) | ((uint64_t)p[off+5]<<40) | ((uint64_t)p[off+6]<<48) | ((uint64_t)p[off+7]<<56);
		byteLength = (uint64_t)p[off+8] | ((uint64_t)p[off+9]<<8) | ((uint64_t)p[off+10]<<16) | ((uint64_t)p[off+11]<<24)
			| ((uint64_t)p[off+12]<<32) | ((uint64_t)p[off+13]<<40) | ((uint64_t)p[off+14]<<48) | ((uint64_t)p[off+15]<<56);
		if (byteOffset + byteLength > (uint64_t)fileSize) break;
		totalSize += (int)byteLength;
	}

	if (totalSize <= 0) {
		FS_FreeFile(fileData);
		return qfalse;
	}

	out = (byte *)Z_Malloc(totalSize);
	p = (const byte *)fileData;
	{
		int offset = 0;
		for (i = 0; i < (int)levelCount; i++) {
			uint64_t off = levelIndexOffset + (uint64_t)i * 16;
			byteOffset = (uint64_t)p[off] | ((uint64_t)p[off+1]<<8) | ((uint64_t)p[off+2]<<16) | ((uint64_t)p[off+3]<<24)
				| ((uint64_t)p[off+4]<<32) | ((uint64_t)p[off+5]<<40) | ((uint64_t)p[off+6]<<48) | ((uint64_t)p[off+7]<<56);
			byteLength = (uint64_t)p[off+8] | ((uint64_t)p[off+9]<<8) | ((uint64_t)p[off+10]<<16) | ((uint64_t)p[off+11]<<24)
				| ((uint64_t)p[off+12]<<32) | ((uint64_t)p[off+13]<<40) | ((uint64_t)p[off+14]<<48) | ((uint64_t)p[off+15]<<56);
			Com_Memcpy(out + offset, p + byteOffset, (int)byteLength);
			offset += (int)byteLength;
		}
	}
	FS_FreeFile(fileData);

	*data = out;
	*width = (int)pixelWidth;
	*height = (int)pixelHeight;
	*format = (int)vkFormat;
	*size = totalSize;
	return qtrue;
}
