#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_LUA
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/*
=================
Lua_SoundPlay
=================
Lua binding: sound_play(sound_name, x, y, z, volume) -> sound_id
=================
*/
static int Lua_SoundPlay(lua_State *L)
{
	const char *soundName;
	vec3_t origin;
	float volume;
	int numArgs = lua_gettop(L);
	
	if (numArgs < 1) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	soundName = lua_tostring(L, 1);
	if (!soundName) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	// Optional position (x, y, z)
	if (numArgs >= 4 && lua_isnumber(L, 2) && lua_isnumber(L, 3) && lua_isnumber(L, 4)) {
		origin[0] = (float)lua_tonumber(L, 2);
		origin[1] = (float)lua_tonumber(L, 3);
		origin[2] = (float)lua_tonumber(L, 4);
	} else {
		VectorClear(origin);
	}
	
	// Optional volume (default 1.0)
	if (numArgs >= 5 && lua_isnumber(L, 5)) {
		volume = (float)lua_tonumber(L, 5);
		if (volume < 0.0f) volume = 0.0f;
		if (volume > 1.0f) volume = 1.0f;
	} else {
		volume = 1.0f;
	}
	
	// TODO: Implement actual sound playback when sound system integration is added
	Com_DPrintf("Lua_SoundPlay: Playing sound %s at (%.2f, %.2f, %.2f) volume %.2f\n",
		soundName, origin[0], origin[1], origin[2], volume);
	
	// Return sound ID (placeholder)
	lua_pushinteger(L, 0);
	return 1;
}

/*
=================
Lua_SoundStop
=================
Lua binding: sound_stop(sound_id)
=================
*/
static int Lua_SoundStop(lua_State *L)
{
	int soundId;
	
	if (lua_gettop(L) < 1 || !lua_isnumber(L, 1)) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	soundId = (int)lua_tointeger(L, 1);
	
	// TODO: Implement actual sound stopping when sound system integration is added
	Com_DPrintf("Lua_SoundStop: Stopping sound %d\n", soundId);
	
	lua_pushboolean(L, 1);
	return 1;
}

/*
=================
Lua_SoundSetVolume
=================
Lua binding: sound_set_volume(sound_id, volume)
=================
*/
static int Lua_SoundSetVolume(lua_State *L)
{
	int soundId;
	float volume;
	
	if (lua_gettop(L) < 2) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
		lua_pushboolean(L, 0);
		return 1;
	}
	
	soundId = (int)lua_tointeger(L, 1);
	volume = (float)lua_tonumber(L, 2);
	
	if (volume < 0.0f) volume = 0.0f;
	if (volume > 1.0f) volume = 1.0f;
	
	// TODO: Implement actual volume setting when sound system integration is added
	Com_DPrintf("Lua_SoundSetVolume: Setting sound %d volume to %.2f\n", soundId, volume);
	
	lua_pushboolean(L, 1);
	return 1;
}

/*
=================
Lua_RegisterSoundBindings
=================
Register all sound bindings with a Lua state
=================
*/
void Lua_RegisterSoundBindings(lua_State *L)
{
	if (!L)
		return;
	
	Lua_RegisterFunction(L, "sound_play", Lua_SoundPlay);
	Lua_RegisterFunction(L, "sound_stop", Lua_SoundStop);
	Lua_RegisterFunction(L, "sound_set_volume", Lua_SoundSetVolume);
}

#endif // USE_LUA

