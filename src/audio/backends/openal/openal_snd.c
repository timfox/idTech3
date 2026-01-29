/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2026

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include <stdlib.h>

#include "../../../qcommon/q_shared.h"
#include "../../../client/client.h"
#include "../../../client/snd_local.h"
#include "../../../client/snd_codec.h"

#include <AL/al.h>
#include <AL/alc.h>

#ifndef ALC_HRTF_SOFT
#define ALC_HRTF_SOFT 0x1992
#endif
#ifndef ALC_DONT_CARE_SOFT
#define ALC_DONT_CARE_SOFT 0x0002
#endif
#ifndef ALC_HRTF_STATUS_SOFT
#define ALC_HRTF_STATUS_SOFT 0x1993
#endif
#ifndef ALC_HRTF_DISABLED_SOFT
#define ALC_HRTF_DISABLED_SOFT 0x1994
#endif
#ifndef ALC_HRTF_ENABLED_SOFT
#define ALC_HRTF_ENABLED_SOFT 0x1995
#endif
#ifndef ALC_HRTF_DENIED_SOFT
#define ALC_HRTF_DENIED_SOFT 0x1996
#endif
#ifndef ALC_HRTF_REQUIRED_SOFT
#define ALC_HRTF_REQUIRED_SOFT 0x1997
#endif
#ifndef ALC_HRTF_HEADPHONES_DETECTED_SOFT
#define ALC_HRTF_HEADPHONES_DETECTED_SOFT 0x1998
#endif
#ifndef ALC_HRTF_UNSUPPORTED_FORMAT_SOFT
#define ALC_HRTF_UNSUPPORTED_FORMAT_SOFT 0x1999
#endif

#ifndef ALC_ALL_DEVICES_SPECIFIER
#define ALC_ALL_DEVICES_SPECIFIER 0x1013
#endif

#ifndef ALC_CAPTURE_DEVICE_SPECIFIER
#define ALC_CAPTURE_DEVICE_SPECIFIER 0x310
#endif
#ifndef ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER
#define ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER 0x311
#endif
#ifndef ALC_CAPTURE_SAMPLES
#define ALC_CAPTURE_SAMPLES 0x312
#endif

// EFX definitions
#ifndef AL_EFFECT_TYPE
#define AL_EFFECT_TYPE 0x8001
#endif
#ifndef AL_EFFECT_REVERB
#define AL_EFFECT_REVERB 0x0001
#endif
#ifndef AL_EFFECT_EAXREVERB
#define AL_EFFECT_EAXREVERB 0x8000
#endif
#ifndef AL_EFFECTSLOT_EFFECT
#define AL_EFFECTSLOT_EFFECT 0x0001
#endif
#ifndef AL_EFFECTSLOT_GAIN
#define AL_EFFECTSLOT_GAIN 0x0002
#endif
#ifndef AL_AUXILIARY_SEND_FILTER
#define AL_AUXILIARY_SEND_FILTER 0x20006
#endif

typedef ALCboolean (ALC_APIENTRY *alcResetDeviceSOFTProc)(ALCdevice *device, const ALCint *attribs);

// EFX function pointer types
typedef void (AL_APIENTRY *LPALGENEFFECTS)(ALsizei, ALuint*);
typedef void (AL_APIENTRY *LPALDELETEEFFECTS)(ALsizei, const ALuint*);
typedef ALboolean (AL_APIENTRY *LPALISEFFECT)(ALuint);
typedef void (AL_APIENTRY *LPALEFFECTI)(ALuint, ALenum, ALint);
typedef void (AL_APIENTRY *LPALEFFECTIV)(ALuint, ALenum, const ALint*);
typedef void (AL_APIENTRY *LPALEFFECTF)(ALuint, ALenum, ALfloat);
typedef void (AL_APIENTRY *LPALEFFECTFV)(ALuint, ALenum, const ALfloat*);

typedef void (AL_APIENTRY *LPALGENAUXILIARYEFFECTSLOTS)(ALsizei, ALuint*);
typedef void (AL_APIENTRY *LPALDELETEAUXILIARYEFFECTSLOTS)(ALsizei, const ALuint*);
typedef ALboolean (AL_APIENTRY *LPALISAUXILIARYEFFECTSLOT)(ALuint);
typedef void (AL_APIENTRY *LPALAUXILIARYEFFECTSLOTI)(ALuint, ALenum, ALint);
typedef void (AL_APIENTRY *LPALAUXILIARYEFFECTSLOTF)(ALuint, ALenum, ALfloat);

// EFX function pointers
static LPALGENEFFECTS alGenEffects;
static LPALDELETEEFFECTS alDeleteEffects;
static LPALISEFFECT alIsEffect;
static LPALEFFECTI alEffecti;
static LPALEFFECTF alEffectf;

static LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
static LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
static LPALISAUXILIARYEFFECTSLOT alIsAuxiliaryEffectSlot;
static LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti;
static LPALAUXILIARYEFFECTSLOTF alAuxiliaryEffectSlotf;

#define AL_MUSIC_BUFFER_COUNT 4
#define AL_RAW_BUFFER_COUNT 8
#define AL_MUSIC_BUFFER_BYTES 32768

typedef struct {
	ALuint source;
	sfx_t *sfx;
	int entnum;
	int entchannel;
	int startTime;
	qboolean fixed_origin;
	qboolean looping;
	qboolean inUse;
	vec3_t origin;
} al_channel_t;

static ALCdevice *alDevice;
static ALCcontext *alContext;
static qboolean alInited;
static qboolean alHrtfAvailable;
static qboolean alHrtfEnabled;

static ALCdevice *alCaptureDevice;
static qboolean alCaptureAvailable;

static qboolean alEfxAvailable;
static ALuint alReverbEffect;
static ALuint alReverbSlot;

static al_channel_t alChannels[MAX_CHANNELS];
static loopSound_t alLoopSounds[MAX_GENTITIES];
static vec3_t alEntityPositions[MAX_GENTITIES];
static vec3_t alEntityVelocities[MAX_GENTITIES];
static qboolean alEntityPosValid[MAX_GENTITIES];
static int alListenerEntity = -1;
static vec3_t alListenerOrigin;
static vec3_t alListenerAxis[3];
static vec3_t alListenerVelocity;

static ALuint *alBufferTable;
static int alBufferCapacity;

static ALuint alMusicSource;
static ALuint alMusicBuffers[AL_MUSIC_BUFFER_COUNT];
static int alMusicNextBuffer;
static snd_stream_t *alBackgroundStream;
static char alBackgroundLoop[MAX_QPATH];

static ALuint alRawSource;
static ALuint alRawBuffers[AL_RAW_BUFFER_COUNT];
static int alRawNextBuffer;
static qboolean alRawBufferQueued[AL_RAW_BUFFER_COUNT];

// Performance metrics
static int alTotalSources;
static int alActiveSources;
static int alBuffersCreated;

static qboolean S_AL_CheckError( const char *label ) {
	ALenum error = alGetError();
	if ( error != AL_NO_ERROR ) {
		Com_Printf( S_COLOR_RED "OpenAL error (%s): 0x%x\n", label, error );
		return qtrue;
	}
	return qfalse;
}

static void S_AL_ListDevices_f( void ) {
	const ALCchar *devices;
	const ALCchar *ptr;
	int count = 0;

	Com_Printf( "----- OpenAL Devices -----\n" );

	if ( alcIsExtensionPresent( NULL, "ALC_ENUMERATE_ALL_EXT" ) == ALC_TRUE ) {
		devices = alcGetString( NULL, ALC_ALL_DEVICES_SPECIFIER );
	} else {
		devices = alcGetString( NULL, ALC_DEVICE_SPECIFIER );
	}

	if ( !devices || !devices[0] ) {
		Com_Printf( "No devices found.\n" );
		Com_Printf( "--------------------------\n" );
		return;
	}

	ptr = devices;
	while ( ptr && *ptr ) {
		Com_Printf( "  %d: %s\n", count++, ptr );
		ptr += strlen( ptr ) + 1;
	}

	if ( alDevice ) {
		const ALCchar *currentDevice = alcGetString( alDevice, ALC_DEVICE_SPECIFIER );
		Com_Printf( "\nCurrent device: %s\n", currentDevice ? currentDevice : "unknown" );
	}

	Com_Printf( "--------------------------\n" );
}

static void S_AL_DeviceInfo_f( void ) {
	const ALCchar *deviceName;
	ALCint alcMajor, alcMinor;
	const ALchar *vendor, *version, *renderer;
	const ALchar *extensions;

	Com_Printf( "----- OpenAL Device Info -----\n" );

	if ( !alInited || !alDevice ) {
		Com_Printf( "OpenAL not initialized.\n" );
		Com_Printf( "------------------------------\n" );
		return;
	}

	alcGetIntegerv( alDevice, ALC_MAJOR_VERSION, 1, &alcMajor );
	alcGetIntegerv( alDevice, ALC_MINOR_VERSION, 1, &alcMinor );

	deviceName = alcGetString( alDevice, ALC_DEVICE_SPECIFIER );
	vendor = alGetString( AL_VENDOR );
	version = alGetString( AL_VERSION );
	renderer = alGetString( AL_RENDERER );
	extensions = alGetString( AL_EXTENSIONS );

	Com_Printf( "Device: %s\n", deviceName ? deviceName : "unknown" );
	Com_Printf( "ALC Version: %d.%d\n", alcMajor, alcMinor );
	Com_Printf( "Vendor: %s\n", vendor ? vendor : "unknown" );
	Com_Printf( "Version: %s\n", version ? version : "unknown" );
	Com_Printf( "Renderer: %s\n", renderer ? renderer : "unknown" );
	Com_Printf( "HRTF: %s\n", alHrtfEnabled ? "enabled" : ( alHrtfAvailable ? "available but disabled" : "not available" ) );
	Com_Printf( "EFX: %s\n", alEfxAvailable ? "available" : "not available" );
	Com_Printf( "Capture: %s\n", alCaptureAvailable ? "available" : "not available" );
	Com_Printf( "\nActive sources: %d/%d\n", alActiveSources, alTotalSources );
	Com_Printf( "Buffers created: %d\n", alBuffersCreated );

	if ( extensions ) {
		Com_Printf( "\nExtensions: %s\n", extensions );
	}

	Com_Printf( "------------------------------\n" );
}

static void S_AL_ClearSources( void ) {
	int i;

	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		if ( alChannels[i].source ) {
			alSourceStop( alChannels[i].source );
			alSourcei( alChannels[i].source, AL_BUFFER, 0 );
			alChannels[i].inUse = qfalse;
			alChannels[i].looping = qfalse;
			alChannels[i].sfx = NULL;
		}
	}

	for ( i = 0; i < MAX_GENTITIES; ++i ) {
		if ( alLoopSounds[i].active ) {
			alLoopSounds[i].active = qfalse;
		}
	}
}

static ALenum S_AL_Format( int channels, int width ) {
	if ( width == 1 ) {
		return (channels == 2) ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;
	}
	return (channels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
}

static void S_AL_EnsureBufferCapacity( int handle ) {
	int newCap;
	ALuint *newTable;

	if ( handle < alBufferCapacity ) {
		return;
	}

	newCap = alBufferCapacity ? alBufferCapacity : 64;
	while ( newCap <= handle ) {
		newCap *= 2;
	}

	newTable = realloc( alBufferTable, newCap * sizeof( *alBufferTable ) );
	if ( !newTable ) {
		Com_Printf( S_COLOR_RED "OpenAL: failed to allocate buffer table\n" );
		return;
	}

	Com_Memset( newTable + alBufferCapacity, 0, ( newCap - alBufferCapacity ) * sizeof( *newTable ) );
	alBufferTable = newTable;
	alBufferCapacity = newCap;
}

static qboolean S_AL_CreateBuffer( int handle, sfx_t *sfx ) {
	int sampleCount;
	int remaining;
	int offset;
	sndBuffer *chunk;
	short *samples;
	ALuint buffer;
	ALenum format;

	if ( !sfx || !sfx->soundData || sfx->soundLength <= 0 ) {
		return qfalse;
	}

	if ( sfx->soundChannels <= 0 || sfx->soundChannels > 2 ) {
		Com_Printf( S_COLOR_YELLOW "OpenAL: invalid channel count %d for %s\n", sfx->soundChannels, sfx->soundName );
		return qfalse;
	}

	format = S_AL_Format( sfx->soundChannels, 2 );
	sampleCount = sfx->soundLength * sfx->soundChannels;
	if ( sampleCount <= 0 ) {
		return qfalse;
	}
	samples = malloc( sampleCount * sizeof( *samples ) );
	if ( !samples ) {
		Com_Printf( S_COLOR_RED "OpenAL: failed to allocate sample buffer for %s\n", sfx->soundName );
		return qfalse;
	}

	remaining = sampleCount;
	offset = 0;
	for ( chunk = sfx->soundData; chunk && remaining > 0; chunk = chunk->next ) {
		int toCopy = remaining > SND_CHUNK_SIZE ? SND_CHUNK_SIZE : remaining;
		Com_Memcpy( samples + offset, chunk->sndChunk, toCopy * sizeof( *samples ) );
		offset += toCopy;
		remaining -= toCopy;
	}

	alGetError();
	alGenBuffers( 1, &buffer );
	if ( buffer == 0 || S_AL_CheckError( "alGenBuffers" ) ) {
		free( samples );
		return qfalse;
	}
	alBufferData( buffer, format, samples, sampleCount * sizeof( *samples ), dma.speed );
	free( samples );

	if ( S_AL_CheckError( "alBufferData(sfx)" ) ) {
		alDeleteBuffers( 1, &buffer );
		return qfalse;
	}

	alBufferTable[handle] = buffer;
	alBuffersCreated++;

	if ( s_openalDebug && s_openalDebug->integer >= 1 ) {
		Com_Printf( "OpenAL: Created buffer %d for %s (%d samples, %d channels)\n", 
			buffer, sfx->soundName, sfx->soundLength, sfx->soundChannels );
	}

	return qtrue;
}

static ALuint S_AL_GetBufferForSfx( int handle ) {
	sfx_t *sfx;

	if ( handle < 0 || handle >= s_numSfx ) {
		return 0;
	}

	S_AL_EnsureBufferCapacity( handle );
	if ( handle >= alBufferCapacity ) {
		return 0;
	}

	if ( alBufferTable[handle] ) {
		return alBufferTable[handle];
	}

	sfx = &s_knownSfx[handle];
	if ( !sfx->inMemory ) {
		S_LoadSound( sfx );
	}

	if ( !S_AL_CreateBuffer( handle, sfx ) ) {
		return 0;
	}

	return alBufferTable[handle];
}

static void S_AL_SetSourcePosition( ALuint source, qboolean relative, const vec3_t origin ) {
	alSourcei( source, AL_SOURCE_RELATIVE, relative ? AL_TRUE : AL_FALSE );
	alSource3f( source, AL_POSITION, origin[0], origin[1], origin[2] );
	alSource3f( source, AL_VELOCITY, 0.0f, 0.0f, 0.0f );
}

static void S_AL_UpdateListener( void ) {
	ALfloat orientation[6];
	float dopplerFactor = 0.0f;

	orientation[0] = alListenerAxis[0][0];
	orientation[1] = alListenerAxis[0][1];
	orientation[2] = alListenerAxis[0][2];
	orientation[3] = alListenerAxis[2][0];
	orientation[4] = alListenerAxis[2][1];
	orientation[5] = alListenerAxis[2][2];

	alListener3f( AL_POSITION, alListenerOrigin[0], alListenerOrigin[1], alListenerOrigin[2] );
	alListener3f( AL_VELOCITY, alListenerVelocity[0], alListenerVelocity[1], alListenerVelocity[2] );
	alListenerfv( AL_ORIENTATION, orientation );
	alListenerf( AL_GAIN, s_volume ? s_volume->value : 1.0f );

	// Update doppler settings
	if ( s_doppler && s_doppler->integer ) {
		dopplerFactor = s_openalDopplerFactor ? s_openalDopplerFactor->value : 1.0f;
	}
	alDopplerFactor( dopplerFactor );
	if ( s_openalDopplerSpeed ) {
		alDopplerVelocity( s_openalDopplerSpeed->value );
	}
}

static int S_AL_FindFreeChannel( int now ) {
	int i;
	int oldest = now;
	int oldestIndex = -1;
	int lowestPriority = 999;
	int lowestPriorityIndex = -1;
	ALint state = 0;

	// First pass: find truly free channel
	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		if ( !alChannels[i].inUse ) {
			return i;
		}
	}

	// Second pass: find stopped sources that are marked inUse
	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		if ( alChannels[i].inUse && !alChannels[i].looping ) {
			alGetSourcei( alChannels[i].source, AL_SOURCE_STATE, &state );
			if ( state != AL_PLAYING ) {
				alChannels[i].inUse = qfalse;
				return i;
			}
		}
	}

	// Third pass: prioritize by entity (listener sounds are lowest priority for reuse)
	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		int priority = 0;
		if ( alChannels[i].looping ) {
			continue; // Never steal looping sounds
		}
		if ( alChannels[i].entnum == alListenerEntity ) {
			priority = 0; // Lowest priority - listener sounds
		} else if ( alChannels[i].fixed_origin ) {
			priority = 5; // Medium priority - fixed position sounds
		} else {
			priority = 10; // Highest priority - entity-attached sounds
		}

		if ( priority < lowestPriority || 
		     (priority == lowestPriority && alChannels[i].startTime < oldest) ) {
			lowestPriority = priority;
			oldest = alChannels[i].startTime;
			lowestPriorityIndex = i;
		}
	}

	if ( lowestPriorityIndex >= 0 ) {
		return lowestPriorityIndex;
	}

	// Final fallback: oldest non-looping channel
	oldest = now;
	oldestIndex = -1;
	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		if ( !alChannels[i].looping && alChannels[i].startTime < oldest ) {
			oldest = alChannels[i].startTime;
			oldestIndex = i;
		}
	}

	return oldestIndex;
}

static int S_AL_FindLoopChannel( int entnum ) {
	int i;

	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		if ( alChannels[i].inUse && alChannels[i].looping && alChannels[i].entnum == entnum ) {
			return i;
		}
	}
	return -1;
}

static void S_AL_UpdateLoopingSounds( void ) {
	int ent;
	int now = cls.framecount;

	for ( ent = 0; ent < MAX_GENTITIES; ++ent ) {
		loopSound_t *loop = &alLoopSounds[ent];
		int channel;
		ALuint buffer;
		vec3_t pos;

		if ( !loop->active ) {
			channel = S_AL_FindLoopChannel( ent );
			if ( channel >= 0 ) {
				alSourceStop( alChannels[channel].source );
				alChannels[channel].inUse = qfalse;
				alChannels[channel].looping = qfalse;
			}
			continue;
		}

		if ( loop->kill && loop->framenum != now ) {
			loop->active = qfalse;
			channel = S_AL_FindLoopChannel( ent );
			if ( channel >= 0 ) {
				alSourceStop( alChannels[channel].source );
				alChannels[channel].inUse = qfalse;
				alChannels[channel].looping = qfalse;
			}
			continue;
		}

		if ( !loop->sfx ) {
			continue;
		}

		buffer = S_AL_GetBufferForSfx( loop->sfx - s_knownSfx );
		if ( !buffer ) {
			continue;
		}

		channel = S_AL_FindLoopChannel( ent );
		if ( channel < 0 ) {
			channel = S_AL_FindFreeChannel( Sys_Milliseconds() );
			if ( channel < 0 ) {
				continue;
			}
			alChannels[channel].inUse = qtrue;
			alChannels[channel].looping = qtrue;
			alChannels[channel].entnum = ent;
			alChannels[channel].sfx = loop->sfx;
			alChannels[channel].startTime = Sys_Milliseconds();
			alChannels[channel].fixed_origin = qtrue;
		}

		if ( alChannels[channel].sfx != loop->sfx ) {
			alChannels[channel].sfx = loop->sfx;
			alSourceStop( alChannels[channel].source );
			alSourcei( alChannels[channel].source, AL_BUFFER, 0 );
		}

		alSourcei( alChannels[channel].source, AL_BUFFER, buffer );
		alSourcei( alChannels[channel].source, AL_LOOPING, AL_TRUE );
		alSourcef( alChannels[channel].source, AL_GAIN, 1.0f );
		alSourcef( alChannels[channel].source, AL_REFERENCE_DISTANCE, 80.0f );
		alSourcef( alChannels[channel].source, AL_ROLLOFF_FACTOR, s_openalRolloff ? s_openalRolloff->value : 1.0f );
		alSourcef( alChannels[channel].source, AL_MAX_DISTANCE, s_openalMaxDistance ? s_openalMaxDistance->value : 2000.0f );

		// Attach EFX reverb if available
		if ( alEfxAvailable && alReverbSlot ) {
			alSource3i( alChannels[channel].source, AL_AUXILIARY_SEND_FILTER, alReverbSlot, 0, 0 );
		}

		if ( ent == alListenerEntity ) {
			VectorClear( pos );
			S_AL_SetSourcePosition( alChannels[channel].source, qtrue, pos );
		} else {
			S_AL_SetSourcePosition( alChannels[channel].source, qfalse, loop->origin );
			alSource3f( alChannels[channel].source, AL_VELOCITY, loop->velocity[0], loop->velocity[1], loop->velocity[2] );
		}

		{
			ALint state = 0;
			alGetSourcei( alChannels[channel].source, AL_SOURCE_STATE, &state );
			if ( state != AL_PLAYING ) {
				alSourcePlay( alChannels[channel].source );
			}
		}
	}
}

static int S_AL_StreamUnqueue( ALuint source, ALuint *buffers, int max) {
	ALint processed = 0;
	int count = 0;

	alGetSourcei( source, AL_BUFFERS_PROCESSED, &processed );
	while ( processed-- > 0 && count < max ) {
		alSourceUnqueueBuffers( source, 1, &buffers[count] );
		count++;
	}

	return count;
}

static qboolean S_AL_StreamFill( ALuint buffer, snd_stream_t *stream ) {
	byte raw[AL_MUSIC_BUFFER_BYTES];
	int bytes;
	int r;
	ALenum format;

	if ( !stream ) {
		return qfalse;
	}

	if ( stream->info.channels <= 0 || stream->info.channels > 2 ) {
		return qfalse;
	}
	if ( stream->info.width != 1 && stream->info.width != 2 ) {
		return qfalse;
	}

	bytes = AL_MUSIC_BUFFER_BYTES;
	r = S_CodecReadStream( stream, bytes, raw );
	if ( r <= 0 ) {
		return qfalse;
	}

	format = S_AL_Format( stream->info.channels, stream->info.width );
	alGetError();
	alBufferData( buffer, format, raw, r, stream->info.rate );
	return !S_AL_CheckError( "alBufferData(stream)" );
}

static void S_AL_UpdateMusic( void ) {
	ALint queued = 0;
	ALint state = 0;
	int refillCount;
	int i;
	ALuint buffers[AL_MUSIC_BUFFER_COUNT];
	int unqueued = 0;

	if ( !alBackgroundStream || !alMusicSource ) {
		return;
	}

	alSourcef( alMusicSource, AL_GAIN, s_musicVolume ? s_musicVolume->value : 1.0f );
	unqueued = S_AL_StreamUnqueue( alMusicSource, buffers, AL_MUSIC_BUFFER_COUNT );
	for ( i = 0; i < unqueued; ++i ) {
		ALuint buffer = buffers[i];
		if ( !S_AL_StreamFill( buffer, alBackgroundStream ) ) {
			if ( alBackgroundLoop[0] ) {
				S_CodecCloseStream( alBackgroundStream );
				alBackgroundStream = S_CodecOpenStream( alBackgroundLoop );
				if ( !alBackgroundStream ) {
					continue;
				}
				if ( !S_AL_StreamFill( buffer, alBackgroundStream ) ) {
					continue;
				}
			} else {
				continue;
			}
		}

		alSourceQueueBuffers( alMusicSource, 1, &buffer );
	}

	alGetSourcei( alMusicSource, AL_SOURCE_STATE, &state );
	if ( state != AL_PLAYING ) {
		alSourcePlay( alMusicSource );
	}
}

static void S_AL_UpdateRaw( void ) {
	ALint state = 0;
	ALuint buffers[AL_RAW_BUFFER_COUNT];
	int unqueued;
	int i;
	int j;

	if ( !alRawSource ) {
		return;
	}

	unqueued = S_AL_StreamUnqueue( alRawSource, buffers, AL_RAW_BUFFER_COUNT );
	for ( i = 0; i < unqueued; ++i ) {
		for ( j = 0; j < AL_RAW_BUFFER_COUNT; ++j ) {
			if ( buffers[i] == alRawBuffers[j] ) {
				alRawBufferQueued[j] = qfalse;
				break;
			}
		}
	}
	alGetSourcei( alRawSource, AL_SOURCE_STATE, &state );
	if ( state != AL_PLAYING ) {
		ALint queued = 0;
		alGetSourcei( alRawSource, AL_BUFFERS_QUEUED, &queued );
		if ( queued > 0 ) {
			alSourcePlay( alRawSource );
		}
	}
}

static void S_AL_StopBackgroundTrack( void ) {
	ALuint buffers[AL_MUSIC_BUFFER_COUNT];

	if ( alBackgroundStream ) {
		S_CodecCloseStream( alBackgroundStream );
		alBackgroundStream = NULL;
	}

	if ( alMusicSource ) {
		alSourceStop( alMusicSource );
		S_AL_StreamUnqueue( alMusicSource, buffers, AL_MUSIC_BUFFER_COUNT );
	}

	alBackgroundLoop[0] = '\0';
}

static void S_AL_StartBackgroundTrack( const char *intro, const char *loop ) {
	int i;

	if ( !intro ) {
		intro = "";
	}
	if ( !loop || !loop[0] ) {
		loop = intro;
	}

	if ( !intro[0] ) {
		S_AL_StopBackgroundTrack();
		return;
	}

	Q_strncpyz( alBackgroundLoop, loop, sizeof( alBackgroundLoop ) );

	if ( alBackgroundStream ) {
		S_CodecCloseStream( alBackgroundStream );
		alBackgroundStream = NULL;
	}

	alBackgroundStream = S_CodecOpenStream( intro );
	if ( !alBackgroundStream ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: couldn't open music file %s\n", intro );
		return;
	}

	alSourcef( alMusicSource, AL_GAIN, s_musicVolume ? s_musicVolume->value : 1.0f );

	for ( i = 0; i < AL_MUSIC_BUFFER_COUNT; ++i ) {
		ALuint buffer = alMusicBuffers[i];
		if ( !S_AL_StreamFill( buffer, alBackgroundStream ) ) {
			break;
		}
		alSourceQueueBuffers( alMusicSource, 1, &buffer );
		alMusicNextBuffer = ( i + 1 ) % AL_MUSIC_BUFFER_COUNT;
	}

	alSourcePlay( alMusicSource );
}

static void S_AL_RawSamples( int samples, int rate, int width, int channels, const byte *data, float volume ) {
	ALint queued = 0;
	ALuint buffer;
	ALuint buffers[AL_RAW_BUFFER_COUNT];
	ALenum format;
	int bytes;
	int unqueued;
	int i;
	int j;
	int bufferIndex = -1;

	if ( !alRawSource ) {
		return;
	}

	if ( !data || samples <= 0 ) {
		return;
	}
	if ( channels <= 0 || channels > 2 ) {
		return;
	}
	if ( width != 1 && width != 2 ) {
		return;
	}

	unqueued = S_AL_StreamUnqueue( alRawSource, buffers, AL_RAW_BUFFER_COUNT );
	for ( i = 0; i < unqueued; ++i ) {
		for ( j = 0; j < AL_RAW_BUFFER_COUNT; ++j ) {
			if ( buffers[i] == alRawBuffers[j] ) {
				alRawBufferQueued[j] = qfalse;
				break;
			}
		}
	}
	alGetSourcei( alRawSource, AL_BUFFERS_QUEUED, &queued );
	if ( queued >= AL_RAW_BUFFER_COUNT ) {
		return;
	}

	if ( unqueued > 0 ) {
		buffer = buffers[0];
		for ( j = 0; j < AL_RAW_BUFFER_COUNT; ++j ) {
			if ( buffer == alRawBuffers[j] ) {
				bufferIndex = j;
				break;
			}
		}
	} else {
		for ( i = 0; i < AL_RAW_BUFFER_COUNT; ++i ) {
			int idx = ( alRawNextBuffer + i ) % AL_RAW_BUFFER_COUNT;
			if ( !alRawBufferQueued[idx] ) {
				buffer = alRawBuffers[idx];
				bufferIndex = idx;
				break;
			}
		}
	}

	if ( bufferIndex < 0 ) {
		return;
	}

	format = S_AL_Format( channels, width );
	bytes = samples * channels * width;
	alGetError();
	alBufferData( buffer, format, data, bytes, rate );
	if ( S_AL_CheckError( "alBufferData(raw)" ) ) {
		return;
	}
	alSourceQueueBuffers( alRawSource, 1, &buffer );
	alSourcef( alRawSource, AL_GAIN, volume );
	alRawBufferQueued[bufferIndex] = qtrue;
	alRawNextBuffer = ( bufferIndex + 1 ) % AL_RAW_BUFFER_COUNT;
}

static void S_AL_StopAllSounds( void ) {
	int i;

	if ( !s_soundStarted ) {
		return;
	}

	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		if ( alChannels[i].inUse ) {
			alSourceStop( alChannels[i].source );
			alChannels[i].inUse = qfalse;
			alChannels[i].looping = qfalse;
		}
	}

	S_AL_StopBackgroundTrack();
}

static void S_AL_ClearLoopingSounds( qboolean killall ) {
	int i;

	for ( i = 0; i < MAX_GENTITIES; ++i ) {
		if ( !alLoopSounds[i].active ) {
			continue;
		}
		if ( killall || alLoopSounds[i].kill ) {
			alLoopSounds[i].active = qfalse;
			{
				int channel = S_AL_FindLoopChannel( i );
				if ( channel >= 0 ) {
					alSourceStop( alChannels[channel].source );
					alChannels[channel].inUse = qfalse;
					alChannels[channel].looping = qfalse;
				}
			}
		}
	}
}

static void S_AL_StopLoopingSound( int entityNum ) {
	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		return;
	}
	alLoopSounds[entityNum].active = qfalse;
	{
		int channel = S_AL_FindLoopChannel( entityNum );
		if ( channel >= 0 ) {
			alSourceStop( alChannels[channel].source );
			alChannels[channel].inUse = qfalse;
			alChannels[channel].looping = qfalse;
		}
	}
}

static void S_AL_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle ) {
	loopSound_t *loop;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_AddLoopingSound: handle %i out of range\n", sfxHandle );
		return;
	}

	loop = &alLoopSounds[entityNum];
	VectorCopy( origin, loop->origin );
	VectorCopy( velocity, loop->velocity );
	loop->sfx = &s_knownSfx[sfxHandle];
	loop->sfx->lastTimeUsed = Sys_Milliseconds();
	loop->active = qtrue;
	loop->kill = qtrue;
	loop->framenum = cls.framecount;
}

static void S_AL_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle ) {
	loopSound_t *loop;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_AddRealLoopingSound: handle %i out of range\n", sfxHandle );
		return;
	}

	loop = &alLoopSounds[entityNum];
	VectorCopy( origin, loop->origin );
	VectorCopy( velocity, loop->velocity );
	loop->sfx = &s_knownSfx[sfxHandle];
	loop->sfx->lastTimeUsed = Sys_Milliseconds();
	loop->active = qtrue;
	loop->kill = qfalse;
	loop->framenum = cls.framecount;
}

static void S_AL_Respatialize( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater ) {
	int i;
	static vec3_t lastListenerOrigin;
	static qboolean firstRespatialize = qtrue;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	(void)inwater;

	alListenerEntity = entityNum;

	// Calculate listener velocity for doppler
	if ( !firstRespatialize && cls.frametime > 0 ) {
		vec3_t delta;
		VectorSubtract( origin, lastListenerOrigin, delta );
		VectorScale( delta, 1000.0f / cls.frametime, alListenerVelocity );
	} else {
		VectorClear( alListenerVelocity );
		firstRespatialize = qfalse;
	}

	VectorCopy( origin, lastListenerOrigin );
	VectorCopy( origin, alListenerOrigin );
	VectorCopy( axis[0], alListenerAxis[0] );
	VectorCopy( axis[1], alListenerAxis[1] );
	VectorCopy( axis[2], alListenerAxis[2] );
	S_AL_UpdateListener();

	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		if ( !alChannels[i].inUse || alChannels[i].looping ) {
			continue;
		}

		if ( alChannels[i].fixed_origin ) {
			S_AL_SetSourcePosition( alChannels[i].source, qfalse, alChannels[i].origin );
		} else if ( alChannels[i].entnum == alListenerEntity ) {
			vec3_t zero = { 0.0f, 0.0f, 0.0f };
			S_AL_SetSourcePosition( alChannels[i].source, qtrue, zero );
		} else if ( alChannels[i].entnum >= 0 && alChannels[i].entnum < MAX_GENTITIES && alEntityPosValid[alChannels[i].entnum] ) {
			S_AL_SetSourcePosition( alChannels[i].source, qfalse, alEntityPositions[alChannels[i].entnum] );
			// Update velocity for doppler
			alSource3f( alChannels[i].source, AL_VELOCITY, 
				alEntityVelocities[alChannels[i].entnum][0],
				alEntityVelocities[alChannels[i].entnum][1],
				alEntityVelocities[alChannels[i].entnum][2] );
		}
	}
}

static void S_AL_UpdateEntityPosition( int entityNum, const vec3_t origin ) {
	vec3_t velocity;

	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "S_UpdateEntityPosition: bad entitynum %i", entityNum );
	}

	// Calculate velocity for doppler
	if ( alEntityPosValid[entityNum] ) {
		VectorSubtract( origin, alEntityPositions[entityNum], velocity );
		VectorScale( velocity, 1000.0f / cls.frametime, alEntityVelocities[entityNum] );
	} else {
		VectorClear( alEntityVelocities[entityNum] );
	}

	VectorCopy( origin, alEntityPositions[entityNum] );
	alEntityPosValid[entityNum] = qtrue;
}

static void S_AL_StartSound( const vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfxHandle ) {
	int now;
	int index;
	ALuint buffer;
	al_channel_t *ch;
	vec3_t pos;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_StartSound: handle %i out of range\n", sfxHandle );
		return;
	}

	buffer = S_AL_GetBufferForSfx( sfxHandle );
	if ( !buffer ) {
		return;
	}

	now = Sys_Milliseconds();
	index = S_AL_FindFreeChannel( now );
	if ( index < 0 ) {
		return;
	}

	ch = &alChannels[index];
	if ( ch->inUse ) {
		alSourceStop( ch->source );
	}

	ch->sfx = &s_knownSfx[sfxHandle];
	ch->sfx->lastTimeUsed = now;
	ch->entnum = entityNum;
	ch->entchannel = entchannel;
	ch->startTime = now;
	ch->looping = qfalse;
	ch->inUse = qtrue;

	alSourcei( ch->source, AL_BUFFER, buffer );
	alSourcei( ch->source, AL_LOOPING, AL_FALSE );
	alSourcef( ch->source, AL_GAIN, 1.0f );
	alSourcef( ch->source, AL_REFERENCE_DISTANCE, 80.0f );
	alSourcef( ch->source, AL_ROLLOFF_FACTOR, s_openalRolloff ? s_openalRolloff->value : 1.0f );
	alSourcef( ch->source, AL_MAX_DISTANCE, s_openalMaxDistance ? s_openalMaxDistance->value : 2000.0f );

	// Attach EFX reverb if available
	if ( alEfxAvailable && alReverbSlot ) {
		alSource3i( ch->source, AL_AUXILIARY_SEND_FILTER, alReverbSlot, 0, 0 );
	}

	if ( origin ) {
		VectorCopy( origin, ch->origin );
		ch->fixed_origin = qtrue;
		S_AL_SetSourcePosition( ch->source, qfalse, ch->origin );
	} else {
		ch->fixed_origin = qfalse;
		if ( entityNum == alListenerEntity ) {
			VectorClear( pos );
			S_AL_SetSourcePosition( ch->source, qtrue, pos );
		} else if ( entityNum >= 0 && entityNum < MAX_GENTITIES && alEntityPosValid[entityNum] ) {
			S_AL_SetSourcePosition( ch->source, qfalse, alEntityPositions[entityNum] );
		} else {
			VectorClear( pos );
			S_AL_SetSourcePosition( ch->source, qtrue, pos );
		}
	}

	alSourcePlay( ch->source );
}

static void S_AL_StartLocalSound( sfxHandle_t sfxHandle, int channelNum ) {
	S_AL_StartSound( NULL, alListenerEntity, channelNum, sfxHandle );
}

static void S_AL_DisableSounds( void ) {
	S_AL_StopAllSounds();
	s_soundMuted = qtrue;
}

static void S_AL_ClearSoundBuffer( void ) {
	S_AL_StopAllSounds();
}

static void S_AL_Update( int msec ) {
	int i;
	ALint state = 0;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	(void)msec;

	s_soundtime = Sys_Milliseconds();

	// Update performance metrics
	alActiveSources = 0;
	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		if ( !alChannels[i].inUse || alChannels[i].looping ) {
			continue;
		}
		alGetSourcei( alChannels[i].source, AL_SOURCE_STATE, &state );
		if ( state != AL_PLAYING ) {
			alChannels[i].inUse = qfalse;
		} else {
			alActiveSources++;
		}
	}

	// Debug output
	if ( s_openalDebug && s_openalDebug->integer >= 2 ) {
		Com_Printf( "OpenAL: %d active sources\n", alActiveSources );
	}

	S_AL_UpdateMusic();
	S_AL_UpdateRaw();
	S_AL_UpdateLoopingSounds();
}

static void S_AL_SoundInfo( void ) {
	const ALCchar *deviceName;
	int activeCount = 0;
	int loopingCount = 0;
	int i;

	Com_Printf( "----- Sound Info -----\n" );
	if ( !s_soundStarted ) {
		Com_Printf( "sound system not started\n" );
	} else {
		deviceName = alcGetString( alDevice, ALC_DEVICE_SPECIFIER );
		Com_Printf( "Using OpenAL device: %s\n", deviceName ? deviceName : "unknown" );
		Com_Printf( "HRTF: %s\n", alHrtfEnabled ? "enabled" : "disabled" );
		Com_Printf( "EFX: %s\n", alEfxAvailable ? "enabled" : "disabled" );
		Com_Printf( "Capture: %s\n", alCaptureAvailable ? "enabled" : "disabled" );
		
		for ( i = 0; i < MAX_CHANNELS; ++i ) {
			if ( alChannels[i].inUse ) {
				activeCount++;
				if ( alChannels[i].looping ) {
					loopingCount++;
				}
			}
		}
		
		Com_Printf( "Active sources: %d (looping: %d)\n", activeCount, loopingCount );
		Com_Printf( "Total buffers: %d\n", alBuffersCreated );
		Com_Printf( "Sample rate: %d Hz\n", dma.speed );
		Com_Printf( "Doppler: factor=%.2f speed=%.1f\n", 
			s_openalDopplerFactor ? s_openalDopplerFactor->value : 1.0f,
			s_openalDopplerSpeed ? s_openalDopplerSpeed->value : 9000.0f );
		Com_Printf( "Distance: rolloff=%.2f max=%.1f\n",
			s_openalRolloff ? s_openalRolloff->value : 1.0f,
			s_openalMaxDistance ? s_openalMaxDistance->value : 2000.0f );
	}
	Com_Printf( "----------------------\n" );
}

static void S_AL_Shutdown( void ) {
	int i;

	if ( !alInited ) {
		return;
	}

	// Remove console commands
	Cmd_RemoveCommand( "s_aldevices" );
	Cmd_RemoveCommand( "s_alinfo" );

	S_AL_StopBackgroundTrack();

	if ( alRawSource ) {
		alSourceStop( alRawSource );
		alDeleteSources( 1, &alRawSource );
		alRawSource = 0;
	}

	if ( alMusicSource ) {
		alSourceStop( alMusicSource );
		alDeleteSources( 1, &alMusicSource );
		alMusicSource = 0;
	}

	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		if ( alChannels[i].source ) {
			alSourceStop( alChannels[i].source );
			alDeleteSources( 1, &alChannels[i].source );
			alChannels[i].source = 0;
		}
	}

	if ( alBufferTable ) {
		for ( i = 0; i < alBufferCapacity; ++i ) {
			if ( alBufferTable[i] ) {
				alDeleteBuffers( 1, &alBufferTable[i] );
			}
		}
		free( alBufferTable );
		alBufferTable = NULL;
		alBufferCapacity = 0;
	}

	alDeleteBuffers( AL_MUSIC_BUFFER_COUNT, alMusicBuffers );
	alDeleteBuffers( AL_RAW_BUFFER_COUNT, alRawBuffers );

	// Clean up EFX
	if ( alEfxAvailable ) {
		if ( alReverbSlot && alDeleteAuxiliaryEffectSlots ) {
			alDeleteAuxiliaryEffectSlots( 1, &alReverbSlot );
			alReverbSlot = 0;
		}
		if ( alReverbEffect && alDeleteEffects ) {
			alDeleteEffects( 1, &alReverbEffect );
			alReverbEffect = 0;
		}
		alEfxAvailable = qfalse;
	}

	// Clean up capture
	if ( alCaptureDevice ) {
		alcCaptureStop( alCaptureDevice );
		alcCaptureCloseDevice( alCaptureDevice );
		alCaptureDevice = NULL;
		alCaptureAvailable = qfalse;
	}

	if ( alContext ) {
		alcMakeContextCurrent( NULL );
		alcDestroyContext( alContext );
		alContext = NULL;
	}

	if ( alDevice ) {
		alcCloseDevice( alDevice );
		alDevice = NULL;
	}

	alInited = qfalse;
	s_soundStarted = qfalse;
}

qboolean S_AL_Init( soundInterface_t *si ) {
	const char *requestedDevice;
	const ALCchar *deviceName;
	alcResetDeviceSOFTProc resetDevice;
	ALCint hrtfAttrs[3];
	int i;

	if ( !si ) {
		return qfalse;
	}

	if ( alInited ) {
		return qtrue;
	}

	s_khz = Cvar_Get( "s_khz", "22", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_khz, "0", "48", CV_INTEGER );
	Cvar_SetDescription( s_khz, "Specifies the sound sampling rate, (8, 11, 22, 44, 48) in kHz. Default value is 22." );
	switch( s_khz->integer ) {
		case 48:
		case 44:
		case 22:
		case 11:
		case 8:
			break;
		default:
			Com_Printf( "WARNING: cvar 's_khz' must be one of (8, 11, 22, 44, 48), setting to '%s'\n", s_khz->resetString );
			Cvar_ForceReset( "s_khz" );
			break;
	}

	dma.speed = s_khz->integer * 1000;
	dma.channels = 2;
	dma.samplebits = 16;
	dma.isfloat = qfalse;
	dma.driver = "OpenAL";

	// Enumerate available devices if debugging
	if ( s_openalDebug && s_openalDebug->integer >= 1 ) {
		const ALCchar *devices;
		const ALCchar *ptr;
		int count = 0;

		Com_Printf( "Available OpenAL devices:\n" );
		if ( alcIsExtensionPresent( NULL, "ALC_ENUMERATE_ALL_EXT" ) == ALC_TRUE ) {
			devices = alcGetString( NULL, ALC_ALL_DEVICES_SPECIFIER );
		} else {
			devices = alcGetString( NULL, ALC_DEVICE_SPECIFIER );
		}

		if ( devices && devices[0] ) {
			ptr = devices;
			while ( ptr && *ptr ) {
				Com_Printf( "  %d: %s\n", count++, ptr );
				ptr += strlen( ptr ) + 1;
			}
		}
	}

	requestedDevice = ( s_openalDevice && s_openalDevice->string && s_openalDevice->string[0] && Q_stricmp( s_openalDevice->string, "default" ) )
		? s_openalDevice->string
		: NULL;

	alDevice = alcOpenDevice( requestedDevice );
	if ( !alDevice ) {
		Com_Printf( S_COLOR_RED "OpenAL: failed to open device '%s'\n", requestedDevice ? requestedDevice : "default" );
		return qfalse;
	}

	alHrtfAvailable = ( alcIsExtensionPresent( alDevice, "ALC_SOFT_HRTF" ) == ALC_TRUE );
	alHrtfEnabled = qfalse;

	if ( s_openalDebug && s_openalDebug->integer >= 1 ) {
		Com_Printf( "OpenAL HRTF extension: %s\n", alHrtfAvailable ? "available" : "not available" );
	}

	resetDevice = (alcResetDeviceSOFTProc)alcGetProcAddress( alDevice, "alcResetDeviceSOFT" );
	if ( alHrtfAvailable && resetDevice && s_openalHrtf && s_openalHrtf->integer ) {
		hrtfAttrs[0] = ALC_HRTF_SOFT;
		hrtfAttrs[1] = ALC_TRUE;
		hrtfAttrs[2] = 0;
		if ( resetDevice( alDevice, hrtfAttrs ) == ALC_TRUE ) {
			ALCint status = 0;
			alcGetIntegerv( alDevice, ALC_HRTF_STATUS_SOFT, 1, &status );
			alHrtfEnabled = ( status == ALC_HRTF_ENABLED_SOFT );
			
			if ( s_openalDebug && s_openalDebug->integer >= 1 ) {
				const char *statusStr = "unknown";
				switch ( status ) {
					case ALC_HRTF_DISABLED_SOFT: statusStr = "disabled"; break;
					case ALC_HRTF_ENABLED_SOFT: statusStr = "enabled"; break;
					case ALC_HRTF_DENIED_SOFT: statusStr = "denied"; break;
					case ALC_HRTF_REQUIRED_SOFT: statusStr = "required"; break;
					case ALC_HRTF_HEADPHONES_DETECTED_SOFT: statusStr = "headphones detected"; break;
					case ALC_HRTF_UNSUPPORTED_FORMAT_SOFT: statusStr = "unsupported format"; break;
				}
				Com_Printf( "OpenAL HRTF status: %s (0x%x)\n", statusStr, status );
			}
		} else if ( s_openalDebug && s_openalDebug->integer >= 1 ) {
			Com_Printf( S_COLOR_YELLOW "OpenAL: failed to enable HRTF\n" );
		}
	}

	alContext = alcCreateContext( alDevice, NULL );
	if ( !alContext || !alcMakeContextCurrent( alContext ) ) {
		Com_Printf( S_COLOR_RED "OpenAL: failed to create context\n" );
		if ( alContext ) {
			alcDestroyContext( alContext );
			alContext = NULL;
		}
		alcCloseDevice( alDevice );
		alDevice = NULL;
		return qfalse;
	}

	alDistanceModel( AL_INVERSE_DISTANCE_CLAMPED );

	// Set doppler parameters
	if ( s_openalDopplerFactor ) {
		alDopplerFactor( s_openalDopplerFactor->value );
	}
	if ( s_openalDopplerSpeed ) {
		alDopplerVelocity( s_openalDopplerSpeed->value );
	}

	// Initialize EFX if available and requested
	alEfxAvailable = qfalse;
	alReverbEffect = 0;
	alReverbSlot = 0;
	if ( s_openalEfx && s_openalEfx->integer && alcIsExtensionPresent( alDevice, "ALC_EXT_EFX" ) == ALC_TRUE ) {
		alGenEffects = (LPALGENEFFECTS)alGetProcAddress( "alGenEffects" );
		alDeleteEffects = (LPALDELETEEFFECTS)alGetProcAddress( "alDeleteEffects" );
		alIsEffect = (LPALISEFFECT)alGetProcAddress( "alIsEffect" );
		alEffecti = (LPALEFFECTI)alGetProcAddress( "alEffecti" );
		alEffectf = (LPALEFFECTF)alGetProcAddress( "alEffectf" );
		alGenAuxiliaryEffectSlots = (LPALGENAUXILIARYEFFECTSLOTS)alGetProcAddress( "alGenAuxiliaryEffectSlots" );
		alDeleteAuxiliaryEffectSlots = (LPALDELETEAUXILIARYEFFECTSLOTS)alGetProcAddress( "alDeleteAuxiliaryEffectSlots" );
		alIsAuxiliaryEffectSlot = (LPALISAUXILIARYEFFECTSLOT)alGetProcAddress( "alIsAuxiliaryEffectSlot" );
		alAuxiliaryEffectSloti = (LPALAUXILIARYEFFECTSLOTI)alGetProcAddress( "alAuxiliaryEffectSloti" );
		alAuxiliaryEffectSlotf = (LPALAUXILIARYEFFECTSLOTF)alGetProcAddress( "alAuxiliaryEffectSlotf" );

		if ( alGenEffects && alGenAuxiliaryEffectSlots ) {
			alGenEffects( 1, &alReverbEffect );
			if ( !S_AL_CheckError( "alGenEffects" ) ) {
				alEffecti( alReverbEffect, AL_EFFECT_TYPE, AL_EFFECT_REVERB );
				if ( !S_AL_CheckError( "alEffecti(AL_EFFECT_TYPE)" ) ) {
					alGenAuxiliaryEffectSlots( 1, &alReverbSlot );
					if ( !S_AL_CheckError( "alGenAuxiliaryEffectSlots" ) ) {
						alAuxiliaryEffectSloti( alReverbSlot, AL_EFFECTSLOT_EFFECT, alReverbEffect );
						alEfxAvailable = !S_AL_CheckError( "alAuxiliaryEffectSloti" );
						if ( alEfxAvailable ) {
							Com_Printf( "OpenAL EFX: enabled\n" );
						}
					}
				}
			}
			
			if ( !alEfxAvailable && s_openalDebug && s_openalDebug->integer >= 1 ) {
				Com_Printf( S_COLOR_YELLOW "OpenAL EFX: initialization failed\n" );
			}
		}
	}

	// Initialize capture if available and requested
	alCaptureAvailable = qfalse;
	alCaptureDevice = NULL;
	if ( s_openalCapture && s_openalCapture->integer ) {
		const ALCchar *captureDeviceName = alcGetString( NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER );
		if ( captureDeviceName ) {
			alCaptureDevice = alcCaptureOpenDevice( captureDeviceName, 48000, AL_FORMAT_MONO16, 4096 );
			if ( alCaptureDevice ) {
				alCaptureAvailable = qtrue;
				Com_Printf( "OpenAL capture: enabled (%s)\n", captureDeviceName );
			}
		}
	}

	deviceName = alcGetString( alDevice, ALC_DEVICE_SPECIFIER );
	Com_Printf( "OpenAL device: %s\n", deviceName ? deviceName : "unknown" );
	Com_Printf( "OpenAL HRTF: %s\n", alHrtfEnabled ? "enabled" : ( alHrtfAvailable ? "disabled" : "unsupported" ) );

	// Register console commands
	Cmd_AddCommand( "s_aldevices", S_AL_ListDevices_f );
	Cmd_AddCommand( "s_alinfo", S_AL_DeviceInfo_f );

	alGenSources( 1, &alMusicSource );
	alSourcei( alMusicSource, AL_SOURCE_RELATIVE, AL_TRUE );
	alSource3f( alMusicSource, AL_POSITION, 0.0f, 0.0f, 0.0f );
	alSourcei( alMusicSource, AL_LOOPING, AL_FALSE );

	alGenSources( 1, &alRawSource );
	alSourcei( alRawSource, AL_SOURCE_RELATIVE, AL_TRUE );
	alSource3f( alRawSource, AL_POSITION, 0.0f, 0.0f, 0.0f );
	alSourcei( alRawSource, AL_LOOPING, AL_FALSE );

	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		alGenSources( 1, &alChannels[i].source );
		alChannels[i].inUse = qfalse;
		alChannels[i].looping = qfalse;
		alChannels[i].sfx = NULL;
	}

	alGenBuffers( AL_MUSIC_BUFFER_COUNT, alMusicBuffers );
	alGenBuffers( AL_RAW_BUFFER_COUNT, alRawBuffers );

	Com_Memset( alEntityPosValid, 0, sizeof( alEntityPosValid ) );
	Com_Memset( alEntityVelocities, 0, sizeof( alEntityVelocities ) );
	Com_Memset( alLoopSounds, 0, sizeof( alLoopSounds ) );
	VectorClear( alListenerVelocity );
	Com_Memset( alRawBufferQueued, 0, sizeof( alRawBufferQueued ) );
	S_AL_ClearSources();

	// Initialize metrics
	alTotalSources = MAX_CHANNELS;
	alActiveSources = 0;
	alBuffersCreated = 0;

	s_soundStarted = qtrue;
	s_soundMuted = qfalse;
	alInited = qtrue;

	si->Shutdown = S_AL_Shutdown;
	si->StartSound = S_AL_StartSound;
	si->StartLocalSound = S_AL_StartLocalSound;
	si->StartBackgroundTrack = S_AL_StartBackgroundTrack;
	si->StopBackgroundTrack = S_AL_StopBackgroundTrack;
	si->RawSamples = S_AL_RawSamples;
	si->StopAllSounds = S_AL_StopAllSounds;
	si->ClearLoopingSounds = S_AL_ClearLoopingSounds;
	si->AddLoopingSound = S_AL_AddLoopingSound;
	si->AddRealLoopingSound = S_AL_AddRealLoopingSound;
	si->StopLoopingSound = S_AL_StopLoopingSound;
	si->Respatialize = S_AL_Respatialize;
	si->UpdateEntityPosition = S_AL_UpdateEntityPosition;
	si->Update = S_AL_Update;
	si->DisableSounds = S_AL_DisableSounds;
	si->BeginRegistration = S_Base_BeginRegistration;
	si->RegisterSound = S_Base_RegisterSound;
	si->ClearSoundBuffer = S_AL_ClearSoundBuffer;
	si->SoundInfo = S_AL_SoundInfo;
	si->SoundList = S_Base_SoundList;

	Com_Printf( "OpenAL audio initialized.\n" );
	return qtrue;
}

// VOIP Capture Support
#ifdef USE_VOIP
void SNDDMA_StartCapture( void ) {
	if ( !alCaptureDevice ) {
		return;
	}
	alcCaptureStart( alCaptureDevice );
	if ( s_openalDebug && s_openalDebug->integer >= 1 ) {
		Com_Printf( "OpenAL: capture started\n" );
	}
}

int SNDDMA_AvailableCaptureSamples( void ) {
	ALCint samples = 0;
	
	if ( !alCaptureDevice ) {
		return 0;
	}
	
	alcGetIntegerv( alCaptureDevice, ALC_CAPTURE_SAMPLES, 1, &samples );
	return samples;
}

void SNDDMA_Capture( int samples, byte *data ) {
	if ( !alCaptureDevice || samples <= 0 ) {
		return;
	}
	
	alcCaptureSamples( alCaptureDevice, data, samples );
	
	if ( s_openalDebug && s_openalDebug->integer >= 2 ) {
		Com_Printf( "OpenAL: captured %d samples\n", samples );
	}
}

void SNDDMA_StopCapture( void ) {
	if ( !alCaptureDevice ) {
		return;
	}
	alcCaptureStop( alCaptureDevice );
	if ( s_openalDebug && s_openalDebug->integer >= 1 ) {
		Com_Printf( "OpenAL: capture stopped\n" );
	}
}
#endif
