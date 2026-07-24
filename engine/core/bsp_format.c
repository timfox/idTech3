#include "bsp_format.h"

#include <limits.h>
#include <string.h>

#define BSP30_HEADER_SIZE (4u + 15u * 8u)
#define IBSP_HEADER_SIZE (8u + 17u * 8u)
#define BSPX_HEADER_SIZE 8u
#define BSPX_DIRECTORY_ENTRY_SIZE 32u

static uint16_t BSP_ReadU16(const uint8_t *p) {
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t BSP_ReadU32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t BSP_ReadU64(const uint8_t *p) {
	return (uint64_t)BSP_ReadU32(p) | ((uint64_t)BSP_ReadU32(p + 4) << 32);
}

static void BSP_WriteU16(uint8_t *p, uint16_t value) {
	p[0] = (uint8_t)value;
	p[1] = (uint8_t)(value >> 8);
}

static void BSP_WriteU32(uint8_t *p, uint32_t value) {
	p[0] = (uint8_t)value;
	p[1] = (uint8_t)(value >> 8);
	p[2] = (uint8_t)(value >> 16);
	p[3] = (uint8_t)(value >> 24);
}

static void BSP_WriteU64(uint8_t *p, uint64_t value) {
	BSP_WriteU32(p, (uint32_t)value);
	BSP_WriteU32(p + 4, (uint32_t)(value >> 32));
}

static void BSP_SetError(bspError_t *error, bspError_t value) {
	if (error) {
		*error = value;
	}
}

bool BSP_CheckedRange(uint64_t fileSize, uint64_t offset, uint64_t length) {
	return offset <= fileSize && length <= fileSize - offset;
}

bool BSP_CheckedMultiply(uint64_t count, uint64_t stride, uint64_t *result) {
	if (!result || (stride != 0 && count > UINT64_MAX / stride)) {
		return false;
	}
	*result = count * stride;
	return true;
}

bool BSP_IsSafeExternalPath(const char *path) {
	const char *segment;
	const char *p;

	if (!path || !*path || path[0] == '/' || path[0] == '\\') {
		return false;
	}
	if (((path[0] >= 'A' && path[0] <= 'Z') ||
		(path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':') {
		return false;
	}
	segment = path;
	for (p = path; ; ++p) {
		if (*p == '\\' || *p == '\0' || *p == '/') {
			size_t length = (size_t)(p - segment);
			if (length == 0 || (length == 1 && segment[0] == '.') ||
				(length == 2 && segment[0] == '.' && segment[1] == '.')) {
				return false;
			}
			if (*p == '\0') {
				break;
			}
			segment = p + 1;
		} else if ((unsigned char)*p < 32u) {
			return false;
		}
	}
	return true;
}

const char *BSP_ErrorString(bspError_t error) {
	static const char *const names[] = {
		"none", "invalid argument", "truncated input", "bad magic",
		"unsupported version", "range outside file", "invalid count",
		"invalid directory", "duplicate lump name", "unsupported schema",
		"unsafe external path"
	};
	return (unsigned)error < sizeof(names) / sizeof(names[0])
		? names[error] : "unknown error";
}

const char *BSP_FormatName(bspFormatFamily_t format) {
	static const char *const names[] = {
		"unknown", "GoldSrc BSP30", "Quake 3 IBSP46", "IBSP47",
		"GoldSrc BSP30 + BSPX", "IBSP46 + BSPX", "IBSP47 + BSPX",
		"native XBSP"
	};
	return (unsigned)format < sizeof(names) / sizeof(names[0])
		? names[format] : names[0];
}

static bool BSP_LegacyDirectoryEnd(const uint8_t *data, uint64_t size,
	bspFormatFamily_t format, uint64_t *end, bspError_t *error) {
	uint32_t count;
	uint64_t directoryOffset;
	uint64_t maximum;
	uint32_t i;

	if (!data || !end) {
		BSP_SetError(error, BSP_ERROR_ARGUMENT);
		return false;
	}
	if (format == BSP_FORMAT_GOLDSRC_BSP30) {
		count = 15;
		directoryOffset = 4;
		maximum = BSP30_HEADER_SIZE;
	} else if (format == BSP_FORMAT_QUAKE3_IBSP46 ||
		format == BSP_FORMAT_QUAKE3_IBSP47) {
		count = 17;
		directoryOffset = 8;
		maximum = IBSP_HEADER_SIZE;
	} else {
		BSP_SetError(error, BSP_ERROR_ARGUMENT);
		return false;
	}
	if (!BSP_CheckedRange(size, 0, maximum)) {
		BSP_SetError(error, BSP_ERROR_TRUNCATED);
		return false;
	}
	for (i = 0; i < count; ++i) {
		const uint8_t *entry = data + directoryOffset + (uint64_t)i * 8u;
		uint32_t offset = BSP_ReadU32(entry);
		uint32_t length = BSP_ReadU32(entry + 4);
		uint64_t lumpEnd;
		if (!BSP_CheckedRange(size, offset, length)) {
			BSP_SetError(error, BSP_ERROR_RANGE);
			return false;
		}
		lumpEnd = (uint64_t)offset + length;
		if (lumpEnd > maximum) {
			maximum = lumpEnd;
		}
	}
	if (maximum > UINT64_MAX - 3u) {
		BSP_SetError(error, BSP_ERROR_RANGE);
		return false;
	}
	*end = (maximum + 3u) & ~UINT64_C(3);
	BSP_SetError(error, BSP_ERROR_NONE);
	return true;
}

static bspFormatFamily_t BSP_BaseFormat(const uint8_t *data, uint64_t size,
	bspError_t *error) {
	uint32_t magic;
	uint32_t version;
	uint64_t ignored;

	if (!data || size < 4) {
		BSP_SetError(error, BSP_ERROR_TRUNCATED);
		return BSP_FORMAT_UNKNOWN;
	}
	magic = BSP_ReadU32(data);
	if (magic == XBSP_MAGIC) {
		if (size < XBSP_HEADER_SIZE) {
			BSP_SetError(error, BSP_ERROR_TRUNCATED);
			return BSP_FORMAT_UNKNOWN;
		}
		if (BSP_ReadU32(data + 4) != XBSP_VERSION) {
			BSP_SetError(error, BSP_ERROR_VERSION);
			return BSP_FORMAT_UNKNOWN;
		}
		BSP_SetError(error, BSP_ERROR_NONE);
		return BSP_FORMAT_NATIVE_XBSP;
	}
	if (magic == 30u) {
		if (!BSP_LegacyDirectoryEnd(data, size, BSP_FORMAT_GOLDSRC_BSP30,
			&ignored, error)) {
			return BSP_FORMAT_UNKNOWN;
		}
		return BSP_FORMAT_GOLDSRC_BSP30;
	}
	if (memcmp(data, "IBSP", 4) != 0) {
		BSP_SetError(error, BSP_ERROR_MAGIC);
		return BSP_FORMAT_UNKNOWN;
	}
	if (size < IBSP_HEADER_SIZE) {
		BSP_SetError(error, BSP_ERROR_TRUNCATED);
		return BSP_FORMAT_UNKNOWN;
	}
	version = BSP_ReadU32(data + 4);
	if (version != 46u && version != 47u) {
		BSP_SetError(error, BSP_ERROR_VERSION);
		return BSP_FORMAT_UNKNOWN;
	}
	if (!BSP_LegacyDirectoryEnd(data, size,
		version == 46u ? BSP_FORMAT_QUAKE3_IBSP46 : BSP_FORMAT_QUAKE3_IBSP47,
		&ignored, error)) {
		return BSP_FORMAT_UNKNOWN;
	}
	return version == 46u ? BSP_FORMAT_QUAKE3_IBSP46 : BSP_FORMAT_QUAKE3_IBSP47;
}

bspFormatFamily_t BSP_DetectFormat(const uint8_t *data, uint64_t size,
	bspError_t *error) {
	bspFormatFamily_t base = BSP_BaseFormat(data, size, error);
	bspxDirectory_t directory;

	if (base == BSP_FORMAT_NATIVE_XBSP) {
		xbspDirectory_t nativeDirectory;
		return XBSP_ReadDirectory(data, size, &nativeDirectory, error)
			? base : BSP_FORMAT_UNKNOWN;
	}
	if (base == BSP_FORMAT_UNKNOWN) {
		return base;
	}
	if (BSPX_ReadDirectory(data, size, base, &directory, NULL)) {
		if (base == BSP_FORMAT_GOLDSRC_BSP30) return BSP_FORMAT_BSP30_WITH_BSPX;
		if (base == BSP_FORMAT_QUAKE3_IBSP46) return BSP_FORMAT_IBSP46_WITH_BSPX;
		return BSP_FORMAT_IBSP47_WITH_BSPX;
	}
	BSP_SetError(error, BSP_ERROR_NONE);
	return base;
}

static bool BSP_RangesOverlap(uint64_t aOffset, uint64_t aSize,
	uint64_t bOffset, uint64_t bSize) {
	if (aSize == 0 || bSize == 0) {
		return false;
	}
	return aOffset < bOffset + bSize && bOffset < aOffset + aSize;
}

bool BSPX_ReadDirectory(const uint8_t *data, uint64_t size,
	bspFormatFamily_t baseFormat, bspxDirectory_t *directory, bspError_t *error) {
	uint64_t offset;
	uint64_t bytes;
	uint32_t count;
	uint32_t i;

	if (!directory) {
		BSP_SetError(error, BSP_ERROR_ARGUMENT);
		return false;
	}
	memset(directory, 0, sizeof(*directory));
	if (!BSP_LegacyDirectoryEnd(data, size, baseFormat, &offset, error)) {
		return false;
	}
	if (!BSP_CheckedRange(size, offset, BSPX_HEADER_SIZE) ||
		memcmp(data + offset, "BSPX", 4) != 0) {
		BSP_SetError(error, BSP_ERROR_MAGIC);
		return false;
	}
	count = BSP_ReadU32(data + offset + 4);
	if (count > BSPX_MAX_LUMPS ||
		!BSP_CheckedMultiply(count, BSPX_DIRECTORY_ENTRY_SIZE, &bytes) ||
		!BSP_CheckedRange(size, offset + BSPX_HEADER_SIZE, bytes)) {
		BSP_SetError(error, BSP_ERROR_COUNT);
		return false;
	}
	directory->directoryOffset = offset;
	directory->count = count;
	for (i = 0; i < count; ++i) {
		const uint8_t *entry = data + offset + BSPX_HEADER_SIZE +
			(uint64_t)i * BSPX_DIRECTORY_ENTRY_SIZE;
		bspLumpView_t *lump = &directory->lumps[i];
		uint32_t j;
		memcpy(lump->name, entry, BSPX_NAME_BYTES);
		lump->name[BSPX_NAME_BYTES] = '\0';
		if (!memchr(lump->name, '\0', BSPX_NAME_BYTES) || !lump->name[0]) {
			BSP_SetError(error, BSP_ERROR_DIRECTORY);
			return false;
		}
		for (j = 0; j < i; ++j) {
			if (strcmp(lump->name, directory->lumps[j].name) == 0) {
				BSP_SetError(error, BSP_ERROR_DUPLICATE);
				return false;
			}
		}
		lump->legacyIndex = -1;
		lump->offset = BSP_ReadU32(entry + 24);
		lump->storedSize = BSP_ReadU32(entry + 28);
		lump->uncompressedSize = lump->storedSize;
		if (!BSP_CheckedRange(size, lump->offset, lump->storedSize)) {
			BSP_SetError(error, BSP_ERROR_RANGE);
			return false;
		}
		if (BSP_RangesOverlap(lump->offset, lump->storedSize, offset,
			BSPX_HEADER_SIZE + bytes)) {
			BSP_SetError(error, BSP_ERROR_DIRECTORY);
			return false;
		}
		for (j = 0; j < i; ++j) {
			if (BSP_RangesOverlap(lump->offset, lump->storedSize,
				directory->lumps[j].offset, directory->lumps[j].storedSize)) {
				BSP_SetError(error, BSP_ERROR_DIRECTORY);
				return false;
			}
		}
		lump->data = data + lump->offset;
	}
	BSP_SetError(error, BSP_ERROR_NONE);
	return true;
}

const bspLumpView_t *BSPX_FindLump(const bspxDirectory_t *directory,
	const char *name) {
	uint32_t i;
	if (!directory || !name) return NULL;
	for (i = 0; i < directory->count; ++i) {
		if (strcmp(directory->lumps[i].name, name) == 0) {
			return &directory->lumps[i];
		}
	}
	return NULL;
}

bool XBSP_ReadDirectory(const uint8_t *data, uint64_t size,
	xbspDirectory_t *directory, bspError_t *error) {
	uint32_t headerSize;
	uint32_t entrySize;
	uint64_t bytes;
	uint32_t i;

	if (!data || !directory) {
		BSP_SetError(error, BSP_ERROR_ARGUMENT);
		return false;
	}
	memset(directory, 0, sizeof(*directory));
	if (!BSP_CheckedRange(size, 0, XBSP_HEADER_SIZE)) {
		BSP_SetError(error, BSP_ERROR_TRUNCATED);
		return false;
	}
	if (BSP_ReadU32(data) != XBSP_MAGIC) {
		BSP_SetError(error, BSP_ERROR_MAGIC);
		return false;
	}
	if (BSP_ReadU32(data + 4) != XBSP_VERSION) {
		BSP_SetError(error, BSP_ERROR_VERSION);
		return false;
	}
	headerSize = BSP_ReadU32(data + 8);
	entrySize = BSP_ReadU32(data + 28);
	directory->flags = BSP_ReadU32(data + 12);
	directory->directoryOffset = BSP_ReadU64(data + 16);
	directory->directoryCount = BSP_ReadU32(data + 24);
	directory->fileSize = BSP_ReadU64(data + 32);
	directory->fileHash = BSP_ReadU64(data + 40);
	memcpy(directory->mapUuid, data + 48, sizeof(directory->mapUuid));
	directory->compatibilityBaseHash = BSP_ReadU64(data + 64);
	if (headerSize < XBSP_HEADER_SIZE || headerSize > size ||
		entrySize < XBSP_DIRECTORY_ENTRY_SIZE ||
		directory->directoryCount > XBSP_MAX_DIRECTORY_ENTRIES ||
		directory->fileSize != size ||
		!BSP_CheckedMultiply(directory->directoryCount, entrySize, &bytes) ||
		!BSP_CheckedRange(size, directory->directoryOffset, bytes)) {
		BSP_SetError(error, BSP_ERROR_DIRECTORY);
		return false;
	}
	for (i = 0; i < directory->directoryCount; ++i) {
		const uint8_t *entry = data + directory->directoryOffset +
			(uint64_t)i * entrySize;
		bspLumpView_t *lump = &directory->lumps[i];
		uint32_t j;
		memcpy(lump->name, entry, BSPX_NAME_BYTES);
		lump->name[BSPX_NAME_BYTES] = '\0';
		if (!memchr(lump->name, '\0', BSPX_NAME_BYTES) || !lump->name[0]) {
			BSP_SetError(error, BSP_ERROR_DIRECTORY);
			return false;
		}
		for (j = 0; j < i; ++j) {
			if (strcmp(lump->name, directory->lumps[j].name) == 0) {
				BSP_SetError(error, BSP_ERROR_DUPLICATE);
				return false;
			}
		}
		lump->legacyIndex = -1;
		lump->schemaVersion = BSP_ReadU16(entry + 24);
		lump->flags = BSP_ReadU16(entry + 26);
		lump->compression = BSP_ReadU32(entry + 28);
		lump->offset = BSP_ReadU64(entry + 32);
		lump->storedSize = BSP_ReadU64(entry + 40);
		lump->uncompressedSize = BSP_ReadU64(entry + 48);
		lump->elementCount = BSP_ReadU64(entry + 56);
		lump->contentHash = BSP_ReadU64(entry + 64);
		if (lump->compression > GX_COMPRESS_ZSTD ||
			!BSP_CheckedRange(size, lump->offset, lump->storedSize) ||
			BSP_RangesOverlap(lump->offset, lump->storedSize,
				directory->directoryOffset, bytes)) {
			BSP_SetError(error, BSP_ERROR_RANGE);
			return false;
		}
		for (j = 0; j < i; ++j) {
			if (BSP_RangesOverlap(lump->offset, lump->storedSize,
				directory->lumps[j].offset, directory->lumps[j].storedSize)) {
				BSP_SetError(error, BSP_ERROR_DIRECTORY);
				return false;
			}
		}
		lump->data = data + lump->offset;
	}
	BSP_SetError(error, BSP_ERROR_NONE);
	return true;
}

bool GX_ReadPayloadHeader(const bspLumpView_t *lump, gxPayloadInfo_t *payload,
	bspError_t *error) {
	const uint8_t *data;
	if (!lump || !payload || !lump->data ||
		lump->storedSize < GX_LUMP_PAYLOAD_HEADER_SIZE) {
		BSP_SetError(error, BSP_ERROR_TRUNCATED);
		return false;
	}
	data = lump->data;
	if (BSP_ReadU32(data) != GX_LUMP_PAYLOAD_MAGIC) {
		BSP_SetError(error, BSP_ERROR_MAGIC);
		return false;
	}
	payload->schemaVersion = BSP_ReadU16(data + 4);
	payload->headerSize = BSP_ReadU16(data + 6);
	payload->flags = BSP_ReadU32(data + 8);
	payload->compression = BSP_ReadU32(data + 12);
	payload->elementCount = BSP_ReadU64(data + 16);
	payload->uncompressedSize = BSP_ReadU64(data + 24);
	payload->storedSize = BSP_ReadU64(data + 32);
	payload->contentHash = BSP_ReadU64(data + 40);
	if (payload->headerSize < GX_LUMP_PAYLOAD_HEADER_SIZE ||
		payload->headerSize > lump->storedSize ||
		payload->compression > GX_COMPRESS_ZSTD ||
		payload->storedSize != lump->storedSize - payload->headerSize) {
		BSP_SetError(error, BSP_ERROR_SCHEMA);
		return false;
	}
	BSP_SetError(error, BSP_ERROR_NONE);
	return true;
}

static const char *const depsLeaves[] = { "GX_NODES" };
static const char *const depsLeafSurf[] = { "GX_LEAVES", "GX_SURFACES" };
static const char *const depsIndices[] = { "GX_VERTICES" };
static const char *const depsCells[] = { "GX_SURFACES", "GX_MODELS" };

static const bspLumpSchema_t bspSchemas[] = {
	{ "GX_META", 1, 1, 0, NULL, 0 },
	{ "GX_STRINGS", 1, 1, 0, NULL, 0 },
	{ "GX_NODES", 1, 1, 0, NULL, 0 },
	{ "GX_LEAVES", 1, 1, 0, depsLeaves, 1 },
	{ "GX_LEAFSURF", 1, 1, 0, depsLeafSurf, 2 },
	{ "GX_SURFACES", 1, 1, 0, NULL, 0 },
	{ "GX_VERTICES", 1, 1, 0, NULL, 0 },
	{ "GX_INDICES", 1, 1, 0, depsIndices, 1 },
	{ "GX_MODELS", 1, 1, 0, NULL, 0 },
	{ "GX_MATERIALS", 1, 1, 0, NULL, 0 },
	{ "GX_VIS", 1, 1, 0, NULL, 0 },
	{ "GX_CLUSTERS", 1, 1, 0, NULL, 0 },
	{ "GX_AREAS", 1, 1, 0, NULL, 0 },
	{ "GX_PORTALS", 1, 1, 0, NULL, 0 },
	{ "GX_LIGHTMAPS", 1, 1, 0, NULL, 0 },
	{ "GX_LIGHTGRID", 1, 1, 0, NULL, 0 },
	{ "GX_PROBES", 1, 1, 0, NULL, 0 },
	{ "GX_DECALS", 1, 1, 0, NULL, 0 },
	{ "GX_OCCLUSION", 1, 1, 0, NULL, 0 },
	{ "GX_PHYSICS", 1, 1, 0, NULL, 0 },
	{ "GX_NAVMESH", 1, 1, 0, NULL, 0 },
	{ "GX_ENTBIN", 1, 1, 0, NULL, 0 },
	{ "GX_ASSETREFS", 1, 1, 0, NULL, 0 },
	{ "GX_CELLS", 1, 1, 0, depsCells, 2 },
	{ "GX_CELLDEPS", 1, 1, 0, NULL, 0 },
	{ "GX_BUILD", 1, 1, 0, NULL, 0 },
	{ "GX_HASHES", 1, 1, 0, NULL, 0 }
};

const bspLumpSchema_t *BSP_LumpRegistry(size_t *count) {
	if (count) *count = sizeof(bspSchemas) / sizeof(bspSchemas[0]);
	return bspSchemas;
}

const bspLumpSchema_t *BSP_FindLumpSchema(const char *name,
	uint16_t schemaVersion) {
	size_t count;
	size_t i;
	const bspLumpSchema_t *schemas = BSP_LumpRegistry(&count);
	if (!name) return NULL;
	for (i = 0; i < count; ++i) {
		if (strcmp(name, schemas[i].name) == 0 &&
			schemaVersion >= schemas[i].minimumVersion &&
			schemaVersion <= schemas[i].maximumVersion) {
			return &schemas[i];
		}
	}
	return NULL;
}

size_t BSPX_WriteHeader(uint8_t *output, size_t capacity, uint32_t lumpCount) {
	if (!output || capacity < BSPX_HEADER_SIZE || lumpCount > BSPX_MAX_LUMPS) {
		return 0;
	}
	memcpy(output, "BSPX", 4);
	BSP_WriteU32(output + 4, lumpCount);
	return BSPX_HEADER_SIZE;
}

size_t BSPX_WriteDirectoryEntry(uint8_t *output, size_t capacity,
	const bspLumpView_t *lump) {
	size_t nameLength;
	if (!output || !lump || capacity < BSPX_DIRECTORY_ENTRY_SIZE ||
		lump->offset > UINT32_MAX || lump->storedSize > UINT32_MAX) {
		return 0;
	}
	nameLength = strlen(lump->name);
	if (nameLength == 0 || nameLength >= BSPX_NAME_BYTES) {
		return 0;
	}
	memset(output, 0, BSPX_DIRECTORY_ENTRY_SIZE);
	memcpy(output, lump->name, nameLength);
	BSP_WriteU32(output + 24, (uint32_t)lump->offset);
	BSP_WriteU32(output + 28, (uint32_t)lump->storedSize);
	return BSPX_DIRECTORY_ENTRY_SIZE;
}

size_t XBSP_WriteHeader(uint8_t *output, size_t capacity,
	const xbspDirectory_t *directory) {
	if (!output || !directory || capacity < XBSP_HEADER_SIZE ||
		directory->directoryCount > XBSP_MAX_DIRECTORY_ENTRIES) {
		return 0;
	}
	memset(output, 0, XBSP_HEADER_SIZE);
	BSP_WriteU32(output, XBSP_MAGIC);
	BSP_WriteU32(output + 4, XBSP_VERSION);
	BSP_WriteU32(output + 8, XBSP_HEADER_SIZE);
	BSP_WriteU32(output + 12, directory->flags);
	BSP_WriteU64(output + 16, directory->directoryOffset);
	BSP_WriteU32(output + 24, directory->directoryCount);
	BSP_WriteU32(output + 28, XBSP_DIRECTORY_ENTRY_SIZE);
	BSP_WriteU64(output + 32, directory->fileSize);
	BSP_WriteU64(output + 40, directory->fileHash);
	memcpy(output + 48, directory->mapUuid, sizeof(directory->mapUuid));
	BSP_WriteU64(output + 64, directory->compatibilityBaseHash);
	return XBSP_HEADER_SIZE;
}

size_t XBSP_WriteDirectoryEntry(uint8_t *output, size_t capacity,
	const bspLumpView_t *lump) {
	size_t nameLength;
	if (!output || !lump || capacity < XBSP_DIRECTORY_ENTRY_SIZE ||
		lump->compression > GX_COMPRESS_ZSTD) {
		return 0;
	}
	nameLength = strlen(lump->name);
	if (nameLength == 0 || nameLength >= BSPX_NAME_BYTES) {
		return 0;
	}
	memset(output, 0, XBSP_DIRECTORY_ENTRY_SIZE);
	memcpy(output, lump->name, nameLength);
	BSP_WriteU16(output + 24, (uint16_t)lump->schemaVersion);
	BSP_WriteU16(output + 26, (uint16_t)lump->flags);
	BSP_WriteU32(output + 28, lump->compression);
	BSP_WriteU64(output + 32, lump->offset);
	BSP_WriteU64(output + 40, lump->storedSize);
	BSP_WriteU64(output + 48, lump->uncompressedSize);
	BSP_WriteU64(output + 56, lump->elementCount);
	BSP_WriteU64(output + 64, lump->contentHash);
	return XBSP_DIRECTORY_ENTRY_SIZE;
}
