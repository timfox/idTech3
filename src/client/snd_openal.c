#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/q_log.h"
#include "snd_public.h"
#include "snd_local.h"
#include "snd_openal.h"

#ifdef USE_OPENAL
#include <AL/al.h>
#include <AL/alc.h>

static ALCdevice *openalDevice = NULL;
static ALCcontext *openalContext = NULL;
static cvar_t *s_openal_enabled;
static cvar_t *s_openal_3d;
static cvar_t *s_openal_occlusion;
static cvar_t *s_openal_reverb;

#define MAX_OPENAL_SOURCES		256
#define MAX_OPENAL_STREAMS		16

typedef struct {
	sndOpenALHandle_t	handle;
	ALuint				source;
	sfxHandle_t			sfxHandle;
	vec3_t				position;
	vec3_t				velocity;
	float				volume;
	float				pitch;
	float				occlusionFactor;
	float				reverbLevel;
	float				reverbDelay;
	qboolean			playing;
	qboolean			looping;
	int					flags;
	int					entityNum;
	int					channel;
} openalSound_t;

static openalSound_t openalSounds[MAX_OPENAL_SOURCES];
static int numOpenALSounds = 0;

/*
=================
SndOpenAL_Init
=================
Initialize OpenAL audio system
=================
*/
qboolean SndOpenAL_Init(void)
{
	int i;
	const ALCchar *deviceName;
	
	s_openal_enabled = Cvar_Get("s_openal_enabled", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(s_openal_enabled, "Enable OpenAL enhanced audio system");
	
	s_openal_3d = Cvar_Get("s_openal_3d", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(s_openal_3d, "Enable 3D positional audio with OpenAL");
	
	s_openal_occlusion = Cvar_Get("s_openal_occlusion", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(s_openal_occlusion, "Enable sound occlusion/obstruction");
	
	s_openal_reverb = Cvar_Get("s_openal_reverb", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(s_openal_reverb, "Enable reverb effects");
	
	if (!s_openal_enabled->integer) {
		return qfalse;
	}
	
	// Open default device
	deviceName = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
	openalDevice = alcOpenDevice(deviceName);
	if (!openalDevice) {
		LOG_SOUND_WARN("OpenAL: failed to open default device");
		return qfalse;
	}
	
	// Create context
	openalContext = alcCreateContext(openalDevice, NULL);
	if (!openalContext) {
		LOG_SOUND_WARN("OpenAL: failed to create context");
		alcCloseDevice(openalDevice);
		openalDevice = NULL;
		return qfalse;
	}
	
	// Make context current
	if (!alcMakeContextCurrent(openalContext)) {
		LOG_SOUND_WARN("OpenAL: failed to make context current");
		alcDestroyContext(openalContext);
		alcCloseDevice(openalDevice);
		openalContext = NULL;
		openalDevice = NULL;
		return qfalse;
	}
	LOG_SOUND_INFO("OpenAL initialized (device: %s)", deviceName ? deviceName : "unknown");
	
	// Initialize sound slots
	for (i = 0; i < MAX_OPENAL_SOURCES; i++) {
		openalSounds[i].handle = SND_OPENAL_INVALID_HANDLE;
		openalSounds[i].playing = qfalse;
		openalSounds[i].source = 0;
	}
	
	numOpenALSounds = 0;
	
	Com_Printf("OpenAL enhanced audio system initialized\n");
	return qtrue;
}

/*
=================
SndOpenAL_Shutdown
=================
Shutdown OpenAL audio system
=================
*/
void SndOpenAL_Shutdown(void)
{
	int i;
	
	SndOpenAL_StopAllSounds();
	
	// Release all sources
	for (i = 0; i < MAX_OPENAL_SOURCES; i++) {
		if (openalSounds[i].source) {
			alDeleteSources(1, &openalSounds[i].source);
			openalSounds[i].source = 0;
		}
	}
	
	// Destroy context and close device
	if (openalContext) {
		alcMakeContextCurrent(NULL);
		alcDestroyContext(openalContext);
		openalContext = NULL;
	}
	
	if (openalDevice) {
		alcCloseDevice(openalDevice);
		openalDevice = NULL;
	}
	
	numOpenALSounds = 0;
}

/*
=================
SndOpenAL_FindFreeSlot
=================
Find a free sound slot
=================
*/
static int SndOpenAL_FindFreeSlot(void)
{
	int i;
	
	for (i = 0; i < MAX_OPENAL_SOURCES; i++) {
		if (!openalSounds[i].playing || openalSounds[i].handle == SND_OPENAL_INVALID_HANDLE) {
			return i;
		}
	}
	
	return -1;
}

/*
=================
SndOpenAL_PlaySound
=================
Play a sound with enhanced 3D properties
=================
*/
sndOpenALHandle_t SndOpenAL_PlaySound(const char *soundName, const sndOpenAL3DProps_t *props)
{
	int slot;
	openalSound_t *sound;
	sfxHandle_t sfxHandle;
	ALuint source;
	// TODO: Implement OpenAL sound playback
	// ALuint buffer;
	// sfx_t *sfx;
	
	if (!s_openal_enabled || !s_openal_enabled->integer) {
		// Fallback to standard sound system
		if (soundName) {
			sfxHandle = S_RegisterSound(soundName, qfalse);
			if (sfxHandle) {
				if (props && (props->flags & SND_OPENAL_3D)) {
					// Cast away const qualifier for compatibility with S_StartSound API
					S_StartSound((vec_t *)props->position, 0, 0, sfxHandle);
				} else {
					S_StartLocalSound(sfxHandle, 0);
				}
			}
		}
		return 0;
	}
	
	if (!soundName || !*soundName)
		return SND_OPENAL_INVALID_HANDLE;
	
	slot = SndOpenAL_FindFreeSlot();
	if (slot < 0)
		return SND_OPENAL_INVALID_HANDLE;
	
	sound = &openalSounds[slot];
	
	// Register sound
	sfxHandle = S_RegisterSound(soundName, qfalse);
	if (!sfxHandle)
		return SND_OPENAL_INVALID_HANDLE;
	
	// TODO: Get sound effect data for OpenAL buffer creation
	// sfx = S_GetSfxByHandle(sfxHandle);
	// if (!sfx)
	//	return SND_OPENAL_INVALID_HANDLE;
	
	// Generate OpenAL source
	alGenSources(1, &source);
	if (alGetError() != AL_NO_ERROR) {
		return SND_OPENAL_INVALID_HANDLE;
	}
	
	// TODO: Create OpenAL buffer from sfx data
	// For now, use standard sound system
	// This would require converting sfx->soundData to OpenAL buffer format
	
	// Initialize sound
	sound->handle = slot;
	sound->source = source;
	sound->sfxHandle = sfxHandle;
	sound->playing = qtrue;
	sound->looping = qfalse;
	sound->volume = 1.0f;
	sound->pitch = 1.0f;
	sound->occlusionFactor = 1.0f;
	sound->reverbLevel = 0.0f;
	sound->reverbDelay = 0.0f;
	sound->flags = 0;
	sound->entityNum = 0;
	sound->channel = 0;
	
	VectorClear(sound->position);
	VectorClear(sound->velocity);
	
	if (props) {
		if (props->flags & SND_OPENAL_3D && s_openal_3d->integer) {
			VectorCopy(props->position, sound->position);
			VectorCopy(props->velocity, sound->velocity);
			sound->flags |= SND_OPENAL_3D;
			
			// Set 3D position
			alSource3f(source, AL_POSITION, props->position[0], props->position[1], props->position[2]);
			alSource3f(source, AL_VELOCITY, props->velocity[0], props->velocity[1], props->velocity[2]);
			alSourcef(source, AL_REFERENCE_DISTANCE, props->minDistance);
			alSourcef(source, AL_MAX_DISTANCE, props->maxDistance);
		}
		sound->volume = props->volume;
		sound->pitch = props->pitch;
		sound->looping = props->looping;
		sound->flags |= props->flags;
		
		alSourcef(source, AL_GAIN, props->volume);
		alSourcef(source, AL_PITCH, props->pitch);
		alSourcei(source, AL_LOOPING, props->looping ? AL_TRUE : AL_FALSE);
	}
	
	// Use standard sound system for now
	// TODO: Implement full OpenAL playback
	if (sound->flags & SND_OPENAL_3D) {
		S_StartSound(sound->position, sound->entityNum, sound->channel, sfxHandle);
	} else {
		S_StartLocalSound(sfxHandle, sound->channel);
	}
	
	numOpenALSounds++;
	return sound->handle;
}

/*
=================
SndOpenAL_StopSound
=================
Stop a playing sound
=================
*/
void SndOpenAL_StopSound(sndOpenALHandle_t handle)
{
	openalSound_t *sound;
	
	if (handle < 0 || handle >= MAX_OPENAL_SOURCES)
		return;
	
	sound = &openalSounds[handle];
	if (!sound->playing)
		return;
	
	if (sound->source) {
		alSourceStop(sound->source);
		alDeleteSources(1, &sound->source);
		sound->source = 0;
	}
	
	S_StopLoopingSound(sound->entityNum);
	sound->playing = qfalse;
	sound->handle = SND_OPENAL_INVALID_HANDLE;
	numOpenALSounds--;
}

/*
=================
SndOpenAL_StopAllSounds
=================
Stop all playing sounds
=================
*/
void SndOpenAL_StopAllSounds(void)
{
	int i;
	
	for (i = 0; i < MAX_OPENAL_SOURCES; i++) {
		if (openalSounds[i].playing) {
			SndOpenAL_StopSound(openalSounds[i].handle);
		}
	}
}

/*
=================
SndOpenAL_SetListenerPosition
=================
Set listener position and orientation for 3D audio
=================
*/
void SndOpenAL_SetListenerPosition(const vec3_t position, const vec3_t forward, const vec3_t up, const vec3_t velocity)
{
	if (!s_openal_enabled || !s_openal_enabled->integer || !openalContext)
		return;
	
	ALfloat orientation[6];
	
	// Set position
	alListener3f(AL_POSITION, position[0], position[1], position[2]);
	
	// Set velocity
	alListener3f(AL_VELOCITY, velocity[0], velocity[1], velocity[2]);
	
	// Set orientation (forward and up vectors)
	orientation[0] = forward[0];
	orientation[1] = forward[1];
	orientation[2] = forward[2];
	orientation[3] = up[0];
	orientation[4] = up[1];
	orientation[5] = up[2];
	alListenerfv(AL_ORIENTATION, orientation);
}

/*
=================
SndOpenAL_SetSoundPosition
=================
Update 3D sound position
=================
*/
void SndOpenAL_SetSoundPosition(sndOpenALHandle_t handle, const vec3_t position)
{
	openalSound_t *sound;
	
	if (handle < 0 || handle >= MAX_OPENAL_SOURCES)
		return;
	
	sound = &openalSounds[handle];
	if (!sound->playing || !sound->source)
		return;
	
	if (sound->flags & SND_OPENAL_3D) {
		VectorCopy(position, sound->position);
		alSource3f(sound->source, AL_POSITION, position[0], position[1], position[2]);
	}
}

/*
=================
SndOpenAL_SetSoundVelocity
=================
Update 3D sound velocity for Doppler effect
=================
*/
void SndOpenAL_SetSoundVelocity(sndOpenALHandle_t handle, const vec3_t velocity)
{
	openalSound_t *sound;
	
	if (handle < 0 || handle >= MAX_OPENAL_SOURCES)
		return;
	
	sound = &openalSounds[handle];
	if (!sound->playing || !sound->source)
		return;
	
	if (sound->flags & SND_OPENAL_3D) {
		VectorCopy(velocity, sound->velocity);
		alSource3f(sound->source, AL_VELOCITY, velocity[0], velocity[1], velocity[2]);
	}
}

/*
=================
SndOpenAL_SetSoundVolume
=================
Set sound volume
=================
*/
void SndOpenAL_SetSoundVolume(sndOpenALHandle_t handle, float volume)
{
	openalSound_t *sound;
	
	if (handle < 0 || handle >= MAX_OPENAL_SOURCES)
		return;
	
	sound = &openalSounds[handle];
	if (!sound->playing || !sound->source)
		return;
	
	sound->volume = volume;
	if (sound->volume < 0.0f) sound->volume = 0.0f;
	if (sound->volume > 1.0f) sound->volume = 1.0f;
	
	alSourcef(sound->source, AL_GAIN, sound->volume);
}

/*
=================
SndOpenAL_SetSoundPitch
=================
Set sound pitch
=================
*/
void SndOpenAL_SetSoundPitch(sndOpenALHandle_t handle, float pitch)
{
	openalSound_t *sound;
	
	if (handle < 0 || handle >= MAX_OPENAL_SOURCES)
		return;
	
	sound = &openalSounds[handle];
	if (!sound->playing || !sound->source)
		return;
	
	sound->pitch = pitch;
	if (sound->pitch < 0.1f) sound->pitch = 0.1f;
	if (sound->pitch > 2.0f) sound->pitch = 2.0f;
	
	alSourcef(sound->source, AL_PITCH, sound->pitch);
}

/*
=================
SndOpenAL_IsSoundPlaying
=================
Check if sound is still playing
=================
*/
qboolean SndOpenAL_IsSoundPlaying(sndOpenALHandle_t handle)
{
	openalSound_t *sound;
	ALint state;
	
	if (handle < 0 || handle >= MAX_OPENAL_SOURCES)
		return qfalse;
	
	sound = &openalSounds[handle];
	if (!sound->playing || !sound->source)
		return qfalse;
	
	alGetSourcei(sound->source, AL_SOURCE_STATE, &state);
	return (state == AL_PLAYING);
}

/*
=================
SndOpenAL_SetOcclusion
=================
Set sound occlusion factor (0.0 = fully occluded, 1.0 = no occlusion)
=================
*/
void SndOpenAL_SetOcclusion(sndOpenALHandle_t handle, float occlusionFactor)
{
	openalSound_t *sound;
	
	if (!s_openal_occlusion || !s_openal_occlusion->integer)
		return;
	
	if (handle < 0 || handle >= MAX_OPENAL_SOURCES)
		return;
	
	sound = &openalSounds[handle];
	if (!sound->playing || !sound->source)
		return;
	
	sound->occlusionFactor = occlusionFactor;
	if (sound->occlusionFactor < 0.0f) sound->occlusionFactor = 0.0f;
	if (sound->occlusionFactor > 1.0f) sound->occlusionFactor = 1.0f;
	
	// Apply occlusion by reducing gain
	alSourcef(sound->source, AL_GAIN, sound->volume * sound->occlusionFactor);
}

/*
=================
SndOpenAL_SetReverb
=================
Set reverb effect parameters
=================
*/
void SndOpenAL_SetReverb(sndOpenALHandle_t handle, float reverbLevel, float reverbDelay)
{
	openalSound_t *sound;
	
	if (!s_openal_reverb || !s_openal_reverb->integer)
		return;
	
	if (handle < 0 || handle >= MAX_OPENAL_SOURCES)
		return;
	
	sound = &openalSounds[handle];
	if (!sound->playing || !sound->source)
		return;
	
	sound->reverbLevel = reverbLevel;
	sound->reverbDelay = reverbDelay;
	
	// TODO: Apply reverb using OpenAL EFX extension if available
}

/*
=================
SndOpenAL_StartStream
=================
Start streaming audio
=================
*/
sndOpenALHandle_t SndOpenAL_StartStream(const char *streamName, qboolean looping)
{
	// TODO: Implement audio streaming with OpenAL
	// For now, use standard background track system
	if (streamName && *streamName) {
		S_StartBackgroundTrack(streamName, looping ? streamName : NULL);
		return 0;
	}
	return SND_OPENAL_INVALID_HANDLE;
}

/*
=================
SndOpenAL_StopStream
=================
Stop streaming audio
=================
*/
void SndOpenAL_StopStream(sndOpenALHandle_t handle)
{
	(void)handle; // Unused parameter - kept for API compatibility
	// TODO: Implement stream stopping
	S_StopBackgroundTrack();
}

#else // !USE_OPENAL

// Stub implementations when OpenAL is not available
qboolean SndOpenAL_Init(void) { return qfalse; }
void SndOpenAL_Shutdown(void) {}
sndOpenALHandle_t SndOpenAL_PlaySound(const char *soundName, const sndOpenAL3DProps_t *props) { return SND_OPENAL_INVALID_HANDLE; }
void SndOpenAL_StopSound(sndOpenALHandle_t handle) {}
void SndOpenAL_StopAllSounds(void) {}
void SndOpenAL_SetListenerPosition(const vec3_t position, const vec3_t forward, const vec3_t up, const vec3_t velocity) {}
void SndOpenAL_SetSoundPosition(sndOpenALHandle_t handle, const vec3_t position) {}
void SndOpenAL_SetSoundVelocity(sndOpenALHandle_t handle, const vec3_t velocity) {}
void SndOpenAL_SetSoundVolume(sndOpenALHandle_t handle, float volume) {}
void SndOpenAL_SetSoundPitch(sndOpenALHandle_t handle, float pitch) {}
qboolean SndOpenAL_IsSoundPlaying(sndOpenALHandle_t handle) { return qfalse; }
void SndOpenAL_SetOcclusion(sndOpenALHandle_t handle, float occlusionFactor) {}
void SndOpenAL_SetReverb(sndOpenALHandle_t handle, float reverbLevel, float reverbDelay) {}
sndOpenALHandle_t SndOpenAL_StartStream(const char *streamName, qboolean looping) { return SND_OPENAL_INVALID_HANDLE; }
void SndOpenAL_StopStream(sndOpenALHandle_t handle) { (void)handle; }

#endif // USE_OPENAL

