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

#include "../qcommon/q_shared.h"
#include "../client/client.h"
#include "../client/snd_local.h"
#include "../client/snd_codec.h"

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

typedef ALCboolean (ALC_APIENTRY *alcResetDeviceSOFTProc)(ALCdevice *device, const ALCint *attribs);

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

static al_channel_t alChannels[MAX_CHANNELS];
static loopSound_t alLoopSounds[MAX_GENTITIES];
static vec3_t alEntityPositions[MAX_GENTITIES];
static qboolean alEntityPosValid[MAX_GENTITIES];
static int alListenerEntity = -1;
static vec3_t alListenerOrigin;
static vec3_t alListenerAxis[3];

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

static qboolean S_AL_CheckError( const char *label ) {
	ALenum error = alGetError();
	if ( error != AL_NO_ERROR ) {
		Com_Printf( S_COLOR_RED "OpenAL error (%s): 0x%x\n", label, error );
		return qtrue;
	}
	return qfalse;
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

	format = S_AL_Format( sfx->soundChannels, 2 );
	sampleCount = sfx->soundLength * sfx->soundChannels;
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

	alGenBuffers( 1, &buffer );
	alBufferData( buffer, format, samples, sampleCount * sizeof( *samples ), dma.speed );
	free( samples );

	if ( S_AL_CheckError( "alBufferData" ) ) {
		alDeleteBuffers( 1, &buffer );
		return qfalse;
	}

	alBufferTable[handle] = buffer;
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

	orientation[0] = alListenerAxis[0][0];
	orientation[1] = alListenerAxis[0][1];
	orientation[2] = alListenerAxis[0][2];
	orientation[3] = alListenerAxis[2][0];
	orientation[4] = alListenerAxis[2][1];
	orientation[5] = alListenerAxis[2][2];

	alListener3f( AL_POSITION, alListenerOrigin[0], alListenerOrigin[1], alListenerOrigin[2] );
	alListener3f( AL_VELOCITY, 0.0f, 0.0f, 0.0f );
	alListenerfv( AL_ORIENTATION, orientation );
	alListenerf( AL_GAIN, s_volume ? s_volume->value : 1.0f );

	alDopplerFactor( ( s_doppler && s_doppler->integer ) ? 1.0f : 0.0f );
}

static int S_AL_FindFreeChannel( int now ) {
	int i;
	int oldest = now;
	int oldestIndex = -1;

	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		if ( !alChannels[i].inUse ) {
			return i;
		}
	}

	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		if ( alChannels[i].startTime < oldest ) {
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
		alSourcef( alChannels[channel].source, AL_ROLLOFF_FACTOR, 1.0f );

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

static void S_AL_StreamUnqueue( ALuint source ) {
	ALint processed = 0;
	ALuint buffer = 0;

	alGetSourcei( source, AL_BUFFERS_PROCESSED, &processed );
	while ( processed-- > 0 ) {
		alSourceUnqueueBuffers( source, 1, &buffer );
	}
}

static qboolean S_AL_StreamFill( ALuint buffer, snd_stream_t *stream ) {
	byte raw[AL_MUSIC_BUFFER_BYTES];
	int bytes;
	int r;
	ALenum format;

	if ( !stream ) {
		return qfalse;
	}

	bytes = AL_MUSIC_BUFFER_BYTES;
	r = S_CodecReadStream( stream, bytes, raw );
	if ( r <= 0 ) {
		return qfalse;
	}

	format = S_AL_Format( stream->info.channels, stream->info.width );
	alBufferData( buffer, format, raw, r, stream->info.rate );
	return !S_AL_CheckError( "alBufferData(stream)" );
}

static void S_AL_UpdateMusic( void ) {
	ALint queued = 0;
	ALint state = 0;
	int refillCount;
	int i;

	if ( !alBackgroundStream || !alMusicSource ) {
		return;
	}

	alSourcef( alMusicSource, AL_GAIN, s_musicVolume ? s_musicVolume->value : 1.0f );
	S_AL_StreamUnqueue( alMusicSource );

	alGetSourcei( alMusicSource, AL_BUFFERS_QUEUED, &queued );
	refillCount = AL_MUSIC_BUFFER_COUNT - queued;
	for ( i = 0; i < refillCount; ++i ) {
		ALuint buffer = alMusicBuffers[alMusicNextBuffer];
		if ( !S_AL_StreamFill( buffer, alBackgroundStream ) ) {
			if ( alBackgroundLoop[0] ) {
				S_CodecCloseStream( alBackgroundStream );
				alBackgroundStream = S_CodecOpenStream( alBackgroundLoop );
				if ( !alBackgroundStream ) {
					break;
				}
				if ( !S_AL_StreamFill( buffer, alBackgroundStream ) ) {
					break;
				}
			} else {
				break;
			}
		}

		alSourceQueueBuffers( alMusicSource, 1, &buffer );
		alMusicNextBuffer = ( alMusicNextBuffer + 1 ) % AL_MUSIC_BUFFER_COUNT;
	}

	alGetSourcei( alMusicSource, AL_SOURCE_STATE, &state );
	if ( state != AL_PLAYING ) {
		alSourcePlay( alMusicSource );
	}
}

static void S_AL_UpdateRaw( void ) {
	ALint state = 0;

	if ( !alRawSource ) {
		return;
	}

	S_AL_StreamUnqueue( alRawSource );
	alGetSourcei( alRawSource, AL_SOURCE_STATE, &state );
	if ( state != AL_PLAYING ) {
		alSourcePlay( alRawSource );
	}
}

static void S_AL_StopBackgroundTrack( void ) {
	if ( alBackgroundStream ) {
		S_CodecCloseStream( alBackgroundStream );
		alBackgroundStream = NULL;
	}

	if ( alMusicSource ) {
		alSourceStop( alMusicSource );
		S_AL_StreamUnqueue( alMusicSource );
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
	ALenum format;
	int bytes;

	if ( !alRawSource ) {
		return;
	}

	S_AL_StreamUnqueue( alRawSource );
	alGetSourcei( alRawSource, AL_BUFFERS_QUEUED, &queued );
	if ( queued >= AL_RAW_BUFFER_COUNT ) {
		return;
	}

	buffer = alRawBuffers[alRawNextBuffer];
	format = S_AL_Format( channels, width );
	bytes = samples * channels * width;
	alBufferData( buffer, format, data, bytes, rate );
	alSourceQueueBuffers( alRawSource, 1, &buffer );
	alSourcef( alRawSource, AL_GAIN, volume );
	alRawNextBuffer = ( alRawNextBuffer + 1 ) % AL_RAW_BUFFER_COUNT;
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

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	(void)inwater;

	alListenerEntity = entityNum;
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
		}
	}
}

static void S_AL_UpdateEntityPosition( int entityNum, const vec3_t origin ) {
	if ( entityNum < 0 || entityNum >= MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "S_UpdateEntityPosition: bad entitynum %i", entityNum );
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
	alSourcef( ch->source, AL_ROLLOFF_FACTOR, 1.0f );

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

	for ( i = 0; i < MAX_CHANNELS; ++i ) {
		if ( !alChannels[i].inUse || alChannels[i].looping ) {
			continue;
		}
		alGetSourcei( alChannels[i].source, AL_SOURCE_STATE, &state );
		if ( state != AL_PLAYING ) {
			alChannels[i].inUse = qfalse;
		}
	}

	S_AL_UpdateMusic();
	S_AL_UpdateRaw();
	S_AL_UpdateLoopingSounds();
}

static void S_AL_SoundInfo( void ) {
	const ALCchar *deviceName;

	Com_Printf( "----- Sound Info -----\n" );
	if ( !s_soundStarted ) {
		Com_Printf( "sound system not started\n" );
	} else {
		deviceName = alcGetString( alDevice, ALC_DEVICE_SPECIFIER );
		Com_Printf( "Using OpenAL device: %s\n", deviceName ? deviceName : "unknown" );
		Com_Printf( "HRTF: %s\n", alHrtfEnabled ? "enabled" : "disabled" );
	}
	Com_Printf( "----------------------\n" );
}

static void S_AL_Shutdown( void ) {
	int i;

	if ( !alInited ) {
		return;
	}

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

	requestedDevice = ( s_openalDevice && s_openalDevice->string && s_openalDevice->string[0] && Q_stricmp( s_openalDevice->string, "default" ) )
		? s_openalDevice->string
		: NULL;

	alDevice = alcOpenDevice( requestedDevice );
	if ( !alDevice ) {
		Com_Printf( S_COLOR_RED "OpenAL: failed to open device\n" );
		return qfalse;
	}

	alHrtfAvailable = ( alcIsExtensionPresent( alDevice, "ALC_SOFT_HRTF" ) == ALC_TRUE );
	alHrtfEnabled = qfalse;

	resetDevice = (alcResetDeviceSOFTProc)alcGetProcAddress( alDevice, "alcResetDeviceSOFT" );
	if ( alHrtfAvailable && resetDevice ) {
		hrtfAttrs[0] = ALC_HRTF_SOFT;
		hrtfAttrs[1] = ( s_openalHrtf && s_openalHrtf->integer ) ? ALC_TRUE : ALC_FALSE;
		hrtfAttrs[2] = 0;
		if ( resetDevice( alDevice, hrtfAttrs ) == ALC_TRUE ) {
			ALCint status = 0;
			alcGetIntegerv( alDevice, ALC_HRTF_STATUS_SOFT, 1, &status );
			alHrtfEnabled = ( status == ALC_HRTF_ENABLED_SOFT );
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

	deviceName = alcGetString( alDevice, ALC_DEVICE_SPECIFIER );
	Com_Printf( "OpenAL device: %s\n", deviceName ? deviceName : "unknown" );
	Com_Printf( "OpenAL HRTF: %s\n", alHrtfEnabled ? "enabled" : ( alHrtfAvailable ? "disabled" : "unsupported" ) );

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
	Com_Memset( alLoopSounds, 0, sizeof( alLoopSounds ) );
	S_AL_ClearSources();

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
