#include "q_shared.h"
#include "qcommon.h"

#if defined(USE_LUA) && !defined(DEDICATED)
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// Sound system functions are declared in snd_public.h, but we can't include client headers here.
// Declare them as extern - they're defined in client code.
// Note: These functions are only available on the client side.
extern sfxHandle_t S_RegisterSound( const char *sample, qboolean compressed );
extern void S_StartSound( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx );
extern void S_StartLocalSound( sfxHandle_t sfx, int channelNum );

/*
=================
Lua_SoundPlay
=================
Lua binding: sound_play(sound_name, x, y, z, volume) -> sound_id
Plays a sound at the specified position (3D) or locally (2D if no position).
Returns the sound handle (sfxHandle_t) as the sound ID.
=================
*/
static int Lua_SoundPlay(lua_State *L)
{
	const char *soundName;
	vec3_t origin;
	float volume;
	int numArgs = lua_gettop(L);
	qboolean hasPosition = qfalse;
	sfxHandle_t sfxHandle;
	
	if (numArgs < 1) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	soundName = lua_tostring(L, 1);
	if (!soundName || !*soundName) {
		lua_pushinteger(L, -1);
		return 1;
	}
	
	// Optional position (x, y, z)
	if (numArgs >= 4 && lua_isnumber(L, 2) && lua_isnumber(L, 3) && lua_isnumber(L, 4)) {
		origin[0] = (float)lua_tonumber(L, 2);
		origin[1] = (float)lua_tonumber(L, 3);
		origin[2] = (float)lua_tonumber(L, 4);
		hasPosition = qtrue;
	} else {
		VectorClear(origin);
		hasPosition = qfalse;
	}
	
	// Optional volume (default 1.0)
	// Note: The sound system doesn't have per-instance volume control,
	// so this parameter is accepted but not directly applied.
	// Volume is controlled globally via CVars (s_volume, s_musicVolume, etc.)
	if (numArgs >= 5 && lua_isnumber(L, 5)) {
		volume = (float)lua_tonumber(L, 5);
		if (volume < 0.0f) volume = 0.0f;
		if (volume > 1.0f) volume = 1.0f;
		// Volume could be applied via CVar in the future, but for now we just validate it
	} else {
		volume = 1.0f;
	}
	
	// Register the sound
	sfxHandle = S_RegisterSound(soundName, qfalse);
	if (sfxHandle <= 0) {
		Com_DPrintf("Lua_SoundPlay: Failed to register sound %s\n", soundName);
		lua_pushinteger(L, -1);
		return 1;
	}
	
	// Play the sound
	if (hasPosition) {
		// Play as 3D positioned sound
		// Use ENTITYNUM_NONE for non-entity sounds, channel 0
		S_StartSound(origin, ENTITYNUM_NONE, 0, sfxHandle);
	} else {
		// Play as local (2D) sound
		// Channel 0 is typically used for UI sounds
		S_StartLocalSound(sfxHandle, 0);
	}
	
	// Return the sound handle as the sound ID
	// Note: The sound system doesn't track individual instances, so this ID
	// represents the sound effect, not a specific playing instance
	lua_pushinteger(L, (int)sfxHandle);
	return 1;
}

/*
=================
Lua_SoundStop
=================
Lua binding: sound_stop(sound_id)
Note: The sound system doesn't track individual sound instances, so we can't
stop a specific playing sound. This function is kept for API compatibility
but doesn't actually stop sounds. Use S_StopAllSounds() if you need to stop all sounds.
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
	
	// The sound system doesn't provide a way to stop individual sound instances.
	// The soundId returned by sound_play() is the sfxHandle_t, which identifies
	// the sound effect, not a specific playing instance.
	// For now, we just acknowledge the call but can't actually stop the sound.
	Com_DPrintf("Lua_SoundStop: Sound system doesn't support stopping individual instances (sound_id: %d)\n", soundId);
	
	// Return success for API compatibility, even though we can't actually stop it
	lua_pushboolean(L, 1);
	return 1;
}

/*
=================
Lua_SoundSetVolume
=================
Lua binding: sound_set_volume(sound_id, volume)
Note: The sound system doesn't support per-instance volume control.
Volume is controlled globally via CVars (s_volume, s_musicVolume, etc.).
This function is kept for API compatibility but doesn't actually change volume.
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
	
	// The sound system doesn't provide per-instance volume control.
	// Volume is controlled globally via CVars. For now, we just validate
	// the volume parameter but don't apply it.
	Com_DPrintf("Lua_SoundSetVolume: Sound system doesn't support per-instance volume (sound_id: %d, volume: %.2f)\n", soundId, volume);
	
	// Return success for API compatibility
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

#endif // defined(USE_LUA) && !defined(DEDICATED)

