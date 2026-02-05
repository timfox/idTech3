#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declare renderer types used by the glints API.
// image_t is defined in the renderer (e.g. tr_local.h), but we avoid including it here.
typedef struct image_s image_t;

// ---- Glint dictionary API ----
//
// Returns an image_t wrapper for the current glint dictionary texture.
// This is the preferred binding path for PBR descriptors (bind like other textures).
const image_t *vk_get_glint_dictionary_image( void );

// Returns the descriptor indexing slot for the glint dictionary (if using descriptor indexing).
// If you don't use indexing, you can ignore this.
uint32_t vk_get_glint_dict_index( void );

// Optional: update/rebuild dictionary when cvars change or reload requested.
typedef struct glint_dict_params_s {
	int entries;
	int levels;
	int size;
	float alpha;
	float lobeSigma;
	int mode;
	uint32_t seed;
} glint_dict_params_t;

void vk_update_glint_dictionary_if_needed( const glint_dict_params_t *params, int forceReload );

#ifdef __cplusplus
} // extern "C"
#endif
