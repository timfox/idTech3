#include "tr_image_loaders.h"
/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

DDS (DirectDraw Surface) image loader.
Reads uncompressed RGBA and BC1/DXT1 compressed DDS textures.
Pure C implementation, no external dependencies.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#define DDS_MAGIC        0x20534444  /* "DDS " */
#define DDPF_FOURCC      0x4
#define DDPF_RGB         0x40
#define DDPF_ALPHAPIXELS 0x1

#define FOURCC_DXT1 0x31545844  /* "DXT1" */
#define FOURCC_DXT3 0x33545844  /* "DXT3" */
#define FOURCC_DXT5 0x35545844  /* "DXT5" */
#define FOURCC_DX10 0x30315844  /* "DX10" */

#define DXGI_FORMAT_BC7_UNORM    98
#define DXGI_FORMAT_BC7_UNORM_SRGB 99

typedef struct {
	uint32_t dxgiFormat;
	uint32_t resourceDimension;
	uint32_t miscFlag;
	uint32_t arraySize;
	uint32_t miscFlags2;
} ddsHeaderDxt10_t;

typedef struct {
	uint32_t size;
	uint32_t flags;
	uint32_t fourCC;
	uint32_t rgbBitCount;
	uint32_t rBitMask, gBitMask, bBitMask, aBitMask;
} ddsPixelFormat_t;

typedef struct {
	uint32_t magic;
	uint32_t size;
	uint32_t flags;
	uint32_t height, width;
	uint32_t pitchOrLinearSize;
	uint32_t depth;
	uint32_t mipMapCount;
	uint32_t reserved1[11];
	ddsPixelFormat_t pixelFormat;
	uint32_t caps, caps2, caps3, caps4;
	uint32_t reserved2;
} ddsHeader_t;

static void DDS_DecompressDXT1Block(const byte *block, byte *out, int stride) {
	uint16_t c0 = block[0] | (block[1] << 8);
	uint16_t c1 = block[2] | (block[3] << 8);
	uint32_t bits = block[4] | (block[5]<<8) | (block[6]<<16) | (block[7]<<24);

	byte colors[4][4];
	colors[0][0] = ((c0>>11)&0x1f)*255/31; colors[0][1] = ((c0>>5)&0x3f)*255/63; colors[0][2] = (c0&0x1f)*255/31; colors[0][3] = 255;
	colors[1][0] = ((c1>>11)&0x1f)*255/31; colors[1][1] = ((c1>>5)&0x3f)*255/63; colors[1][2] = (c1&0x1f)*255/31; colors[1][3] = 255;

	if (c0 > c1) {
		colors[2][0] = (2*colors[0][0]+colors[1][0]+1)/3; colors[2][1] = (2*colors[0][1]+colors[1][1]+1)/3;
		colors[2][2] = (2*colors[0][2]+colors[1][2]+1)/3; colors[2][3] = 255;
		colors[3][0] = (colors[0][0]+2*colors[1][0]+1)/3; colors[3][1] = (colors[0][1]+2*colors[1][1]+1)/3;
		colors[3][2] = (colors[0][2]+2*colors[1][2]+1)/3; colors[3][3] = 255;
	} else {
		colors[2][0] = (colors[0][0]+colors[1][0])/2; colors[2][1] = (colors[0][1]+colors[1][1])/2;
		colors[2][2] = (colors[0][2]+colors[1][2])/2; colors[2][3] = 255;
		colors[3][0] = colors[3][1] = colors[3][2] = 0; colors[3][3] = 0;
	}

	int y, x;
	for (y = 0; y < 4; y++) {
		for (x = 0; x < 4; x++) {
			int idx = (bits >> ((y*4+x)*2)) & 3;
			int outIdx = y * stride + x * 4;
			out[outIdx+0] = colors[idx][0];
			out[outIdx+1] = colors[idx][1];
			out[outIdx+2] = colors[idx][2];
			out[outIdx+3] = colors[idx][3];
		}
	}
}

void R_LoadDDS(const char *filename, byte **pic, int *width, int *height) {
	void *fileData;
	int fileSize;
	const ddsHeader_t *hdr;
	const byte *data;
	int w, h, numPixels;
	byte *out;

	*pic = NULL; *width = 0; *height = 0;

	fileSize = FS_ReadFile(filename, &fileData);
	if (fileSize <= 0 || !fileData) return;

	if (fileSize < (int)sizeof(ddsHeader_t)) {
		FS_FreeFile(fileData);
		return;
	}

	hdr = (const ddsHeader_t *)fileData;
	if (hdr->magic != DDS_MAGIC || hdr->size != 124) {
		Com_Printf(S_COLOR_YELLOW "DDS: invalid header in %s\n", filename);
		FS_FreeFile(fileData);
		return;
	}

	w = (int)hdr->width;
	h = (int)hdr->height;
	if (w <= 0 || h <= 0 || w > 16384 || h > 16384) {
		Com_Printf(S_COLOR_YELLOW "DDS: invalid dimensions %dx%d in %s\n", w, h, filename);
		FS_FreeFile(fileData);
		return;
	}

	data = (const byte *)fileData + sizeof(ddsHeader_t);
	numPixels = w * h;
	out = (byte *)Z_Malloc(numPixels * 4);

	if ((hdr->pixelFormat.flags & DDPF_FOURCC) && hdr->pixelFormat.fourCC == FOURCC_DXT1) {
		int bx, by;
		int blocksW = (w + 3) / 4;
		int blocksH = (h + 3) / 4;
		const byte *blockData = data;

		Com_Memset(out, 0, numPixels * 4);
		for (by = 0; by < blocksH; by++) {
			for (bx = 0; bx < blocksW; bx++) {
				byte blockOut[4 * 4 * 4];
				DDS_DecompressDXT1Block(blockData, blockOut, 4 * 4);
				blockData += 8;

				int py, px;
				for (py = 0; py < 4 && (by*4+py) < h; py++) {
					for (px = 0; px < 4 && (bx*4+px) < w; px++) {
						int srcIdx = py * 16 + px * 4;
						int dstIdx = ((by*4+py) * w + (bx*4+px)) * 4;
						out[dstIdx+0] = blockOut[srcIdx+0];
						out[dstIdx+1] = blockOut[srcIdx+1];
						out[dstIdx+2] = blockOut[srcIdx+2];
						out[dstIdx+3] = blockOut[srcIdx+3];
					}
				}
			}
		}
	}
	else if (hdr->pixelFormat.flags & DDPF_RGB) {
		int bpp = hdr->pixelFormat.rgbBitCount / 8;
		int i;
		qboolean hasAlpha = (hdr->pixelFormat.flags & DDPF_ALPHAPIXELS) ? qtrue : qfalse;

		for (i = 0; i < numPixels && (int)((data - (const byte*)fileData) + bpp) <= fileSize; i++) {
			uint32_t pixel = 0;
			Com_Memcpy(&pixel, data + i * bpp, bpp);
			out[i*4+0] = (byte)((pixel & hdr->pixelFormat.rBitMask) >> 16);
			out[i*4+1] = (byte)((pixel & hdr->pixelFormat.gBitMask) >> 8);
			out[i*4+2] = (byte)(pixel & hdr->pixelFormat.bBitMask);
			out[i*4+3] = hasAlpha ? (byte)((pixel & hdr->pixelFormat.aBitMask) >> 24) : 255;
		}
	}
	else {
		Com_Printf(S_COLOR_YELLOW "DDS: unsupported format in %s (flags=0x%x, fourCC=0x%x)\n",
			filename, hdr->pixelFormat.flags, hdr->pixelFormat.fourCC);
		Z_Free(out);
		FS_FreeFile(fileData);
		return;
	}

	FS_FreeFile(fileData);
	*pic = out; *width = w; *height = h;
}

/*
===============
R_LoadDDS_Compressed

Load BC7 compressed DDS (DX10 header). Returns raw block data for GPU upload.
===============
*/
qboolean R_LoadDDS_Compressed(const char *filename, byte **data, int *width, int *height, int *format, int *size) {
	void *fileData;
	int fileSize;
	const ddsHeader_t *hdr;
	const ddsHeaderDxt10_t *dx10;
	const byte *src;
	byte *out;
	int w, h;
	int totalSize;
	int level;
	int blockW, blockH;
	int levelSize;

	*data = NULL; *width = 0; *height = 0; *format = 0; *size = 0;

	fileSize = FS_ReadFile(filename, &fileData);
	if (fileSize <= 0 || !fileData) return qfalse;

	if (fileSize < (int)(sizeof(ddsHeader_t) + sizeof(ddsHeaderDxt10_t))) {
		FS_FreeFile(fileData);
		return qfalse;
	}

	hdr = (const ddsHeader_t *)fileData;
	if (hdr->magic != DDS_MAGIC || hdr->size != 124) {
		FS_FreeFile(fileData);
		return qfalse;
	}

	if (!(hdr->pixelFormat.flags & DDPF_FOURCC) || hdr->pixelFormat.fourCC != FOURCC_DX10) {
		FS_FreeFile(fileData);
		return qfalse;
	}

	dx10 = (const ddsHeaderDxt10_t *)((const byte *)fileData + sizeof(ddsHeader_t));
	if (dx10->dxgiFormat != DXGI_FORMAT_BC7_UNORM && dx10->dxgiFormat != DXGI_FORMAT_BC7_UNORM_SRGB) {
		FS_FreeFile(fileData);
		return qfalse;
	}

	w = (int)hdr->width;
	h = (int)hdr->height;
	if (w <= 0 || h <= 0 || w > 16384 || h > 16384) {
		FS_FreeFile(fileData);
		return qfalse;
	}

	src = (const byte *)fileData + sizeof(ddsHeader_t) + sizeof(ddsHeaderDxt10_t);

	/* Compute total size for all mip levels */
	totalSize = 0;
	for (level = 0; ; level++) {
		int lw = w >> level; int lh = h >> level;
		if (lw < 1) lw = 1;
		if (lh < 1) lh = 1;
		blockW = (lw + 3) / 4;
		blockH = (lh + 3) / 4;
		levelSize = blockW * blockH * 16;
		totalSize += levelSize;
		if ((lw == 1 && lh == 1) || (hdr->mipMapCount > 0 && level + 1 >= (int)hdr->mipMapCount))
			break;
	}

	if ((int)(src - (const byte *)fileData) + totalSize > fileSize) {
		FS_FreeFile(fileData);
		return qfalse;
	}

	out = (byte *)Z_Malloc(totalSize);
	Com_Memcpy(out, src, totalSize);
	FS_FreeFile(fileData);

	*data = out;
	*width = w;
	*height = h;
	*format = (dx10->dxgiFormat == DXGI_FORMAT_BC7_UNORM_SRGB) ? 146 : 145; /* VK_FORMAT_BC7_* */
	*size = totalSize;
	return qtrue;
}
