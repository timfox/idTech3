/*
=============================================================================
Atmosphere and Mood System Implementation
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk.h"
#include "vk_atmosphere.h"

#ifdef USE_VULKAN

// CVars (extern declarations - defined in tr_init.c)
extern cvar_t *r_atmosphere;
extern cvar_t *r_atmospherePreset;
extern cvar_t *r_fogDensity;
extern cvar_t *r_bloomIntensity;

// Preset definitions
static const atmosphere_params_t atmospherePresets[] = {
	// ATMOSPHERE_BRUTAL
	{
		.exposure = 0.8f,
		.contrast = 1.3f,
		.saturation = 1.1f,
		.brightness = 0.9f,
		.fogDensity = 0.0f,
		.fogStart = 0.0f,
		.fogEnd = 0.0f,
		.fogColor = { 0.0f, 0.0f, 0.0f },
		.fogHeightFalloff = 0.0f,
		.bloomIntensity = 0.2f,
		.bloomThreshold = 1.5f,
		.bloomSize = 0.5f,
		.colorTint = { 1.0f, 0.95f, 0.9f },
		.colorTemperature = 5500.0f,
		.dofFocusDistance = 0.0f,
		.dofBlurRadius = 0.0f,
		.timeOfDay = 0.5f,
		.weatherIntensity = 0.0f,
		.flags = 0
	},
	
	// ATMOSPHERE_MYSTERIOUS
	{
		.exposure = 1.1f,
		.contrast = 0.9f,
		.saturation = 0.8f,
		.brightness = 1.0f,
		.fogDensity = 0.05f,
		.fogStart = 100.0f,
		.fogEnd = 2000.0f,
		.fogColor = { 0.3f, 0.3f, 0.4f },
		.fogHeightFalloff = 0.5f,
		.bloomIntensity = 0.8f,
		.bloomThreshold = 0.8f,
		.bloomSize = 1.2f,
		.colorTint = { 0.9f, 0.9f, 1.0f },
		.colorTemperature = 4000.0f,
		.dofFocusDistance = 500.0f,
		.dofBlurRadius = 0.3f,
		.timeOfDay = 0.2f,
		.weatherIntensity = 0.3f,
		.flags = 0
	},
	
	// ATMOSPHERE_COMBAT
	{
		.exposure = 1.0f,
		.contrast = 1.2f,
		.saturation = 1.3f,
		.brightness = 1.1f,
		.fogDensity = 0.01f,
		.fogStart = 0.0f,
		.fogEnd = 0.0f,
		.fogColor = { 0.0f, 0.0f, 0.0f },
		.fogHeightFalloff = 0.0f,
		.bloomIntensity = 0.4f,
		.bloomThreshold = 1.2f,
		.bloomSize = 0.3f,
		.colorTint = { 1.1f, 0.95f, 0.9f },
		.colorTemperature = 6000.0f,
		.dofFocusDistance = 0.0f,
		.dofBlurRadius = 0.0f,
		.timeOfDay = 0.5f,
		.weatherIntensity = 0.0f,
		.flags = 0
	},
	
	// ATMOSPHERE_CALM
	{
		.exposure = 1.0f,
		.contrast = 1.0f,
		.saturation = 1.0f,
		.brightness = 1.0f,
		.fogDensity = 0.0f,
		.fogStart = 0.0f,
		.fogEnd = 0.0f,
		.fogColor = { 0.0f, 0.0f, 0.0f },
		.fogHeightFalloff = 0.0f,
		.bloomIntensity = 0.1f,
		.bloomThreshold = 1.0f,
		.bloomSize = 0.5f,
		.colorTint = { 1.0f, 1.0f, 1.0f },
		.colorTemperature = 5500.0f,
		.dofFocusDistance = 0.0f,
		.dofBlurRadius = 0.0f,
		.timeOfDay = 0.5f,
		.weatherIntensity = 0.0f,
		.flags = 0
	}
};

/*
=============================================================================
Atmosphere System Initialization
=============================================================================
*/
void vk_atmosphere_init( void )
{
	if ( vk.atmosphere.initialized ) {
		return;
	}
	
	ri.Printf( PRINT_ALL, "Initializing atmosphere system...\n" );
	
	// Initialize with default preset
	vk.atmosphere.currentPreset = ATMOSPHERE_CALM;
	vk.atmosphere.transitionTime = 0.0f;
	vk.atmosphere.transitionDuration = 0.0f;
	
	Com_Memcpy( &vk.atmosphere.currentParams, &atmospherePresets[ATMOSPHERE_CALM], sizeof( atmosphere_params_t ) );
	Com_Memcpy( &vk.atmosphere.targetParams, &atmospherePresets[ATMOSPHERE_CALM], sizeof( atmosphere_params_t ) );
	Com_Memcpy( &vk.atmosphere.baseParams, &atmospherePresets[ATMOSPHERE_CALM], sizeof( atmosphere_params_t ) );
	
	// Create GPU buffer for atmosphere parameters
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof( atmosphere_params_t );
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	VK_CHECK( qvkCreateBuffer( vk.device, &bufferInfo, NULL, &vk.atmosphere.atmosphereBuffer ) );
	
	VkMemoryRequirements memReqs;
	qvkGetBufferMemoryRequirements( vk.device, vk.atmosphere.atmosphereBuffer, &memReqs );
	
	uint32_t memoryType = find_memory_type( memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );
	
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = memoryType;
	
	VK_CHECK( qvkAllocateMemory( vk.device, &allocInfo, NULL, &vk.atmosphere.atmosphereBufferMemory ) );
	VK_CHECK( qvkBindBufferMemory( vk.device, vk.atmosphere.atmosphereBuffer, vk.atmosphere.atmosphereBufferMemory, 0 ) );
	
	ri.Printf( PRINT_ALL, "Atmosphere system: Initialized\n" );
	
	vk.atmosphere.initialized = qtrue;
}

/*
=============================================================================
Atmosphere System Shutdown
=============================================================================
*/
void vk_atmosphere_shutdown( void )
{
	if ( !vk.atmosphere.initialized ) {
		return;
	}
	
	if ( vk.atmosphere.atmosphereBuffer != VK_NULL_HANDLE ) {
		qvkDestroyBuffer( vk.device, vk.atmosphere.atmosphereBuffer, NULL );
		vk.atmosphere.atmosphereBuffer = VK_NULL_HANDLE;
	}
	
	if ( vk.atmosphere.atmosphereBufferMemory != VK_NULL_HANDLE ) {
		qvkFreeMemory( vk.device, vk.atmosphere.atmosphereBufferMemory, NULL );
		vk.atmosphere.atmosphereBufferMemory = VK_NULL_HANDLE;
	}
	
	vk.atmosphere.initialized = qfalse;
	ri.Printf( PRINT_ALL, "Atmosphere system: Shutdown complete\n" );
}

/*
=============================================================================
Lerp Atmosphere Parameters
=============================================================================
*/
static void lerp_atmosphere_params( const atmosphere_params_t *a, const atmosphere_params_t *b, float t, atmosphere_params_t *out )
{
	out->exposure = LERP( a->exposure, b->exposure, t );
	out->contrast = LERP( a->contrast, b->contrast, t );
	out->saturation = LERP( a->saturation, b->saturation, t );
	out->brightness = LERP( a->brightness, b->brightness, t );
	
	out->fogDensity = LERP( a->fogDensity, b->fogDensity, t );
	out->fogStart = LERP( a->fogStart, b->fogStart, t );
	out->fogEnd = LERP( a->fogEnd, b->fogEnd, t );
	VectorScale( a->fogColor, 1.0f - t, out->fogColor );
	VectorMA( out->fogColor, t, b->fogColor, out->fogColor );
	out->fogHeightFalloff = LERP( a->fogHeightFalloff, b->fogHeightFalloff, t );
	
	out->bloomIntensity = LERP( a->bloomIntensity, b->bloomIntensity, t );
	out->bloomThreshold = LERP( a->bloomThreshold, b->bloomThreshold, t );
	out->bloomSize = LERP( a->bloomSize, b->bloomSize, t );
	
	VectorScale( a->colorTint, 1.0f - t, out->colorTint );
	VectorMA( out->colorTint, t, b->colorTint, out->colorTint );
	out->colorTemperature = LERP( a->colorTemperature, b->colorTemperature, t );
	
	out->dofFocusDistance = LERP( a->dofFocusDistance, b->dofFocusDistance, t );
	out->dofBlurRadius = LERP( a->dofBlurRadius, b->dofBlurRadius, t );
	
	out->timeOfDay = LERP( a->timeOfDay, b->timeOfDay, t );
	out->weatherIntensity = LERP( a->weatherIntensity, b->weatherIntensity, t );
	
	out->flags = a->flags | b->flags;
}

/*
=============================================================================
Atmosphere System Update
=============================================================================
*/
void vk_atmosphere_update( void )
{
	if ( !vk.atmosphere.enabled || !vk.atmosphere.initialized ) {
		return;
	}
	
	static float lastFrameTime = 0.0f;
	float frameTime = tr.refdef.floatTime - lastFrameTime;
	if ( frameTime <= 0.0f || frameTime > 0.1f ) {
		frameTime = 0.016f; // Default to 60fps
	}
	lastFrameTime = tr.refdef.floatTime;
	
	// Update transition
	if ( vk.atmosphere.transitionDuration > 0.0f ) {
		vk.atmosphere.transitionTime += frameTime;
		
		if ( vk.atmosphere.transitionTime >= vk.atmosphere.transitionDuration ) {
			vk.atmosphere.transitionTime = vk.atmosphere.transitionDuration;
			vk.atmosphere.transitionDuration = 0.0f;
			Com_Memcpy( &vk.atmosphere.currentParams, &vk.atmosphere.targetParams, sizeof( atmosphere_params_t ) );
		} else {
			float t = vk.atmosphere.transitionTime / vk.atmosphere.transitionDuration;
			// Smoothstep interpolation for better transitions
			// Optimized: t * t * (3 - 2*t) avoids branching
			t = Com_Clamp( 0.0f, 1.0f, t );
			t = t * t * ( 3.0f - 2.0f * t );
			lerp_atmosphere_params( &vk.atmosphere.baseParams, &vk.atmosphere.targetParams, t, &vk.atmosphere.currentParams );
		}
	}
	
	// Upload to GPU
	void *mapped;
	VK_CHECK( qvkMapMemory( vk.device, vk.atmosphere.atmosphereBufferMemory, 0, sizeof( atmosphere_params_t ), 0, &mapped ) );
	Com_Memcpy( mapped, &vk.atmosphere.currentParams, sizeof( atmosphere_params_t ) );
	qvkUnmapMemory( vk.device, vk.atmosphere.atmosphereBufferMemory );
}

/*
=============================================================================
Set Atmosphere Preset
=============================================================================
*/
void vk_atmosphere_set_preset( atmosphere_preset_t preset, float transitionTime )
{
	if ( preset >= ATMOSPHERE_CUSTOM || preset < 0 ) {
		return;
	}
	
	vk.atmosphere.currentPreset = preset;
	vk.atmosphere.transitionDuration = transitionTime;
	vk.atmosphere.transitionTime = 0.0f;
	
	Com_Memcpy( &vk.atmosphere.baseParams, &vk.atmosphere.currentParams, sizeof( atmosphere_params_t ) );
	Com_Memcpy( &vk.atmosphere.targetParams, &atmospherePresets[preset], sizeof( atmosphere_params_t ) );
}

/*
=============================================================================
Set Custom Atmosphere Parameters
=============================================================================
*/
void vk_atmosphere_set_custom( const atmosphere_params_t *params, float transitionTime )
{
	vk.atmosphere.currentPreset = ATMOSPHERE_CUSTOM;
	vk.atmosphere.transitionDuration = transitionTime;
	vk.atmosphere.transitionTime = 0.0f;
	
	Com_Memcpy( &vk.atmosphere.baseParams, &vk.atmosphere.currentParams, sizeof( atmosphere_params_t ) );
	Com_Memcpy( &vk.atmosphere.targetParams, params, sizeof( atmosphere_params_t ) );
}

/*
=============================================================================
Set Exposure
=============================================================================
*/
void vk_atmosphere_set_exposure( float exposure, float transitionTime )
{
	vk.atmosphere.targetParams.exposure = exposure;
	vk.atmosphere.transitionDuration = transitionTime;
	vk.atmosphere.transitionTime = 0.0f;
	Com_Memcpy( &vk.atmosphere.baseParams, &vk.atmosphere.currentParams, sizeof( atmosphere_params_t ) );
}

/*
=============================================================================
Set Fog
=============================================================================
*/
void vk_atmosphere_set_fog( float density, float start, float end, const vec3_t color, float transitionTime )
{
	vk.atmosphere.targetParams.fogDensity = density;
	vk.atmosphere.targetParams.fogStart = start;
	vk.atmosphere.targetParams.fogEnd = end;
	VectorCopy( color, vk.atmosphere.targetParams.fogColor );
	vk.atmosphere.transitionDuration = transitionTime;
	vk.atmosphere.transitionTime = 0.0f;
	Com_Memcpy( &vk.atmosphere.baseParams, &vk.atmosphere.currentParams, sizeof( atmosphere_params_t ) );
}

/*
=============================================================================
Set Bloom
=============================================================================
*/
void vk_atmosphere_set_bloom( float intensity, float threshold, float size, float transitionTime )
{
	vk.atmosphere.targetParams.bloomIntensity = intensity;
	vk.atmosphere.targetParams.bloomThreshold = threshold;
	vk.atmosphere.targetParams.bloomSize = size;
	vk.atmosphere.transitionDuration = transitionTime;
	vk.atmosphere.transitionTime = 0.0f;
	Com_Memcpy( &vk.atmosphere.baseParams, &vk.atmosphere.currentParams, sizeof( atmosphere_params_t ) );
}

/*
=============================================================================
Set Color Tint
=============================================================================
*/
void vk_atmosphere_set_color_tint( const vec3_t color, float transitionTime )
{
	VectorCopy( color, vk.atmosphere.targetParams.colorTint );
	vk.atmosphere.transitionDuration = transitionTime;
	vk.atmosphere.transitionTime = 0.0f;
	Com_Memcpy( &vk.atmosphere.baseParams, &vk.atmosphere.currentParams, sizeof( atmosphere_params_t ) );
}

/*
=============================================================================
Set Time of Day
=============================================================================
*/
void vk_atmosphere_set_time_of_day( float timeOfDay, float transitionTime )
{
	vk.atmosphere.targetParams.timeOfDay = Com_Clamp( 0.0f, 1.0f, timeOfDay );
	vk.atmosphere.transitionDuration = transitionTime;
	vk.atmosphere.transitionTime = 0.0f;
	Com_Memcpy( &vk.atmosphere.baseParams, &vk.atmosphere.currentParams, sizeof( atmosphere_params_t ) );
}

/*
=============================================================================
Register Lua Functions
=============================================================================
*/
#ifdef USE_LUA
#include "../../common/qcommon.h"

/*
=============================================================================
Lua Atmosphere Bindings
=============================================================================
*/

static int lua_atmosphere_set_preset( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 1 ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	
	int preset = (int)lua_tointeger( L, 1 );
	float transitionTime = 0.0f;
	if ( numArgs >= 2 ) {
		transitionTime = (float)lua_tonumber( L, 2 );
	}
	
	if ( preset >= 0 && preset < ATMOSPHERE_CUSTOM ) {
		vk_atmosphere_set_preset( (atmosphere_preset_t)preset, transitionTime );
		lua_pushboolean( L, 1 );
	} else {
		lua_pushboolean( L, 0 );
	}
	return 1;
}

static int lua_atmosphere_set_exposure( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 1 ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	
	float exposure = (float)lua_tonumber( L, 1 );
	float transitionTime = 0.0f;
	if ( numArgs >= 2 ) {
		transitionTime = (float)lua_tonumber( L, 2 );
	}
	
	vk_atmosphere_set_exposure( exposure, transitionTime );
	lua_pushboolean( L, 1 );
	return 1;
}

static int lua_atmosphere_set_fog( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 3 ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	
	float density = (float)lua_tonumber( L, 1 );
	float start = (float)lua_tonumber( L, 2 );
	float end = (float)lua_tonumber( L, 3 );
	
	vec3_t color = { 0.5f, 0.5f, 0.5f };
	if ( numArgs >= 6 ) {
		color[0] = (float)lua_tonumber( L, 4 );
		color[1] = (float)lua_tonumber( L, 5 );
		color[2] = (float)lua_tonumber( L, 6 );
	}
	
	float transitionTime = 0.0f;
	if ( numArgs >= 7 ) {
		transitionTime = (float)lua_tonumber( L, 7 );
	}
	
	vk_atmosphere_set_fog( density, start, end, color, transitionTime );
	lua_pushboolean( L, 1 );
	return 1;
}

static int lua_atmosphere_set_bloom( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 1 ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	
	float intensity = (float)lua_tonumber( L, 1 );
	float threshold = 1.0f;
	float size = 0.5f;
	float transitionTime = 0.0f;
	
	if ( numArgs >= 2 ) {
		threshold = (float)lua_tonumber( L, 2 );
	}
	if ( numArgs >= 3 ) {
		size = (float)lua_tonumber( L, 3 );
	}
	if ( numArgs >= 4 ) {
		transitionTime = (float)lua_tonumber( L, 4 );
	}
	
	vk_atmosphere_set_bloom( intensity, threshold, size, transitionTime );
	lua_pushboolean( L, 1 );
	return 1;
}

static int lua_atmosphere_set_color_tint( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 3 ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	
	vec3_t color;
	color[0] = (float)lua_tonumber( L, 1 );
	color[1] = (float)lua_tonumber( L, 2 );
	color[2] = (float)lua_tonumber( L, 3 );
	
	float transitionTime = 0.0f;
	if ( numArgs >= 4 ) {
		transitionTime = (float)lua_tonumber( L, 4 );
	}
	
	vk_atmosphere_set_color_tint( color, transitionTime );
	lua_pushboolean( L, 1 );
	return 1;
}

static int lua_atmosphere_set_time_of_day( lua_State *L )
{
	int numArgs = lua_gettop( L );
	if ( numArgs < 1 ) {
		lua_pushboolean( L, 0 );
		return 1;
	}
	
	float timeOfDay = (float)lua_tonumber( L, 1 );
	float transitionTime = 0.0f;
	if ( numArgs >= 2 ) {
		transitionTime = (float)lua_tonumber( L, 2 );
	}
	
	vk_atmosphere_set_time_of_day( timeOfDay, transitionTime );
	lua_pushboolean( L, 1 );
	return 1;
}

#endif // USE_LUA

void vk_atmosphere_register_lua_functions( void *luaState )
{
#ifdef USE_LUA
	if ( !luaState ) {
		return;
	}
	
	lua_State *L = (lua_State *)luaState;
	
	// Register atmosphere functions
	Lua_RegisterFunction( L, "AtmosphereSetPreset", lua_atmosphere_set_preset );
	Lua_RegisterFunction( L, "AtmosphereSetExposure", lua_atmosphere_set_exposure );
	Lua_RegisterFunction( L, "AtmosphereSetFog", lua_atmosphere_set_fog );
	Lua_RegisterFunction( L, "AtmosphereSetBloom", lua_atmosphere_set_bloom );
	Lua_RegisterFunction( L, "AtmosphereSetColorTint", lua_atmosphere_set_color_tint );
	Lua_RegisterFunction( L, "AtmosphereSetTimeOfDay", lua_atmosphere_set_time_of_day );
	
	Com_Printf( "Atmosphere system: Registered Lua bindings\n" );
#else
	(void)luaState; // Suppress unused parameter warning when USE_LUA is not defined
#endif // USE_LUA
}

#endif // USE_VULKAN

