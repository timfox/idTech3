#pragma once

#ifdef USE_VULKAN

#include <stdint.h>

#define DEFERRED_RENDERING_CONTRACT_VERSION 3u

typedef struct deferredRenderingContract_s {
	uint32_t version;
	uint32_t hash;
	uint32_t gbufferQuality;
	uint32_t normalEncoding;
	uint32_t materialEncoding;
	uint32_t ownershipEncoding;
	uint32_t brdfVersion;
	uint32_t clusterContractVersion;
	uint32_t lightmapContractVersion;
	uint32_t shadowContractVersion;
	uint32_t aoOwnershipVersion;
	uint32_t emissiveOwnershipVersion;
} deferredRenderingContract_t;

typedef struct clusterLightingFrame_s {
	uint64_t frameNumber;
	uint32_t generation;
	uint32_t tileCountX;
	uint32_t tileCountY;
	uint32_t zSliceCount;
	uint32_t lightCount;
	uint32_t listEntryCount;
	uint32_t overflowCount;
	uint32_t depthGeneration;
	uint32_t lightDataGeneration;
} clusterLightingFrame_t;

typedef enum deferredCertificationLevel_e {
	DEFERRED_UNCERTIFIED = 0,
	DEFERRED_STATIC_READY,
	DEFERRED_GBUFFER_CERTIFIED,
	DEFERRED_MATERIAL_CERTIFIED,
	DEFERRED_DIRECT_LIGHT_CERTIFIED,
	DEFERRED_INDIRECT_LIGHT_CERTIFIED,
	DEFERRED_OWNERSHIP_CERTIFIED,
	DEFERRED_FORWARD_PARITY_CERTIFIED,
	DEFERRED_PRODUCTION_CERTIFIED
} deferredCertificationLevel_t;

void vk_deferred_certification_register( void );
void vk_deferred_certification_begin_frame( void );

#endif
