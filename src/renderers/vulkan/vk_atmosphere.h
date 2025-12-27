/*
=============================================================================
Atmosphere and Mood System
Provides scriptable lighting, fog, and post-processing effects
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

// Forward declarations (full definitions in vk.h)
// atmosphere_preset_t and atmosphere_params_t are already typedef'd in vk.h


// Atmosphere system state
typedef struct {
	qboolean enabled;
	qboolean initialized;
	
	atmosphere_params_t currentParams;
	atmosphere_params_t targetParams;
	atmosphere_params_t baseParams;
	
	atmosphere_preset_t currentPreset;
	float transitionTime;
	float transitionDuration;
	
	// GPU buffer for atmosphere parameters
	VkBuffer atmosphereBuffer;
	VkDeviceMemory atmosphereBufferMemory;
	
	// Volumetric Fog resources
	VkPipeline volumetricFogPipeline;
	VkPipelineLayout volumetricFogLayout;
	VkDescriptorSetLayout volumetricFogDescriptorLayout;
	VkDescriptorSet volumetricFogDescriptorSet;
	VkImage volumetricFogImage;
	VkImageView volumetricFogImageView;
	VkDeviceMemory volumetricFogImageMemory;
	image_t *noiseTexture;

	// Composite resources
	VkPipeline compositePipeline;
	VkPipelineLayout compositeLayout;
	VkDescriptorSetLayout compositeDescriptorLayout;
	VkDescriptorSet compositeDescriptorSet;

	// Scripting interface
	void *luaState;
} atmosphere_system_t;

#ifdef __cplusplus
extern "C" {
#endif

// External API
void vk_atmosphere_init( void );
void vk_atmosphere_shutdown( void );
void vk_atmosphere_update( void );

// Volumetric Fog API
void vk_volumetric_fog_init( void );
void vk_volumetric_fog_shutdown( void );
void vk_volumetric_fog_render( void );

// Preset control
void vk_atmosphere_set_preset( atmosphere_preset_t preset, float transitionTime );
void vk_atmosphere_set_custom( const atmosphere_params_t *params, float transitionTime );

// Parameter control
void vk_atmosphere_set_exposure( float exposure, float transitionTime );
void vk_atmosphere_set_fog( float density, float start, float end, const vec3_t color, float transitionTime );
void vk_atmosphere_set_bloom( float intensity, float threshold, float size, float transitionTime );
void vk_atmosphere_set_color_tint( const vec3_t color, float transitionTime );
void vk_atmosphere_set_time_of_day( float timeOfDay, float transitionTime );

// Scripting interface
void vk_atmosphere_register_lua_functions( void *luaState );

#ifdef __cplusplus
}
#endif

// CVars
extern cvar_t *r_atmosphere;
extern cvar_t *r_atmospherePreset;
extern cvar_t *r_fogDensity;
extern cvar_t *r_bloomIntensity;

#endif // USE_VULKAN

