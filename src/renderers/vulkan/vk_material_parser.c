/*
=============================================================================
Material File Parser Implementation
.mat file parser
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk_material_parser.h"
#include "vk_material_system.h"

#ifdef USE_VULKAN

#include <string.h>
#include <stdlib.h>

static materialParser_t materialParser;

// Property name mapping
static const struct {
    const char* name;
    materialPropertyType_t type;
} propertyMap[] = {
    {"texture_base", MATERIAL_PROP_TEXTURE_BASE},
    {"texture_normals", MATERIAL_PROP_TEXTURE_NORMALS},
    {"texture_emissive", MATERIAL_PROP_TEXTURE_EMISSIVE},
    {"roughness_override", MATERIAL_PROP_ROUGHNESS_OVERRIDE},
    {"metallic", MATERIAL_PROP_METALLIC},
    {"emissive_factor", MATERIAL_PROP_EMISSIVE_FACTOR},
    {"kind", MATERIAL_PROP_KIND},
    {"is_light", MATERIAL_PROP_IS_LIGHT},
    {"correct_albedo", MATERIAL_PROP_CORRECT_ALBEDO},
    {"synth_emissive", MATERIAL_PROP_SYNTH_EMISSIVE},
    {"emissive_threshold", MATERIAL_PROP_EMISSIVE_THRESHOLD},
    {"bump_scale", MATERIAL_PROP_BUMP_SCALE},
    {"base_factor", MATERIAL_PROP_BASE_FACTOR},
};

// Helper function to trim whitespace
static char* trim_whitespace(char* str) {
    char* end;

    // Trim leading space
    while (*str == ' ' || *str == '\t') str++;

    if (*str == 0) return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;

    // Write new null terminator
    *(end + 1) = 0;

    return str;
}

// Parse a single material property line
static qboolean parse_material_property(const char* line, materialProperty_t* prop) {
    char key[MAX_MATERIAL_NAME];
    char value[MAX_MATERIAL_NAME];

    // Parse "key value" format
    if (sscanf(line, "%s %[^\n]", key, value) != 2) {
        return qfalse;
    }

    // Find property type
    for (size_t i = 0; i < ARRAY_LEN(propertyMap); i++) {
        if (strcmp(key, propertyMap[i].name) == 0) {
            prop->type = propertyMap[i].type;

            // Parse value based on type
            if (prop->type == MATERIAL_PROP_TEXTURE_BASE ||
                prop->type == MATERIAL_PROP_TEXTURE_NORMALS ||
                prop->type == MATERIAL_PROP_TEXTURE_EMISSIVE ||
                prop->type == MATERIAL_PROP_KIND) {
                // String value
                Q_strncpyz(prop->value.stringValue, value, sizeof(prop->value.stringValue));
            } else if (prop->type == MATERIAL_PROP_ROUGHNESS_OVERRIDE ||
                       prop->type == MATERIAL_PROP_METALLIC ||
                       prop->type == MATERIAL_PROP_EMISSIVE_FACTOR ||
                       prop->type == MATERIAL_PROP_EMISSIVE_THRESHOLD ||
                       prop->type == MATERIAL_PROP_BUMP_SCALE ||
                       prop->type == MATERIAL_PROP_BASE_FACTOR) {
                // Float value
                prop->value.floatValue = atof(value);
            } else {
                // Integer value
                prop->value.intValue = atoi(value);
            }

            return qtrue;
        }
    }

    // Unknown property
    return qfalse;
}

// Parse material entries from a file
static void parse_material_file(materialFile_t* file) {
    char* buffer;
    int len = ri.FS_ReadFile(file->filename, (void**)&buffer);

    if (len < 0 || !buffer) {
        ri.Printf(PRINT_DEVELOPER, "Failed to read material file: %s\n", file->filename);
        return;
    }

    char* ptr = buffer;
    char line[1024];
    materialEntry_t* currentEntry = NULL;
    int lineNum = 0;

    while (*ptr && file->numEntries < MAX_MATERIAL_ENTRIES) {
        // Read line
        char* lineEnd = strchr(ptr, '\n');
		if (!lineEnd) {
			// Last line
			Q_strncpyz(line, ptr, sizeof(line));
			ptr += strlen(ptr);
		} else {
			size_t lineLen = (size_t)(lineEnd - ptr);
			if (lineLen >= sizeof(line)) lineLen = sizeof(line) - 1;
			memcpy(line, ptr, lineLen);
			line[lineLen] = 0;
			ptr = lineEnd + 1;
		}

        lineNum++;
        char* trimmed = trim_whitespace(line);

        // Skip empty lines and comments
        if (!*trimmed || *trimmed == '#' || *trimmed == '/' || *trimmed == ';') {
            continue;
        }

        // Check if this is a material definition (ends with ':')
        char* colon = strchr(trimmed, ':');
        if (colon) {
            // New material entry
            *colon = 0; // Remove colon
            char* textureNames = trim_whitespace(trimmed);

            if (*textureNames) {
                currentEntry = &file->entries[file->numEntries++];
                Q_strncpyz(currentEntry->textureNames, textureNames, sizeof(currentEntry->textureNames));
                currentEntry->numProperties = 0;
            }
        } else if (currentEntry && *trimmed) {
            // Property for current material
            materialProperty_t prop;
            if (parse_material_property(trimmed, &prop) &&
                currentEntry->numProperties < MAX_MATERIAL_PROPERTIES) {
                currentEntry->properties[currentEntry->numProperties++] = prop;
            }
        }
    }

    ri.FS_FreeFile(buffer);

    ri.Printf(PRINT_DEVELOPER, "Parsed %d material entries from %s\n", file->numEntries, file->filename);
}

// Load all material files
void vk_material_parser_load_files(void) {
	char filename[MAX_QPATH];
	char** fileList;
	int numFiles;

	// Load global material files from materials/ directory
	fileList = ri.FS_ListFiles("materials", ".mat", &numFiles);
	if (fileList) {
		for (int i = 0; i < numFiles && materialParser.numFiles < MAX_MATERIAL_FILES; i++) {
			Com_sprintf(filename, sizeof(filename), "materials/%s", fileList[i]);
			Q_strncpyz(materialParser.files[materialParser.numFiles].filename, filename,
					   sizeof(materialParser.files[materialParser.numFiles].filename));
			parse_material_file(&materialParser.files[materialParser.numFiles]);
			materialParser.numFiles++;
		}
		ri.FS_FreeFileList(fileList);
	}

	ri.Printf(PRINT_ALL, "Loaded %d material files with %d total entries\n",
			  materialParser.numFiles,
			  materialParser.numFiles > 0 ? materialParser.files[0].numEntries : 0);
}

// Find material entry by texture name
const materialEntry_t* vk_material_parser_find_entry(const char* textureName) {
    for (int fileIdx = 0; fileIdx < materialParser.numFiles; fileIdx++) {
        materialFile_t* file = &materialParser.files[fileIdx];

        for (int entryIdx = 0; entryIdx < file->numEntries; entryIdx++) {
            materialEntry_t* entry = &file->entries[entryIdx];

            // Check if texture name matches any in the comma-separated list
            char tempNames[MAX_MATERIAL_NAME];
            Q_strncpyz(tempNames, entry->textureNames, sizeof(tempNames));

            char* token = strtok(tempNames, ",");
            while (token) {
                char* trimmedToken = trim_whitespace(token);

                // Support wildcard matching (* replaced with texture name)
                if (strchr(trimmedToken, '*')) {
                    // Simple wildcard replacement - * becomes texture name
                    char pattern[MAX_MATERIAL_NAME];
                    char* starPos = strchr(trimmedToken, '*');
                    int prefixLen = starPos - trimmedToken;

                    memcpy(pattern, trimmedToken, prefixLen);
                    pattern[prefixLen] = 0;

                    char* suffix = starPos + 1;
                    char fullPattern[MAX_MATERIAL_NAME];
                    Com_sprintf(fullPattern, sizeof(fullPattern), "%s%s%s", pattern, textureName, suffix);

                    if (strcmp(fullPattern, textureName) == 0) {
                        return entry;
                    }
                } else if (strcmp(trimmedToken, textureName) == 0) {
                    return entry;
                }

                token = strtok(NULL, ",");
            }
        }
    }

    return NULL;
}

// Apply material entry properties to material parameters
void vk_material_parser_apply_to_material(const materialEntry_t* entry, const char* materialName, material_params_t* params) {
    if (!entry || !params) return;

    // Apply each property
    for (int i = 0; i < entry->numProperties; i++) {
        const materialProperty_t* prop = &entry->properties[i];

        switch (prop->type) {
            case MATERIAL_PROP_ROUGHNESS_OVERRIDE:
                params->roughness = prop->value.floatValue;
                break;

            case MATERIAL_PROP_METALLIC:
                params->metallic = prop->value.floatValue;
                break;

            case MATERIAL_PROP_EMISSIVE_FACTOR:
                params->emissive[0] = prop->value.floatValue;
                params->emissive[1] = prop->value.floatValue;
                params->emissive[2] = prop->value.floatValue;
                params->flags |= MATERIAL_EMISSIVE;
                break;

            case MATERIAL_PROP_IS_LIGHT:
                if (prop->value.intValue) {
                    params->flags |= MATERIAL_EMISSIVE;
                }
                break;

            case MATERIAL_PROP_EMISSIVE_THRESHOLD:
                // This would be used for automatic emissive texture generation
                // For now, we'll store it in a comment or extension field
                break;

            case MATERIAL_PROP_BUMP_SCALE:
                params->normalScale = prop->value.floatValue;
                break;

            case MATERIAL_PROP_BASE_FACTOR:
                params->baseColor[0] *= prop->value.floatValue;
                params->baseColor[1] *= prop->value.floatValue;
                params->baseColor[2] *= prop->value.floatValue;
                break;

            default:
                // Other properties handled elsewhere (texture overrides)
                break;
        }
    }

    ri.Printf(PRINT_DEVELOPER, "Applied material properties to %s\n", materialName);
}

void vk_material_parser_init(void) {
    memset(&materialParser, 0, sizeof(materialParser));
    materialParser.initialized = qtrue;

    ri.Printf(PRINT_DEVELOPER, "Material parser initialized\n");
}

void vk_material_parser_shutdown(void) {
    memset(&materialParser, 0, sizeof(materialParser));
    materialParser.initialized = qfalse;

    ri.Printf(PRINT_DEVELOPER, "Material parser shutdown\n");
}

// Apply material entry properties to a shader stage
void vk_material_parser_apply_to_shader_stage(const materialEntry_t* entry, shaderStage_t* stage) {
    if (!entry || !stage) return;

    // Apply each property to the shader stage
    for (int i = 0; i < entry->numProperties; i++) {
        const materialProperty_t* prop = &entry->properties[i];

        switch (prop->type) {
            case MATERIAL_PROP_TEXTURE_BASE:
                // This would override the base texture - for now, just log
                ri.Printf(PRINT_DEVELOPER, "Material file specifies texture_base override: %s\n", prop->value.stringValue);
                break;

            case MATERIAL_PROP_TEXTURE_NORMALS:
                // This would override the normal texture - for now, just log
                ri.Printf(PRINT_DEVELOPER, "Material file specifies texture_normals override: %s\n", prop->value.stringValue);
                break;

            case MATERIAL_PROP_TEXTURE_EMISSIVE:
                // This would override the emissive texture - for now, just log
                ri.Printf(PRINT_DEVELOPER, "Material file specifies texture_emissive override: %s\n", prop->value.stringValue);
                break;

            case MATERIAL_PROP_ROUGHNESS_OVERRIDE:
                // Apply roughness to specular scale (green channel)
                stage->specularScale[1] = prop->value.floatValue;
                ri.Printf(PRINT_DEVELOPER, "Applied roughness override: %.2f\n", prop->value.floatValue);
                break;

            case MATERIAL_PROP_METALLIC:
                // Apply metallic to specular scale (blue channel)
                stage->specularScale[2] = prop->value.floatValue;
                ri.Printf(PRINT_DEVELOPER, "Applied metallic: %.2f\n", prop->value.floatValue);
                break;

            case MATERIAL_PROP_EMISSIVE_FACTOR:
                // Apply emissive factor (red channel of specular scale for emissive)
                stage->specularScale[0] = prop->value.floatValue;
                ri.Printf(PRINT_DEVELOPER, "Applied emissive factor: %.2f\n", prop->value.floatValue);
                break;

            case MATERIAL_PROP_IS_LIGHT:
                if (prop->value.intValue) {
                    // Mark as emissive
                    stage->specularScale[0] = 1.0f; // Enable emissive
                    ri.Printf(PRINT_DEVELOPER, "Marked as light source\n");
                }
                break;

            default:
                ri.Printf(PRINT_DEVELOPER, "Unhandled material property type: %d\n", prop->type);
                break;
        }
    }
}

#endif // USE_VULKAN
