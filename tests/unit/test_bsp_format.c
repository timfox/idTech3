#include "q_shared.h"
#include "qfiles.h"
#include "qfiles_bsp30.h"
#include "bsp_format.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef char assert_legacy_lump_size[(sizeof(lump_t) == 8) ? 1 : -1];
typedef char assert_ibsp46_header_size[(sizeof(dheader_t) == 144) ? 1 : -1];
typedef char assert_bsp30_lump_size[(sizeof(bsp30_lump_t) == 8) ? 1 : -1];
typedef char assert_bsp30_header_size[(sizeof(bsp30_header_t) == 124) ? 1 : -1];
typedef char assert_bsp30_node_size[(sizeof(bsp30_node_t) == 24) ? 1 : -1];
typedef char assert_bsp30_leaf_size[(sizeof(bsp30_leaf_t) == 28) ? 1 : -1];
typedef char assert_bsp30_face_size[(sizeof(bsp30_face_t) == 20) ? 1 : -1];

static void put16(uint8_t *p, uint16_t value) {
	p[0] = (uint8_t)value;
	p[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *p, uint32_t value) {
	p[0] = (uint8_t)value;
	p[1] = (uint8_t)(value >> 8);
	p[2] = (uint8_t)(value >> 16);
	p[3] = (uint8_t)(value >> 24);
}

static void put64(uint8_t *p, uint64_t value) {
	put32(p, (uint32_t)value);
	put32(p + 4, (uint32_t)(value >> 32));
}

static void test_checked_arithmetic(void) {
	uint64_t result = 0;
	assert(BSP_CheckedRange(100, 90, 10));
	assert(!BSP_CheckedRange(100, 91, 10));
	assert(!BSP_CheckedRange(UINT64_MAX, UINT64_MAX - 1, 3));
	assert(BSP_CheckedMultiply(1024, 64, &result));
	assert(result == 65536);
	assert(!BSP_CheckedMultiply(UINT64_MAX, 2, &result));
	assert(!BSP_CheckedMultiply(1, 1, NULL));
}

static void test_paths(void) {
	assert(BSP_IsSafeExternalPath("maps/example.xbsp.d/GX_LIGHTMAPS.a.bin"));
	assert(!BSP_IsSafeExternalPath("../secret"));
	assert(!BSP_IsSafeExternalPath("maps/../secret"));
	assert(!BSP_IsSafeExternalPath("/tmp/lump"));
	assert(!BSP_IsSafeExternalPath("C:/tmp/lump"));
	assert(!BSP_IsSafeExternalPath("maps//lump"));
}

static void make_bsp30(uint8_t *data, size_t size) {
	memset(data, 0, size);
	put32(data, 30);
}

static void make_ibsp(uint8_t *data, size_t size, uint32_t version) {
	memset(data, 0, size);
	memcpy(data, "IBSP", 4);
	put32(data + 4, version);
}

static void make_vbsp(uint8_t *data, size_t size, uint32_t version) {
	memset(data, 0, size);
	memcpy(data, "VBSP", 4);
	put32(data + 4, version);
}

static void make_dark_messiah_vbsp(uint8_t *data, size_t size) {
	memset(data, 0, size);
	memcpy(data, "VBSP", 4);
	put16(data + 4, 20);
	put16(data + 6, 4);
}

static void make_rbsp(uint8_t *data, size_t size, uint32_t version) {
	memset(data, 0, size);
	memcpy(data, "RBSP", 4);
	put32(data + 4, version);
}

static void test_legacy_detection(void) {
	uint8_t bsp30[124];
	uint8_t ibsp[144];
	bspError_t error;
	make_bsp30(bsp30, sizeof(bsp30));
	make_ibsp(ibsp, sizeof(ibsp), 46);
	assert(BSP_DetectFormat(bsp30, sizeof(bsp30), &error) ==
		BSP_FORMAT_GOLDSRC_BSP30);
	assert(BSP_DetectFormat(ibsp, sizeof(ibsp), &error) ==
		BSP_FORMAT_QUAKE3_IBSP46);
	put32(ibsp + 4, 47);
	assert(BSP_DetectFormat(ibsp, sizeof(ibsp), &error) ==
		BSP_FORMAT_QUAKE3_IBSP47);
	put32(ibsp + 4, 99);
	assert(BSP_DetectFormat(ibsp, sizeof(ibsp), &error) == BSP_FORMAT_UNKNOWN);
	assert(error == BSP_ERROR_VERSION);
}

static void test_source_family_detection(void) {
	uint8_t vbsp[1036];
	bspError_t error;
	const bspFormatDescriptor_t *descriptor;

	make_vbsp(vbsp, sizeof(vbsp), 20);
	assert(BSP_DetectFormat(vbsp, sizeof(vbsp), &error) ==
		BSP_FORMAT_SOURCE_VBSP);
	descriptor = BSP_FormatDescriptor(BSP_FORMAT_SOURCE_VBSP);
	assert(descriptor && descriptor->lumpCount == 64);
	assert(descriptor->lumpLayout == BSP_LUMP_LAYOUT_OFFSET_LENGTH_VERSION_FOURCC);

	make_vbsp(vbsp, sizeof(vbsp), 21);
	assert(BSP_DetectFormat(vbsp, sizeof(vbsp), &error) ==
		BSP_FORMAT_SOURCE_VBSP_L4D2);
	descriptor = BSP_FormatDescriptor(BSP_FORMAT_SOURCE_VBSP_L4D2);
	assert(descriptor && descriptor->lumpLayout == BSP_LUMP_LAYOUT_VERSION_OFFSET_LENGTH_FOURCC);

	make_dark_messiah_vbsp(vbsp, sizeof(vbsp));
	assert(BSP_DetectFormat(vbsp, sizeof(vbsp), &error) ==
		BSP_FORMAT_SOURCE_VBSP_DARK_MESSIAH);
}

static void test_respawn_family_detection(void) {
	uint8_t rbsp[1036];
	bspError_t error;

	make_rbsp(rbsp, sizeof(rbsp), 29);
	assert(BSP_DetectFormat(rbsp, sizeof(rbsp), &error) ==
		BSP_FORMAT_RESPAWN_RBSP);
	make_rbsp(rbsp, sizeof(rbsp), 36);
	assert(BSP_DetectFormat(rbsp, sizeof(rbsp), &error) ==
		BSP_FORMAT_RESPAWN_RBSP2);
	make_rbsp(rbsp, sizeof(rbsp), 47);
	assert(BSP_DetectFormat(rbsp, sizeof(rbsp), &error) ==
		BSP_FORMAT_APEX_RBSP);
	make_rbsp(rbsp, sizeof(rbsp), 99);
	assert(BSP_DetectFormat(rbsp, sizeof(rbsp), &error) == BSP_FORMAT_UNKNOWN);
	assert(error == BSP_ERROR_VERSION);
}

static void test_bspx(void) {
	uint8_t file[180];
	bspxDirectory_t directory;
	bspError_t error;
	const bspLumpView_t *lump;
	make_bsp30(file, sizeof(file));
	memcpy(file + 124, "BSPX", 4);
	put32(file + 128, 1);
	memcpy(file + 132, "GX_META", 7);
	put32(file + 156, 164);
	put32(file + 160, 16);
	memset(file + 164, 0x5a, 16);
	assert(BSPX_ReadDirectory(file, sizeof(file), BSP_FORMAT_GOLDSRC_BSP30,
		&directory, &error));
	assert(directory.count == 1);
	lump = BSPX_FindLump(&directory, "GX_META");
	assert(lump && lump->storedSize == 16 && lump->data[0] == 0x5a);
	assert(BSP_DetectFormat(file, sizeof(file), &error) ==
		BSP_FORMAT_BSP30_WITH_BSPX);

	memcpy(file + 164, "BSPX", 4); /* trailing bytes never confuse detection */
	assert(BSP_DetectFormat(file, sizeof(file), &error) ==
		BSP_FORMAT_BSP30_WITH_BSPX);

	put32(file + 128, BSPX_MAX_LUMPS + 1);
	assert(!BSPX_ReadDirectory(file, sizeof(file), BSP_FORMAT_GOLDSRC_BSP30,
		&directory, &error));
	assert(error == BSP_ERROR_COUNT);
}

static void test_bspx_duplicate_and_overlap(void) {
	uint8_t file[228];
	bspxDirectory_t directory;
	bspError_t error;
	make_bsp30(file, sizeof(file));
	memcpy(file + 124, "BSPX", 4);
	put32(file + 128, 2);
	memcpy(file + 132, "GX_META", 7);
	put32(file + 156, 196);
	put32(file + 160, 16);
	memcpy(file + 164, "GX_META", 7);
	put32(file + 188, 212);
	put32(file + 192, 16);
	assert(!BSPX_ReadDirectory(file, sizeof(file), BSP_FORMAT_GOLDSRC_BSP30,
		&directory, &error));
	assert(error == BSP_ERROR_DUPLICATE);

	memcpy(file + 164, "GX_NODES", 8);
	put32(file + 188, 204);
	put32(file + 192, 16);
	assert(!BSPX_ReadDirectory(file, sizeof(file), BSP_FORMAT_GOLDSRC_BSP30,
		&directory, &error));
	assert(error == BSP_ERROR_DIRECTORY);
}

static void test_bspx_serialization(void) {
	uint8_t bytes[40];
	bspLumpView_t lump;
	memset(bytes, 0xcc, sizeof(bytes));
	memset(&lump, 0, sizeof(lump));
	strcpy(lump.name, "VENDOR_UNKNOWN");
	lump.offset = 0x12345678u;
	lump.storedSize = 0x10203040u;
	assert(BSPX_WriteHeader(bytes, sizeof(bytes), 1) == 8);
	assert(memcmp(bytes, "BSPX", 4) == 0 && bytes[4] == 1);
	assert(BSPX_WriteDirectoryEntry(bytes + 8, sizeof(bytes) - 8, &lump) == 32);
	assert(memcmp(bytes + 8, "VENDOR_UNKNOWN", 14) == 0);
	assert(bytes[32] == 0x78 && bytes[35] == 0x12);
	assert(bytes[36] == 0x40 && bytes[39] == 0x10);
	lump.offset = UINT64_C(1) << 32;
	assert(BSPX_WriteDirectoryEntry(bytes + 8, sizeof(bytes) - 8, &lump) == 0);
}

static void test_xbsp(void) {
	uint8_t file[180];
	xbspDirectory_t source;
	xbspDirectory_t parsed;
	bspLumpView_t entry;
	bspError_t error;
	memset(&source, 0, sizeof(source));
	memset(&entry, 0, sizeof(entry));
	memset(file, 0, sizeof(file));
	source.flags = 3;
	source.directoryOffset = XBSP_HEADER_SIZE;
	source.directoryCount = 1;
	source.fileSize = sizeof(file);
	source.fileHash = UINT64_C(0x1122334455667788);
	source.compatibilityBaseHash = UINT64_C(0x8877665544332211);
	strcpy(entry.name, "GX_NODES");
	entry.schemaVersion = 1;
	entry.offset = 176;
	entry.storedSize = 4;
	entry.uncompressedSize = 4;
	entry.elementCount = 1;
	entry.contentHash = 9;
	assert(XBSP_WriteHeader(file, sizeof(file), &source) == XBSP_HEADER_SIZE);
	assert(XBSP_WriteDirectoryEntry(file + XBSP_HEADER_SIZE,
		sizeof(file) - XBSP_HEADER_SIZE, &entry) == XBSP_DIRECTORY_ENTRY_SIZE);
	memcpy(file + 176, "node", 4);
	assert(XBSP_ReadDirectory(file, sizeof(file), &parsed, &error));
	assert(parsed.directoryCount == 1);
	assert(parsed.lumps[0].offset == 176);
	assert(parsed.fileHash == source.fileHash);
	assert(BSP_DetectFormat(file, sizeof(file), &error) ==
		BSP_FORMAT_NATIVE_XBSP);

	put64(file + XBSP_HEADER_SIZE + 32, UINT64_MAX - 1);
	assert(!XBSP_ReadDirectory(file, sizeof(file), &parsed, &error));
	assert(error == BSP_ERROR_RANGE);
}

static void test_xbsp_64bit_serialization(void) {
	uint8_t entryBytes[XBSP_DIRECTORY_ENTRY_SIZE];
	bspLumpView_t entry;
	memset(&entry, 0, sizeof(entry));
	strcpy(entry.name, "GX_VIS");
	entry.schemaVersion = 1;
	entry.offset = UINT64_C(0x0000000200000010);
	entry.storedSize = UINT64_C(0x0000000100000020);
	entry.uncompressedSize = UINT64_C(0x0000000300000040);
	assert(XBSP_WriteDirectoryEntry(entryBytes, sizeof(entryBytes), &entry) ==
		sizeof(entryBytes));
	assert(entryBytes[36] == 2);
	assert(entryBytes[44] == 1);
	assert(entryBytes[52] == 3);
}

static void test_gx_payload_and_registry(void) {
	uint8_t bytes[56];
	bspLumpView_t lump;
	gxPayloadInfo_t payload;
	bspError_t error;
	size_t count = 0;
	memset(bytes, 0, sizeof(bytes));
	memset(&lump, 0, sizeof(lump));
	put32(bytes, GX_LUMP_PAYLOAD_MAGIC);
	put16(bytes + 4, 1);
	put16(bytes + 6, GX_LUMP_PAYLOAD_HEADER_SIZE);
	put64(bytes + 16, 2);
	put64(bytes + 24, 8);
	put64(bytes + 32, 8);
	put64(bytes + 40, 0x1234);
	lump.data = bytes;
	lump.storedSize = sizeof(bytes);
	assert(GX_ReadPayloadHeader(&lump, &payload, &error));
	assert(payload.elementCount == 2 && payload.storedSize == 8);
	assert(BSP_LumpRegistry(&count) != NULL && count >= 27);
	assert(BSP_FindLumpSchema("GX_LEAVES", 1) != NULL);
	assert(BSP_FindLumpSchema("GX_LEAVES", 2) == NULL);
	assert(BSP_FindLumpSchema("UNKNOWN", 1) == NULL);
}

int main(void) {
	test_checked_arithmetic();
	test_paths();
	test_legacy_detection();
	test_source_family_detection();
	test_respawn_family_detection();
	test_bspx();
	test_bspx_duplicate_and_overlap();
	test_bspx_serialization();
	test_xbsp();
	test_xbsp_64bit_serialization();
	test_gx_payload_and_registry();
	puts("bsp_format: all tests passed");
	return 0;
}
