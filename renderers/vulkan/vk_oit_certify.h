/*
===========================================================================
WBOIT live certification / soak / status (Stage A).
See docs/WBOIT_GPU_CERTIFICATION.md and docs/MOMENT_OIT_STOCHASTIC_ALPHA.md.
===========================================================================
*/
#ifndef VK_OIT_CERTIFY_H
#define VK_OIT_CERTIFY_H

#ifdef __cplusplus
extern "C" {
#endif

void vk_oit_certify_init( void );
void vk_oit_certify_shutdown( void );

/* Per-frame soak / anomaly hooks (called from vk_oit_pass). */
void vk_oit_certify_frame_tick( void );
void vk_oit_certify_note_anomaly( const char *reason );

/* Certification levels (docs/WBOIT_GPU_CERTIFICATION.md). */
typedef enum {
	VK_OIT_CERT_NONE = 0,
	VK_OIT_CERT_STATIC_GATES,
	VK_OIT_CERT_LIVE_BASIC,
	VK_OIT_CERT_LIVE_FULL,
	VK_OIT_CERT_LIVE_SOAKED,
	VK_OIT_CERT_SPINE_1_1
} vkOitCertLevel_t;

vkOitCertLevel_t vk_oit_certification_level( void );
const char *vk_oit_certification_level_name( vkOitCertLevel_t level );

#ifdef __cplusplus
}
#endif

#endif
