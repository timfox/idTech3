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

cvar_t *s_volume;
cvar_t *s_musicVolume;
cvar_t *s_doppler;
cvar_t *s_muteWhenMinimized;
cvar_t *s_muteWhenUnfocused;
#ifdef USE_OPENAL
cvar_t *s_openal;
cvar_t *s_openalDevice;
cvar_t *s_openalHrtf;
cvar_t *s_openalEfx;
cvar_t *s_openalEfxPreset;
cvar_t *s_openalCapture;
cvar_t *s_openalDopplerFactor;
cvar_t *s_openalDopplerSpeed;
cvar_t *s_openalRolloff;
cvar_t *s_openalMaxDistance;
cvar_t *s_openalLowpass;
cvar_t *s_openalLowpassHf;
cvar_t *s_openalOcclusion;
cvar_t *s_openalOcclusionGain;
cvar_t *s_openalOcclusionHf;
cvar_t *s_openalVoipSpatial;
cvar_t *s_openalVoipGain;
cvar_t *s_openalDebug;
#endif

cvar_t *s_musicLayer;
cvar_t *s_musicLayerEnabled;
cvar_t *s_musicLayerVolume;
cvar_t *s_musicIntensity;

static soundInterface_t si;

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
S_VoipSamples
=================
*/
void S_VoipSamples( int entityNum, const vec3_t origin, int samples, int rate,
		    int width, int channels, const byte *data, float volume )
{
	if ( si.VoipSamples ) {
		si.VoipSamples( entityNum, origin, samples, rate, width, channels, data, volume );
	} else if ( si.RawSamples ) {
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
S_Init
=================
*/
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

#ifdef USE_OPENAL
	s_openal = Cvar_Get( "s_openal", "0", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_CheckRange( s_openal, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_openal, "Enables the OpenAL audio backend (requires restart)." );
	
	s_openalDevice = Cvar_Get( "s_openalDevice", "default", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( s_openalDevice, "OpenAL device name (\"default\" uses the system default)." );
	
	s_openalHrtf = Cvar_Get( "s_openalHrtf", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_openalHrtf, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_openalHrtf, "Enable OpenAL HRTF if supported by the device." );
	
	s_openalEfx = Cvar_Get( "s_openalEfx", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_openalEfx, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_openalEfx, "Enable OpenAL EFX environmental audio effects." );

	s_openalEfxPreset = Cvar_Get( "s_openalEfxPreset", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_openalEfxPreset, "0", "4", CV_INTEGER );
	Cvar_SetDescription( s_openalEfxPreset, "OpenAL EFX reverb preset (0=off, 1=generic, 2=hall, 3=cave, 4=underwater)." );
	
	s_openalCapture = Cvar_Get( "s_openalCapture", "1", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_CheckRange( s_openalCapture, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_openalCapture, "Enable OpenAL audio capture for VOIP." );
	
	s_openalDopplerFactor = Cvar_Get( "s_openalDopplerFactor", "1.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_openalDopplerFactor, "0", "10", CV_FLOAT );
	Cvar_SetDescription( s_openalDopplerFactor, "OpenAL doppler effect strength (0=off, 1=normal, higher=exaggerated)." );
	
	s_openalDopplerSpeed = Cvar_Get( "s_openalDopplerSpeed", "9000", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_openalDopplerSpeed, "1", "100000", CV_FLOAT );
	Cvar_SetDescription( s_openalDopplerSpeed, "Speed of sound for doppler effect (units/second)." );
	
	s_openalRolloff = Cvar_Get( "s_openalRolloff", "1.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_openalRolloff, "0", "10", CV_FLOAT );
	Cvar_SetDescription( s_openalRolloff, "OpenAL distance attenuation rolloff factor." );
	
	s_openalMaxDistance = Cvar_Get( "s_openalMaxDistance", "2000", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_openalMaxDistance, "100", "10000", CV_FLOAT );
	Cvar_SetDescription( s_openalMaxDistance, "Maximum distance for sound attenuation." );

	s_openalLowpass = Cvar_Get( "s_openalLowpass", "0.0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_openalLowpass, "0", "1", CV_FLOAT );
	Cvar_SetDescription( s_openalLowpass, "Enable global OpenAL low-pass filter (0=off, 1=full) requires restart." );

	s_openalLowpassHf = Cvar_Get( "s_openalLowpassHf", "0.5", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_openalLowpassHf, "0.05", "1", CV_FLOAT );
	Cvar_SetDescription( s_openalLowpassHf, "OpenAL low-pass high-frequency gain (0.05-1.0). Requires restart." );

	s_openalOcclusion = Cvar_Get( "s_openalOcclusion", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_openalOcclusion, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_openalOcclusion, "Enable OpenAL per-source occlusion/obstruction (requires restart)." );

	s_openalOcclusionGain = Cvar_Get( "s_openalOcclusionGain", "0.5", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_openalOcclusionGain, "0.05", "1", CV_FLOAT );
	Cvar_SetDescription( s_openalOcclusionGain, "Occluded direct gain multiplier for OpenAL sources." );

	s_openalOcclusionHf = Cvar_Get( "s_openalOcclusionHf", "0.2", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_openalOcclusionHf, "0.05", "1", CV_FLOAT );
	Cvar_SetDescription( s_openalOcclusionHf, "Occluded high-frequency gain for OpenAL low-pass filter." );

	s_openalVoipSpatial = Cvar_Get( "s_openalVoipSpatial", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_openalVoipSpatial, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_openalVoipSpatial, "Route VOIP through spatial OpenAL sources." );

	s_openalVoipGain = Cvar_Get( "s_openalVoipGain", "1.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_openalVoipGain, "0", "2", CV_FLOAT );
	Cvar_SetDescription( s_openalVoipGain, "VOIP gain multiplier for OpenAL sources." );
	
	s_openalDebug = Cvar_Get( "s_openalDebug", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_openalDebug, "0", "2", CV_INTEGER );
	Cvar_SetDescription( s_openalDebug, "OpenAL debugging output level (0=off, 1=basic, 2=verbose)." );
#endif

	s_musicLayer = Cvar_Get( "s_musicLayer", "", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( s_musicLayer, "Optional secondary music layer track (path). Requires restart." );

	s_musicLayerEnabled = Cvar_Get( "s_musicLayerEnabled", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_CheckRange( s_musicLayerEnabled, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_musicLayerEnabled, "Enable dynamic music layering (requires restart)." );

	s_musicLayerVolume = Cvar_Get( "s_musicLayerVolume", "1.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_musicLayerVolume, "0", "2", CV_FLOAT );
	Cvar_SetDescription( s_musicLayerVolume, "Base volume multiplier for the secondary music layer." );

	s_musicIntensity = Cvar_Get( "s_musicIntensity", "0.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_musicIntensity, "0", "1", CV_FLOAT );
	Cvar_SetDescription( s_musicIntensity, "Music intensity value for dynamic layers (0-1)." );

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

#ifdef USE_OPENAL
		if ( s_openal->integer ) {
			Com_Printf( "OpenAL backend requested.\n" );
			started = S_AL_Init( &si );
			if ( !started ) {
				Com_Printf( S_COLOR_YELLOW "OpenAL init failed, falling back to default backend.\n" );
			}
		} else {
			Com_Printf( "OpenAL backend disabled (s_openal=0).\n" );
		}
#endif
		if ( !started ) {
			started = S_Base_Init( &si );
		}

		if ( started ) {
			if( !S_ValidSoundInterface( &si ) ) {
				Com_Error( ERR_FATAL, "Sound interface invalid" );
			}

			S_SoundInfo();
			Com_Printf( "Sound initialization successful.\n" );
		} else {
			Com_Printf( "Sound initialization failed.\n" );
		}
	}

	Com_Printf( "--------------------------------\n");
}


/*
=================
S_Shutdown
=================
*/
void S_Shutdown( void )
{
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

	S_CodecShutdown();

	cls.soundStarted = qfalse;
}
