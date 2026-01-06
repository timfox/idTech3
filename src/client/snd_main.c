/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)

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

#include "client.h"
#include "snd_codec.h"
#include "snd_local.h"
#include "snd_public.h"
#include "snd_openal.h"
#include "snd_audio_thread.h"

#ifdef USE_OPENAL
#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#endif

#ifdef USE_OPENAL
// OpenAL externals for sound info
extern int numOpenALSounds;
extern ALCdevice *openalDevice;
extern ALCcontext *openalContext;
extern qboolean openalEfxAvailable;
#define MAX_OPENAL_SOURCES 256
#else
// Stub definitions when OpenAL is not available
static int numOpenALSounds = 0;
static ALCdevice *openalDevice = NULL;
static ALCcontext *openalContext = NULL;
static qboolean openalEfxAvailable = qfalse;
#define MAX_OPENAL_SOURCES 256
#endif

cvar_t *s_volume;
cvar_t *s_musicVolume;
cvar_t *s_doppler;
cvar_t *s_muteWhenMinimized;
cvar_t *s_muteWhenUnfocused;

static soundInterface_t si;

// Function declarations
void S_AudioThreads_f( void );

/*
=================
S_ValidateInterface
=================
*/
static qboolean S_ValidSoundInterface( const soundInterface_t *s )
{
	if( !s->Shutdown ) return qfalse;
	if( !s->StartSound ) return qfalse;
	if( !s->StartLocalSound ) return qfalse;
	if( !s->StartBackgroundTrack ) return qfalse;
	if( !s->StopBackgroundTrack ) return qfalse;
	if( !s->RawSamples ) return qfalse;
	if( !s->StopAllSounds ) return qfalse;
	if( !s->ClearLoopingSounds ) return qfalse;
	if( !s->AddLoopingSound ) return qfalse;
	if( !s->AddRealLoopingSound ) return qfalse;
	if( !s->StopLoopingSound ) return qfalse;
	if( !s->Respatialize ) return qfalse;
	if( !s->UpdateEntityPosition ) return qfalse;
	if( !s->Update ) return qfalse;
	if( !s->DisableSounds ) return qfalse;
	if( !s->BeginRegistration ) return qfalse;
	if( !s->RegisterSound ) return qfalse;
	if( !s->ClearSoundBuffer ) return qfalse;
	if( !s->SoundInfo ) return qfalse;
	if( !s->SoundList ) return qfalse;

	return qtrue;
}


/*
=================
S_StartSound
=================
*/
void S_StartSound( vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx )
{
	if( si.StartSound ) {
		si.StartSound( origin, entnum, entchannel, sfx );
	}
}


/*
=================
S_StartLocalSound
=================
*/
void S_StartLocalSound( sfxHandle_t sfx, int channelNum )
{
	if( si.StartLocalSound ) {
		si.StartLocalSound( sfx, channelNum );
	}
}


/*
=================
S_StartBackgroundTrack
=================
*/
void S_StartBackgroundTrack( const char *intro, const char *loop )
{
	if( si.StartBackgroundTrack ) {
		si.StartBackgroundTrack( intro, loop );
	}
}


/*
=================
S_StopBackgroundTrack
=================
*/
void S_StopBackgroundTrack( void )
{
	if( si.StopBackgroundTrack ) {
		si.StopBackgroundTrack( );
	}
}


/*
=================
S_RawSamples
=================
*/
void S_RawSamples (int samples, int rate, int width, int channels,
		   const byte *data, float volume)
{
	if( si.RawSamples ) {
		si.RawSamples( samples, rate, width, channels, data, volume );
	}
}


/*
=================
S_StopAllSounds
=================
*/
void S_StopAllSounds( void )
{
	if( si.StopAllSounds ) {
		si.StopAllSounds();
	}
}


/*
=================
S_ClearLoopingSounds
=================
*/
void S_ClearLoopingSounds( qboolean killall )
{
	if( si.ClearLoopingSounds ) {
		si.ClearLoopingSounds( killall );
	}
}


/*
=================
S_AddLoopingSound
=================
*/
void S_AddLoopingSound( int entityNum, const vec3_t origin,
		const vec3_t velocity, sfxHandle_t sfx )
{
	if( si.AddLoopingSound ) {
		si.AddLoopingSound( entityNum, origin, velocity, sfx );
	}
}


/*
=================
S_AddRealLoopingSound
=================
*/
void S_AddRealLoopingSound( int entityNum, const vec3_t origin,
		const vec3_t velocity, sfxHandle_t sfx )
{
	if( si.AddRealLoopingSound ) {
		si.AddRealLoopingSound( entityNum, origin, velocity, sfx );
	}
}


/*
=================
S_StopLoopingSound
=================
*/
void S_StopLoopingSound( int entityNum )
{
	if( si.StopLoopingSound ) {
		si.StopLoopingSound( entityNum );
	}
}


/*
=================
S_Respatialize
=================
*/
void S_Respatialize( int entityNum, const vec3_t origin,
		vec3_t axis[3], int inwater )
{
	if( si.Respatialize ) {
		si.Respatialize( entityNum, origin, axis, inwater );
	}
}


/*
=================
S_UpdateEntityPosition
=================
*/
void S_UpdateEntityPosition( int entityNum, const vec3_t origin )
{
	if( si.UpdateEntityPosition ) {
		si.UpdateEntityPosition( entityNum, origin );
	}
}


/*
=================
S_Update
=================
*/
void S_Update( int msec )
{
	if ( si.Update ) {
		si.Update( msec );
	}
}


/*
=================
S_DisableSounds
=================
*/
void S_DisableSounds( void )
{
	if( si.DisableSounds ) {
		si.DisableSounds();
	}
}


/*
=================
S_BeginRegistration
=================
*/
void S_BeginRegistration( void )
{
	if ( si.BeginRegistration ) {
		si.BeginRegistration();
	}
}


/*
=================
S_RegisterSound
=================
*/
sfxHandle_t	S_RegisterSound( const char *sample, qboolean compressed )
{
	if ( !sample || !*sample ) {
		Com_Printf( "NULL sound\n" );
		return 0;
	}

	if( si.RegisterSound ) {
		return si.RegisterSound( sample, compressed );
	} else {
		return 0;
	}
}


/*
=================
S_ClearSoundBuffer
=================
*/
void S_ClearSoundBuffer( void )
{
	if( si.ClearSoundBuffer ) {
		si.ClearSoundBuffer();
	}
}


/*
=================
S_SoundInfo
=================
*/
static void S_SoundInfo( void )
{
	if( si.SoundInfo ) {
		si.SoundInfo();
	}
}


/*
=================
S_SoundList
=================
*/
static void S_SoundList( void )
{
	if( si.SoundList ) {
		si.SoundList();
	}
}

//=============================================================================

/*
=================
S_Play_f
=================
*/
static void S_Play_f( void ) {
	int 		i;
	int			c;
	sfxHandle_t	h;

	if( !si.RegisterSound || !si.StartLocalSound ) {
		return;
	}

	c = Cmd_Argc();

	if( c < 2 ) {
		Com_Printf ("Usage: play <sound filename> [sound filename] [sound filename] ...\n");
		return;
	}

	for( i = 1; i < c; i++ ) {
		h = si.RegisterSound( Cmd_Argv(i), qfalse );

		if( h ) {
			si.StartLocalSound( h, CHAN_LOCAL_SOUND );
		}
	}
}


/*
=================
S_Music_f
=================
*/
static void S_Music_f( void ) {
	int		c;

	if( !si.StartBackgroundTrack ) {
		return;
	}

	c = Cmd_Argc();

	if ( c == 2 ) {
		si.StartBackgroundTrack( Cmd_Argv(1), NULL );
	} else if ( c == 3 ) {
		si.StartBackgroundTrack( Cmd_Argv(1), Cmd_Argv(2) );
	} else {
		Com_Printf ("Usage: music <musicfile> [loopfile]\n");
		return;
	}

}


/*
=================
S_StopMusic_f
=================
*/
static void S_StopMusic_f( void )
{
	if ( !si.StopBackgroundTrack )
		return;

	si.StopBackgroundTrack();
}


//=============================================================================

/*
=================
S_OpenAL_Info_f
=================
Display OpenAL information
=================
*/
static void S_OpenAL_Info_f(void)
{
	if (si.SoundInfo) {
		si.SoundInfo();
	}
}

/*
=================
S_OpenAL_Reverb_f
=================
Control environmental reverb settings
=================
*/
static void S_OpenAL_Reverb_f(void)
{
	int argc = Cmd_Argc();
	float roomSize, dampening, wetness, dryMix;

	if (argc < 5) {
		Com_Printf("Usage: s_openal_reverb <roomSize> <dampening> <wetness> <dryMix>\n");
		Com_Printf("All values are 0.0 to 1.0\n");
		return;
	}

	roomSize = Com_Clamp(0.0f, 1.0f, atof(Cmd_Argv(1)));
	dampening = Com_Clamp(0.0f, 1.0f, atof(Cmd_Argv(2)));
	wetness = Com_Clamp(0.0f, 1.0f, atof(Cmd_Argv(3)));
	dryMix = Com_Clamp(0.0f, 1.0f, atof(Cmd_Argv(4)));

	SndOpenAL_SetEnvironmentReverb(roomSize, dampening, wetness, dryMix);
	Com_Printf("OpenAL reverb set: room=%.2f, damp=%.2f, wet=%.2f, dry=%.2f\n",
			   roomSize, dampening, wetness, dryMix);
}

/*
=================
S_Init
=================
*/
// Complete OpenAL sound interface wrapper functions
static soundInterface_t openalInterface;

static qboolean S_OpenAL_Init(void) {
	return SndOpenAL_Init();
}

static void S_OpenAL_Shutdown(void) {
	SndOpenAL_Shutdown();
}

static void S_OpenAL_StartSound(const vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx) {
	sfx_t *sfxData = S_GetSfxByHandle(sfx);
	if (!sfxData) return;

	sndOpenAL3DProps_t props = {0};

	// Set 3D properties based on origin
	if (origin) {
		VectorCopy(origin, props.position);
		props.flags |= SND_OPENAL_3D;
		props.minDistance = 128.0f;
		props.maxDistance = 1024.0f;

		// Enable Doppler effect for moving sounds
		if (s_doppler && s_doppler->integer) {
			props.flags |= SND_OPENAL_DOPPLER;
		}
	} else {
		// Local sounds
		props.minDistance = 1.0f;
		props.maxDistance = 1.0f;
	}

	props.volume = 1.0f;
	props.pitch = 1.0f;
	props.looping = qfalse;
	props.entityNum = entnum;
	props.channel = entchannel;

	SndOpenAL_PlaySound(sfxData->soundName, &props);
}

static void S_OpenAL_StartLocalSound(sfxHandle_t sfx, int channelNum) {
	sfx_t *sfxData = S_GetSfxByHandle(sfx);
	if (!sfxData) return;

	sndOpenAL3DProps_t props = {0};
	props.minDistance = 1.0f;
	props.maxDistance = 1.0f;
	props.volume = 1.0f;
	props.pitch = 1.0f;
	props.looping = qfalse;
	props.flags = 0; // No 3D effects for local sounds
	props.entityNum = -1;
	props.channel = channelNum;

	SndOpenAL_PlaySound(sfxData->soundName, &props);
}

static void S_OpenAL_StartBackgroundTrack(const char *intro, const char *loop) {
	// Use OpenAL streaming for background music
	if (intro && *intro) {
		SndOpenAL_StartStream(intro, loop && *loop);
	}
}

static void S_OpenAL_StopBackgroundTrack(void) {
	SndOpenAL_StopStream(0); // Stop all streams
}

static void S_OpenAL_RawSamples(int samples, int rate, int width, int channels, const byte *data, float volume) {
	// OpenAL handles raw samples through its mixing system
	// This would need additional implementation for streaming raw audio
	(void)samples; (void)rate; (void)width; (void)channels; (void)data; (void)volume;
	// TODO: Implement raw sample streaming with OpenAL
}

static void S_OpenAL_ClearLoopingSounds(qboolean killall) {
	// OpenAL handles looping sounds through its source management
	// This would need to track looping sounds separately
	(void)killall;
	// TODO: Implement looping sound management
}

static void S_OpenAL_AddLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle) {
	sfx_t *sfxData = S_GetSfxByHandle(sfxHandle);
	if (!sfxData) return;

	sndOpenAL3DProps_t props = {0};
	if (origin) {
		VectorCopy(origin, props.position);
	}
	if (velocity) {
		VectorCopy(velocity, props.velocity);
	}

	props.minDistance = 128.0f;
	props.maxDistance = 1024.0f;
	props.volume = 1.0f;
	props.pitch = 1.0f;
	props.looping = qtrue;
	props.flags = SND_OPENAL_3D | SND_OPENAL_DOPPLER;
	props.entityNum = entityNum;

	SndOpenAL_PlaySound(sfxData->soundName, &props);
}

static void S_OpenAL_StopLoopingSound(int entityNum) {
	// Need to implement entity-based sound stopping in OpenAL
	// For now, just stop all looping sounds
	SndOpenAL_StopAllSounds();
}

static void S_OpenAL_Respatialize(int entityNum, const vec3_t origin, vec3_t axis[3], int inwater) {
	// Update listener position and orientation
	vec3_t forward, up, velocity = {0, 0, 0};

	if (axis) {
		VectorCopy(axis[0], forward);
		VectorCopy(axis[2], up);
	} else {
		VectorSet(forward, 0, 1, 0);
		VectorSet(up, 0, 0, 1);
	}

	SndOpenAL_SetListenerPosition(origin ? origin : vec3_origin, forward, up, velocity);
	(void)entityNum; (void)inwater; // Not used in current implementation
}

static void S_OpenAL_UpdateEntityPosition(int entityNum, const vec3_t origin) {
	// Update entity position for 3D sounds
	// This would require tracking entity-to-sound mappings
	(void)entityNum; (void)origin;
	// TODO: Implement entity position tracking for 3D sounds
}

static void S_OpenAL_Update(int msec) {
	// OpenAL handles internal updates, but we could add custom processing here
	(void)msec;
	// TODO: Add any OpenAL-specific update processing if needed
}

static void S_OpenAL_DisableSounds(void) {
	SndOpenAL_StopAllSounds();
}

static void S_OpenAL_BeginRegistration(void) {
	// OpenAL doesn't need explicit registration
}

static sfxHandle_t S_OpenAL_RegisterSound(const char *sample, qboolean compressed) {
	// Use existing sound registration system
	return S_RegisterSound(sample, compressed);
}

static void S_OpenAL_ClearSoundBuffer(void) {
	SndOpenAL_StopAllSounds();
}

#ifdef USE_OPENAL
static void S_OpenAL_SoundInfo(void) {
	Com_Printf("OpenAL Sound System:\n");
	Com_Printf("  Device: %s\n", openalDevice ? alcGetString(openalDevice, ALC_DEVICE_SPECIFIER) : "None");
	Com_Printf("  Context: %s\n", openalContext ? "Active" : "None");
	Com_Printf("  EFX Available: %s\n", openalEfxAvailable ? "Yes" : "No");
	Com_Printf("  Sources: %d/%d\n", numOpenALSounds, MAX_OPENAL_SOURCES);
}
#else
static void S_OpenAL_SoundInfo(void) {
	Com_Printf("OpenAL Sound System: Not available (compiled without OpenAL support)\n");
}
#endif
static void S_OpenAL_StopAllSounds(void) { SndOpenAL_StopAllSounds(); }

// Initialize OpenAL sound interface structure
static void S_InitOpenALInterface(void) {
	Com_Memset(&openalInterface, 0, sizeof(soundInterface_t));

	openalInterface.Shutdown = S_OpenAL_Shutdown;
	openalInterface.StartSound = S_OpenAL_StartSound;
	openalInterface.StartLocalSound = S_OpenAL_StartLocalSound;
	openalInterface.StartBackgroundTrack = S_OpenAL_StartBackgroundTrack;
	openalInterface.StopBackgroundTrack = S_OpenAL_StopBackgroundTrack;
	openalInterface.RawSamples = S_OpenAL_RawSamples;
	openalInterface.StopAllSounds = S_OpenAL_StopAllSounds;
	openalInterface.ClearLoopingSounds = S_OpenAL_ClearLoopingSounds;
	openalInterface.AddLoopingSound = S_OpenAL_AddLoopingSound;
	openalInterface.AddRealLoopingSound = S_OpenAL_AddLoopingSound; // Same as regular looping
	openalInterface.StopLoopingSound = S_OpenAL_StopLoopingSound;
	openalInterface.Respatialize = S_OpenAL_Respatialize;
	openalInterface.UpdateEntityPosition = S_OpenAL_UpdateEntityPosition;
	openalInterface.Update = S_OpenAL_Update;
	openalInterface.DisableSounds = S_OpenAL_DisableSounds;
	openalInterface.BeginRegistration = S_OpenAL_BeginRegistration;
	openalInterface.RegisterSound = S_OpenAL_RegisterSound;
	openalInterface.ClearSoundBuffer = S_OpenAL_ClearSoundBuffer;
	openalInterface.SoundInfo = S_OpenAL_SoundInfo;
}

void S_Init( void )
{
	cvar_t		*cv;
	qboolean	started = qfalse;

	Com_Printf( "------ Initializing Sound ------\n" );

	s_volume = Cvar_Get( "s_volume", "0.8", CVAR_ARCHIVE );
	Cvar_CheckRange( s_volume, "0", "1", CV_FLOAT );
	Cvar_SetDescription( s_volume, "Sets master volume for all game audio." );
	s_musicVolume = Cvar_Get( "s_musicVolume", "0.25", CVAR_ARCHIVE );
	Cvar_CheckRange( s_musicVolume, "0", "1", CV_FLOAT );
	Cvar_SetDescription( s_musicVolume, "Sets volume for in-game music only." );
	s_doppler = Cvar_Get( "s_doppler", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_doppler, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_doppler, "Enables doppler effect on moving projectiles." );
	s_muteWhenUnfocused = Cvar_Get( "s_muteWhenUnfocused", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( s_muteWhenUnfocused, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_muteWhenUnfocused, "Mutes all audio while game window is unfocused." );
	s_muteWhenMinimized = Cvar_Get( "s_muteWhenMinimized", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( s_muteWhenMinimized, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_muteWhenMinimized, "Mutes all audio while game is minimized." );

	// Audio threading CVars
	Cvar_Get( "s_audio_threading", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( Cvar_Get( "s_audio_threading", "1", CVAR_ARCHIVE ), "Enable audio threading for low-latency processing." );

	cv = Cvar_Get( "s_initsound", "1", 0 );
	Cvar_SetDescription( cv, "Whether or not to startup the sound system." );
	if ( !cv->integer ) {
		Com_Printf( "Sound disabled.\n" );
	} else {

		S_CodecInit();

		Cmd_AddCommand( "play", S_Play_f );
		Cmd_AddCommand( "music", S_Music_f );
		Cmd_AddCommand( "stopmusic", S_StopMusic_f );
		Cmd_AddCommand( "s_list", S_SoundList );
		Cmd_AddCommand( "s_stop", S_StopAllSounds );
		Cmd_AddCommand( "s_info", S_SoundInfo );
		Cmd_AddCommand( "audiothreads", S_AudioThreads_f );

		// Add OpenAL-specific commands
		Cmd_AddCommand( "s_openal_info", S_OpenAL_Info_f );
		Cmd_AddCommand( "s_openal_reverb", S_OpenAL_Reverb_f );

		// Initialize OpenAL interface
		S_InitOpenALInterface();

		// Try OpenAL first for enhanced spatialization
		if ( !started ) {
			cvar_t *s_useOpenAL = Cvar_Get("s_useOpenAL", "1", CVAR_ARCHIVE);
			Cvar_SetDescription(s_useOpenAL, "Use OpenAL for enhanced 3D spatial audio (recommended)");

			if (s_useOpenAL->integer) {
				Com_Printf("Trying OpenAL sound system...\n");
				started = S_ValidSoundInterface(&openalInterface) && S_OpenAL_Init();
				if (started) {
					Com_Memcpy(&si, &openalInterface, sizeof(soundInterface_t));
					Com_Printf("OpenAL sound system initialized with spatialization support\n");
				} else {
					Com_Printf("OpenAL initialization failed, falling back to DMA sound system\n");
				}
			}
		}

		// Fall back to DMA sound system if OpenAL failed or is disabled
		if ( !started ) {
			started = S_Base_Init( &si );
			if (started) {
				Com_Printf("DMA sound system initialized\n");
			}
		}

		if ( started ) {
			if( !S_ValidSoundInterface( &si ) ) {
				Com_Error( ERR_FATAL, "Sound interface invalid" );
			}

			S_SoundInfo();
			Com_Printf( "Sound initialization successful.\n" );

			// Initialize audio thread system
			if (!AudioThread_Init()) {
				Com_Printf( "Audio thread system initialization failed.\n" );
			} else {
				// Enable mixing thread by default for low-latency audio processing
				AudioThread_EnableThread(AUDIO_THREAD_MIXING);
			}
		} else {
			Com_Printf( "Sound initialization failed.\n" );
		}
	}

	Com_Printf( "--------------------------------\n");
}


/*
=================
S_AudioThreads_f
=================
*/
void S_AudioThreads_f( void ) {
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "Usage: audiothreads <command> [args]\n" );
		ri.Printf( PRINT_ALL, "Commands:\n" );
		ri.Printf( PRINT_ALL, "  status          - Show current thread status\n" );
		ri.Printf( PRINT_ALL, "  enable <type>   - Enable dedicated audio thread\n" );
		ri.Printf( PRINT_ALL, "  disable <type>  - Disable dedicated audio thread\n" );
		ri.Printf( PRINT_ALL, "  stats [type]    - Show thread statistics\n" );
		ri.Printf( PRINT_ALL, "  wait [type]     - Wait for thread completion\n" );
		ri.Printf( PRINT_ALL, "\nThread types: mixing, spatial, streaming, effects\n" );
		return;
	}

	const char* cmd = ri.Cmd_Argv(1);

	if ( Q_stricmp( cmd, "status" ) == 0 ) {
		ri.Printf( PRINT_ALL, "=== Audio Threads Status ===\n" );
		ri.Printf( PRINT_ALL, "Audio Threading: %s\n", audio_thread_system.enabled ? "Enabled" : "Disabled" );
		ri.Printf( PRINT_ALL, "Active Threads: %d\n", atomic_load_explicit(&audio_thread_system.active_threads, memory_order_relaxed) );

		const char* threadNames[AUDIO_THREAD_MAX] = {
			"Mixing", "Spatial", "Streaming", "Effects"
		};

		for ( int i = 0; i < AUDIO_THREAD_MAX; i++ ) {
			ri.Printf( PRINT_ALL, "  %s Thread: %s\n", threadNames[i],
				AudioThread_IsThreadEnabled( (audio_thread_type_t)i ) ? "Enabled" : "Disabled" );
		}
	}
	else if ( Q_stricmp( cmd, "enable" ) == 0 ) {
		if ( ri.Cmd_Argc() < 3 ) {
			ri.Printf( PRINT_ALL, "Usage: audiothreads enable <type>\n" );
			return;
		}

		const char* typeStr = ri.Cmd_Argv(2);
		audio_thread_type_t threadType = AUDIO_THREAD_MAX;

		if ( Q_stricmp( typeStr, "mixing" ) == 0 ) threadType = AUDIO_THREAD_MIXING;
		else if ( Q_stricmp( typeStr, "spatial" ) == 0 ) threadType = AUDIO_THREAD_SPATIAL;
		else if ( Q_stricmp( typeStr, "streaming" ) == 0 ) threadType = AUDIO_THREAD_STREAMING;
		else if ( Q_stricmp( typeStr, "effects" ) == 0 ) threadType = AUDIO_THREAD_EFFECTS;

		if ( threadType == AUDIO_THREAD_MAX ) {
			ri.Printf( PRINT_ALL, "Invalid thread type: %s\n", typeStr );
			return;
		}

		if ( AudioThread_EnableThread( threadType ) ) {
			ri.Printf( PRINT_ALL, "Enabled %s audio thread\n", typeStr );
		} else {
			ri.Printf( PRINT_ALL, "Failed to enable %s audio thread\n", typeStr );
		}
	}
	else if ( Q_stricmp( cmd, "disable" ) == 0 ) {
		if ( ri.Cmd_Argc() < 3 ) {
			ri.Printf( PRINT_ALL, "Usage: audiothreads disable <type>\n" );
			return;
		}

		const char* typeStr = ri.Cmd_Argv(2);
		audio_thread_type_t threadType = AUDIO_THREAD_MAX;

		if ( Q_stricmp( typeStr, "mixing" ) == 0 ) threadType = AUDIO_THREAD_MIXING;
		else if ( Q_stricmp( typeStr, "spatial" ) == 0 ) threadType = AUDIO_THREAD_SPATIAL;
		else if ( Q_stricmp( typeStr, "streaming" ) == 0 ) threadType = AUDIO_THREAD_STREAMING;
		else if ( Q_stricmp( typeStr, "effects" ) == 0 ) threadType = AUDIO_THREAD_EFFECTS;

		if ( threadType == AUDIO_THREAD_MAX ) {
			ri.Printf( PRINT_ALL, "Invalid thread type: %s\n", typeStr );
			return;
		}

		AudioThread_DisableThread( threadType );
		ri.Printf( PRINT_ALL, "Disabled %s audio thread\n", typeStr );
	}
	else if ( Q_stricmp( cmd, "stats" ) == 0 ) {
		if ( ri.Cmd_Argc() >= 3 ) {
			const char* typeStr = ri.Cmd_Argv(2);
			audio_thread_type_t threadType = AUDIO_THREAD_MAX;

			if ( Q_stricmp( typeStr, "mixing" ) == 0 ) threadType = AUDIO_THREAD_MIXING;
			else if ( Q_stricmp( typeStr, "spatial" ) == 0 ) threadType = AUDIO_THREAD_SPATIAL;
			else if ( Q_stricmp( typeStr, "streaming" ) == 0 ) threadType = AUDIO_THREAD_STREAMING;
			else if ( Q_stricmp( typeStr, "effects" ) == 0 ) threadType = AUDIO_THREAD_EFFECTS;

			if ( threadType != AUDIO_THREAD_MAX ) {
				uint64_t processedItems = 0;
				float avgTimeMs = 0.0f;
				uint64_t totalTimeNs = 0;

				AudioThread_GetStats( threadType, &processedItems, &avgTimeMs, &totalTimeNs );
				ri.Printf( PRINT_ALL, "%s Thread Stats:\n", typeStr );
				ri.Printf( PRINT_ALL, "  Processed Items: %llu\n", (unsigned long long)processedItems );
				ri.Printf( PRINT_ALL, "  Average Time: %.2f ms\n", avgTimeMs );
				ri.Printf( PRINT_ALL, "  Total Time: %.2f ms\n", totalTimeNs / 1000000.0 );
			} else {
				ri.Printf( PRINT_ALL, "Invalid thread type: %s\n", typeStr );
			}
		} else {
			ri.Printf( PRINT_ALL, "=== All Audio Thread Statistics ===\n" );
			const char* threadNames[AUDIO_THREAD_MAX] = {
				"Mixing", "Spatial", "Streaming", "Effects"
			};

			for ( int i = 0; i < AUDIO_THREAD_MAX; i++ ) {
				if ( AudioThread_IsThreadEnabled( (audio_thread_type_t)i ) ) {
					uint64_t processedItems = 0;
					float avgTimeMs = 0.0f;
					uint64_t totalTimeNs = 0;

					AudioThread_GetStats( (audio_thread_type_t)i, &processedItems, &avgTimeMs, &totalTimeNs );
					ri.Printf( PRINT_ALL, "%s: %llu items, %.2f ms avg\n", threadNames[i],
						(unsigned long long)processedItems, avgTimeMs );
				}
			}
			ri.Printf( PRINT_ALL, "Total Frames: %llu\n",
				(unsigned long long)atomic_load_explicit(&audio_thread_system.total_audio_frames_processed, memory_order_relaxed) );
		}
	}
	else if ( Q_stricmp( cmd, "wait" ) == 0 ) {
		if ( ri.Cmd_Argc() >= 3 ) {
			const char* typeStr = ri.Cmd_Argv(2);
			audio_thread_type_t threadType = AUDIO_THREAD_MAX;

			if ( Q_stricmp( typeStr, "mixing" ) == 0 ) threadType = AUDIO_THREAD_MIXING;
			else if ( Q_stricmp( typeStr, "spatial" ) == 0 ) threadType = AUDIO_THREAD_SPATIAL;
			else if ( Q_stricmp( typeStr, "streaming" ) == 0 ) threadType = AUDIO_THREAD_STREAMING;
			else if ( Q_stricmp( typeStr, "effects" ) == 0 ) threadType = AUDIO_THREAD_EFFECTS;

			if ( threadType != AUDIO_THREAD_MAX ) {
				AudioThread_WaitForThread( threadType );
				ri.Printf( PRINT_ALL, "Waited for %s audio thread\n", typeStr );
			} else {
				ri.Printf( PRINT_ALL, "Invalid thread type: %s\n", typeStr );
			}
		} else {
			AudioThread_WaitForAllThreads();
			ri.Printf( PRINT_ALL, "Waited for all audio threads\n" );
		}
	}
	else {
		ri.Printf( PRINT_ALL, "Unknown command: %s\n", cmd );
		ri.Printf( PRINT_ALL, "Use 'audiothreads' with no arguments for help\n" );
	}
}

/*
=================
S_Shutdown
=================
*/
void S_Shutdown( void )
{
	// Shutdown OpenAL enhanced audio system
	SndOpenAL_Shutdown();
	
	if ( si.StopAllSounds ) {
		si.StopAllSounds();
	}

	if ( si.Shutdown ) {
		si.Shutdown();
	}

	Com_Memset( &si, 0, sizeof( soundInterface_t ) );

	Cmd_RemoveCommand( "play" );
	Cmd_RemoveCommand( "music");
	Cmd_RemoveCommand( "stopmusic");
	Cmd_RemoveCommand( "s_list" );
	Cmd_RemoveCommand( "s_stop" );
	Cmd_RemoveCommand( "s_info" );
	Cmd_RemoveCommand( "audiothreads" );
	Cmd_RemoveCommand( "s_openal_info" );
	Cmd_RemoveCommand( "s_openal_reverb" );

	S_CodecShutdown();

	// Shutdown OpenAL enhanced audio system
	SndOpenAL_Shutdown();

	// Shutdown audio thread system
	AudioThread_Shutdown();

	cls.soundStarted = qfalse;
}
