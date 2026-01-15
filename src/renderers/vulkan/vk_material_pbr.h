/*
=============================================================================
id Tech 3 - Material System Header

Ports a .mat file material system to id Tech 3.
=============================================================================
*/

#pragma once

#ifdef USE_VULKAN

#include "tr_local.h"
#include "vk_material_system.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
struct pbr_material_q2rtx_s;
typedef struct pbr_material_q2rtx_s pbr_material_q2rtx_t;

// Initialize material system
void MAT_Q2RTX_Init(void);

// Shutdown material system
void MAT_Q2RTX_Shutdown(void);

// Load all materials from materials/ directory
void MAT_Q2RTX_LoadMaterials(const char *game_dir);

// Load map-specific materials
void MAT_Q2RTX_LoadMapMaterials(const char *mapname);

// Find a material by name
pbr_material_q2rtx_t *MAT_Q2RTX_Find(const char *name);

// Get material properties for Q3 material system
qboolean MAT_Q2RTX_GetMaterialProperties(const char *texture_name, struct material_params_s *params);

#ifdef __cplusplus
}
#endif

#endif // USE_VULKAN
