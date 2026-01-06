#include "../common/q_shared.h"
#include "../common/qcommon.h"
#include "../common/q_log.h"
#include "snd_public.h"
#include "snd_local.h"
#include "snd_openal.h"

// Global variables for OpenAL state (available even when OpenAL is not compiled)
int numOpenALSounds = 0;
#ifdef USE_OPENAL
ALCdevice *openalDevice = NULL;
ALCcontext *openalContext = NULL;
#else
void *openalDevice = NULL;
void *openalContext = NULL;
#endif
qboolean openalEfxAvailable = qfalse;

#ifdef USE_OPENAL
#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>

static cvar_t *s_openal_enabled;
static cvar_t *s_openal_3d;
static cvar_t *s_openal_occlusion;
static cvar_t *s_openal_reverb;
static ALuint openalReverbEffect = 0;
static ALuint openalReverbSlot = 0;

#define MAX_OPENAL_SOURCES		256
#define MAX_OPENAL_STREAMS		16

typedef struct {
	sndOpenALHandle_t	handle;
	ALuint				source;
	ALuint				buffer;
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

static qboolean SndOpenAL_CreateBufferFromSfx( sfxHandle_t sfxHandle, ALuint *outBuffer );
static qboolean SndOpenAL_AttachBufferToSource( ALuint source, ALuint buffer );
static void SndOpenAL_InitEfx(void);
static void SndOpenAL_ShutdownEfx(void);

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
	
	s_openal_occlusion = Cvar_Get("s_openal_occlusion", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(s_openal_occlusion, "Enable sound occlusion/obstruction (may impact performance)");

	s_openal_reverb = Cvar_Get("s_openal_reverb", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(s_openal_reverb, "Enable reverb effects (may impact performance)");
	
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

	SndOpenAL_InitEfx();
	
	// Initialize sound slots
	for (i = 0; i < MAX_OPENAL_SOURCES; i++) {
		openalSounds[i].handle = SND_OPENAL_INVALID_HANDLE;
		openalSounds[i].playing = qfalse;
		openalSounds[i].source = 0;
		openalSounds[i].buffer = 0;
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
		if (openalSounds[i].buffer) {
			alDeleteBuffers(1, &openalSounds[i].buffer);
			openalSounds[i].buffer = 0;
		}
	}

	SndOpenAL_ShutdownEfx();
	
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
SndOpenAL_CreateBufferFromSfx
=================
Create an OpenAL buffer by flattening the engine's sfx data into
interleaved 16-bit PCM and uploading it to the device.
=================
*/
static qboolean SndOpenAL_CreateBufferFromSfx( sfxHandle_t sfxHandle, ALuint *outBuffer ) {
	sfx_t *sfx;
	ALenum format;
	size_t sampleCount, byteCount, copied;
	short *pcm;
	sndBuffer *chunk;

	sfx = S_GetSfxByHandle( sfxHandle );
	if ( !sfx ) {
		return qfalse;
	}

	// Currently only mono/stereo 16-bit are expected after resampling
	if ( sfx->soundChannels <= 1 ) {
		format = AL_FORMAT_MONO16;
	} else {
		format = AL_FORMAT_STEREO16;
	}

	sampleCount = (size_t)sfx->soundLength * (size_t)sfx->soundChannels;
	byteCount = sampleCount * sizeof(short);
	if ( sampleCount == 0 || byteCount == 0 ) {
		return qfalse;
	}

	pcm = Z_Malloc( byteCount );
	if ( !pcm ) {
		return qfalse;
	}

	// Flatten the linked chunks into contiguous PCM
	chunk = sfx->soundData;
	copied = 0;
	while ( chunk && copied < sampleCount ) {
		size_t toCopy = SND_CHUNK_SIZE;
		if ( copied + toCopy > sampleCount ) {
			toCopy = sampleCount - copied;
		}
		Com_Memcpy( pcm + copied, chunk->sndChunk, toCopy * sizeof(short) );
		copied += toCopy;
		chunk = chunk->next;
	}

	if ( copied < sampleCount ) {
		Z_Free( pcm );
		return qfalse;
	}

	alGenBuffers( 1, outBuffer );
	if ( alGetError() != AL_NO_ERROR ) {
		Z_Free( pcm );
		return qfalse;
	}

	alBufferData( *outBuffer, format, pcm, (ALsizei)byteCount, dma.speed );
	Z_Free( pcm );

	return ( alGetError() == AL_NO_ERROR );
}

static qboolean SndOpenAL_AttachBufferToSource( ALuint source, ALuint buffer ) {
	alSourcei( source, AL_BUFFER, buffer );
	return ( alGetError() == AL_NO_ERROR );
}

/*
=================
SndOpenAL_InitEfx
=================
Detect and set up a shared reverb effect/slot if the EFX extension is present.
=================
*/
static void SndOpenAL_InitEfx(void) {
#ifdef AL_EFFECT_REVERB
	if (!openalDevice || !openalContext) {
		return;
	}

	if (!alcIsExtensionPresent(openalDevice, "ALC_EXT_EFX")) {
		return;
	}

	alGetError(); // clear

	// Set global Doppler effect parameters
	alDopplerFactor(1.0f);     // Scale factor for Doppler effect
	alDopplerVelocity(343.3f); // Speed of sound in air (m/s)

	alGenEffects(1, &openalReverbEffect);
	if (alGetError() != AL_NO_ERROR) {
		openalReverbEffect = 0;
		return;
	}

	alEffecti(openalReverbEffect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
	if (alGetError() != AL_NO_ERROR) {
		alDeleteEffects(1, &openalReverbEffect);
		openalReverbEffect = 0;
		return;
	}

	// Reasonable defaults
	alEffectf(openalReverbEffect, AL_REVERB_GAIN, 0.32f);
	alEffectf(openalReverbEffect, AL_REVERB_DECAY_TIME, 1.5f);

	alGenAuxiliaryEffectSlots(1, &openalReverbSlot);
	if (alGetError() != AL_NO_ERROR) {
		alDeleteEffects(1, &openalReverbEffect);
		openalReverbEffect = 0;
		openalReverbSlot = 0;
		return;
	}

	alAuxiliaryEffectSloti(openalReverbSlot, AL_EFFECTSLOT_EFFECT, openalReverbEffect);
	if (alGetError() != AL_NO_ERROR) {
		alDeleteAuxiliaryEffectSlots(1, &openalReverbSlot);
		alDeleteEffects(1, &openalReverbEffect);
		openalReverbSlot = 0;
		openalReverbEffect = 0;
		return;
	}

	openalEfxAvailable = qtrue;
#endif
}

static void SndOpenAL_ShutdownEfx(void) {
#ifdef AL_EFFECT_REVERB
	if (openalReverbSlot) {
		alDeleteAuxiliaryEffectSlots(1, &openalReverbSlot);
		openalReverbSlot = 0;
	}
	if (openalReverbEffect) {
		alDeleteEffects(1, &openalReverbEffect);
		openalReverbEffect = 0;
	}
	openalEfxAvailable = qfalse;
#endif
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
	int slot = -1;
	openalSound_t *sound = NULL;
	sfxHandle_t sfxHandle;
	ALuint source = 0;
	ALuint buffer = 0;
	
	if (!s_openal_enabled || !s_openal_enabled->integer || !openalContext) {
		goto fallback_legacy;
	}
	
	if (!soundName || !*soundName) {
		return SND_OPENAL_INVALID_HANDLE;
	}
	
	slot = SndOpenAL_FindFreeSlot();
	if (slot < 0)
		return SND_OPENAL_INVALID_HANDLE;
	
	sound = &openalSounds[slot];
	
	// Register sound
	sfxHandle = S_RegisterSound(soundName, qfalse);
	if (!sfxHandle)
		goto fallback_legacy;
	
	// Generate OpenAL source
	alGenSources(1, &source);
	if (alGetError() != AL_NO_ERROR) {
		goto fallback_legacy;
	}

	// Upload buffer
	if (!SndOpenAL_CreateBufferFromSfx( sfxHandle, &buffer )) {
		alDeleteSources(1, &source);
		goto fallback_legacy;
	}
	
	// Initialize sound
	sound->handle = slot;
	sound->source = source;
	sound->buffer = buffer;
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

			// Enhanced 3D spatialization
			alSource3f(source, AL_POSITION, props->position[0], props->position[1], props->position[2]);
			alSource3f(source, AL_VELOCITY, props->velocity[0], props->velocity[1], props->velocity[2]);

			// Distance attenuation with better defaults
			float minDist = props->minDistance > 0 ? props->minDistance : 128.0f;
			float maxDist = props->maxDistance > 0 ? props->maxDistance : 1024.0f;
			alSourcef(source, AL_REFERENCE_DISTANCE, minDist);
			alSourcef(source, AL_MAX_DISTANCE, maxDist);
			alSourcef(source, AL_ROLLOFF_FACTOR, 1.0f); // Linear distance attenuation

			// Directional sound cone for more realistic spatialization
			alSourcef(source, AL_CONE_INNER_ANGLE, 360.0f); // Omnidirectional
			alSourcef(source, AL_CONE_OUTER_ANGLE, 360.0f);
			alSourcef(source, AL_CONE_OUTER_GAIN, 0.0f);

			// Enable Doppler effect for moving sounds
			if (props->flags & SND_OPENAL_DOPPLER && s_doppler && s_doppler->integer) {
				// Doppler settings are global, but we can adjust per-source velocity
				alSource3f(source, AL_VELOCITY, props->velocity[0], props->velocity[1], props->velocity[2]);
			}
		} else {
			// 2D sounds are positioned relative to listener
			alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
			alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
			alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
		}
		sound->volume = props->volume;
		sound->pitch = props->pitch;
		sound->looping = props->looping;
		sound->flags |= props->flags;
		
		alSourcef(source, AL_GAIN, props->volume);
		alSourcef(source, AL_PITCH, props->pitch);
		alSourcei(source, AL_LOOPING, props->looping ? AL_TRUE : AL_FALSE);
	}
	
	if (!SndOpenAL_AttachBufferToSource( source, buffer )) {
		alDeleteSources(1, &source);
		alDeleteBuffers(1, &buffer);
		goto fallback_legacy;
	}

	alSourcePlay( source );
	if ( alGetError() != AL_NO_ERROR ) {
		alDeleteSources(1, &source);
		alDeleteBuffers(1, &buffer);
		goto fallback_legacy;
	}

	numOpenALSounds++;
	return sound->handle;

fallback_legacy:
	if ( sound ) {
		if ( source ) {
			alDeleteSources( 1, &source );
		}
		if ( buffer ) {
			alDeleteBuffers( 1, &buffer );
		}
		sound->handle = SND_OPENAL_INVALID_HANDLE;
		sound->playing = qfalse;
		sound->source = 0;
		sound->buffer = 0;
	}
	// Fallback to standard sound system to ensure audio is still heard
	if (soundName) {
		sfxHandle_t legacy = S_RegisterSound(soundName, qfalse);
		if (legacy) {
			if (props && (props->flags & SND_OPENAL_3D)) {
				S_StartSound((vec_t *)props->position, 0, 0, legacy);
			} else {
				S_StartLocalSound(legacy, 0);
			}
		}
	}
	return SND_OPENAL_INVALID_HANDLE;
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
	if (sound->buffer) {
		alDeleteBuffers(1, &sound->buffer);
		sound->buffer = 0;
	}
	
	S_StopLoopingSound(sound->entityNum);
	sound->playing = qfalse;
	sound->handle = SND_OPENAL_INVALID_HANDLE;
	if ( numOpenALSounds > 0 ) {
		numOpenALSounds--;
	}
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
	float clampedLevel;
	float clampedDecay;
	
	if (!s_openal_reverb || !s_openal_reverb->integer)
		return;
	
	if (handle < 0 || handle >= MAX_OPENAL_SOURCES)
		return;
	
	sound = &openalSounds[handle];
	if (!sound->playing || !sound->source)
		return;
	
	sound->reverbLevel = reverbLevel;
	sound->reverbDelay = reverbDelay;

#ifdef AL_EFFECT_REVERB
	if (openalEfxAvailable && openalReverbSlot && openalReverbEffect) {
		clampedLevel = Com_Clamp(0.0f, 1.0f, reverbLevel);
		clampedDecay = Com_Clamp(0.1f, 20.0f, reverbDelay > 0.0f ? reverbDelay : 1.0f);

		alEffectf(openalReverbEffect, AL_REVERB_GAIN, clampedLevel);
		alEffectf(openalReverbEffect, AL_REVERB_DECAY_TIME, clampedDecay);
		alAuxiliaryEffectSloti(openalReverbSlot, AL_EFFECTSLOT_EFFECT, openalReverbEffect);

		// Bind the effect send 0 to this source
#ifndef AL_FILTER_NULL
#define AL_FILTER_NULL 0
#endif
		alSource3i(sound->source, AL_AUXILIARY_SEND_FILTER, openalReverbSlot, 0, AL_FILTER_NULL);
	}
#endif
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

/*
=================
SndOpenAL_SetEnvironmentReverb
=================
Set global environmental reverb parameters
=================
*/
void SndOpenAL_SetEnvironmentReverb(float roomSize, float dampening, float wetness, float dryMix)
{
	if (!s_openal_reverb || !s_openal_reverb->integer || !openalEfxAvailable)
		return;

#ifdef AL_EFFECT_REVERB
	if (openalReverbEffect && openalReverbSlot) {
		// Clamp values to reasonable ranges
		float clampedRoomSize = Com_Clamp(0.0f, 1.0f, roomSize);
		float clampedDampening = Com_Clamp(0.0f, 1.0f, dampening);
		float clampedWetness = Com_Clamp(0.0f, 1.0f, wetness);
		float clampedDryMix = Com_Clamp(0.0f, 1.0f, dryMix);

		alEffectf(openalReverbEffect, AL_REVERB_GAIN, clampedWetness);
		alEffectf(openalReverbEffect, AL_REVERB_DECAY_TIME, 0.5f + clampedRoomSize * 2.0f);
		alEffectf(openalReverbEffect, AL_REVERB_DECAY_HFRATIO, 0.5f + clampedDampening * 0.5f);
		alEffectf(openalReverbEffect, AL_REVERB_DENSITY, clampedDryMix);

		alAuxiliaryEffectSloti(openalReverbSlot, AL_EFFECTSLOT_EFFECT, openalReverbEffect);
	}
#endif
}

/*
=================
SndOpenAL_UpdateEnvironmentalAudio
=================
Update environmental audio effects based on listener position
=================
*/
void SndOpenAL_UpdateEnvironmentalAudio(void)
{
	// This would be called regularly to update environmental effects
	// based on the player's current environment (indoor/outdoor, room size, etc.)
	// For now, this is a placeholder for future environmental audio features
}

/*
=================
SndOpenAL_SetSoundCone
=================
Set directional sound cone parameters for more realistic spatialization
=================
*/
void SndOpenAL_SetSoundCone(sndOpenALHandle_t handle, float innerAngle, float outerAngle, float outerGain)
{
	openalSound_t *sound;

	if (!s_openal_3d || !s_openal_3d->integer)
		return;

	if (handle < 0 || handle >= MAX_OPENAL_SOURCES)
		return;

	sound = &openalSounds[handle];
	if (!sound->playing || !sound->source)
		return;

	// Set directional sound cone
	alSourcef(sound->source, AL_CONE_INNER_ANGLE, Com_Clamp(0.0f, 360.0f, innerAngle));
	alSourcef(sound->source, AL_CONE_OUTER_ANGLE, Com_Clamp(0.0f, 360.0f, outerAngle));
	alSourcef(sound->source, AL_CONE_OUTER_GAIN, Com_Clamp(0.0f, 1.0f, outerGain));
}

/*
=================
SndOpenAL_SetSoundDirectivity
=================
Set sound directivity for HRTF-like effects
=================
*/
void SndOpenAL_SetSoundDirectivity(sndOpenALHandle_t handle, float directivity, float directivitySharpness)
{
	openalSound_t *sound;

	if (!s_openal_3d || !s_openal_3d->integer)
		return;

	if (handle < 0 || handle >= MAX_OPENAL_SOURCES)
		return;

	sound = &openalSounds[handle];
	if (!sound->playing || !sound->source)
		return;

	// Directivity affects how directional the sound is
	// Higher directivity = more focused sound field
	float clampedDirectivity = Com_Clamp(0.0f, 1.0f, directivity);
	float clampedSharpness = Com_Clamp(0.1f, 10.0f, directivitySharpness);

	// Map directivity to cone angles
	float innerAngle = 360.0f * (1.0f - clampedDirectivity * 0.8f);
	float outerAngle = innerAngle + (360.0f - innerAngle) * clampedSharpness * 0.1f;
	float outerGain = 1.0f - clampedDirectivity * 0.5f;

	SndOpenAL_SetSoundCone(handle, innerAngle, outerAngle, outerGain);
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
void SndOpenAL_SetEnvironmentReverb(float roomSize, float dampening, float wetness, float dryMix) { (void)roomSize; (void)dampening; (void)wetness; (void)dryMix; }
void SndOpenAL_UpdateEnvironmentalAudio(void) {}
void SndOpenAL_SetSoundCone(sndOpenALHandle_t handle, float innerAngle, float outerAngle, float outerGain) { (void)handle; (void)innerAngle; (void)outerAngle; (void)outerGain; }
void SndOpenAL_SetSoundDirectivity(sndOpenALHandle_t handle, float directivity, float directivitySharpness) { (void)handle; (void)directivity; (void)directivitySharpness; }

#endif // USE_OPENAL

