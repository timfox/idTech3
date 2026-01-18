#ifndef __SND_OPENAL_H__
#define __SND_OPENAL_H__

#include "../common/q_shared.h"

// Enhanced audio features using OpenAL
#define SND_OPENAL_ENABLED		0x0001
#define SND_OPENAL_3D			0x0002
#define SND_OPENAL_OCCLUSION		0x0004
#define SND_OPENAL_REVERB		0x0008
#define SND_OPENAL_DOPPLER		0x0010
#define SND_OPENAL_DIRECTIONAL		0x0020

// Sound handle type
typedef int sndOpenALHandle_t;
#define SND_OPENAL_INVALID_HANDLE	-1

// 3D sound properties
typedef struct {
	vec3_t		position;
	vec3_t		velocity;
	float		minDistance;
	float		maxDistance;
	float		volume;
	float		pitch;
	qboolean	looping;
	int			flags;
	int			entityNum;
	int			channel;
} sndOpenAL3DProps_t;

// Initialization
qboolean	SndOpenAL_Init(void);
void		SndOpenAL_Shutdown(void);

// Sound playback
sndOpenALHandle_t SndOpenAL_PlaySound(const char *soundName, const sndOpenAL3DProps_t *props);
void		SndOpenAL_StopSound(sndOpenALHandle_t handle);
void		SndOpenAL_StopAllSounds(void);

// 3D positioning
void		SndOpenAL_SetListenerPosition(const vec3_t position, const vec3_t forward, const vec3_t up, const vec3_t velocity);
void		SndOpenAL_SetSoundPosition(sndOpenALHandle_t handle, const vec3_t position);
void		SndOpenAL_SetSoundVelocity(sndOpenALHandle_t handle, const vec3_t velocity);

// Sound properties
void		SndOpenAL_SetSoundVolume(sndOpenALHandle_t handle, float volume);
void		SndOpenAL_SetSoundPitch(sndOpenALHandle_t handle, float pitch);
qboolean	SndOpenAL_IsSoundPlaying(sndOpenALHandle_t handle);

// Occlusion/obstruction
void		SndOpenAL_SetOcclusion(sndOpenALHandle_t handle, float occlusionFactor);
void		SndOpenAL_SetReverb(sndOpenALHandle_t handle, float reverbLevel, float reverbDelay);

// Streaming
sndOpenALHandle_t SndOpenAL_StartStream(const char *streamName, qboolean looping);
void		SndOpenAL_StopStream(sndOpenALHandle_t handle);

// Environmental audio
void		SndOpenAL_SetEnvironmentReverb(float roomSize, float dampening, float wetness, float dryMix);
void		SndOpenAL_UpdateEnvironmentalAudio(void);

// Advanced spatialization
void		SndOpenAL_SetSoundCone(sndOpenALHandle_t handle, float innerAngle, float outerAngle, float outerGain);
void		SndOpenAL_SetSoundDirectivity(sndOpenALHandle_t handle, float directivity, float directivitySharpness);

#endif // __SND_OPENAL_H__

