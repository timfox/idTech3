/*
===========================================================================
Copyright (C) 2024 id Tech 3

This file provides zstd compression integration for enhanced compression
capabilities. It wraps zstd functions with engine-style APIs.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_ZSTD
#include "zstd.h"
#include "zstd_errors.h"

// CVar to control compression method
static cvar_t *com_compression;

/*
=================
ZSTD_Compress
=================
Compress data using zstd
=================
*/
int ZSTD_Compress(void *dst, size_t dstCapacity, const void *src, size_t srcSize, int compressionLevel)
{
	size_t result;
	
	if (!dst || !src || dstCapacity == 0 || srcSize == 0)
		return -1;
	
	// Clamp compression level to valid range
	if (compressionLevel < ZSTD_minCLevel())
		compressionLevel = ZSTD_minCLevel();
	if (compressionLevel > ZSTD_maxCLevel())
		compressionLevel = ZSTD_maxCLevel();
	
	result = ZSTD_compress(dst, dstCapacity, src, srcSize, compressionLevel);
	
	if (ZSTD_isError(result))
		return -1;
	
	return (int)result;
}

/*
=================
ZSTD_Decompress
=================
Decompress data using zstd
=================
*/
int ZSTD_Decompress(void *dst, size_t dstCapacity, const void *src, size_t srcSize)
{
	size_t result;
	
	if (!dst || !src || dstCapacity == 0 || srcSize == 0)
		return -1;
	
	result = ZSTD_decompress(dst, dstCapacity, src, srcSize);
	
	if (ZSTD_isError(result))
		return -1;
	
	return (int)result;
}

/*
=================
ZSTD_GetCompressedSize
=================
Get the size needed for compressed data
=================
*/
size_t ZSTD_GetCompressedSize(const void *src, size_t srcSize, int compressionLevel)
{
	if (!src || srcSize == 0)
		return 0;
	
	// Clamp compression level
	if (compressionLevel < ZSTD_minCLevel())
		compressionLevel = ZSTD_minCLevel();
	if (compressionLevel > ZSTD_maxCLevel())
		compressionLevel = ZSTD_maxCLevel();
	
	return ZSTD_compressBound(srcSize);
}

/*
=================
ZSTD_GetDecompressedSize
=================
Get the decompressed size from compressed data
=================
*/
size_t ZSTD_GetDecompressedSize(const void *src, size_t srcSize)
{
	unsigned long long result;
	
	if (!src || srcSize == 0)
		return 0;
	
	result = ZSTD_getFrameContentSize(src, srcSize);
	
	if (result == ZSTD_CONTENTSIZE_ERROR || result == ZSTD_CONTENTSIZE_UNKNOWN)
		return 0;
	
	return (size_t)result;
}

/*
=================
ZSTD_Init
=================
Initialize zstd subsystem
=================
*/
void ZSTD_Init(void)
{
	com_compression = Cvar_Get("com_compression", "1", CVAR_ARCHIVE | CVAR_LATCH);
	Cvar_SetDescription(com_compression, "Compression method: 0 = zlib, 1 = zstd (default)");
}

#endif // USE_ZSTD

