#pragma once

#include "tr_local.h"

/* Groom-side contract for deferred software hair.  The payload is deliberately
 * independent of the legacy model formats so a future importer can feed it
 * without changing clustered lighting ownership. */
typedef struct {
	uint32_t firstLayer;
	uint32_t layerCount;
	uint32_t strandCount;
	uint32_t controlPointCount;
	float lodLambda;
	float bakedAo;
	uint32_t styleIndex;
	uint32_t flags;
} vkHairBundle_t;

typedef struct {
	uint32_t bundleCount;
	uint32_t layerCount;
	uint32_t strandCount;
	uint32_t controlPointCount;
	uint32_t visibilityBits;
	uint32_t visibilityFormat; /* 1 = packed depth/material 64-bit key */
	uint32_t visibilityWords;
	uint32_t owner;
	uint32_t lodGeneration;
	uint32_t clusterGeneration;
} vkHairDeferredContract_t;

void vk_hair_deferred_init( void );
void vk_hair_deferred_shutdown( void );
void vk_hair_deferred_status_f( void );
const vkHairDeferredContract_t *vk_hair_deferred_contract( void );
