/*
===============================================================================
Shader script fuzzing harness for AFL/libFuzzer
===============================================================================
*/

#include "../src/common/q_shared.h"
#include "../src/renderercommon/tr_common.h"

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

// Mock shader parsing structures
typedef struct {
	char name[MAX_QPATH];
	char *text;
	int line;
} shader_t;

#ifdef USE_LIBFUZZER
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	// Basic bounds checking
	if (size == 0 || size > 100 * 1024) { // Limit to 100KB
		return 0;
	}

	// Create a null-terminated string from the input
	char *shader_text = (char *)malloc(size + 1);
	if (!shader_text) {
		return 0;
	}

	memcpy(shader_text, data, size);
	shader_text[size] = '\0';

	// Basic shader syntax validation
	char *ptr = shader_text;
	int brace_depth = 0;
	qboolean in_comment = qfalse;
	qboolean in_string = qfalse;

	while (*ptr) {
		char c = *ptr;

		if (in_comment) {
			if (c == '\n') {
				in_comment = qfalse;
			}
		} else if (in_string) {
			if (c == '"') {
				in_string = qfalse;
			} else if (c == '\\' && ptr[1]) {
				ptr++; // Skip escaped character
			}
		} else {
			if (c == '/' && ptr[1] == '/') {
				in_comment = qtrue;
				ptr++; // Skip second slash
			} else if (c == '"') {
				in_string = qtrue;
			} else if (c == '{') {
				brace_depth++;
				if (brace_depth > 10) { // Prevent excessive nesting
					free(shader_text);
					return 0;
				}
			} else if (c == '}') {
				brace_depth--;
				if (brace_depth < 0) {
					free(shader_text);
					return 0; // Unmatched closing brace
				}
			}
		}

		ptr++;
	}

	// Check for balanced braces
	if (brace_depth != 0) {
		free(shader_text);
		return 0;
	}

	// Try to parse basic shader structure
	char *line_start = shader_text;
	char *line_end;
	int line_count = 0;

	while ((line_end = strchr(line_start, '\n')) != NULL && line_count < 50) {
		*line_end = '\0';

		// Skip whitespace
		char *trimmed = line_start;
		while (*trimmed && (*trimmed == ' ' || *trimmed == '\t')) {
			trimmed++;
		}

		// Check for basic shader directives
		if (*trimmed) {
			if (Q_strncmp(trimmed, "surfaceparm", 11) == 0 ||
				Q_strncmp(trimmed, "cull", 4) == 0 ||
				Q_strncmp(trimmed, "deformvertexes", 14) == 0 ||
				Q_strncmp(trimmed, "fogparms", 8) == 0 ||
				Q_strncmp(trimmed, "polygonoffset", 13) == 0 ||
				Q_strncmp(trimmed, "portal", 6) == 0 ||
				Q_strncmp(trimmed, "skyparms", 8) == 0 ||
				Q_strncmp(trimmed, "sort", 4) == 0 ||
				Q_strncmp(trimmed, "tesssize", 8) == 0 ||
				Q_strncmp(trimmed, "clampmap", 8) == 0 ||
				Q_strncmp(trimmed, "map", 3) == 0 ||
				Q_strncmp(trimmed, "animmap", 7) == 0 ||
				Q_strncmp(trimmed, "blendfunc", 9) == 0 ||
				Q_strncmp(trimmed, "rgbgen", 6) == 0 ||
				Q_strncmp(trimmed, "alphagen", 8) == 0 ||
				Q_strncmp(trimmed, "tcgen", 5) == 0 ||
				Q_strncmp(trimmed, "tcmod", 5) == 0) {
				// Valid shader directive
			}
		}

		line_start = line_end + 1;
		line_count++;
	}

	free(shader_text);
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

	uint8_t buffer[100 * 1024]; // 100KB should be enough for most shaders
	ssize_t size = read(fd, buffer, sizeof(buffer));
	close(fd);

	if (size < 0) {
		perror("read");
		return 1;
	}

	return LLVMFuzzerTestOneInput(buffer, size);
}

#endif // USE_LIBFUZZER
