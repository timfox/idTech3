#pragma once

/*
 * Phase 1.6 — evidence persistence + dependency invalidation.
 */


#include "../common/tr_types.h"
#include "vk_renderer_p1_cert.h"

void vk_renderer_p1_evidence_register( void );

uint32_t vk_renderer_p1_profile_hash( void );
uint32_t vk_renderer_p1_evidence_build_id( void );

/* Invalidate stages whose dependency token matches (e.g. "bloom.frag", "velocity", "threshold"). */
void vk_renderer_p1_evidence_invalidate_dep( const char *depToken, const char *reason );

/* Enrich JSON export with build/GPU/driver/hashes (called from cert export). */
void vk_renderer_p1_evidence_fill_header( char *buf, int bufSize, int *outOff );

