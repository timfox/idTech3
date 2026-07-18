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
#ifndef AUDIO_SND_LOCAL_H
#define AUDIO_SND_LOCAL_H

// snd_local.h -- private sound definitions


#include "q_shared.h"
#include "qcommon.h"
#include "snd_public.h"

#define	PAINTBUFFER_SIZE		4096					// this is in samples

#define SND_CHUNK_SIZE			1024					// samples
#define SND_CHUNK_SIZE_FLOAT	(SND_CHUNK_SIZE/2)		// floats
#define SND_CHUNK_SIZE_BYTE		(SND_CHUNK_SIZE*2)		// floats

typedef struct {
	int			left;	// the final values will be clamped to +/- 0x00ffff00 and shifted down
	int			right;
} portable_samplepair_t;

typedef struct adpcm_state {
	short	sample;		/* Previous output value */
	char	index;		/* Index into stepsize table */
} adpcm_state_t;

typedef	struct sndBuffer_s {
	short					sndChunk[SND_CHUNK_SIZE];
	struct sndBuffer_s		*next;
	int						size;
	adpcm_state_t			adpcm;
} sndBuffer;

typedef struct sfx_s {
	sndBuffer		*soundData;
	qboolean		defaultSound;			// couldn't be loaded, so use buzz
	qboolean		inMemory;				// not in Memory
	qboolean		soundCompressed;		// not in Memory
	int				soundCompressionMethod;
	int 			soundLength;
	int				soundChannels;
	char 			soundName[MAX_QPATH];
	int				lastTimeUsed;
	struct sfx_s	*next;
} sfx_t;

typedef struct {
	unsigned int channels;
	unsigned int samples;				// mono samples in buffer
	int			fullsamples;			// samples with all channels in buffer (samples divided by channels)
	int			submission_chunk;		// don't mix less than this #
	int			samplebits;
	int			isfloat;
	int			speed;
	byte		*buffer;
	const char	*driver;
} dma_t;

extern byte *dma_buffer2;

#define START_SAMPLE_IMMEDIATE	0x7fffffff

#define MAX_DOPPLER_SCALE 50.0f //arbitrary

typedef struct loopSound_s {
	vec3_t		origin;
	vec3_t		velocity;
	sfx_t		*sfx;
	int			mergeFrame;
	qboolean	active;
	qboolean	kill;
	qboolean	doppler;
	float		dopplerScale;
	float		oldDopplerScale;
	int			framenum;
} loopSound_t;

typedef struct
{
	int			allocTime;
	int			startSample;	// START_SAMPLE_IMMEDIATE = set immediately on next mix
	int			entnum;			// to allow overriding a specific sound
	int			entchannel;		// to allow overriding a specific sound
	int			leftvol;		// 0-255 volume after spatialization
	int			rightvol;		// 0-255 volume after spatialization
	int			master_vol;		// 0-255 volume before spatialization
	float		dopplerScale;
	float		oldDopplerScale;
	vec3_t		origin;			// only use if fixed_origin is set
	qboolean	fixed_origin;	// use origin instead of fetching entnum's origin
	sfx_t		*thesfx;		// sfx structure
	qboolean	doppler;
} channel_t;


#define WAV_FORMAT_PCM			0x0001
#define WAVE_FORMAT_IEEE_FLOAT	0x0003

typedef struct {
	int			format;
	int			rate;
	int			width;
	int			channels;
	int			samples;
	int			dataofs;		// chunk starts this many bytes from file start
} wavinfo_t;

// Interface between Q3 sound "api" and the sound backend
typedef struct
{
	void (*Shutdown)(void);
	void (*StartSound)( const vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx );
	void (*StartLocalSound)( sfxHandle_t sfx, int channelNum );
	void (*StartBackgroundTrack)( const char *intro, const char *loop );
	void (*StopBackgroundTrack)( void );
	void (*RawSamples)(int samples, int rate, int width, int channels, const byte *data, float volume);
	void (*VoipSamples)(int entityNum, const vec3_t origin, int samples, int rate, int width, int channels, const byte *data, float volume);
	void (*StopAllSounds)( void );
	void (*ClearLoopingSounds)( qboolean killall );
	void (*AddLoopingSound)( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
	void (*AddRealLoopingSound)( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx );
	void (*StopLoopingSound)(int entityNum );
	void (*Respatialize)( int entityNum, const vec3_t origin, vec3_t axis[3], int inwater );
	void (*UpdateEntityPosition)( int entityNum, const vec3_t origin );
	void (*Update)( int msec );
	void (*DisableSounds)( void );
	void (*BeginRegistration)( void );
	sfxHandle_t (*RegisterSound)( const char *sample, qboolean compressed );
	void (*ClearSoundBuffer)( void );
	void (*SoundInfo)( void );
	void (*SoundList)( void );
} soundInterface_t;


/*
====================================================================

  SYSTEM SPECIFIC FUNCTIONS

====================================================================
*/

// initializes cycling through a DMA buffer and returns information on it
qboolean SNDDMA_Init(void);

// gets the current DMA position
int		SNDDMA_GetDMAPos(void);

// shutdown the DMA xfer.
void	SNDDMA_Shutdown(void);

void	SNDDMA_BeginPainting (void);

void	SNDDMA_Submit(void);

//====================================================================

#define	MAX_CHANNELS			96

extern	channel_t   s_channels[MAX_CHANNELS];
extern	channel_t   loop_channels[MAX_CHANNELS];
extern	int		numLoopChannels;

extern	int		s_soundtime;
extern	int		s_paintedtime;
extern	int		s_rawend;
extern	vec3_t	listener_forward;
extern	vec3_t	listener_right;
extern	vec3_t	listener_up;
extern	dma_t	dma;

#define	MAX_RAW_SAMPLES	16384
extern	portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];

extern cvar_t *s_volume;
extern cvar_t *s_musicVolume;
extern cvar_t *s_musicLayer;
extern cvar_t *s_musicLayerEnabled;
extern cvar_t *s_musicLayerVolume;
extern cvar_t *s_musicIntensity;
extern cvar_t *s_doppler;
extern cvar_t *s_muteWhenUnfocused;
extern cvar_t *s_muteWhenMinimized;

#ifdef USE_OPENAL
extern cvar_t *s_openal;
extern cvar_t *s_openalDevice;
extern cvar_t *s_openalHrtf;
extern cvar_t *s_openalEfx;
extern cvar_t *s_openalEfxPreset;
extern cvar_t *s_openalCapture;
extern cvar_t *s_openalCaptureDevice;
extern cvar_t *s_openalDopplerFactor;
extern cvar_t *s_openalDopplerSpeed;
extern cvar_t *s_openalRolloff;
extern cvar_t *s_openalMaxDistance;
extern cvar_t *s_openalLowpass;
extern cvar_t *s_openalLowpassHf;
extern cvar_t *s_openalOcclusion;
extern cvar_t *s_openalOcclusionGain;
extern cvar_t *s_openalOcclusionHf;
extern cvar_t *s_openalVoipSpatial;
extern cvar_t *s_openalVoipGain;
extern cvar_t *s_openalDebug;
extern cvar_t *s_acoustics_enable;
extern cvar_t *s_acoustics_debug;
extern cvar_t *s_acoustics_draw;
extern cvar_t *s_acoustics_print_interval_ms;
extern cvar_t *s_acoustics_hz;
extern cvar_t *s_acoustics_rays;
extern cvar_t *s_acoustics_reflection_effort;
extern cvar_t *s_acoustics_maxdist;
extern cvar_t *s_acoustics_near;
extern cvar_t *s_acoustics_cone_deg;
extern cvar_t *s_acoustics_smooth_ms;
extern cvar_t *s_acoustics_smooth_min_alpha;
extern cvar_t *s_acoustics_smooth_max_alpha;
extern cvar_t *s_acoustics_preset_force;
extern cvar_t *s_acoustics_preset_blend;
extern cvar_t *s_acoustics_outdoor_threshold;
extern cvar_t *s_acoustics_tunnel_threshold;
extern cvar_t *s_acoustics_roomsize_small;
extern cvar_t *s_acoustics_roomsize_medium;
extern cvar_t *s_acoustics_efx_enable;
extern cvar_t *s_acoustics_wet;
extern cvar_t *s_acoustics_dry;
extern cvar_t *s_acoustics_bypass;
extern cvar_t *s_acoustics_air_absorb;
extern cvar_t *s_acoustics_occlusion_enable;
extern cvar_t *s_acoustics_occlusion_strength;
extern cvar_t *s_acoustics_occlusion_hf;
extern cvar_t *s_acoustics_occlusion_trace_sources;
extern cvar_t *s_acoustics_occlusion_max_sources;
extern cvar_t *s_acoustics_occlusion_min_gain;
extern cvar_t *s_acoustics_budget_us;
extern cvar_t *s_acoustics_warn_efx;
extern cvar_t *s_acoustics_decay_scale;
extern cvar_t *r_acoustics_reflectivity;
#endif

extern cvar_t *s_testsound;
extern cvar_t *s_khz;

extern sfx_t s_knownSfx[];
extern int s_numSfx;
extern qboolean s_soundStarted;
extern qboolean s_soundMuted;

qboolean S_LoadSound( sfx_t *sfx );

// Reusable base sound registration helpers
void S_Base_BeginRegistration( void );
sfxHandle_t S_Base_RegisterSound( const char *name, qboolean compressed );
void S_Base_SoundList( void );

#ifdef USE_OPENAL
qboolean S_AL_Init( soundInterface_t *si );
#endif

void		SND_free(sndBuffer *v);
sndBuffer*	SND_malloc( void );
void		SND_setup( void );
void		SND_shutdown( void );

void S_PaintChannels(int endtime);

// spatializes a channel
void S_Spatialize(channel_t *ch);

// adpcm functions
int  S_AdpcmMemoryNeeded( const wavinfo_t *info );
void S_AdpcmEncodeSound( sfx_t *sfx, short *samples );
void S_AdpcmEncode( short indata[], char outdata[], int len, struct adpcm_state *state );
void S_AdpcmDecode( const char indata[], short outdata[], int len, struct adpcm_state *state );
void S_AdpcmGetSamples(sndBuffer *chunk, short *to);

// wavelet function

#define SENTINEL_MULAW_ZERO_RUN 127
#define SENTINEL_MULAW_FOUR_BIT_RUN 126

void S_FreeOldestSound( void );

#define	NXStream byte

void encodeWavelet(sfx_t *sfx, short *packets);
void decodeWavelet( sndBuffer *stream, short *packets);

void encodeMuLaw( sfx_t *sfx, short *packets);
extern short mulawToShort[256];

extern short *sfxScratchBuffer;
extern sfx_t *sfxScratchPointer;
extern int	   sfxScratchIndex;

qboolean S_Base_Init( soundInterface_t *si );
void S_Base_StopLoopingSound( int entityNum );
void S_Base_ClearLoopingSounds( qboolean killall );
void S_Base_AddLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle );
void S_Base_AddRealLoopingSound( int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfxHandle );
void S_Base_UpdateEntityPosition( int entityNum, const vec3_t origin );
void S_Base_Respatialize( int entityNum, const vec3_t head, vec3_t axis[3], int inwater );
void S_AddLoopSounds( void );
portable_samplepair_t *S_GetRawSamplePointer( void );

#endif // AUDIO_SND_LOCAL_H
