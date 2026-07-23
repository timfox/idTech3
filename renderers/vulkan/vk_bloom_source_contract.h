#pragma once

/*
 * Renderer IQ P1 — bloom source contract (BloomSourceHDR).
 * See docs/BLOOM_SOURCE_INTEGRITY.md.
 */

#ifdef USE_VULKAN

#include "../common/tr_types.h"

#define BLOOM_SOURCE_CONTRACT_VERSION 1u

#define BLOOM_CONTRIB_OPAQUE                 ( 1u << 0 )
#define BLOOM_CONTRIB_GI                     ( 1u << 1 )
#define BLOOM_CONTRIB_WBOIT                  ( 1u << 2 )
#define BLOOM_CONTRIB_ADDITIVE               ( 1u << 3 )
#define BLOOM_CONTRIB_SPECIAL_TRANSPARENCY   ( 1u << 4 )
#define BLOOM_CONTRIB_WEAPON_OPAQUE          ( 1u << 5 )
#define BLOOM_CONTRIB_WEAPON_EMISSIVE        ( 1u << 6 )
#define BLOOM_CONTRIB_VOLUMETRIC             ( 1u << 7 )

typedef struct bloomSourceContract_s {
	uint32_t generation;
	uint64_t frameNumber;

	uint32_t contributorMask;
	uint32_t colorSpace;    /* vkColorSpace_t */
	uint32_t exposureState; /* 0 = pre-exposure scene linear */

	uint32_t width;
	uint32_t height;

	uint32_t sceneHdrGeneration;
	uint32_t contractVersion;
	uint32_t contractHash;

	char lastWriter[64];
	uint32_t validateFails;
} bloomSourceContract_t;

void vk_bloom_source_contract_register( void );
void vk_bloom_source_contract_begin_frame( void );
void vk_bloom_source_note_contributor( uint32_t maskBit, const char *writer );
void vk_bloom_source_note_extract( const char *writerName );
const bloomSourceContract_t *vk_bloom_source_contract_get( void );
qboolean vk_bloom_source_contract_validate( char *errBuf, int errBufSize );
void vk_bloom_source_status_f( void );

#endif /* USE_VULKAN */
