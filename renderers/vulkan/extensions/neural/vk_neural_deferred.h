#pragma once

#include "tr_local.h"

/* PBNDS+ input contract. The neural model may shade these features, but may
 * not reinterpret authoritative material IDs or primary visibility. */
typedef struct {
	uint32_t channelCount;
	uint32_t gbufferGeneration;
	uint32_t clusterGeneration;
	uint32_t owner;       /* 0 inactive, 1 neural shading advisory */
	uint32_t outputOwner; /* 1 = compare/advisory; 2 = bounded SceneHDR blend */
	uint32_t darkEnergyGate;
	uint32_t modelVersion;
	uint32_t inputConvention;
} vkNeuralDeferredContract_t;

void vk_neural_deferred_init( void );
void vk_neural_deferred_shutdown( void );
void vk_neural_deferred_status_f( void );
const vkNeuralDeferredContract_t *vk_neural_deferred_contract( void );

