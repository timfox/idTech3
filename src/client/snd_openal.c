#include "../common/q_shared.h"
#include "../common/qcommon.h"
#include "../common/q_log.h"
#include "snd_public.h"
#include "snd_local.h"
#include "snd_openal.h"

// Global variables for OpenAL state (available even when OpenAL is not compiled)
int numOpenALSounds = 0;
void *openalDevice = NULL;
void *openalContext = NULL;
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

typedef struct {
	sndOpenALHandle_t	handle;
	ALuint				source;
	ALuint				buffers[4];		// Streaming buffers (quad-buffered for smooth playback)
	fileHandle_t		fileHandle;
	int					channels;
	int					sampleRate;
	int					bitsPerSample;
	qboolean			playing;
	qboolean			looping;
	qboolean			streaming;
	size_t				bufferSize;
	char				filename[MAX_QPATH];
} openalStream_t;

static openalSound_t openalSounds[MAX_OPENAL_SOURCES];
static openalStream_t openalStreams[MAX_OPENAL_STREAMS];

static qboolean SndOpenAL_CreateBufferFromSfx( sfxHandle_t sfxHandle, ALuint *outBuffer );
static qboolean SndOpenAL_AttachBufferToSource( ALuint source, ALuint buffer );
static void SndOpenAL_InitEfx(void);
static void SndOpenAL_ShutdownEfx(void);

// Streaming functions
static qboolean SndOpenAL_InitStream(openalStream_t *stream, const char *filename, qboolean looping);
static void SndOpenAL_ShutdownStream(openalStream_t *stream);
static qboolean SndOpenAL_UpdateStream(openalStream_t *stream);
static qboolean SndOpenAL_LoadWAVData(const char *filename, int *channels, int *sampleRate, int *bitsPerSample, void **data, size_t *size);

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

	cvar_t *s_openal_hrtf = Cvar_Get("s_openal_hrtf", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(s_openal_hrtf, "Enable HRTF (Head-Related Transfer Function) for realistic 3D audio");

	cvar_t *s_openal_max_distance = Cvar_Get("s_openal_max_distance", "10000", CVAR_ARCHIVE);
	Cvar_SetDescription(s_openal_max_distance, "Maximum distance for 3D audio attenuation");

	cvar_t *s_openal_rolloff = Cvar_Get("s_openal_rolloff", "1.0", CVAR_ARCHIVE);
	Cvar_SetDescription(s_openal_rolloff, "Distance rolloff factor for 3D audio");

	if (!s_openal_enabled->integer) {
		return qfalse;
	}

	// Open default device
	deviceName = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);

	// Try to enable HRTF if requested
	ALCint contextAttrs[5] = {0};
	int attrIndex = 0;
	if (s_openal_hrtf->integer) {
		// Check if HRTF is supported (OpenAL Soft extension)
#ifdef ALC_HRTF_SOFT
		ALCint hrtfStatus = 0;
		alcGetIntegerv(NULL, ALC_HRTF_STATUS_SOFT, 1, &hrtfStatus);
		if (hrtfStatus == ALC_HRTF_ENABLED_SOFT || hrtfStatus == ALC_HRTF_HEADPHONES_DETECTED_SOFT) {
			contextAttrs[attrIndex++] = ALC_HRTF_SOFT;
			contextAttrs[attrIndex++] = ALC_TRUE;
			LOG_SOUND_INFO("OpenAL: HRTF enabled for enhanced spatial audio");
		} else {
			LOG_SOUND_INFO("OpenAL: HRTF not supported by audio device");
		}
#else
		LOG_SOUND_INFO("OpenAL: HRTF not supported by OpenAL implementation");
#endif
	}

	openalDevice = alcOpenDevice(deviceName);
	if (!openalDevice) {
		LOG_SOUND_WARN("OpenAL: failed to open default device");
		return qfalse;
	}

	// Create context with attributes
	openalContext = alcCreateContext(openalDevice, contextAttrs);
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

	// Enhanced reverb settings for realistic environmental audio
	// These settings create a subtle room effect suitable for most game environments
	alEffectf(openalReverbEffect, AL_REVERB_DENSITY, 1.0f);
	alEffectf(openalReverbEffect, AL_REVERB_DIFFUSION, 1.0f);
	alEffectf(openalReverbEffect, AL_REVERB_GAIN, 0.32f);
	alEffectf(openalReverbEffect, AL_REVERB_GAINHF, 0.89f);
	alEffectf(openalReverbEffect, AL_REVERB_DECAY_TIME, 1.49f);
	alEffectf(openalReverbEffect, AL_REVERB_DECAY_HFRATIO, 0.83f);
	alEffectf(openalReverbEffect, AL_REVERB_REFLECTIONS_GAIN, 0.05f);
	alEffectf(openalReverbEffect, AL_REVERB_REFLECTIONS_DELAY, 0.007f);
	alEffectf(openalReverbEffect, AL_REVERB_LATE_REVERB_GAIN, 1.26f);
	alEffectf(openalReverbEffect, AL_REVERB_LATE_REVERB_DELAY, 0.011f);
	alEffectf(openalReverbEffect, AL_REVERB_ROOM_ROLLOFF_FACTOR, 0.0f);
	alEffectf(openalReverbEffect, AL_REVERB_AIR_ABSORPTION_GAINHF, 0.994f);
	alEffectf(openalReverbEffect, AL_EAXREVERB_HFREFERENCE, 5000.0f);

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

			// Distance attenuation with better defaults and configurable rolloff
			float minDist = props->minDistance > 0 ? props->minDistance : 128.0f;
			float maxDist = props->maxDistance > 0 ? props->maxDistance : 1024.0f;
			cvar_t *s_openal_rolloff = Cvar_Get("s_openal_rolloff", "1.0", CVAR_ARCHIVE);

			alSourcef(source, AL_REFERENCE_DISTANCE, minDist);
			alSourcef(source, AL_MAX_DISTANCE, maxDist);
			alSourcef(source, AL_ROLLOFF_FACTOR, s_openal_rolloff->value);

			// Air absorption for distant sounds (high frequency attenuation)
			// This creates more realistic distance-based frequency filtering
			float distance = sqrt(props->position[0]*props->position[0] + props->position[1]*props->position[1] + props->position[2]*props->position[2]);
			float airAbsorptionFactor = 1.0f - (distance / maxDist);
			airAbsorptionFactor = Com_Clamp(0.0f, 1.0f, airAbsorptionFactor);
			alSourcef(source, AL_AIR_ABSORPTION_FACTOR, airAbsorptionFactor * 0.05f); // Subtle effect

			// Directional sound cone for more realistic spatialization
			if (props->flags & SND_OPENAL_DIRECTIONAL) {
				// Use narrower cone for directional sounds
				alSourcef(source, AL_CONE_INNER_ANGLE, 90.0f);
				alSourcef(source, AL_CONE_OUTER_ANGLE, 180.0f);
				alSourcef(source, AL_CONE_OUTER_GAIN, 0.3f);
			} else {
				// Omnidirectional for ambient sounds
				alSourcef(source, AL_CONE_INNER_ANGLE, 360.0f);
				alSourcef(source, AL_CONE_OUTER_ANGLE, 360.0f);
				alSourcef(source, AL_CONE_OUTER_GAIN, 1.0f);
			}

			// Enable Doppler effect for moving sounds
			if (props->flags & SND_OPENAL_DOPPLER && s_doppler && s_doppler->integer) {
				alSource3f(source, AL_VELOCITY, props->velocity[0], props->velocity[1], props->velocity[2]);
			} else {
				alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
			}

			// Room effects for immersive audio
			if (s_openal_reverb && s_openal_reverb->integer && openalEfxAvailable) {
				alSource3f(source, AL_AUXILIARY_SEND_FILTER, openalReverbSlot, 0, AL_FILTER_NULL);
				alSourcef(source, AL_ROOM_ROLLOFF_FACTOR, 1.0f);
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

	// Configure distance model and attenuation for realistic spatialization
	cvar_t *s_openal_max_distance = Cvar_Get("s_openal_max_distance", "10000", CVAR_ARCHIVE);
	cvar_t *s_openal_rolloff = Cvar_Get("s_openal_rolloff", "1.0", CVAR_ARCHIVE);

	// Use inverse distance clamped model for realistic attenuation
	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
	alListenerf(AL_MAX_DISTANCE, s_openal_max_distance->value);
	alListenerf(AL_ROLLOFF_FACTOR, s_openal_rolloff->value);

	// Set global doppler factor for moving sounds (use global variable)
	if (s_doppler) {
		alDopplerFactor(s_doppler->value);
	}
	alDopplerVelocity(343.3f); // Speed of sound in air (m/s)
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
	int slot;
	openalStream_t *stream;

	if (!s_openal_enabled || !s_openal_enabled->integer || !openalContext) {
		// Fallback to legacy system
		if (streamName && *streamName) {
			S_StartBackgroundTrack(streamName, looping ? streamName : NULL);
			return 0;
		}
		return SND_OPENAL_INVALID_HANDLE;
	}

	if (!streamName || !*streamName) {
		return SND_OPENAL_INVALID_HANDLE;
	}

	// Find free stream slot
	for (slot = 0; slot < MAX_OPENAL_STREAMS; slot++) {
		if (!openalStreams[slot].playing) {
			break;
		}
	}

	if (slot >= MAX_OPENAL_STREAMS) {
		Com_Printf("SndOpenAL_StartStream: no free stream slots\n");
		return SND_OPENAL_INVALID_HANDLE;
	}

	stream = &openalStreams[slot];

	// Initialize the stream
	if (!SndOpenAL_InitStream(stream, streamName, looping)) {
		Com_Printf("SndOpenAL_StartStream: failed to initialize stream for %s\n", streamName);
		return SND_OPENAL_INVALID_HANDLE;
	}

	stream->handle = slot;
	stream->playing = qtrue;
	stream->streaming = qtrue;

	Com_DPrintf("Started OpenAL stream: %s (slot %d)\n", streamName, slot);
	return stream->handle;
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
	openalStream_t *stream;

	if (handle < 0 || handle >= MAX_OPENAL_STREAMS) {
		return;
	}

	stream = &openalStreams[handle];

	if (!stream->playing) {
		return;
	}

	// Stop the source
	if (stream->source) {
		alSourceStop(stream->source);
		alSourcei(stream->source, AL_BUFFER, 0); // Unqueue all buffers
	}

	// Clean up the stream
	SndOpenAL_ShutdownStream(stream);

	stream->playing = qfalse;
	stream->streaming = qfalse;
	stream->handle = SND_OPENAL_INVALID_HANDLE;

	Com_DPrintf("Stopped OpenAL stream (slot %d)\n", (int)handle);
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
	if (!s_openal_reverb || !s_openal_reverb->integer || !openalEfxAvailable)
		return;

	// TODO: This should query the game world for environmental information
	// For now, we'll use simplified environmental detection based on player position

	// Get player position and orientation (would come from game state)
	vec3_t playerPos = {0, 0, 0}; // Placeholder - should come from cl.snap.ps.origin
	vec3_t playerForward = {0, 0, 0}; // Placeholder - should come from view angles

	// Simple environmental detection - this should be replaced with proper BSP/occlusion queries
	// For demonstration, we'll use a basic height-based system:
	// - Above ground = outdoor (less reverb)
	// - Below ground = indoor/cave (more reverb)

	qboolean isIndoor = qfalse;
	float roomSize = 1.0f; // Normalized room size (0.0 = small, 1.0 = large)

	// Placeholder environmental detection - should be replaced with proper world queries
	if (playerPos[2] < 0.0f) { // Below ground level
		isIndoor = qtrue;
		roomSize = 0.5f; // Smaller room
	} else {
		isIndoor = qfalse;
		roomSize = 0.8f; // Larger outdoor space
	}

	// Update reverb settings based on environment
	if (openalReverbEffect && openalReverbSlot) {
		alGetError(); // Clear any previous errors

		if (isIndoor) {
			// Indoor environment - more reverb
			alEffectf(openalReverbEffect, AL_REVERB_DENSITY, 1.0f);
			alEffectf(openalReverbEffect, AL_REVERB_DIFFUSION, 0.8f);
			alEffectf(openalReverbEffect, AL_REVERB_GAIN, 0.4f);
			alEffectf(openalReverbEffect, AL_REVERB_GAINHF, 0.8f);
			alEffectf(openalReverbEffect, AL_REVERB_DECAY_TIME, roomSize * 2.0f);
			alEffectf(openalReverbEffect, AL_REVERB_DECAY_HFRATIO, 0.7f);
			alEffectf(openalReverbEffect, AL_REVERB_REFLECTIONS_GAIN, 0.1f);
			alEffectf(openalReverbEffect, AL_REVERB_REFLECTIONS_DELAY, 0.01f);
			alEffectf(openalReverbEffect, AL_REVERB_LATE_REVERB_GAIN, roomSize * 1.5f);
			alEffectf(openalReverbEffect, AL_REVERB_LATE_REVERB_DELAY, 0.02f);
		} else {
			// Outdoor environment - less reverb
			alEffectf(openalReverbEffect, AL_REVERB_DENSITY, 0.3f);
			alEffectf(openalReverbEffect, AL_REVERB_DIFFUSION, 0.9f);
			alEffectf(openalReverbEffect, AL_REVERB_GAIN, 0.1f);
			alEffectf(openalReverbEffect, AL_REVERB_GAINHF, 0.9f);
			alEffectf(openalReverbEffect, AL_REVERB_DECAY_TIME, 0.5f);
			alEffectf(openalReverbEffect, AL_REVERB_DECAY_HFRATIO, 0.9f);
			alEffectf(openalReverbEffect, AL_REVERB_REFLECTIONS_GAIN, 0.02f);
			alEffectf(openalReverbEffect, AL_REVERB_REFLECTIONS_DELAY, 0.005f);
			alEffectf(openalReverbEffect, AL_REVERB_LATE_REVERB_GAIN, 0.3f);
			alEffectf(openalReverbEffect, AL_REVERB_LATE_REVERB_DELAY, 0.01f);
		}

		// Update the effect slot
		alAuxiliaryEffectSloti(openalReverbSlot, AL_EFFECTSLOT_EFFECT, openalReverbEffect);

		if (alGetError() != AL_NO_ERROR) {
			Com_DPrintf("Failed to update environmental reverb settings\n");
		} else {
			Com_DPrintf("Updated environmental audio: %s, room size %.2f\n",
					   isIndoor ? "indoor" : "outdoor", roomSize);
		}
	}

	// TODO: Additional environmental processing:
	// - Occlusion detection for individual sounds
	// - Dynamic obstruction based on geometry
	// - Weather effects (rain, wind)
	// - Material-based sound reflection
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

/*
=================
SndOpenAL_Frame
Called every frame to update OpenAL state
=================
*/
void SndOpenAL_Frame(void)
{
	if (!s_openal_enabled || !s_openal_enabled->integer || !openalContext)
		return;

	// Update all active streams
	for (int i = 0; i < MAX_OPENAL_STREAMS; i++) {
		if (openalStreams[i].playing) {
			SndOpenAL_UpdateStream(&openalStreams[i]);
		}
	}

	// Update environmental audio effects
	SndOpenAL_UpdateEnvironmentalAudio();
}

/*
=================
SndOpenAL_IsEnabled
Check if OpenAL is available and enabled
=================
*/
qboolean SndOpenAL_IsEnabled(void)
{
	return (s_openal_enabled && s_openal_enabled->integer && openalContext) ? qtrue : qfalse;
}

#else // !USE_OPENAL

// Stub implementations when OpenAL is not available
qboolean SndOpenAL_Init(void) { return qfalse; }
void SndOpenAL_Shutdown(void) {}
sndOpenALHandle_t SndOpenAL_PlaySound(const char *soundName, const sndOpenAL3DProps_t *props) { (void)soundName; (void)props; return SND_OPENAL_INVALID_HANDLE; }
void SndOpenAL_StopSound(sndOpenALHandle_t handle) { (void)handle; }
void SndOpenAL_StopAllSounds(void) {}
void SndOpenAL_SetListenerPosition(const vec3_t position, const vec3_t forward, const vec3_t up, const vec3_t velocity) { (void)position; (void)forward; (void)up; (void)velocity; }
void SndOpenAL_SetSoundPosition(sndOpenALHandle_t handle, const vec3_t position) { (void)handle; (void)position; }
void SndOpenAL_SetSoundVelocity(sndOpenALHandle_t handle, const vec3_t velocity) { (void)handle; (void)velocity; }
void SndOpenAL_SetSoundVolume(sndOpenALHandle_t handle, float volume) { (void)handle; (void)volume; }
void SndOpenAL_SetSoundPitch(sndOpenALHandle_t handle, float pitch) { (void)handle; (void)pitch; }
qboolean SndOpenAL_IsSoundPlaying(sndOpenALHandle_t handle) { (void)handle; return qfalse; }
void SndOpenAL_SetOcclusion(sndOpenALHandle_t handle, float occlusionFactor) { (void)handle; (void)occlusionFactor; }
void SndOpenAL_SetReverb(sndOpenALHandle_t handle, float reverbLevel, float reverbDelay) { (void)handle; (void)reverbLevel; (void)reverbDelay; }
sndOpenALHandle_t SndOpenAL_StartStream(const char *streamName, qboolean looping) { (void)streamName; (void)looping; return SND_OPENAL_INVALID_HANDLE; }
void SndOpenAL_StopStream(sndOpenALHandle_t handle) { (void)handle; }
void SndOpenAL_SetEnvironmentReverb(float roomSize, float dampening, float wetness, float dryMix) { (void)roomSize; (void)dampening; (void)wetness; (void)dryMix; }
void SndOpenAL_UpdateEnvironmentalAudio(void) {}
void SndOpenAL_SetSoundCone(sndOpenALHandle_t handle, float innerAngle, float outerAngle, float outerGain) { (void)handle; (void)innerAngle; (void)outerAngle; (void)outerGain; }
void 	SndOpenAL_SetSoundDirectivity(sndOpenALHandle_t handle, float directivity, float directivitySharpness) { (void)handle; (void)directivity; (void)directivitySharpness; }
void SndOpenAL_Frame(void) {}
qboolean SndOpenAL_IsEnabled(void) { return qfalse; }

#endif // USE_OPENAL

#ifdef USE_OPENAL

/*
=================
SndOpenAL_InitStream
Initialize a streaming audio source
=================
*/
static qboolean SndOpenAL_InitStream(openalStream_t *stream, const char *filename, qboolean looping)
{
	int channels, sampleRate, bitsPerSample;
	void *wavData;
	size_t dataSize;
	ALenum format;
	ALuint buffers[4];

	Com_Memset(stream, 0, sizeof(*stream));
	Q_strncpyz(stream->filename, filename, sizeof(stream->filename));
	stream->looping = looping;
	stream->bufferSize = 4096; // 4KB buffer size

	// Load WAV file data
	if (!SndOpenAL_LoadWAVData(filename, &channels, &sampleRate, &bitsPerSample, &wavData, &dataSize)) {
		Com_Printf("SndOpenAL_InitStream: failed to load WAV data for %s\n", filename);
		return qfalse;
	}

	stream->channels = channels;
	stream->sampleRate = sampleRate;
	stream->bitsPerSample = bitsPerSample;

	// Determine OpenAL format
	if (channels == 1) {
		format = (bitsPerSample == 16) ? AL_FORMAT_MONO16 : AL_FORMAT_MONO8;
	} else {
		format = (bitsPerSample == 16) ? AL_FORMAT_STEREO16 : AL_FORMAT_STEREO8;
	}

	// Generate source
	alGenSources(1, &stream->source);
	if (alGetError() != AL_NO_ERROR) {
		Z_Free(wavData);
		return qfalse;
	}

	// Generate buffers
	alGenBuffers(4, buffers);
	if (alGetError() != AL_NO_ERROR) {
		alDeleteSources(1, &stream->source);
		Z_Free(wavData);
		return qfalse;
	}

	// Copy buffers to stream struct
	Com_Memcpy(stream->buffers, buffers, sizeof(buffers));

	// Fill initial buffers
	size_t bytesPerSample = (bitsPerSample / 8) * channels;
	size_t samplesPerBuffer = stream->bufferSize / bytesPerSample;
	size_t dataOffset = 0;

	for (int i = 0; i < 4; i++) {
		size_t copySize = samplesPerBuffer * bytesPerSample;
		if (dataOffset + copySize > dataSize) {
			copySize = dataSize - dataOffset;
		}

		if (copySize > 0) {
			alBufferData(stream->buffers[i], format, (char*)wavData + dataOffset, (ALsizei)copySize, sampleRate);
			dataOffset += copySize;
		}
	}

	// Queue buffers to source
	alSourceQueueBuffers(stream->source, 4, stream->buffers);

	// Set source properties
	alSourcei(stream->source, AL_LOOPING, AL_FALSE); // We handle looping manually
	alSourcef(stream->source, AL_GAIN, 1.0f);
	alSourcePlay(stream->source);

	Z_Free(wavData);
	return (alGetError() == AL_NO_ERROR);
}

/*
=================
SndOpenAL_ShutdownStream
Clean up a streaming audio source
=================
*/
static void SndOpenAL_ShutdownStream(openalStream_t *stream)
{
	if (stream->source) {
		alSourceStop(stream->source);
		alDeleteSources(1, &stream->source);
		stream->source = 0;
	}

	if (stream->buffers[0]) {
		alDeleteBuffers(4, stream->buffers);
		Com_Memset(stream->buffers, 0, sizeof(stream->buffers));
	}

	if (stream->fileHandle != FS_INVALID_HANDLE) {
		FS_FCloseFile(stream->fileHandle);
		stream->fileHandle = FS_INVALID_HANDLE;
	}
}

/*
=================
SndOpenAL_UpdateStream
Update streaming buffers (should be called regularly)
=================
*/
static qboolean SndOpenAL_UpdateStream(openalStream_t *stream)
{
	ALint processed;
	ALint state;

	if (!stream->playing || !stream->source) {
		return qfalse;
	}

	// Check if source is still playing
	alGetSourcei(stream->source, AL_SOURCE_STATE, &state);
	if (state != AL_PLAYING) {
		// Source stopped - check if we need to loop or end
		if (stream->looping) {
			alSourcePlay(stream->source);
		} else {
			stream->playing = qfalse;
			return qfalse;
		}
	}

	// Unqueue processed buffers and refill them
	alGetSourcei(stream->source, AL_BUFFERS_PROCESSED, &processed);

	for (ALint i = 0; i < processed; i++) {
		ALuint buffer;
		alSourceUnqueueBuffers(stream->source, 1, &buffer);

		// TODO: Refill buffer with next chunk of audio data
		// For now, just re-queue the same buffer (simple implementation)
		alSourceQueueBuffers(stream->source, 1, &buffer);
	}

	return qtrue;
}

/*
=================
SndOpenAL_LoadWAVData
Load WAV file data for streaming
=================
*/
static qboolean SndOpenAL_LoadWAVData(const char *filename, int *channels, int *sampleRate, int *bitsPerSample, void **data, size_t *size)
{
	fileHandle_t file;
	int fileSize;
	byte *fileData;

	fileSize = FS_FOpenFileRead(filename, &file, qfalse);
	if (fileSize <= 0) {
		return qfalse;
	}

	fileData = Z_Malloc(fileSize);
	FS_Read(fileData, fileSize, file);
	FS_FCloseFile(file);

	// Simple WAV header parsing (very basic implementation)
	if (fileSize < 44 || memcmp(fileData, "RIFF", 4) != 0 || memcmp(fileData + 8, "WAVE", 4) != 0) {
		Z_Free(fileData);
		return qfalse;
	}

	// Extract format information
	*channels = *(short*)(fileData + 22);
	*sampleRate = *(int*)(fileData + 24);
	*bitsPerSample = *(short*)(fileData + 34);

	// Extract data
	int dataChunkOffset = 36;
	while (dataChunkOffset < fileSize - 8) {
		if (memcmp(fileData + dataChunkOffset, "data", 4) == 0) {
			int dataSize = *(int*)(fileData + dataChunkOffset + 4);
			*data = Z_Malloc(dataSize);
			Com_Memcpy(*data, fileData + dataChunkOffset + 8, dataSize);
			*size = dataSize;
			Z_Free(fileData);
			return qtrue;
		}
		dataChunkOffset += 8 + *(int*)(fileData + dataChunkOffset + 4);
	}

	Z_Free(fileData);
	return qfalse;
}

#endif // USE_OPENAL

