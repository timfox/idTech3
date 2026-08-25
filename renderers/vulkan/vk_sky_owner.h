#pragma once


/*
 * Raster Ultra 1.7 — exclusive sky radiance ownership.
 * Only one owner contributes visible sky for a view.
 */

typedef enum {
	VK_SKY_OWNER_CLASSIC = 0,   /* Q3 skybox / skyParms (default, classic maps) */
	VK_SKY_OWNER_PHYSICAL = 1,  /* physical atmosphere (Nishita / LUTs) */
	VK_SKY_OWNER_HDR = 2,       /* authored HDR cubemap / panorama */
	VK_SKY_OWNER_SOLID = 3,     /* fallback solid / no sky */
	VK_SKY_OWNER_COUNT
} vkSkyOwner_t;

void vk_sky_owner_register_cvars( void );
void vk_sky_owner_init( void );

vkSkyOwner_t vk_sky_owner( void );
const char *vk_sky_owner_name( vkSkyOwner_t owner );

/* True when classic skybox/cloud shells should draw. */
qboolean vk_sky_owner_wants_classic_skybox( void );

/* True when physical atmosphere may paint far-depth sky. */
qboolean vk_sky_owner_wants_physical_sky( void );

/* True when HDR panorama owns sky. */
qboolean vk_sky_owner_wants_hdr_sky( void );

void vk_sky_owner_status_f( void );

