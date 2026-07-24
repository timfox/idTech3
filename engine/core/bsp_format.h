/*
 * Additive BSP container support.  Legacy disk declarations remain in
 * qfiles.h and qfiles_bsp30.h; this API never aliases file bytes as structs.
 */
#ifndef BSP_FORMAT_H
#define BSP_FORMAT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BSP_INDEX_INVALID UINT32_MAX
#define BSPX_NAME_BYTES 24u
#define BSPX_MAX_LUMPS 4096u
#define XBSP_MAGIC 0x50534258u
#define XBSP_VERSION 1u
#define XBSP_HEADER_SIZE 104u
#define XBSP_DIRECTORY_ENTRY_SIZE 72u
#define XBSP_MAX_DIRECTORY_ENTRIES 256u
#define GX_LUMP_PAYLOAD_MAGIC 0x504c5847u
#define GX_LUMP_PAYLOAD_HEADER_SIZE 48u

typedef uint64_t bspOffset64_t;
typedef uint64_t bspSize64_t;
typedef uint64_t bspCount64_t;
typedef uint32_t bspNodeIndex_t;
typedef uint32_t bspLeafIndex_t;
typedef uint32_t bspSurfaceIndex_t;
typedef uint32_t bspVertexIndex_t;
typedef uint32_t bspMaterialIndex_t;
typedef uint32_t bspModelIndex_t;

typedef enum {
	BSP_FORMAT_UNKNOWN = 0,
	BSP_FORMAT_GOLDSRC_BSP30,
	BSP_FORMAT_QUAKE3_IBSP46,
	BSP_FORMAT_QUAKE3_IBSP47,
	BSP_FORMAT_BSP30_WITH_BSPX,
	BSP_FORMAT_IBSP46_WITH_BSPX,
	BSP_FORMAT_IBSP47_WITH_BSPX,
	BSP_FORMAT_NATIVE_XBSP
} bspFormatFamily_t;

typedef enum {
	BSP_ERROR_NONE = 0,
	BSP_ERROR_ARGUMENT,
	BSP_ERROR_TRUNCATED,
	BSP_ERROR_MAGIC,
	BSP_ERROR_VERSION,
	BSP_ERROR_RANGE,
	BSP_ERROR_COUNT,
	BSP_ERROR_DIRECTORY,
	BSP_ERROR_DUPLICATE,
	BSP_ERROR_SCHEMA,
	BSP_ERROR_PATH
} bspError_t;

typedef enum {
	GX_COMPRESS_NONE = 0,
	GX_COMPRESS_LZ4 = 1,
	GX_COMPRESS_ZSTD = 2
} gxCompression_t;

typedef struct {
	const uint8_t *data;
	uint64_t size;
	bspFormatFamily_t format;
	uint32_t version;
	bool byteSwapped;
	bool ownsMemory;
} bspFileView_t;

typedef struct {
	char name[BSPX_NAME_BYTES + 1u];
	int32_t legacyIndex;
	const uint8_t *data;
	uint64_t offset;
	uint64_t storedSize;
	uint64_t uncompressedSize;
	uint64_t elementCount;
	uint32_t schemaVersion;
	uint32_t flags;
	uint32_t compression;
	uint64_t contentHash;
} bspLumpView_t;

typedef struct {
	uint64_t directoryOffset;
	uint32_t count;
	bspLumpView_t lumps[BSPX_MAX_LUMPS];
} bspxDirectory_t;

typedef struct {
	uint32_t flags;
	uint64_t directoryOffset;
	uint32_t directoryCount;
	uint64_t fileSize;
	uint64_t fileHash;
	uint8_t mapUuid[16];
	uint64_t compatibilityBaseHash;
	bspLumpView_t lumps[XBSP_MAX_DIRECTORY_ENTRIES];
} xbspDirectory_t;

typedef struct {
	uint16_t schemaVersion;
	uint16_t headerSize;
	uint32_t flags;
	uint32_t compression;
	uint64_t elementCount;
	uint64_t uncompressedSize;
	uint64_t storedSize;
	uint64_t contentHash;
} gxPayloadInfo_t;

typedef struct {
	float normal[3];
	float distance;
} bspRuntimePlane_t;

typedef struct {
	int32_t planeIndex;
	int32_t children[2];
	float mins[3];
	float maxs[3];
	uint32_t firstSurface;
	uint32_t surfaceCount;
} bspRuntimeNode_t;

typedef struct {
	int32_t cluster;
	int32_t area;
	float mins[3];
	float maxs[3];
	uint32_t firstLeafSurface;
	uint32_t leafSurfaceCount;
	uint32_t firstLeafBrush;
	uint32_t leafBrushCount;
	uint32_t contents;
	uint32_t flags;
} bspRuntimeLeaf_t;

typedef struct {
	uint32_t firstVertex;
	uint32_t vertexCount;
	uint32_t firstIndex;
	uint32_t indexCount;
	uint32_t materialIndex;
	uint32_t flags;
} bspRuntimeSurface_t;

typedef struct {
	float position[3];
	float normal[3];
	float texcoord[2];
	float lightmapCoord[2];
	uint8_t color[4];
} bspRuntimeVertex_t;

typedef struct {
	uint32_t firstSurface;
	uint32_t surfaceCount;
	float mins[3];
	float maxs[3];
} bspRuntimeModel_t;

typedef struct {
	const char *name;
	uint32_t flags;
} bspRuntimeMaterial_t;

typedef struct {
	uint8_t *data;
	uint64_t size;
	uint32_t clusterCount;
	uint32_t rowSize;
} bspRuntimeVisibility_t;

typedef struct {
	void *lightmaps;
	uint32_t lightmapCount;
	void *lightGrid;
	uint32_t lightGridCount;
} bspRuntimeLighting_t;

typedef struct {
	bspLumpView_t *known;
	uint32_t knownCount;
	bspLumpView_t *unknown;
	uint32_t unknownCount;
} bspRuntimeExtensions_t;

typedef struct {
	bspRuntimeNode_t *nodes;
	uint32_t nodeCount;
	bspRuntimeLeaf_t *leaves;
	uint32_t leafCount;
	bspRuntimePlane_t *planes;
	uint32_t planeCount;
	bspRuntimeSurface_t *surfaces;
	uint32_t surfaceCount;
	bspRuntimeVertex_t *vertices;
	uint32_t vertexCount;
	uint32_t *indices;
	uint32_t indexCount;
	uint32_t *leafSurfaces;
	uint32_t leafSurfaceCount;
	bspRuntimeModel_t *models;
	uint32_t modelCount;
	bspRuntimeMaterial_t *materials;
	uint32_t materialCount;
	bspRuntimeVisibility_t visibility;
	bspRuntimeLighting_t lighting;
	bspRuntimeExtensions_t extensions;
} bspRuntimeWorld_t;

typedef struct {
	const char *name;
	uint16_t minimumVersion;
	uint16_t maximumVersion;
	uint32_t flags;
	const char *const *dependencies;
	uint32_t dependencyCount;
} bspLumpSchema_t;

bool BSP_CheckedRange(uint64_t fileSize, uint64_t offset, uint64_t length);
bool BSP_CheckedMultiply(uint64_t count, uint64_t stride, uint64_t *result);
bool BSP_IsSafeExternalPath(const char *path);
const char *BSP_ErrorString(bspError_t error);
const char *BSP_FormatName(bspFormatFamily_t format);

bspFormatFamily_t BSP_DetectFormat(const uint8_t *data, uint64_t size,
	bspError_t *error);
bool BSPX_ReadDirectory(const uint8_t *data, uint64_t size,
	bspFormatFamily_t baseFormat, bspxDirectory_t *directory, bspError_t *error);
const bspLumpView_t *BSPX_FindLump(const bspxDirectory_t *directory,
	const char *name);
bool XBSP_ReadDirectory(const uint8_t *data, uint64_t size,
	xbspDirectory_t *directory, bspError_t *error);
bool GX_ReadPayloadHeader(const bspLumpView_t *lump, gxPayloadInfo_t *payload,
	bspError_t *error);

const bspLumpSchema_t *BSP_LumpRegistry(size_t *count);
const bspLumpSchema_t *BSP_FindLumpSchema(const char *name,
	uint16_t schemaVersion);

size_t BSPX_WriteHeader(uint8_t *output, size_t capacity, uint32_t lumpCount);
size_t BSPX_WriteDirectoryEntry(uint8_t *output, size_t capacity,
	const bspLumpView_t *lump);
size_t XBSP_WriteHeader(uint8_t *output, size_t capacity,
	const xbspDirectory_t *directory);
size_t XBSP_WriteDirectoryEntry(uint8_t *output, size_t capacity,
	const bspLumpView_t *lump);

#endif
