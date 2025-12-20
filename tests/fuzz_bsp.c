/*
===============================================================================
BSP file fuzzing harness for AFL/libFuzzer
===============================================================================
*/

#include "../src/qcommon/q_shared.h"
#include "../src/qcommon/bsp.h"

// Mock implementations for fuzzing
void Com_Error(errorParm_t level, const char *error, ...) {
	(void)level;
	// Don't exit in fuzzing mode - just return
}

void Com_Printf(const char *fmt, ...) {
	(void)fmt;
	// Silent in fuzzing mode
}

void Com_DPrintf(const char *fmt, ...) {
	(void)fmt;
	// Silent in fuzzing mode
}

void *Hunk_Alloc(int size, ha_pref preference) {
	(void)preference;
	return malloc(size);
}

void Hunk_Free(void *ptr) {
	free(ptr);
}

#ifdef USE_LIBFUZZER
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	// Basic bounds checking
	if (size < sizeof(dheader_t)) {
		return 0;
	}

	// Check BSP header
	const dheader_t *header = (const dheader_t *)data;
	if (header->ident != BSP_IDENT) {
		return 0; // Not a BSP file
	}

	if (header->version != BSP_VERSION) {
		return 0; // Wrong version
	}

	// Verify lump offsets are within bounds
	for (int i = 0; i < HEADER_LUMPS; i++) {
		const lump_t *lump = &header->lumps[i];
		size_t lump_end = lump->fileofs + lump->filelen;

		if (lump_end > size) {
			return 0; // Lump extends beyond file
		}

		// Basic lump size sanity checks
		switch (i) {
		case LUMP_SHADERS:
			if (lump->filelen % sizeof(dshader_t) != 0) {
				return 0; // Invalid shader lump size
			}
			break;
		case LUMP_PLANES:
			if (lump->filelen % sizeof(dplane_t) != 0) {
				return 0; // Invalid planes lump size
			}
			break;
		case LUMP_BRUSHES:
			if (lump->filelen % sizeof(dbrush_t) != 0) {
				return 0; // Invalid brushes lump size
			}
			break;
		case LUMP_BRUSHSIDES:
			if (lump->filelen % sizeof(dbrushside_t) != 0) {
				return 0; // Invalid brushsides lump size
			}
			break;
		case LUMP_MODELS:
			if (lump->filelen % sizeof(dmodel_t) != 0) {
				return 0; // Invalid models lump size
			}
			break;
		default:
			// Other lumps are variable size or don't need validation here
			break;
		}
	}

	// Try to access some lump data to test parsing
	const lump_t *entities_lump = &header->lumps[LUMP_ENTITIES];
	if (entities_lump->filelen > 0 && entities_lump->fileofs + entities_lump->filelen <= size) {
		const char *entities = (const char *)data + entities_lump->fileofs;
		// Basic entity string validation - should be null-terminated
		size_t entity_len = strnlen(entities, entities_lump->filelen);
		if (entity_len >= entities_lump->filelen && entities_lump->filelen > 0) {
			return 0; // Entity string not properly terminated
		}
	}

	// Test shader lump parsing
	const lump_t *shaders_lump = &header->lumps[LUMP_SHADERS];
	if (shaders_lump->filelen > 0 && shaders_lump->fileofs + shaders_lump->filelen <= size) {
		int num_shaders = shaders_lump->filelen / sizeof(dshader_t);
		const dshader_t *shaders = (const dshader_t *)(data + shaders_lump->fileofs);

		// Validate shader names are null-terminated
		for (int i = 0; i < num_shaders && i < 100; i++) { // Limit to prevent infinite loops
			size_t name_len = strnlen(shaders[i].shader, sizeof(shaders[i].shader));
			if (name_len >= sizeof(shaders[i].shader)) {
				return 0; // Shader name not properly terminated
			}
		}
	}

	return 0;
}

#else // AFL mode

#include <unistd.h>
#include <fcntl.h>

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
		return 1;
	}

	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	uint8_t buffer[1024 * 1024]; // 1MB should be enough for most BSPs
	ssize_t size = read(fd, buffer, sizeof(buffer));
	close(fd);

	if (size < 0) {
		perror("read");
		return 1;
	}

	return LLVMFuzzerTestOneInput(buffer, size);
}

#endif // USE_LIBFUZZER
