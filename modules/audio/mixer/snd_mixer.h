/*
===========================================================================
Wwise-inspired runtime mixer (buses, RTPCs, states, events, replay)
===========================================================================
*/

#ifndef SND_MIXER_H
#define SND_MIXER_H

#include "q_shared.h"

typedef enum {
	SND_BUS_MASTER = 0,
	SND_BUS_SFX,
	SND_BUS_UI,
	SND_BUS_VOICE,
	SND_BUS_MUSIC,
	SND_BUS_AMB,
	SND_BUS_COUNT
} snd_bus_t;

void S_Mixer_Init( void );
void S_Mixer_Shutdown( void );
void S_Mixer_Frame( int msec );

snd_bus_t S_Mixer_ChannelToBus( int entchannel );
float S_Mixer_SourceGain( snd_bus_t bus, int entchannel );
float S_Mixer_MusicGain( void );

void S_Mixer_SetListenerOrigin( const vec3_t origin );
qboolean S_Mixer_PropagationAllowed( const vec3_t srcPos );

void S_Mixer_OnPlaySound( const char *sampleName, int entchannel, const vec3_t origin );
void S_Mixer_NotifyVoiceActive( void );

qboolean S_Mixer_PlayEvent( const char *eventName, const vec3_t origin, int entnum );
void S_Mixer_SetState( const char *group, const char *state );
void S_Mixer_SetRTPC( const char *name, float value );
float S_Mixer_GetRTPC( const char *name );

void S_Mixer_ReloadEvents( void );

#endif
