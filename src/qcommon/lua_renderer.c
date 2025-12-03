#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_LUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// Forward declarations for renderer functions
// These are declared in tr_shaders_enhanced.h but we can't include renderer headers from qcommon
// So we use forward declarations

// Shader parameter types (from tr_shaders_enhanced.h)
#define SHADER_PARAM_FLOAT		0
#define SHADER_PARAM_VEC2		1
#define SHADER_PARAM_VEC3		2
#define SHADER_PARAM_VEC4		3
#define SHADER_PARAM_INT		4
#define SHADER_PARAM_BOOL		5
#define SHADER_PARAM_TEXTURE	6
#define SHADER_PARAM_COLOR		7

// Shader animation types (from tr_shaders_enhanced.h)
#define SHADER_ANIM_LINEAR			0
#define SHADER_ANIM_SINE			1
#define SHADER_ANIM_COSINE			2
#define SHADER_ANIM_TRIANGLE		3
#define SHADER_ANIM_SAWTOOTH		4
#define SHADER_ANIM_INVERSE_SAWTOOTH	5
#define SHADER_ANIM_NOISE			6
#define SHADER_ANIM_CUSTOM			7

// Particle emitter types (from tr_particles_enhanced.h)
#define EMITTER_POINT		0
#define EMITTER_BOX			1
#define EMITTER_SPHERE		2
#define EMITTER_CYLINDER	3
#define EMITTER_MESH		4
#define EMITTER_BURST		5

// Renderer function declarations - these will be resolved at runtime when renderer is loaded
// Using weak symbols so they can be overridden by the renderer module

// Forward declarations
__attribute__((weak)) int R_FindProceduralShader(const char *name);
__attribute__((weak)) int R_CreateParticleEmitter(int type, const float *origin, const float *mins, const float *maxs, float radius, float height);
__attribute__((weak)) void R_SetEmitterRate(int emitterId, float rate);
__attribute__((weak)) int R_FindShaderParameter(int shaderId, const char *name);
__attribute__((weak)) int R_AddShaderParameter(int shaderId, const char *name, int type);
__attribute__((weak)) void R_SetShaderParameterFloat(int shaderId, int paramIndex, float value);
__attribute__((weak)) int R_CreateProceduralShader(const char *name, int baseShader);
__attribute__((weak)) void R_SetShaderPBR(int shaderId, qboolean enabled);
__attribute__((weak)) void R_SetShaderMetallic(int shaderId, float metallic);
__attribute__((weak)) void R_SetShaderRoughness(int shaderId, float roughness);
__attribute__((weak)) void R_SetShaderAlbedo(int shaderId, const float *albedo);
__attribute__((weak)) void R_SetShaderParameterAnimation(int shaderId, int paramIndex, int animType, float speed, float min, float max);

// Weak symbol implementations
__attribute__((weak)) int R_FindProceduralShader(const char *name) { (void)name; return -1; }
__attribute__((weak)) int R_CreateParticleEmitter(int type, const float *origin, const float *mins, const float *maxs, float radius, float height) { (void)type; (void)origin; (void)mins; (void)maxs; (void)radius; (void)height; return -1; }
__attribute__((weak)) void R_SetEmitterRate(int emitterId, float rate) { (void)emitterId; (void)rate; }
__attribute__((weak)) int R_FindShaderParameter(int shaderId, const char *name) { (void)shaderId; (void)name; return -1; }
__attribute__((weak)) int R_AddShaderParameter(int shaderId, const char *name, int type) { (void)shaderId; (void)name; (void)type; return -1; }
__attribute__((weak)) void R_SetShaderParameterFloat(int shaderId, int paramIndex, float value) { (void)shaderId; (void)paramIndex; (void)value; }
__attribute__((weak)) int R_CreateProceduralShader(const char *name, int baseShader) { (void)name; (void)baseShader; return -1; }
__attribute__((weak)) void R_SetShaderPBR(int shaderId, qboolean enabled) { (void)shaderId; (void)enabled; }
__attribute__((weak)) void R_SetShaderMetallic(int shaderId, float metallic) { (void)shaderId; (void)metallic; }
__attribute__((weak)) void R_SetShaderRoughness(int shaderId, float roughness) { (void)shaderId; (void)roughness; }
__attribute__((weak)) void R_SetShaderAlbedo(int shaderId, const float *albedo) { (void)shaderId; (void)albedo; }
__attribute__((weak)) void R_SetShaderParameterAnimation(int shaderId, int paramIndex, int animType, float speed, float min, float max) { (void)shaderId; (void)paramIndex; (void)animType; (void)speed; (void)min; (void)max; }
// RE_RegisterShaderNoMip is in renderer interface, accessed via ri

/*
=================
Lua_RendererSpawnParticle
=================
Lua binding: renderer_spawn_particle(x, y, z, vx, vy, vz, r, g, b, size, life, shader) -> particle_id
=================
*/
static int Lua_RendererSpawnParticle(lua_State *L)
{
	vec3_t origin, velocity, color;
	float size, life;
	const char *shaderName;
	int numArgs = lua_gettop(L);
	
	if (numArgs < 12) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) ||
		!lua_isnumber(L, 4) || !lua_isnumber(L, 5) || !lua_isnumber(L, 6) ||
		!lua_isnumber(L, 7) || !lua_isnumber(L, 8) || !lua_isnumber(L, 9) ||
		!lua_isnumber(L, 10) || !lua_isnumber(L, 11)) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	origin[0] = (float)lua_tonumber(L, 1);
	origin[1] = (float)lua_tonumber(L, 2);
	origin[2] = (float)lua_tonumber(L, 3);
	
	velocity[0] = (float)lua_tonumber(L, 4);
	velocity[1] = (float)lua_tonumber(L, 5);
	velocity[2] = (float)lua_tonumber(L, 6);
	
	color[0] = (float)lua_tonumber(L, 7);
	color[1] = (float)lua_tonumber(L, 8);
	color[2] = (float)lua_tonumber(L, 9);
	
	size = (float)lua_tonumber(L, 10);
	life = (float)lua_tonumber(L, 11);
	
	shaderName = lua_tostring(L, 12);
	if (!shaderName) {
		shaderName = "default";
	}
	
	// TODO: Implement actual particle spawning when renderer integration is added
	Com_DPrintf("Lua_RendererSpawnParticle: Spawning particle at (%.2f, %.2f, %.2f)\n",
		origin[0], origin[1], origin[2]);
	
	// Return particle ID (placeholder)
	lua_pushinteger(L, 0);
	return 1;
}

/*
=================
Lua_RendererSetShaderParameter
=================
Lua binding: renderer_set_shader_param(shader_name, param_name, value)
=================
*/
static int Lua_RendererSetShaderParameter(lua_State *L)
{
	const char *shaderName;
	const char *paramName;
	float value;
	int shaderId;
	int paramIndex;
	
	if (lua_gettop(L) < 3) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	shaderName = lua_tostring(L, 1);
	paramName = lua_tostring(L, 2);
	
	if (!shaderName || !paramName || !lua_isnumber(L, 3)) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	value = (float)lua_tonumber(L, 3);
	
	shaderId = R_FindProceduralShader(shaderName);
	if (shaderId < 0) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	paramIndex = R_FindShaderParameter(shaderId, paramName);
	if (paramIndex < 0) {
		// Create parameter if it doesn't exist
		paramIndex = R_AddShaderParameter(shaderId, paramName, SHADER_PARAM_FLOAT);
		if (paramIndex < 0) {
			lua_pushboolean(L, 0);
			return 1;
		}
	}
	
	R_SetShaderParameterFloat(shaderId, paramIndex, value);
	lua_pushboolean(L, 1);
	return 1;
}

/*
=================
Lua_RendererCreateProceduralShader
=================
Lua binding: renderer_create_procedural_shader(name, base_shader_name) -> shader_id
=================
*/
static int Lua_RendererCreateProceduralShader(lua_State *L)
{
	const char *name;
	const char *baseShaderName;
	qhandle_t baseShader;
	int shaderId;
	
	if (lua_gettop(L) < 1) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	name = lua_tostring(L, 1);
	if (!name) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	baseShader = -1;
	if (lua_gettop(L) >= 2) {
		baseShaderName = lua_tostring(L, 2);
		if (baseShaderName) {
			// Use renderer interface to register shader
			// Note: This requires renderer to be initialized
			// For now, we'll use -1 (no base shader) if renderer isn't available
			baseShader = -1; // TODO: Access via renderer interface
		}
	}
	
	shaderId = R_CreateProceduralShader(name, baseShader);
	lua_pushinteger(L, shaderId);
	return 1;
}

/*
=================
Lua_RendererSetShaderPBR
=================
Lua binding: renderer_set_shader_pbr(shader_id, metallic, roughness, r, g, b)
=================
*/
static int Lua_RendererSetShaderPBR(lua_State *L)
{
	int shaderId;
	float metallic, roughness;
	vec3_t albedo;
	
	if (lua_gettop(L) < 6) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	shaderId = (int)lua_tointeger(L, 1);
	metallic = (float)lua_tonumber(L, 2);
	roughness = (float)lua_tonumber(L, 3);
	albedo[0] = (float)lua_tonumber(L, 4);
	albedo[1] = (float)lua_tonumber(L, 5);
	albedo[2] = (float)lua_tonumber(L, 6);
	
	R_SetShaderPBR(shaderId, qtrue);
	R_SetShaderMetallic(shaderId, metallic);
	R_SetShaderRoughness(shaderId, roughness);
	R_SetShaderAlbedo(shaderId, albedo);
	
	lua_pushboolean(L, 1);
	return 1;
}

/*
=================
Lua_RendererSetShaderParameterAnimation
=================
Lua binding: renderer_set_shader_param_animation(shader_id, param_name, anim_type, speed, min, max)
=================
*/
static int Lua_RendererSetShaderParameterAnimation(lua_State *L)
{
	int shaderId;
	const char *paramName;
	int animType;
	float speed, min, max;
	int paramIndex;
	
	if (lua_gettop(L) < 6) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	shaderId = (int)lua_tointeger(L, 1);
	paramName = lua_tostring(L, 2);
	animType = (int)lua_tointeger(L, 3);
	speed = (float)lua_tonumber(L, 4);
	min = (float)lua_tonumber(L, 5);
	max = (float)lua_tonumber(L, 6);
	
	if (!paramName) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	paramIndex = R_FindShaderParameter(shaderId, paramName);
	if (paramIndex < 0) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	R_SetShaderParameterAnimation(shaderId, paramIndex, animType, speed, min, max);
	lua_pushboolean(L, 1);
	return 1;
}

/*
=================
Lua_RendererAddFlare
=================
Lua binding: renderer_add_flare(x, y, z, r, g, b, size) -> flare_id
=================
*/
static int Lua_RendererAddFlare(lua_State *L)
{
	vec3_t origin, color;
	float size;
	int numArgs = lua_gettop(L);
	
	if (numArgs < 7) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) ||
		!lua_isnumber(L, 4) || !lua_isnumber(L, 5) || !lua_isnumber(L, 6) ||
		!lua_isnumber(L, 7)) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	origin[0] = (float)lua_tonumber(L, 1);
	origin[1] = (float)lua_tonumber(L, 2);
	origin[2] = (float)lua_tonumber(L, 3);
	
	color[0] = (float)lua_tonumber(L, 4);
	color[1] = (float)lua_tonumber(L, 5);
	color[2] = (float)lua_tonumber(L, 6);
	
	size = (float)lua_tonumber(L, 7);
	
	// TODO: Implement actual flare addition when renderer integration is added
	Com_DPrintf("Lua_RendererAddFlare: Adding flare at (%.2f, %.2f, %.2f)\n",
		origin[0], origin[1], origin[2]);
	
	// Return flare ID (placeholder)
	lua_pushinteger(L, 0);
	return 1;
}

/*
=================
Lua_RendererCreateEmitter
=================
Lua binding: renderer_create_emitter(type, x, y, z, ...) -> emitter_id
=================
*/
static int Lua_RendererCreateEmitter(lua_State *L)
{
	int type;
	vec3_t origin, mins, maxs;
	float radius, height;
	int emitterId;
	
	if (lua_gettop(L) < 4) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	type = (int)lua_tointeger(L, 1);
	origin[0] = (float)lua_tonumber(L, 2);
	origin[1] = (float)lua_tonumber(L, 3);
	origin[2] = (float)lua_tonumber(L, 4);
	
	// Default values
	VectorSet(mins, -10.0f, -10.0f, -10.0f);
	VectorSet(maxs, 10.0f, 10.0f, 10.0f);
	radius = 10.0f;
	height = 20.0f;
	
	// Get optional parameters
	if (lua_gettop(L) >= 7) {
		mins[0] = (float)lua_tonumber(L, 5);
		mins[1] = (float)lua_tonumber(L, 6);
		mins[2] = (float)lua_tonumber(L, 7);
	}
	if (lua_gettop(L) >= 10) {
		maxs[0] = (float)lua_tonumber(L, 8);
		maxs[1] = (float)lua_tonumber(L, 9);
		maxs[2] = (float)lua_tonumber(L, 10);
	}
	if (lua_gettop(L) >= 11) {
		radius = (float)lua_tonumber(L, 11);
	}
	if (lua_gettop(L) >= 12) {
		height = (float)lua_tonumber(L, 12);
	}
	
	emitterId = R_CreateParticleEmitter(type, origin, mins, maxs, radius, height);
	lua_pushinteger(L, emitterId);
	return 1;
}

/*
=================
Lua_RendererSetEmitterRate
=================
Lua binding: renderer_set_emitter_rate(emitter_id, rate)
=================
*/
static int Lua_RendererSetEmitterRate(lua_State *L)
{
	int emitterId;
	float rate;
	
	if (lua_gettop(L) < 2) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	emitterId = (int)lua_tointeger(L, 1);
	rate = (float)lua_tonumber(L, 2);
	
	R_SetEmitterRate(emitterId, rate);
	lua_pushboolean(L, 1);
	return 1;
}

/*
=================
Lua_RegisterRendererBindings
=================
Register all renderer bindings with a Lua state
=================
*/
void Lua_RegisterRendererBindings(lua_State *L)
{
	if (!L)
		return;
	
	Lua_RegisterFunction(L, "renderer_spawn_particle", Lua_RendererSpawnParticle);
	Lua_RegisterFunction(L, "renderer_set_shader_param", Lua_RendererSetShaderParameter);
	Lua_RegisterFunction(L, "renderer_add_flare", Lua_RendererAddFlare);
	Lua_RegisterFunction(L, "renderer_create_emitter", Lua_RendererCreateEmitter);
	Lua_RegisterFunction(L, "renderer_set_emitter_rate", Lua_RendererSetEmitterRate);
	Lua_RegisterFunction(L, "renderer_create_procedural_shader", Lua_RendererCreateProceduralShader);
	Lua_RegisterFunction(L, "renderer_set_shader_pbr", Lua_RendererSetShaderPBR);
	Lua_RegisterFunction(L, "renderer_set_shader_param_animation", Lua_RendererSetShaderParameterAnimation);
	
	// Register material system bindings
	extern void vk_material_register_lua_functions( void *luaState );
	vk_material_register_lua_functions( L );
	
	// Register atmosphere system bindings
	extern void vk_atmosphere_register_lua_functions( void *luaState );
	vk_atmosphere_register_lua_functions( L );
}

#endif // USE_LUA

