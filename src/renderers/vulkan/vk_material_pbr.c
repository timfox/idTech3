/*
=============================================================================
id Tech 3 - Material System

Ports a .mat file material system to id Tech 3.
Adapts Q2 material format to Q3's shader system.

Based on material system reference sources.
=============================================================================
*/

#ifdef USE_VULKAN

#include "tr_local.h"
extern refimport_t ri;
#include "vk.h"
#include "vk_material_system.h"
#include "vk_material_parser.h"
#include "../../common/q_shared.h"
#include "../../common/qcommon.h"

#include <string.h>
#include <stdlib.h>

#define MAX_PBR_MATERIALS 4096
#define MAX_MATERIAL_NAME_LENGTH 64

// PBR material structure (adapted from material system reference)
typedef struct pbr_material_q2rtx_s {
    char name[MAX_QPATH];
    char filename_base[MAX_QPATH];
    char filename_normals[MAX_QPATH];
    char filename_emissive[MAX_QPATH];
    char filename_mask[MAX_QPATH];
    char source_matfile[MAX_QPATH];
    uint32_t source_line;
    
    // Material properties
    float bump_scale;
    float roughness_override;      // -1 = use texture, >=0 = override value
    float metalness_factor;
    float emissive_factor;
    float specular_factor;
    float base_factor;
    
    // Material flags
    uint32_t flags;
    #define MAT_FLAG_IS_LIGHT         0x01
    #define MAT_FLAG_CORRECT_ALBEDO   0x02
    #define MAT_FLAG_SYNTH_EMISSIVE   0x04
    #define MAT_FLAG_BSP_RADIANCE     0x08
    
    // Texture references (will map to Q3 image_t - forward declared)
    struct image_s *image_base;
    struct image_s *image_normals;
    struct image_s *image_emissive;
    struct image_s *image_mask;
    
    // Emissive synthesis
    qboolean synth_emissive;
    int emissive_threshold;
    float default_radiance;
    
    // Animation
    int num_frames;
    int next_frame;
    qboolean light_styles;
    
    // Registration
    int registration_sequence;
} pbr_material_q2rtx_t;

static pbr_material_q2rtx_t r_materials_q2rtx[MAX_PBR_MATERIALS];
static int r_materials_q2rtx_count = 0;

static char *MAT_StrTok(char *str, const char *delim, char **saveptr)
{
	char *token;
	char *cursor;

	if (str != NULL) {
		*saveptr = str;
	}

	cursor = *saveptr;
	if (cursor == NULL) {
		return NULL;
	}

	cursor += strspn(cursor, delim);
	if (*cursor == '\0') {
		*saveptr = NULL;
		return NULL;
	}

	token = cursor;
	cursor = token + strcspn(token, delim);
	if (*cursor != '\0') {
		*cursor = '\0';
		cursor++;
	} else {
		cursor = NULL;
	}

	*saveptr = cursor;
	return token;
}

// CVARs
static cvar_t *r_mat_enable = NULL;
static cvar_t *r_mat_debug = NULL;

/*
===============
MAT_Q2RTX_Init

Initialize material system
===============
*/
void MAT_Q2RTX_Init(void)
{
    // Register CVARs
    r_mat_enable = ri.Cvar_Get("r_mat_enable", "1", CVAR_ARCHIVE);
    r_mat_debug = ri.Cvar_Get("r_mat_debug", "0", CVAR_ARCHIVE);
    
    // Clear material table
    memset(r_materials_q2rtx, 0, sizeof(r_materials_q2rtx));
    r_materials_q2rtx_count = 0;
    
    Com_Printf("Q2RTX-style material system initialized\n");
}

/*
===============
MAT_Q2RTX_Shutdown

Shutdown material system
===============
*/
void MAT_Q2RTX_Shutdown(void)
{
    // Clear material table
    memset(r_materials_q2rtx, 0, sizeof(r_materials_q2rtx));
    r_materials_q2rtx_count = 0;
    
    Com_Printf("Q2RTX-style material system shutdown\n");
}

/*
===============
MAT_Q2RTX_ParseLine

Parse a single line from a .mat file
Format: texture_name1, texture_name2: property value
===============
*/
static qboolean MAT_Q2RTX_ParseLine(const char *line, const char *filename, int line_num)
{
    char line_copy[1024];
    char *material_names;
    char *properties;
    char *token;
    char *saveptr;
    
    // Skip empty lines and comments
    if (!line || line[0] == '\0' || line[0] == '#' || line[0] == '/') {
        return qfalse;
    }
    
    Q_strncpyz(line_copy, line, sizeof(line_copy));
    
    // Find colon separator
    char *colon = strchr(line_copy, ':');
    if (!colon) {
        return qfalse; // Invalid line format
    }
    
    *colon = '\0';
    material_names = line_copy;
    properties = colon + 1;
    
    // Skip whitespace
    while (*material_names == ' ' || *material_names == '\t') material_names++;
    while (*properties == ' ' || *properties == '\t') properties++;
    
    // Parse material names (comma-separated)
    saveptr = NULL;
    token = MAT_StrTok(material_names, ",", &saveptr);
    
    while (token) {
        // Trim whitespace
        while (*token == ' ' || *token == '\t') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t')) *end-- = '\0';
        
        if (*token) {
            // Find or create material
            pbr_material_q2rtx_t *mat = NULL;
            for (int i = 0; i < r_materials_q2rtx_count; i++) {
                if (!Q_stricmp(r_materials_q2rtx[i].name, token)) {
                    mat = &r_materials_q2rtx[i];
                    break;
                }
            }
            
            if (!mat) {
                if (r_materials_q2rtx_count >= MAX_PBR_MATERIALS) {
                    Com_Printf("WARNING: Maximum materials reached\n");
                    break;
                }
                mat = &r_materials_q2rtx[r_materials_q2rtx_count++];
                memset(mat, 0, sizeof(pbr_material_q2rtx_t));
                Q_strncpyz(mat->name, token, sizeof(mat->name));
                Q_strncpyz(mat->source_matfile, filename, sizeof(mat->source_matfile));
                mat->source_line = line_num;
                
                // Default values
                mat->bump_scale = 1.0f;
                mat->roughness_override = -1.0f; // Use texture
                mat->metalness_factor = 1.0f;
                mat->emissive_factor = 1.0f;
                mat->specular_factor = 1.0f;
                mat->base_factor = 1.0f;
            }
            
            // Parse properties
            char prop_copy[1024];
            Q_strncpyz(prop_copy, properties, sizeof(prop_copy));
            char *prop_token = MAT_StrTok(prop_copy, " \t", &saveptr);
            
            while (prop_token) {
                if (!Q_stricmp(prop_token, "texture_base")) {
                    prop_token = MAT_StrTok(NULL, " \t", &saveptr);
                    if (prop_token) {
                        Q_strncpyz(mat->filename_base, prop_token, sizeof(mat->filename_base));
                    }
                } else if (!Q_stricmp(prop_token, "texture_normals")) {
                    prop_token = MAT_StrTok(NULL, " \t", &saveptr);
                    if (prop_token) {
                        Q_strncpyz(mat->filename_normals, prop_token, sizeof(mat->filename_normals));
                    }
                } else if (!Q_stricmp(prop_token, "texture_emissive")) {
                    prop_token = MAT_StrTok(NULL, " \t", &saveptr);
                    if (prop_token) {
                        Q_strncpyz(mat->filename_emissive, prop_token, sizeof(mat->filename_emissive));
                    }
                } else if (!Q_stricmp(prop_token, "texture_mask")) {
                    prop_token = MAT_StrTok(NULL, " \t", &saveptr);
                    if (prop_token) {
                        Q_strncpyz(mat->filename_mask, prop_token, sizeof(mat->filename_mask));
                    }
                } else if (!Q_stricmp(prop_token, "is_light")) {
                    prop_token = MAT_StrTok(NULL, " \t", &saveptr);
                    if (prop_token && atoi(prop_token)) {
                        mat->flags |= MAT_FLAG_IS_LIGHT;
                    }
                } else if (!Q_stricmp(prop_token, "correct_albedo")) {
                    prop_token = MAT_StrTok(NULL, " \t", &saveptr);
                    if (prop_token && atoi(prop_token)) {
                        mat->flags |= MAT_FLAG_CORRECT_ALBEDO;
                    }
                } else if (!Q_stricmp(prop_token, "synth_emissive")) {
                    prop_token = MAT_StrTok(NULL, " \t", &saveptr);
                    if (prop_token && atoi(prop_token)) {
                        mat->flags |= MAT_FLAG_SYNTH_EMISSIVE;
                        mat->synth_emissive = qtrue;
                    }
                } else if (!Q_stricmp(prop_token, "emissive_threshold")) {
                    prop_token = MAT_StrTok(NULL, " \t", &saveptr);
                    if (prop_token) {
                        mat->emissive_threshold = atoi(prop_token);
                    }
                } else if (!Q_stricmp(prop_token, "roughness")) {
                    prop_token = MAT_StrTok(NULL, " \t", &saveptr);
                    if (prop_token) {
                        mat->roughness_override = atof(prop_token);
                    }
                } else if (!Q_stricmp(prop_token, "metalness")) {
                    prop_token = MAT_StrTok(NULL, " \t", &saveptr);
                    if (prop_token) {
                        mat->metalness_factor = atof(prop_token);
                    }
                } else if (!Q_stricmp(prop_token, "emissive_factor")) {
                    prop_token = MAT_StrTok(NULL, " \t", &saveptr);
                    if (prop_token) {
                        mat->emissive_factor = atof(prop_token);
                    }
                } else if (!Q_stricmp(prop_token, "bump_scale")) {
                    prop_token = MAT_StrTok(NULL, " \t", &saveptr);
                    if (prop_token) {
                        mat->bump_scale = atof(prop_token);
                    }
                }
                
                prop_token = MAT_StrTok(NULL, " \t", &saveptr);
            }
        }
        
        token = MAT_StrTok(NULL, ",", &saveptr);
    }
    
    return qtrue;
}

/*
===============
MAT_Q2RTX_LoadFile

Load a .mat file
===============
*/
static void MAT_Q2RTX_LoadFile(const char *filename)
{
    char *buffer;
    int filelen;
    char *p, *line;
    int line_num = 0;
    
    filelen = ri.FS_ReadFile(filename, (void **)&buffer);
    if (!buffer || filelen <= 0) {
        return;
    }
    
    p = buffer;
    line_num = 1;
    
    while (p < buffer + filelen) {
        // Find end of line
        line = p;
        while (p < buffer + filelen && *p != '\n' && *p != '\r') {
            p++;
        }
        
        // Null-terminate line
        char old_char = *p;
        *p = '\0';
        
        // Parse line
        MAT_Q2RTX_ParseLine(line, filename, line_num);
        
        // Restore character and advance
        *p = old_char;
        if (p < buffer + filelen) {
            p++; // Skip newline
            if (*p == '\r') p++; // Skip \r if present
        }
        line_num++;
    }
    
    ri.FS_FreeFile(buffer);
    
    if (r_mat_debug && r_mat_debug->integer) {
        Com_Printf("Loaded material file: %s (%d materials)\n", filename, r_materials_q2rtx_count);
    }
}

/*
===============
MAT_Q2RTX_LoadMaterials

Load all .mat files from materials/ directory
===============
*/
void MAT_Q2RTX_LoadMaterials(const char *game_dir)
{
    char path[MAX_OSPATH];
    char **file_list = NULL;
    int num_files = 0;
    int i;
    
    // Load global materials from materials/*.mat
    (void)game_dir;
    file_list = ri.FS_ListFiles("materials", ".mat", &num_files);
    
    if (file_list && num_files > 0) {
        // Sort alphabetically (later files override earlier ones)
        // Simple bubble sort
        for (i = 0; i < num_files - 1; i++) {
            for (int j = 0; j < num_files - i - 1; j++) {
                if (Q_stricmp(file_list[j], file_list[j + 1]) > 0) {
                    char *tmp = file_list[j];
                    file_list[j] = file_list[j + 1];
                    file_list[j + 1] = tmp;
                }
            }
        }

        for (i = 0; i < num_files; i++) {
            Com_sprintf(path, sizeof(path), "materials/%s", file_list[i]);
            MAT_Q2RTX_LoadFile(path);
        }

        ri.FS_FreeFileList(file_list);
    }
    
    // Load map-specific materials from <mapname>.mat
    // This will be called when a map is loaded
}

/*
===============
MAT_Q2RTX_LoadMapMaterials

Load map-specific material file
===============
*/
void MAT_Q2RTX_LoadMapMaterials(const char *mapname)
{
    char path[MAX_OSPATH];
    
    if (!mapname || !mapname[0]) {
        return;
    }
    
    // Load <mapname>.mat from game directory
    Com_sprintf(path, sizeof(path), "materials/%s.mat", mapname);
    MAT_Q2RTX_LoadFile(path);
}

/*
===============
MAT_Q2RTX_Find

Find a material by name
===============
*/
pbr_material_q2rtx_t *MAT_Q2RTX_Find(const char *name)
{
    if (!name || !name[0]) {
        return NULL;
    }
    
    // Remove extension if present
    char name_noext[MAX_QPATH];
    Q_strncpyz(name_noext, name, sizeof(name_noext));
    char *ext = strrchr(name_noext, '.');
    if (ext) {
        *ext = '\0';
    }
    
    // Search for material
    for (int i = 0; i < r_materials_q2rtx_count; i++) {
        if (!Q_stricmp(r_materials_q2rtx[i].name, name_noext)) {
            return &r_materials_q2rtx[i];
        }
    }
    
    return NULL;
}

/*
===============
MAT_Q2RTX_GetMaterialProperties

Get material properties for a texture name
Returns properties in Q3 material_params_s format
===============
*/
qboolean MAT_Q2RTX_GetMaterialProperties(const char *texture_name, struct material_params_s *params)
{
    pbr_material_q2rtx_t *mat = MAT_Q2RTX_Find(texture_name);
    
    if (!mat) {
        return qfalse;
    }
    
    // Map material properties to Q3 material params
    if (params) {
        // Base color factor (map to baseColor)
        VectorSet(params->baseColor, mat->base_factor, mat->base_factor, mat->base_factor);
        
        // Roughness
        if (mat->roughness_override >= 0.0f) {
            params->roughness = mat->roughness_override;
        }
        
        // Metallic
        params->metallic = mat->metalness_factor;
        
        // Emissive
        float emissive_intensity = mat->emissive_factor;
        if (mat->flags & MAT_FLAG_IS_LIGHT) {
            // Make emissive more prominent
            emissive_intensity *= 2.0f;
        }
        VectorSet(params->emissive, emissive_intensity, emissive_intensity, emissive_intensity);
        params->emissiveStrength = emissive_intensity;
        
        // Normal scale
        params->normalScale = mat->bump_scale;
        
        // Flags
        if (mat->flags & MAT_FLAG_IS_LIGHT) {
            params->flags |= MATERIAL_EMISSIVE;
        }
    }
    
    return qtrue;
}

#endif // USE_VULKAN
