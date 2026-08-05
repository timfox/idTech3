#pragma once

#ifdef USE_VULKAN

/*
 * Raster Ultra 1.10 — scene-linear HDR + display transform contract.
 * Does not use post to hide incorrect lighting. RT stays off.
 */

typedef enum {
	VK_PRESENT_COLOR_SDR_SRGB = 0,
	VK_PRESENT_COLOR_SDR_WIDE,
	VK_PRESENT_COLOR_HDR10_PQ,
	VK_PRESENT_COLOR_SCRGB,
	VK_PRESENT_COLOR_COUNT
} vkPresentColorMode_t;

typedef enum {
	VK_TONEMAP_OFF = 0,
	VK_TONEMAP_REINHARD,
	VK_TONEMAP_ACES,
	VK_TONEMAP_FILMIC,
	VK_TONEMAP_AGX,
	VK_TONEMAP_NEUTRAL_REFERENCE
} vkTonemapMode_t;

typedef struct vkPresentColorContract_s {
	const char *workingSpace;     /* scene-linear Rec.709/P3 working */
	const char *sceneUnits;       /* relative scene-referred radiance */
	float preExposure;
	float prevPreExposure;
	float paperWhiteNits;
	float peakLuminanceNits;
	float uiReferenceWhiteNits;
	vkPresentColorMode_t displayMode;
	vkTonemapMode_t tonemapMode;
	qboolean swapchainIsSrgb;
	qboolean hdrDisplayRequested;
	qboolean hdrDisplayAvailable;
	qboolean doubleGammaForbidden;
} vkPresentColorContract_t;

void vk_present_color_register_cvars( void );
void vk_present_color_init( void );
void vk_present_color_shutdown( void );

/* Call after surface format selection to probe HDR10/scRGB candidates. */
void vk_present_color_on_surface_formats( const VkSurfaceFormatKHR *candidates, uint32_t count );
void vk_present_color_apply_selection( VkSurfaceFormatKHR *inoutPresent );

qboolean vk_present_color_active( void );
const vkPresentColorContract_t *vk_present_color_contract( void );

/* Prefer AgX (4) when Ultra overlay sets r_presentTonemapPreference. The
 * neutral-reference diagnostic (5) remains available through r_tonemap. */
int vk_present_color_preferred_tonemap( void );

void vk_present_color_status_f( void );

#endif /* USE_VULKAN */
