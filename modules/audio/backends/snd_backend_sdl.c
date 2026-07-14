/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

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

#include <SDL3/SDL.h>

#include "q_shared.h"
#include "../snd_local.h"
#include "../../client/client.h"
#include "../../client/platform/cl_voip.h"

qboolean snd_inited = qfalse;

extern cvar_t *s_khz;
cvar_t *s_sdlBits;
cvar_t *s_sdlChannels;
cvar_t *s_sdlDevSamps;
cvar_t *s_sdlMixSamps;

/* The audio callback. All the magic happens here. */
static int dmapos = 0;
static int dmasize = 0;

static SDL_AudioStream *sdlPlaybackStream = NULL;

#if defined USE_OPUS && defined USE_SDL && !defined USE_OPENAL
#define USE_SDL_AUDIO_CAPTURE

static SDL_AudioStream *sdlCaptureStream = NULL;
static cvar_t *s_sdlCapture;
static float sdlMasterGain = 1.0f;
#endif


/*
===============
SNDDMA_FillBuffer

Fill dma ring into a byte buffer for the AudioStream callback.
===============
*/
static void SNDDMA_FillBuffer( void *userdata, Uint8 *stream, int len )
{
	(void)userdata;
	int pos = (dmapos * (dma.samplebits/8));
	if (pos >= dmasize)
		dmapos = pos = 0;

	if (!snd_inited)  /* shouldn't happen, but just in case... */
	{
		memset(stream, '\0', len);
		return;
	}
	else
	{
		int tobufend = dmasize - pos;  /* bytes to buffer's end. */
		int len1 = len;
		int len2 = 0;

		if (len1 > tobufend)
		{
			len1 = tobufend;
			len2 = len - len1;
		}
		memcpy(stream, dma.buffer + pos, len1);
		if (len2 <= 0)
			dmapos += (len1 / (dma.samplebits/8));
		else  /* wraparound? */
		{
			memcpy(stream+len1, dma.buffer, len2);
			dmapos = (len2 / (dma.samplebits/8));
		}
	}

	if (dmapos >= dmasize)
		dmapos = 0;

#ifdef USE_SDL_AUDIO_CAPTURE
	if (sdlMasterGain != 1.0f)
	{
		int i;
		if (dma.isfloat && (dma.samplebits == 32))
		{
			float *ptr = (float *) stream;
			len /= sizeof (*ptr);
			for (i = 0; i < len; i++, ptr++)
			{
				*ptr *= sdlMasterGain;
			}
		}
		else if (dma.samplebits == 16)
		{
			Sint16 *ptr = (Sint16 *) stream;
			len /= sizeof (*ptr);
			for (i = 0; i < len; i++, ptr++)
			{
				*ptr = (Sint16) (((float) *ptr) * sdlMasterGain);
			}
		}
		else if (dma.samplebits == 8)
		{
			Uint8 *ptr = (Uint8 *) stream;
			len /= sizeof (*ptr);
			for (i = 0; i < len; i++, ptr++)
			{
				*ptr = (Uint8) (((float) *ptr) * sdlMasterGain);
			}
		}
	}
#endif
}


/*
===============
SNDDMA_AudioStreamCallback
===============
*/
static void SDLCALL SNDDMA_AudioStreamCallback( void *userdata, SDL_AudioStream *stream,
	int additional_amount, int total_amount )
{
	(void)total_amount;
	if ( additional_amount > 0 )
	{
		Uint8 *data = SDL_stack_alloc( Uint8, additional_amount );
		if ( data )
		{
			SNDDMA_FillBuffer( userdata, data, additional_amount );
			SDL_PutAudioStreamData( stream, data, additional_amount );
			SDL_stack_free( data );
		}
	}
}

static const struct
{
	SDL_AudioFormat	enumFormat;
	const char	*stringFormat;
} formatToStringTable[ ] =
{
	{ SDL_AUDIO_U8,     "SDL_AUDIO_U8" },
	{ SDL_AUDIO_S8,     "SDL_AUDIO_S8" },
	{ SDL_AUDIO_S16LE,  "SDL_AUDIO_S16LE" },
	{ SDL_AUDIO_S16BE,  "SDL_AUDIO_S16BE" },
	{ SDL_AUDIO_S32LE,  "SDL_AUDIO_S32LE" },
	{ SDL_AUDIO_S32BE,  "SDL_AUDIO_S32BE" },
	{ SDL_AUDIO_F32LE,  "SDL_AUDIO_F32LE" },
	{ SDL_AUDIO_F32BE,  "SDL_AUDIO_F32BE" }
};

static int formatToStringTableSize = ARRAY_LEN( formatToStringTable );

/*
===============
SNDDMA_PrintAudiospec
===============
*/
static void SNDDMA_PrintAudiospec(const char *str, const SDL_AudioSpec *spec)
{
	const char *fmt = NULL;
	int i;

	Com_Printf( "%s:\n", str );

	for ( i = 0; i < formatToStringTableSize; i++ ) {
		if( spec->format == formatToStringTable[ i ].enumFormat ) {
			fmt = formatToStringTable[ i ].stringFormat;
		}
	}

	if ( fmt ) {
		Com_Printf( "  Format:   %s\n", fmt );
	} else {
		Com_Printf( "  Format:   " S_COLOR_RED "UNKNOWN\n");
	}

	Com_Printf( "  Freq:     %d\n", (int) spec->freq );
	Com_Printf( "  Channels: %d\n", (int) spec->channels );
}


static int SNDDMA_KHzToHz( int khz )
{
	switch ( khz )
	{
		case 48: return 48000;
		case 44: return 44100;
		case 22: return 22050;
		case 11: return 11025;
		case  8: return  8000;
		default: return 44100;  /* fallback: at least 44 kHz */
	}
}


/*
===============
SNDDMA_Init
===============
*/
qboolean SNDDMA_Init( void )
{
	SDL_AudioSpec desired;
	int tmp;
	int callbackSamples;

	if ( snd_inited )
		return qtrue;

	{
		s_sdlBits = Cvar_Get( "s_sdlBits", "16", CVAR_ARCHIVE_ND | CVAR_LATCH );
		Cvar_CheckRange( s_sdlBits, "8", "16", CV_INTEGER );
		Cvar_SetDescription( s_sdlBits, "Bits per-sample to request for SDL audio output (possible options: 8 or 16). When set to 0 it uses 16." );

		s_sdlChannels = Cvar_Get( "s_sdlChannels", "2", CVAR_ARCHIVE_ND | CVAR_LATCH );
		Cvar_CheckRange( s_sdlChannels, "1", "2", CV_INTEGER );
		Cvar_SetDescription( s_sdlChannels, "Number of audio channels to request for SDL audio output. The Quake 3 audio mixer only supports mono and stereo. Additional channels are silent." );

		s_sdlDevSamps = Cvar_Get( "s_sdlDevSamps", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
		Cvar_SetDescription( s_sdlDevSamps, "Number of audio samples to provide to the SDL audio output device. When set to 0 it picks a value based on s_sdlSpeed." );
		s_sdlMixSamps = Cvar_Get( "s_sdlMixSamps", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
		Cvar_SetDescription( s_sdlMixSamps, "Number of audio samples for Quake 3's audio mixer when using SDL audio output." );
	}

	Com_Printf( "SDL_Init( SDL_INIT_AUDIO )... " );

	if ( !SDL_Init( SDL_INIT_AUDIO ) )
	{
		Com_Printf( "FAILED (%s)\n", SDL_GetError() );
		return qfalse;
	}

	Com_Printf( "OK\n" );

	Com_Printf( "SDL audio driver is \"%s\".\n", SDL_GetCurrentAudioDriver() );

	memset( &desired, '\0', sizeof (desired) );

	desired.freq = SNDDMA_KHzToHz( s_khz->integer );
	if ( desired.freq == 0 )
		desired.freq = 44100;

	tmp = s_sdlBits->integer;
	if ( tmp < 16 )
		tmp = 8;

	desired.format = ((tmp == 16) ? SDL_AUDIO_S16 : SDL_AUDIO_U8);
	desired.channels = s_sdlChannels->integer;

	if ( s_sdlDevSamps->integer )
		callbackSamples = s_sdlDevSamps->integer;
	else
	{
		if (desired.freq <= 11025)
			callbackSamples = 256;
		else if (desired.freq <= 22050)
			callbackSamples = 512;
		else if (desired.freq <= 44100)
			callbackSamples = 1024;
		else
			callbackSamples = 2048;
	}
	(void)callbackSamples;

	sdlPlaybackStream = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired,
		SNDDMA_AudioStreamCallback, NULL );
	if ( !sdlPlaybackStream )
	{
		Com_Printf( "SDL_OpenAudioDeviceStream() failed: %s\n", SDL_GetError() );
		SDL_QuitSubSystem( SDL_INIT_AUDIO );
		return qfalse;
	}

	SNDDMA_PrintAudiospec( "SDL_AudioSpec", &desired );

	tmp = s_sdlMixSamps->integer;
	if ( !tmp )
		tmp = (callbackSamples * desired.channels) * 10;

	tmp -= tmp % desired.channels;
	tmp = log2pad( tmp, 1 );

	dmapos = 0;
	dma.samplebits = SDL_AUDIO_BITSIZE( desired.format );
	dma.isfloat = SDL_AUDIO_ISFLOAT( desired.format );
	dma.channels = desired.channels;
	dma.samples = tmp;
	dma.fullsamples = dma.samples / dma.channels;
	dma.submission_chunk = 1;
	dma.speed = desired.freq;
	dmasize = (dma.samples * (dma.samplebits/8));
	dma.buffer = calloc(1, dmasize);

#ifdef USE_SDL_AUDIO_CAPTURE
	s_sdlCapture = Cvar_Get( "s_sdlCapture", "1", CVAR_ARCHIVE | CVAR_LATCH );
	Cvar_SetDescription( s_sdlCapture, "Set to 1 to enable SDL audio capture." );
	if (Q_stricmp(SDL_GetCurrentAudioDriver(), "pulseaudio") == 0)
	{
		Com_Printf("SDL audio capture support disabled for pulseaudio (https://bugzilla.libsdl.org/show_bug.cgi?id=4087)\n");
	}
	else if (!s_sdlCapture->integer)
	{
		Com_Printf("SDL audio capture support disabled by user ('+set s_sdlCapture 1' to enable)\n");
	}
#if USE_MUMBLE
	else if (cl_useMumble->integer)
	{
		Com_Printf("SDL audio capture support disabled for Mumble support\n");
	}
#endif
	else
	{
		SDL_AudioSpec spec;
		SDL_zero(spec);
		spec.freq = 48000;
		spec.format = SDL_AUDIO_S16;
		spec.channels = 1;
		sdlCaptureStream = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, NULL, NULL );
		Com_Printf( "SDL capture device %s.\n",
				    (sdlCaptureStream == NULL) ? "failed to open" : "opened");
	}

	sdlMasterGain = 1.0f;
#endif

	Com_Printf("Starting SDL audio callback...\n");
	SDL_ResumeAudioDevice( SDL_GetAudioStreamDevice( sdlPlaybackStream ) );

	Com_Printf("SDL3 audio backend initialized.\n");
	snd_inited = qtrue;
	return qtrue;
}


/*
===============
SNDDMA_GetDMAPos
===============
*/
int SNDDMA_GetDMAPos( void )
{
	return dmapos;
}


/*
===============
SNDDMA_Shutdown
===============
*/
void SNDDMA_Shutdown( void )
{
	if (sdlPlaybackStream != NULL)
	{
		Com_Printf("Closing SDL audio playback device...\n");
		SDL_DestroyAudioStream(sdlPlaybackStream);
		Com_Printf("SDL audio playback device closed.\n");
		sdlPlaybackStream = NULL;
	}

#ifdef USE_SDL_AUDIO_CAPTURE
	if (sdlCaptureStream)
	{
		Com_Printf("Closing SDL audio capture device...\n");
		SDL_DestroyAudioStream(sdlCaptureStream);
		Com_Printf("SDL audio capture device closed.\n");
		sdlCaptureStream = NULL;
	}
#endif

	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	free(dma.buffer);
	dma.buffer = NULL;
	dmapos = dmasize = 0;
	snd_inited = qfalse;
	Com_Printf("SDL audio shut down.\n");
}


/*
===============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit( void )
{
}

/*
===============
SNDDMA_Activate

SDL backend does not require explicit activation on window focus changes.
Keep as a no-op to satisfy platform callers.
===============
*/
void SNDDMA_Activate( void )
{
}


/*
===============
SNDDMA_BeginPainting
===============
*/
void SNDDMA_BeginPainting( void )
{
}


#if defined USE_OPUS && defined USE_SDL_AUDIO_CAPTURE
void SNDDMA_StartCapture(void)
{
	if (sdlCaptureStream)
	{
		SDL_ClearAudioStream(sdlCaptureStream);
		SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(sdlCaptureStream));
	}
}

int SNDDMA_AvailableCaptureSamples(void)
{
	return sdlCaptureStream ? (SDL_GetAudioStreamAvailable(sdlCaptureStream) / 2) : 0;
}

void SNDDMA_Capture(int samples, byte *data)
{
	if (sdlCaptureStream)
	{
		SDL_GetAudioStreamData(sdlCaptureStream, data, samples * 2);
	}
	else
	{
		SDL_memset(data, '\0', samples * 2);
	}
}

void SNDDMA_StopCapture(void)
{
	if (sdlCaptureStream)
	{
		SDL_PauseAudioDevice(SDL_GetAudioStreamDevice(sdlCaptureStream));
	}
}

void SNDDMA_MasterGain( float val )
{
	sdlMasterGain = val;
}
#endif
