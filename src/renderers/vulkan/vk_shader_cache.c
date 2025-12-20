/*
=============================================================================
Vulkan Shader Cache System

Provides persistent caching of compiled shaders to improve loading times.
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk.h"
#include "vk_shader_cache.h"

#ifdef USE_VULKAN

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define SHADER_CACHE_VERSION 1
#define SHADER_CACHE_MAGIC "Q3VKSC"
#define MAX_SHADER_CACHE_ENTRIES 1024

typedef struct {
	char magic[8]; // "Q3VKSC"
	uint32_t version;
	uint32_t num_entries;
	uint64_t device_id; // To ensure cache is device-specific
} shader_cache_header_t;

typedef struct {
	uint32_t name_hash; // Hash of shader name for quick lookup
	uint32_t spirv_size;
	uint64_t last_modified; // File timestamp
	uint32_t crc32; // CRC32 of SPIR-V data for validation
} shader_cache_entry_t;

qboolean shader_cache_enabled = qfalse;
static char shader_cache_path[MAX_OSPATH];
static int shader_cache_fd = -1;

/*
=============================================================================
Shader Cache Utilities
=============================================================================
*/

static uint32_t calculate_crc32(const void *data, size_t size) {
	uint32_t crc = 0xFFFFFFFF;
	const uint8_t *bytes = (const uint8_t *)data;

	for (size_t i = 0; i < size; i++) {
		crc ^= bytes[i];
		for (int j = 0; j < 8; j++) {
			crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
		}
	}

	return crc ^ 0xFFFFFFFF;
}

static uint32_t hash_string(const char *str) {
	uint32_t hash = 5381;
	int c;

	while ((c = *str++)) {
		hash = ((hash << 5) + hash) + c; // hash * 33 + c
	}

	return hash;
}

qboolean create_shader_cache_directory(void) {
	char cache_dir[MAX_OSPATH];

	Com_sprintf(cache_dir, sizeof(cache_dir), "%s/shader_cache", Sys_DefaultBasePath());

#ifdef _WIN32
	CreateDirectoryA(cache_dir, NULL);
	return qtrue;
#else
	char cmd[MAX_OSPATH * 2];
	Com_sprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", cache_dir);
	return system(cmd) == 0;
#endif
}

/*
=============================================================================
Shader Cache Initialization
=============================================================================
*/

qboolean vk_shader_cache_init(void) {
	// Check if shader caching is enabled
	cvar_t *r_vk_shaderCache = ri.Cvar_Get("r_vk_shaderCache", "1", CVAR_ARCHIVE);
	if (!r_vk_shaderCache->integer) {
		return qtrue;
	}

	// Get device properties for cache validation
	VkPhysicalDeviceProperties props;
	qvkGetPhysicalDeviceProperties(vk.physical_device, &props);

	// Create cache directory
	if (!create_shader_cache_directory()) {
		ri.Printf(PRINT_WARNING, "Vulkan: Failed to create shader cache directory\n");
		return qfalse;
	}

	// Create cache file path
	Com_sprintf(shader_cache_path, sizeof(shader_cache_path), "%s/shader_cache/vulkan_%08X.cache",
		Sys_DefaultBasePath(), (uint32_t)props.deviceID);

	// Try to open existing cache file
	shader_cache_fd = open(shader_cache_path, O_RDWR | O_CREAT, 0644);
	if (shader_cache_fd < 0) {
		ri.Printf(PRINT_WARNING, "Vulkan: Failed to open shader cache file: %s\n", shader_cache_path);
		return qfalse;
	}

	// Check if cache file is valid
	shader_cache_header_t header;
	ssize_t bytes_read = read(shader_cache_fd, &header, sizeof(header));

	if (bytes_read == sizeof(header) &&
		memcmp(header.magic, SHADER_CACHE_MAGIC, sizeof(header.magic)) == 0 &&
		header.version == SHADER_CACHE_VERSION &&
		header.device_id == props.deviceID) {

		ri.Printf(PRINT_ALL, "Vulkan: Loaded shader cache with %u entries\n", header.num_entries);
		shader_cache_enabled = qtrue;
	} else {
		// Invalid or missing cache, initialize new one
		memset(&header, 0, sizeof(header));
		Q_strncpyz(header.magic, SHADER_CACHE_MAGIC, sizeof(header.magic));
		header.version = SHADER_CACHE_VERSION;
		header.num_entries = 0;
		header.device_id = props.deviceID;

		lseek(shader_cache_fd, 0, SEEK_SET);
		if (write(shader_cache_fd, &header, sizeof(header)) != sizeof(header)) {
			close(shader_cache_fd);
			shader_cache_fd = -1;
			ri.Printf(PRINT_WARNING, "Vulkan: Failed to initialize shader cache\n");
			return qfalse;
		}

		ri.Printf(PRINT_ALL, "Vulkan: Created new shader cache\n");
		shader_cache_enabled = qtrue;
	}

	return qtrue;
}

/*
=============================================================================
Shader Cache Operations
=============================================================================
*/

qboolean vk_shader_cache_get(const char *shader_name, void **spirv_data, size_t *spirv_size) {
	if (!shader_cache_enabled || shader_cache_fd < 0) {
		return qfalse;
	}

	uint32_t name_hash = hash_string(shader_name);
	shader_cache_header_t header;

	// Read header
	lseek(shader_cache_fd, 0, SEEK_SET);
	if (read(shader_cache_fd, &header, sizeof(header)) != sizeof(header)) {
		return qfalse;
	}

	// Search for entry
	for (uint32_t i = 0; i < header.num_entries; i++) {
		shader_cache_entry_t entry;

		if (read(shader_cache_fd, &entry, sizeof(entry)) != sizeof(entry)) {
			break;
		}

		if (entry.name_hash == name_hash) {
			// Check if SPIR-V data is still available
			if (entry.spirv_size == 0) {
				break;
			}

			*spirv_data = ri.Malloc(entry.spirv_size);
			if (!*spirv_data) {
				return qfalse;
			}

			if (read(shader_cache_fd, *spirv_data, entry.spirv_size) != (ssize_t)entry.spirv_size) {
				ri.Free(*spirv_data);
				return qfalse;
			}

			// Validate CRC32
			uint32_t computed_crc = calculate_crc32(*spirv_data, entry.spirv_size);
			if (computed_crc != entry.crc32) {
				ri.Printf(PRINT_WARNING, "Vulkan: Shader cache entry corrupted for %s\n", shader_name);
				ri.Free(*spirv_data);
				return qfalse;
			}

			*spirv_size = entry.spirv_size;
			ri.Printf(PRINT_ALL, "Vulkan: Loaded cached shader: %s (%zu bytes)\n", shader_name, *spirv_size);
			return qtrue;
		}

		// Skip SPIR-V data
		lseek(shader_cache_fd, entry.spirv_size, SEEK_CUR);
	}

	return qfalse;
}

qboolean vk_shader_cache_put(const char *shader_name, const void *spirv_data, size_t spirv_size) {
	if (!shader_cache_enabled || shader_cache_fd < 0 || !spirv_data || spirv_size == 0) {
		return qfalse;
	}

	uint32_t name_hash = hash_string(shader_name);
	uint32_t crc32 = calculate_crc32(spirv_data, spirv_size);

	shader_cache_header_t header;

	// Read current header
	lseek(shader_cache_fd, 0, SEEK_SET);
	if (read(shader_cache_fd, &header, sizeof(header)) != sizeof(header)) {
		return qfalse;
	}

	// Check if we already have this entry
	qboolean entry_exists = qfalse;
	off_t entry_offset = sizeof(header);

	for (uint32_t i = 0; i < header.num_entries; i++) {
		shader_cache_entry_t entry;

		if (read(shader_cache_fd, &entry, sizeof(entry)) != sizeof(entry)) {
			break;
		}

		if (entry.name_hash == name_hash) {
			entry_exists = qtrue;
			break;
		}

		entry_offset = lseek(shader_cache_fd, 0, SEEK_CUR) + entry.spirv_size;
	}

	if (entry_exists) {
		// Update existing entry
		lseek(shader_cache_fd, entry_offset, SEEK_SET);
	} else {
		// Add new entry at the end
		if (header.num_entries >= MAX_SHADER_CACHE_ENTRIES) {
			ri.Printf(PRINT_WARNING, "Vulkan: Shader cache full, cannot add %s\n", shader_name);
			return qfalse;
		}

		lseek(shader_cache_fd, 0, SEEK_END);
		header.num_entries++;
	}

	// Write entry
	shader_cache_entry_t entry = {};
	entry.name_hash = name_hash;
	entry.spirv_size = spirv_size;
	entry.last_modified = ri.Milliseconds(); // Simple timestamp
	entry.crc32 = crc32;

	if (write(shader_cache_fd, &entry, sizeof(entry)) != sizeof(entry) ||
		write(shader_cache_fd, spirv_data, spirv_size) != (ssize_t)spirv_size) {
		ri.Printf(PRINT_WARNING, "Vulkan: Failed to write shader cache entry\n");
		return qfalse;
	}

	// Update header
	lseek(shader_cache_fd, 0, SEEK_SET);
	if (write(shader_cache_fd, &header, sizeof(header)) != sizeof(header)) {
		ri.Printf(PRINT_WARNING, "Vulkan: Failed to write shader cache header\n");
		return qfalse;
	}

	// Ensure data is written to disk
	fsync(shader_cache_fd);

	ri.Printf(PRINT_ALL, "Vulkan: Cached shader: %s (%zu bytes)\n", shader_name, spirv_size);
	return qtrue;
}

/*
=============================================================================
Shader Cache Shutdown
=============================================================================
*/

void vk_shader_cache_shutdown(void) {
	if (shader_cache_fd >= 0) {
		close(shader_cache_fd);
		shader_cache_fd = -1;
	}

	shader_cache_enabled = qfalse;
}

#endif // USE_VULKAN
