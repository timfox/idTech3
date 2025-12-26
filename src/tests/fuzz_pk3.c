/*
===============================================================================
PK3 archive fuzzing harness for AFL/libFuzzer
===============================================================================
*/

#include "../src/common/q_shared.h"
#include "../src/common/unzip.h"

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

void *Z_Malloc(int size) {
	return malloc(size);
}

void Z_Free(void *ptr) {
	free(ptr);
}

#ifdef USE_LIBFUZZER
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	// Basic bounds checking
	if (size < sizeof(zipHeader_t)) {
		return 0;
	}

	// Check if this looks like a ZIP file
	const zipHeader_t *header = (const zipHeader_t *)data;
	if (header->zipid != 0x04034b50) {
		return 0; // Not a ZIP file
	}

	// Try to parse the central directory
	size_t offset = 0;
	int file_count = 0;

	while (offset < size - sizeof(zipHeader_t) && file_count < 100) {
		const zipHeader_t *file_header = (const zipHeader_t *)(data + offset);

		if (file_header->zipid != 0x04034b50) {
			break; // No more files
		}

		// Validate file header
		if (file_header->compressed != 0 && file_header->compressed != 8) {
			return 0; // Unsupported compression method
		}

		// Check filename bounds
		size_t filename_end = offset + sizeof(zipHeader_t) + file_header->namelen;
		if (filename_end > size) {
			return 0; // Filename extends beyond buffer
		}

		// Check extra field bounds
		size_t extra_end = filename_end + file_header->extralen;
		if (extra_end > size) {
			return 0; // Extra field extends beyond buffer
		}

		// Check compressed data bounds
		size_t data_end = extra_end + file_header->compressed_size;
		if (data_end > size) {
			return 0; // Compressed data extends beyond buffer
		}

		// Validate filename (should not contain dangerous paths)
		const char *filename = (const char *)(data + offset + sizeof(zipHeader_t));
		if (file_header->namelen > 0) {
			if (strstr(filename, "..") != NULL ||
				strstr(filename, "\\") != NULL ||
				memchr(filename, '\0', file_header->namelen) != NULL) {
				return 0; // Dangerous or malformed filename
			}
		}

		// Move to next file header
		offset = data_end;
		file_count++;

		// Prevent infinite loops
		if (file_count > 1000) {
			return 0;
		}
	}

	// Try to find and validate central directory
	size_t central_dir_offset = size - sizeof(zipCentralHeader_t);
	while (central_dir_offset > 0) {
		const zipCentralHeader_t *central = (const zipCentralHeader_t *)(data + central_dir_offset);

		if (central->zipid == 0x02014b50) {
			// Found central directory entry
			if (central->namelen > 1000 || central->commentlen > 1000) {
				return 0; // Suspiciously large name/comment
			}

			size_t name_end = central_dir_offset + sizeof(zipCentralHeader_t) + central->namelen;
			size_t comment_end = name_end + central->commentlen;

			if (name_end > size || comment_end > size) {
				return 0; // Central directory entry extends beyond buffer
			}

			// Validate filename in central directory
			const char *filename = (const char *)(data + central_dir_offset + sizeof(zipCentralHeader_t));
			if (central->namelen > 0) {
				if (strstr(filename, "..") != NULL ||
					strstr(filename, "\\") != NULL ||
					memchr(filename, '\0', central->namelen) != NULL) {
					return 0; // Dangerous or malformed filename
				}
			}

			break; // Found valid central directory
		}

		central_dir_offset--;
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

	uint8_t buffer[1024 * 1024]; // 1MB should be enough for most PK3 files
	ssize_t size = read(fd, buffer, sizeof(buffer));
	close(fd);

	if (size < 0) {
		perror("read");
		return 1;
	}

	return LLVMFuzzerTestOneInput(buffer, size);
}

#endif // USE_LIBFUZZER
