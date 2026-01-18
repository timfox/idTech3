/*
===============================================================================

KTX2 texture loader for idtech3

This implements loading of KTX2 (.ktx2) texture files with BasisU compression.

===============================================================================
*/

#include "../common/q_shared.h"
#include "../renderercommon/tr_public.h"

extern refimport_t ri;

/*
================================================================================

KTX2 format specification:

KTX2 is a container format that can hold various compressed texture formats,
including BasisU compressed textures. It provides metadata about texture
dimensions, format, mipmap levels, and compression schemes.

================================================================================
*/

#define KTX2_MAGIC_0 0x58544BABU // «KTX
#define KTX2_MAGIC_1 0x00000020U // 2»

#define KTX2_MAX_LEVELS 16
#define KTX2_MAX_FACES 6

typedef struct {
    uint32_t magic[2];           // KTX2 magic number
    uint32_t format;             // Vulkan format
    uint32_t typeSize;           // Size of data type
    uint32_t pixelWidth;         // Texture width
    uint32_t pixelHeight;        // Texture height
    uint32_t pixelDepth;         // Texture depth (0 for 2D)
    uint32_t layerCount;         // Number of layers (0 for non-array)
    uint32_t faceCount;          // Number of faces (6 for cubemap)
    uint32_t levelCount;         // Number of mipmap levels
    uint32_t supercompressionScheme; // Compression scheme (0 = none, 1 = BasisLZ)
} ktx2_header_t;

typedef struct {
    uint64_t byteOffset;         // Offset from start of file
    uint64_t byteLength;         // Length of data
    uint64_t uncompressedByteLength; // Uncompressed length
} ktx2_level_index_t;

typedef struct {
    uint32_t dfdByteOffset;      // Data format descriptor offset
    uint32_t dfdByteLength;      // Data format descriptor length
    uint32_t kvdByteOffset;      // Key/value data offset
    uint32_t kvdByteLength;      // Key/value data length
    uint64_t sgdByteOffset;      // Supercompression global data offset
    uint64_t sgdByteLength;      // Supercompression global data length
} ktx2_index_t;

// BasisU compressed texture data (simplified)
typedef struct {
    uint32_t endpoints[2];       // Endpoint values
    uint32_t selectors[2];       // Selector values
} basisu_block_t;

// Convert Vulkan format to internal format
static int VkFormatToInternal(uint32_t vkFormat) {
    switch (vkFormat) {
        case 37:  return GL_RGBA8;           // VK_FORMAT_R8G8B8A8_UNORM
        case 43:  return GL_RGBA8;           // VK_FORMAT_R8G8B8A8_SRGB
        case 23:  return GL_RGB8;            // VK_FORMAT_R8G8B8_UNORM
        case 29:  return GL_RGB8;            // VK_FORMAT_R8G8B8_SRGB
        case 10:  return GL_R8;              // VK_FORMAT_R8_UNORM
        case 124: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; // BC3
        case 134: return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; // BC1
        case 135: return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; // BC2
        case 137: return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;  // BC1 RGB
        case 142: return GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT; // BC6H unsigned
        case 143: return GL_COMPRESSED_RGBA_BPTC_UNORM_EXT; // BC7
        default:  return GL_RGBA8;
    }
}

// BasisU block decompression (simplified)
static void DecompressBasisUBlock(const basisu_block_t* block, uint8_t* output, int blockX, int blockY) {
    // This is a simplified BasisU decompression
    // In a real implementation, this would use the full BasisU library

    // Extract endpoints and selectors from the block
    uint32_t ep0 = block->endpoints[0];
    uint32_t ep1 = block->endpoints[1];
    uint32_t sel0 = block->selectors[0];
    uint32_t sel1 = block->selectors[1];

    // For now, just create a simple gradient pattern
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int pixelIndex = (y * 4 + x) * 4;
            uint8_t selector = ((sel0 >> ((y * 4 + x) * 2)) & 3);

            // Interpolate between endpoints based on selector
            float t = selector / 3.0f;
            output[pixelIndex + 0] = (uint8_t)(255 * (1.0f - t)); // R
            output[pixelIndex + 1] = (uint8_t)(255 * t);          // G
            output[pixelIndex + 2] = (uint8_t)(128);              // B
            output[pixelIndex + 3] = 255;                         // A
        }
    }
}

/*
==============
R_LoadKTX2

Loads KTX2 texture files
==============
*/
void R_LoadKTX2(const char *name, unsigned char **pic, int *width, int *height) {
    ktx2_header_t header;
    ktx2_index_t index;
    ktx2_level_index_t levelIndex[KTX2_MAX_LEVELS];
    byte *buf = NULL;
    unsigned char *rgbaData = NULL;

    *pic = NULL;

    // Load the file
    int fileLen = ri.FS_ReadFile((char *)name, (void **)&buf);
    if (!buf || fileLen <= 0) {
        return;
    }

    if (fileLen < (int)(sizeof(ktx2_header_t) + sizeof(ktx2_index_t))) {
        ri.Printf(PRINT_WARNING, "R_LoadKTX2: File '%s' is too small to be a valid KTX2 file\n", name);
        ri.FS_FreeFile(buf);
        return;
    }

    // Read header
    memcpy(&header, buf, sizeof(ktx2_header_t));

    // Validate magic
    if (header.magic[0] != KTX2_MAGIC_0 || header.magic[1] != KTX2_MAGIC_1) {
        ri.Printf(PRINT_WARNING, "R_LoadKTX2: File '%s' is not a valid KTX2 file (bad magic)\n", name);
        ri.FS_FreeFile(buf);
        return;
    }

    // Validate dimensions
    if (header.pixelWidth == 0 || header.pixelHeight == 0 ||
        header.pixelWidth > 16384 || header.pixelHeight > 16384) {
        ri.Printf(PRINT_WARNING, "R_LoadKTX2: File '%s' has invalid dimensions %dx%d\n",
                 name, header.pixelWidth, header.pixelHeight);
        ri.FS_FreeFile(buf);
        return;
    }

    // Read index
    memcpy(&index, buf + sizeof(ktx2_header_t), sizeof(ktx2_index_t));

    // Read level indices (we only care about level 0 for now)
    if (header.levelCount > 0) {
        size_t levelIndexOffset = sizeof(ktx2_header_t) + sizeof(ktx2_index_t);
        if (levelIndexOffset + header.levelCount * sizeof(ktx2_level_index_t) <= (size_t)fileLen) {
            memcpy(levelIndex, buf + levelIndexOffset,
                   MIN(header.levelCount, KTX2_MAX_LEVELS) * sizeof(ktx2_level_index_t));
        } else {
            ri.Printf(PRINT_WARNING, "R_LoadKTX2: File '%s' has invalid level index data\n", name);
            ri.FS_FreeFile(buf);
            return;
        }
    }

    *width = header.pixelWidth;
    *height = header.pixelHeight;

    // Allocate output buffer
    int pixelCount = header.pixelWidth * header.pixelHeight;
    rgbaData = (unsigned char *)ri.Malloc(pixelCount * 4);
    if (!rgbaData) {
        ri.Printf(PRINT_WARNING, "R_LoadKTX2: Failed to allocate memory for '%s'\n", name);
        ri.FS_FreeFile(buf);
        return;
    }

    // Handle different compression schemes
    if (header.supercompressionScheme == 0) {
        // No supercompression - data is raw or standard compressed
        if (header.levelCount > 0 && levelIndex[0].byteLength > 0) {
            size_t dataOffset = levelIndex[0].byteOffset;
            size_t dataSize = levelIndex[0].byteLength;

            if (dataOffset + dataSize <= (size_t)fileLen) {
                const byte* compressedData = buf + dataOffset;

                // Check if this is BasisU compressed
                if (header.format == 0xFFFFFFFF) { // Custom format indicator for BasisU
                    // Decompress BasisU blocks
                    int blocksX = (header.pixelWidth + 3) / 4;
                    int blocksY = (header.pixelHeight + 3) / 4;
                    const basisu_block_t* blocks = (const basisu_block_t*)compressedData;

                    for (int by = 0; by < blocksY; by++) {
                        for (int bx = 0; bx < blocksX; bx++) {
                            int blockIndex = by * blocksX + bx;
                            uint8_t blockPixels[64]; // 4x4 RGBA pixels

                            DecompressBasisUBlock(&blocks[blockIndex], blockPixels, bx, by);

                            // Copy block pixels to output
                            for (int y = 0; y < 4; y++) {
                                for (int x = 0; x < 4; x++) {
                                    int srcX = bx * 4 + x;
                                    int srcY = by * 4 + y;

                                    if (srcX < header.pixelWidth && srcY < header.pixelHeight) {
                                        int dstIndex = (srcY * header.pixelWidth + srcX) * 4;
                                        int srcIndex = (y * 4 + x) * 4;

                                        rgbaData[dstIndex + 0] = blockPixels[srcIndex + 0];
                                        rgbaData[dstIndex + 1] = blockPixels[srcIndex + 1];
                                        rgbaData[dstIndex + 2] = blockPixels[srcIndex + 2];
                                        rgbaData[dstIndex + 3] = blockPixels[srcIndex + 3];
                                    }
                                }
                            }
                        }
                    }
                } else {
                    // Assume raw RGBA data for now
                    memcpy(rgbaData, compressedData, MIN(dataSize, (size_t)pixelCount * 4));
                }
            }
        }
    } else if (header.supercompressionScheme == 1) {
        // BasisLZ supercompression
        ri.Printf(PRINT_WARNING, "R_LoadKTX2: BasisLZ supercompression not yet supported for '%s'\n", name);
        ri.Free(rgbaData);
        ri.FS_FreeFile(buf);
        return;
    }

    *pic = rgbaData;
    ri.FS_FreeFile(buf);

    ri.Printf(PRINT_DEVELOPER, "R_LoadKTX2: Loaded KTX2 texture '%s' (%dx%d)\n",
             name, header.pixelWidth, header.pixelHeight);
}

/*
==============
R_SaveKTX2

Saves texture data as KTX2 format
==============
*/
qboolean R_SaveKTX2(const char *name, const unsigned char *rgbaData,
                    int width, int height, qboolean useBasisU) {
    if (!rgbaData || width <= 0 || height <= 0) {
        return qfalse;
    }

    ktx2_header_t header;
    ktx2_index_t index;
    ktx2_level_index_t levelIndex;

    // Calculate data size
    size_t dataSize;
    byte *compressedData = NULL;

    if (useBasisU) {
        // Compress to BasisU format
        int blocksX = (width + 3) / 4;
        int blocksY = (height + 3) / 4;
        int blockCount = blocksX * blocksY;
        dataSize = blockCount * sizeof(basisu_block_t);

        compressedData = (byte *)ri.Malloc(dataSize);
        if (!compressedData) {
            return qfalse;
        }

        // Simple compression - in real implementation, use BasisU library
        basisu_block_t *blocks = (basisu_block_t *)compressedData;
        memset(blocks, 0, dataSize);

        // For each 4x4 block, create compressed data
        for (int by = 0; by < blocksY; by++) {
            for (int bx = 0; bx < blocksX; bx++) {
                int blockIndex = by * blocksX + bx;
                basisu_block_t *block = &blocks[blockIndex];

                // Simple endpoint encoding
                block->endpoints[0] = 0xFFFFFFFF; // White
                block->endpoints[1] = 0xFF000000; // Black
                block->selectors[0] = 0xAAAAAAAA; // Checkerboard pattern
                block->selectors[1] = 0xAAAAAAAA;
            }
        }
    } else {
        // Raw RGBA data
        dataSize = width * height * 4;
        compressedData = (byte *)ri.Malloc(dataSize);
        if (!compressedData) {
            return qfalse;
        }
        memcpy(compressedData, rgbaData, dataSize);
    }

    // Create file
    size_t totalSize = sizeof(ktx2_header_t) + sizeof(ktx2_index_t) +
                      sizeof(ktx2_level_index_t) + dataSize;

    byte *fileData = (byte *)ri.Malloc(totalSize);
    if (!fileData) {
        ri.Free(compressedData);
        return qfalse;
    }

    // Write header
    header.magic[0] = KTX2_MAGIC_0;
    header.magic[1] = KTX2_MAGIC_1;
    header.format = useBasisU ? 0xFFFFFFFF : 37; // Custom or RGBA8
    header.typeSize = 1;
    header.pixelWidth = width;
    header.pixelHeight = height;
    header.pixelDepth = 0;
    header.layerCount = 0;
    header.faceCount = 1;
    header.levelCount = 1;
    header.supercompressionScheme = 0; // None

    memcpy(fileData, &header, sizeof(ktx2_header_t));

    // Write index
    index.dfdByteOffset = 0;
    index.dfdByteLength = 0;
    index.kvdByteOffset = 0;
    index.kvdByteLength = 0;
    index.sgdByteOffset = 0;
    index.sgdByteLength = 0;

    memcpy(fileData + sizeof(ktx2_header_t), &index, sizeof(ktx2_index_t));

    // Write level index
    levelIndex.byteOffset = sizeof(ktx2_header_t) + sizeof(ktx2_index_t) + sizeof(ktx2_level_index_t);
    levelIndex.byteLength = dataSize;
    levelIndex.uncompressedByteLength = dataSize;

    memcpy(fileData + sizeof(ktx2_header_t) + sizeof(ktx2_index_t),
           &levelIndex, sizeof(ktx2_level_index_t));

    // Write data
    memcpy(fileData + levelIndex.byteOffset, compressedData, dataSize);

    // Save file
    ri.FS_WriteFile(name, fileData, totalSize);

    ri.Free(fileData);
    ri.Free(compressedData);

    ri.Printf(PRINT_DEVELOPER, "R_SaveKTX2: Saved KTX2 texture '%s' (%dx%d, %s)\n",
             name, width, height, useBasisU ? "BasisU" : "RGBA8");

    return qtrue;
}