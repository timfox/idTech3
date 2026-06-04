/*
===========================================================================
Wwise-inspired runtime mixer (OpenAL path)
===========================================================================
Inspired by Audiokinetic Wwise 2026.1 Sound Engine concepts:
  - Output buses with per-bus gain
  - RTPC-driven mix (game intensity, combat, etc.)
  - State groups (gameplay / location switches)
  - Auto-ducking when voice is active
  - Named sound events (data-driven + console)
  - Replay ring buffer for mix debugging
  - Max propagation distance for spatial acoustics

Console:
  snd_playevent <name> [entnum]
  snd_setstate <group> <state>
  snd_setrtpc <name> <value>
  snd_mixer_info
  snd_replay_dump [path]
  snd_replay_clear
  snd_mixer_reload
===========================================================================
*/

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "../snd_local.h"
#include "../snd_public.h"
#include "snd_mixer.h"

#include <math.h>
#include <stdio.h>

#define SND_MIXER_MAX_EVENTS		64
#define SND_MIXER_MAX_RTPC		16
#define SND_MIXER_MAX_STATE_GROUPS	8
#define SND_MIXER_MAX_STATES_PER_GROUP	8
#define SND_MIXER_REPLAY_DEFAULT	256
#define SND_MIXER_EVENT_PATH_LEN	128

typedef struct {
	char		name[32];
	float		value;
} snd_rtpc_t;

typedef struct {
	char		name[24];
	float		sfxMult;
	float		musicMult;
	float		uiMult;
	float		voiceMult;
	float		ambMult;
} snd_state_t;

typedef struct {
	char		group[24];
	snd_state_t	states[SND_MIXER_MAX_STATES_PER_GROUP];
	int			stateCount;
	int			activeIndex;
} snd_state_group_t;

typedef struct {
	char		name[48];
	char		sample[MAX_QPATH];
	snd_bus_t	bus;
	float		volume;
	sfxHandle_t	sfx;
	qboolean	loaded;
} snd_event_t;

typedef struct {
	int			timeMs;
	char		event[48];
	char		sample[MAX_QPATH];
	int			entchannel;
	float		busGain;
	float		x, y, z;
} snd_replay_entry_t;

static cvar_t *s_mixer_enable;
static cvar_t *s_bus_sfx;
static cvar_t *s_bus_ui;
static cvar_t *s_bus_voice;
static cvar_t *s_bus_music;
static cvar_t *s_bus_amb;
static cvar_t *s_mixer_duck_enable;
static cvar_t *s_mixer_duck_amount;
static cvar_t *s_mixer_duck_attack_ms;
static cvar_t *s_mixer_duck_release_ms;
static cvar_t *s_mixer_propagation_max;
static cvar_t *s_mixer_replay_enable;
static cvar_t *s_mixer_replay_capacity;
static cvar_t *s_rtpc_gameIntensity;
static cvar_t *s_rtpc_combat;
static cvar_t *s_mixer_state_gameplay;
static cvar_t *s_mixer_state_location;

static snd_rtpc_t snd_rtpcs[SND_MIXER_MAX_RTPC];
static int snd_rtpcCount;

static snd_state_group_t snd_stateGroups[SND_MIXER_MAX_STATE_GROUPS];
static int snd_stateGroupCount;

static snd_event_t snd_events[SND_MIXER_MAX_EVENTS];
static int snd_eventCount;
static qboolean snd_eventsLoaded;

static snd_replay_entry_t *snd_replay;
static int snd_replayCap;
static int snd_replayHead;
static int snd_replayCount;

static vec3_t snd_listenerOrigin;
static qboolean snd_listenerValid;

static float snd_duckCurrent;
static int snd_voiceActiveUntilMs;

static float S_Mixer_Clamp01( float v ) {
	if ( v < 0.0f ) {
		return 0.0f;
	}
	if ( v > 1.0f ) {
		return 1.0f;
	}
	return v;
}

static float S_Mixer_BusCvar( snd_bus_t bus ) {
	switch ( bus ) {
	case SND_BUS_SFX:
		return s_bus_sfx ? s_bus_sfx->value : 1.0f;
	case SND_BUS_UI:
		return s_bus_ui ? s_bus_ui->value : 1.0f;
	case SND_BUS_VOICE:
		return s_bus_voice ? s_bus_voice->value : 1.0f;
	case SND_BUS_MUSIC:
		return s_bus_music ? s_bus_music->value : 1.0f;
	case SND_BUS_AMB:
		return s_bus_amb ? s_bus_amb->value : 1.0f;
	default:
		return 1.0f;
	}
}

static const snd_state_t *S_Mixer_ActiveState( const char *groupName ) {
	int g;

	for ( g = 0; g < snd_stateGroupCount; ++g ) {
		if ( !Q_stricmp( snd_stateGroups[g].group, groupName ) ) {
			if ( snd_stateGroups[g].activeIndex >= 0 &&
				snd_stateGroups[g].activeIndex < snd_stateGroups[g].stateCount ) {
				return &snd_stateGroups[g].states[snd_stateGroups[g].activeIndex];
			}
			return NULL;
		}
	}
	return NULL;
}

static float S_Mixer_StateMult( snd_bus_t bus ) {
	const snd_state_t *gp;
	const snd_state_t *loc;
	float m = 1.0f;

	gp = S_Mixer_ActiveState( "gameplay" );
	if ( gp ) {
		switch ( bus ) {
		case SND_BUS_SFX: m *= gp->sfxMult; break;
		case SND_BUS_MUSIC: m *= gp->musicMult; break;
		case SND_BUS_UI: m *= gp->uiMult; break;
		case SND_BUS_VOICE: m *= gp->voiceMult; break;
		case SND_BUS_AMB: m *= gp->ambMult; break;
		default: break;
		}
	}

	loc = S_Mixer_ActiveState( "location" );
	if ( loc ) {
		switch ( bus ) {
		case SND_BUS_SFX: m *= loc->sfxMult; break;
		case SND_BUS_MUSIC: m *= loc->musicMult; break;
		case SND_BUS_UI: m *= loc->uiMult; break;
		case SND_BUS_VOICE: m *= loc->voiceMult; break;
		case SND_BUS_AMB: m *= loc->ambMult; break;
		default: break;
		}
	}

	return m;
}

static float S_Mixer_RtpcMult( snd_bus_t bus ) {
	float intensity;
	float combat;

	intensity = s_rtpc_gameIntensity ? s_rtpc_gameIntensity->value : 0.0f;
	combat = s_rtpc_combat ? s_rtpc_combat->value : 0.0f;
	intensity = S_Mixer_Clamp01( intensity );
	combat = S_Mixer_Clamp01( combat );

	if ( bus == SND_BUS_SFX ) {
		return 1.0f + combat * 0.15f;
	}
	if ( bus == SND_BUS_AMB ) {
		return 1.0f - intensity * 0.1f;
	}
	if ( bus == SND_BUS_MUSIC ) {
		return 1.0f;
	}
	return 1.0f;
}

static snd_state_group_t *S_Mixer_FindOrAddGroup( const char *group ) {
	int i;

	for ( i = 0; i < snd_stateGroupCount; ++i ) {
		if ( !Q_stricmp( snd_stateGroups[i].group, group ) ) {
			return &snd_stateGroups[i];
		}
	}
	if ( snd_stateGroupCount >= SND_MIXER_MAX_STATE_GROUPS ) {
		return NULL;
	}
	Q_strncpyz( snd_stateGroups[snd_stateGroupCount].group, group,
		sizeof( snd_stateGroups[0].group ) );
	snd_stateGroups[snd_stateGroupCount].stateCount = 0;
	snd_stateGroups[snd_stateGroupCount].activeIndex = 0;
	return &snd_stateGroups[snd_stateGroupCount++];
}

static void S_Mixer_RegisterBuiltinStates( void ) {
	snd_state_group_t *gp;
	snd_state_group_t *loc;

	gp = S_Mixer_FindOrAddGroup( "gameplay" );
	if ( gp && gp->stateCount == 0 ) {
		Q_strncpyz( gp->states[0].name, "default", sizeof( gp->states[0].name ) );
		gp->states[0].sfxMult = gp->states[0].musicMult = gp->states[0].uiMult = 1.0f;
		gp->states[0].voiceMult = gp->states[0].ambMult = 1.0f;
		Q_strncpyz( gp->states[1].name, "combat", sizeof( gp->states[1].name ) );
		gp->states[1].sfxMult = 1.1f;
		gp->states[1].musicMult = 0.95f;
		gp->states[1].uiMult = gp->states[1].voiceMult = gp->states[1].ambMult = 1.0f;
		Q_strncpyz( gp->states[2].name, "paused", sizeof( gp->states[2].name ) );
		gp->states[2].sfxMult = gp->states[2].musicMult = gp->states[2].uiMult = 0.0f;
		gp->states[2].voiceMult = gp->states[2].ambMult = 0.0f;
		gp->stateCount = 3;
		gp->activeIndex = 0;
	}

	loc = S_Mixer_FindOrAddGroup( "location" );
	if ( loc && loc->stateCount == 0 ) {
		Q_strncpyz( loc->states[0].name, "default", sizeof( loc->states[0].name ) );
		loc->states[0].sfxMult = loc->states[0].musicMult = loc->states[0].uiMult = 1.0f;
		loc->states[0].voiceMult = loc->states[0].ambMult = 1.0f;
		Q_strncpyz( loc->states[1].name, "underwater", sizeof( loc->states[1].name ) );
		loc->states[1].sfxMult = 0.65f;
		loc->states[1].ambMult = 0.5f;
		loc->states[1].musicMult = 0.7f;
		loc->states[1].uiMult = loc->states[1].voiceMult = 0.9f;
		Q_strncpyz( loc->states[2].name, "interior", sizeof( loc->states[2].name ) );
		loc->states[2].sfxMult = 0.95f;
		loc->states[2].ambMult = 1.05f;
		loc->states[2].musicMult = loc->states[2].uiMult = loc->states[2].voiceMult = 1.0f;
		loc->stateCount = 3;
		loc->activeIndex = 0;
	}
}

static snd_bus_t S_Mixer_ParseBus( const char *token ) {
	if ( !token || !token[0] ) {
		return SND_BUS_SFX;
	}
	if ( !Q_stricmp( token, "ui" ) || !Q_stricmp( token, "menu" ) ) {
		return SND_BUS_UI;
	}
	if ( !Q_stricmp( token, "voice" ) || !Q_stricmp( token, "vo" ) ) {
		return SND_BUS_VOICE;
	}
	if ( !Q_stricmp( token, "music" ) ) {
		return SND_BUS_MUSIC;
	}
	if ( !Q_stricmp( token, "amb" ) || !Q_stricmp( token, "ambient" ) ) {
		return SND_BUS_AMB;
	}
	return SND_BUS_SFX;
}

static void S_Mixer_RegisterBuiltinEvents( void ) {
	if ( snd_eventCount > 0 ) {
		return;
	}
	/* Placeholders: mod soundevents.txt overrides with real paths */
	Q_strncpyz( snd_events[0].name, "ui_click", sizeof( snd_events[0].name ) );
	Q_strncpyz( snd_events[0].sample, "sound/misc/menu1.wav", sizeof( snd_events[0].sample ) );
	snd_events[0].bus = SND_BUS_UI;
	snd_events[0].volume = 1.0f;
	snd_eventCount = 1;
}

static void S_Mixer_ParseEventLine( char *lineBuf ) {
	const char *cursor;
	const char *name;
	const char *busTok;
	const char *volTok;
	const char *path;
	snd_event_t *ev;
	float vol;
	char *line = lineBuf;

	while ( *line == ' ' || *line == '\t' ) {
		line++;
	}
	if ( !line[0] || line[0] == '#' || line[0] == ';' ) {
		return;
	}

	cursor = line;
	name = COM_Parse( &cursor );
	if ( !name[0] ) {
		return;
	}
	busTok = COM_Parse( &cursor );
	volTok = COM_Parse( &cursor );
	path = COM_Parse( &cursor );
	if ( !path[0] ) {
		return;
	}
	if ( snd_eventCount >= SND_MIXER_MAX_EVENTS ) {
		return;
	}

	vol = volTok[0] ? (float)atof( volTok ) : 1.0f;
	ev = &snd_events[snd_eventCount++];
	Q_strncpyz( ev->name, name, sizeof( ev->name ) );
	Q_strncpyz( ev->sample, path, sizeof( ev->sample ) );
	ev->bus = S_Mixer_ParseBus( busTok );
	ev->volume = S_Mixer_Clamp01( vol );
	ev->sfx = 0;
	ev->loaded = qfalse;
}

void S_Mixer_ReloadEvents( void ) {
	byte *buf;
	int len;
	char *text;
	char *line;
	char path[SND_MIXER_EVENT_PATH_LEN];
	int f;

	snd_eventCount = 0;
	snd_eventsLoaded = qfalse;
	S_Mixer_RegisterBuiltinEvents();

	for ( f = 0; f < 2; ++f ) {
		if ( f == 0 ) {
			Q_strncpyz( path, "sound/soundevents.txt", sizeof( path ) );
		} else {
			Q_strncpyz( path, "scripts/soundevents.txt", sizeof( path ) );
		}
		len = FS_ReadFile( path, (void **)&buf );
		if ( len <= 0 || !buf ) {
			continue;
		}
		text = (char *)Z_Malloc( len + 1 );
		Com_Memcpy( text, buf, len );
		text[len] = '\0';
		FS_FreeFile( buf );

		line = text;
		while ( line && line[0] ) {
			char *next = strchr( line, '\n' );
			if ( next ) {
				*next = '\0';
				next++;
			}
			S_Mixer_ParseEventLine( line );
			line = next;
		}
		Z_Free( text );
		Com_Printf( "Mixer: loaded sound events from %s (%d total)\n", path, snd_eventCount );
	}

	snd_eventsLoaded = qtrue;
}

static void S_Mixer_EnsureEvents( void ) {
	if ( !snd_eventsLoaded ) {
		S_Mixer_ReloadEvents();
	}
}

static void S_Mixer_ReplayPush( const char *eventName, const char *sample, int entchannel,
	float busGain, const vec3_t origin ) {
	snd_replay_entry_t *e;
	int idx;

	if ( !s_mixer_replay_enable || !s_mixer_replay_enable->integer ) {
		return;
	}
	if ( !snd_replay || snd_replayCap <= 0 ) {
		return;
	}

	idx = snd_replayHead;
	e = &snd_replay[idx];
	e->timeMs = Sys_Milliseconds();
	Q_strncpyz( e->event, eventName ? eventName : "", sizeof( e->event ) );
	Q_strncpyz( e->sample, sample ? sample : "", sizeof( e->sample ) );
	e->entchannel = entchannel;
	e->busGain = busGain;
	if ( origin ) {
		e->x = origin[0];
		e->y = origin[1];
		e->z = origin[2];
	} else {
		e->x = e->y = e->z = 0.0f;
	}

	snd_replayHead = ( snd_replayHead + 1 ) % snd_replayCap;
	if ( snd_replayCount < snd_replayCap ) {
		snd_replayCount++;
	}
}

snd_bus_t S_Mixer_ChannelToBus( int entchannel ) {
	switch ( entchannel ) {
	case CHAN_LOCAL:
	case CHAN_LOCAL_SOUND:
		return SND_BUS_UI;
	case CHAN_VOICE:
	case CHAN_ANNOUNCER:
		return SND_BUS_VOICE;
	case CHAN_WEAPON:
	case CHAN_ITEM:
	case CHAN_BODY:
		return SND_BUS_SFX;
	case CHAN_AUTO:
	default:
		return SND_BUS_SFX;
	}
}

float S_Mixer_SourceGain( snd_bus_t bus, int entchannel ) {
	float g;
	(void)entchannel;

	if ( !s_mixer_enable || !s_mixer_enable->integer ) {
		return 1.0f;
	}

	g = S_Mixer_BusCvar( bus );
	g *= S_Mixer_StateMult( bus );
	g *= S_Mixer_RtpcMult( bus );

	if ( bus == SND_BUS_MUSIC || bus == SND_BUS_VOICE ) {
		/* duck applies via S_Mixer_MusicGain for music; voice stays clear */
	} else if ( s_mixer_duck_enable && s_mixer_duck_enable->integer && snd_duckCurrent > 0.0f ) {
		g *= ( 1.0f - snd_duckCurrent );
	}

	return S_Mixer_Clamp01( g );
}

float S_Mixer_MusicGain( void ) {
	float g;

	if ( !s_mixer_enable || !s_mixer_enable->integer ) {
		return 1.0f;
	}

	g = S_Mixer_BusCvar( SND_BUS_MUSIC );
	g *= S_Mixer_StateMult( SND_BUS_MUSIC );
	if ( s_mixer_duck_enable && s_mixer_duck_enable->integer ) {
		g *= ( 1.0f - snd_duckCurrent );
	}
	return S_Mixer_Clamp01( g );
}

void S_Mixer_SetListenerOrigin( const vec3_t origin ) {
	if ( origin ) {
		VectorCopy( origin, snd_listenerOrigin );
		snd_listenerValid = qtrue;
	} else {
		snd_listenerValid = qfalse;
	}
}

qboolean S_Mixer_PropagationAllowed( const vec3_t srcPos ) {
	float maxDist;
	float distSq;

	if ( !s_mixer_enable || !s_mixer_enable->integer ) {
		return qtrue;
	}
	if ( !srcPos || !snd_listenerValid ) {
		return qtrue;
	}

	maxDist = s_mixer_propagation_max ? s_mixer_propagation_max->value : 1536.0f;
	if ( maxDist <= 0.0f ) {
		return qtrue;
	}

	distSq = DistanceSquared( srcPos, snd_listenerOrigin );
	return distSq <= ( maxDist * maxDist );
}

void S_Mixer_OnPlaySound( const char *sampleName, int entchannel, const vec3_t origin ) {
	snd_bus_t bus;
	float gain;

	if ( !s_mixer_enable || !s_mixer_enable->integer ) {
		return;
	}

	bus = S_Mixer_ChannelToBus( entchannel );
	gain = S_Mixer_SourceGain( bus, entchannel );

	if ( bus == SND_BUS_VOICE ) {
		S_Mixer_NotifyVoiceActive();
	}

	S_Mixer_ReplayPush( "", sampleName, entchannel, gain, origin );
}

void S_Mixer_NotifyVoiceActive( void ) {
	snd_voiceActiveUntilMs = Sys_Milliseconds() + 400;
}

void S_Mixer_Frame( int msec ) {
	float target;
	float attack;
	float release;
	float alpha;
	int now;

	if ( !s_mixer_enable || !s_mixer_enable->integer ) {
		snd_duckCurrent = 0.0f;
		return;
	}

	now = Sys_Milliseconds();
	target = 0.0f;
	if ( s_mixer_duck_enable && s_mixer_duck_enable->integer && now < snd_voiceActiveUntilMs ) {
		target = s_mixer_duck_amount ? s_mixer_duck_amount->value : 0.35f;
	}
	target = S_Mixer_Clamp01( target );

	attack = s_mixer_duck_attack_ms ? (float)s_mixer_duck_attack_ms->integer : 80.0f;
	release = s_mixer_duck_release_ms ? (float)s_mixer_duck_release_ms->integer : 350.0f;
	if ( attack < 1.0f ) {
		attack = 1.0f;
	}
	if ( release < 1.0f ) {
		release = 1.0f;
	}

	if ( target > snd_duckCurrent ) {
		alpha = (float)msec / attack;
	} else {
		alpha = (float)msec / release;
	}
	if ( alpha > 1.0f ) {
		alpha = 1.0f;
	}

	snd_duckCurrent += ( target - snd_duckCurrent ) * alpha;

	if ( s_rtpc_gameIntensity && s_musicIntensity ) {
		s_rtpc_gameIntensity->value = s_musicIntensity->value;
		s_rtpc_gameIntensity->modified = qtrue;
	}

	if ( s_mixer_state_gameplay && s_mixer_state_gameplay->string[0] ) {
		S_Mixer_SetState( "gameplay", s_mixer_state_gameplay->string );
	}
	if ( s_mixer_state_location && s_mixer_state_location->string[0] ) {
		S_Mixer_SetState( "location", s_mixer_state_location->string );
	}
}

void S_Mixer_SetState( const char *group, const char *state ) {
	snd_state_group_t *g;
	int i;

	if ( !group || !group[0] || !state || !state[0] ) {
		return;
	}

	for ( i = 0; i < snd_stateGroupCount; ++i ) {
		int s;

		if ( Q_stricmp( snd_stateGroups[i].group, group ) ) {
			continue;
		}
		g = &snd_stateGroups[i];
		for ( s = 0; s < g->stateCount; ++s ) {
			if ( !Q_stricmp( g->states[s].name, state ) ) {
				if ( g->activeIndex != s ) {
					g->activeIndex = s;
					Com_Printf( "Mixer: state %s -> %s\n", group, state );
				}
				return;
			}
		}
		return;
	}
}

void S_Mixer_SetRTPC( const char *name, float value ) {
	int i;

	if ( !name || !name[0] ) {
		return;
	}

	if ( !Q_stricmp( name, "gameIntensity" ) && s_rtpc_gameIntensity ) {
		Cvar_SetValue( s_rtpc_gameIntensity->name, value );
		return;
	}
	if ( !Q_stricmp( name, "combat" ) && s_rtpc_combat ) {
		Cvar_SetValue( s_rtpc_combat->name, value );
		return;
	}

	for ( i = 0; i < snd_rtpcCount; ++i ) {
		if ( !Q_stricmp( snd_rtpcs[i].name, name ) ) {
			snd_rtpcs[i].value = value;
			return;
		}
	}
	if ( snd_rtpcCount < SND_MIXER_MAX_RTPC ) {
		Q_strncpyz( snd_rtpcs[snd_rtpcCount].name, name, sizeof( snd_rtpcs[0].name ) );
		snd_rtpcs[snd_rtpcCount].value = value;
		snd_rtpcCount++;
	}
}

float S_Mixer_GetRTPC( const char *name ) {
	int i;

	if ( !name ) {
		return 0.0f;
	}
	if ( !Q_stricmp( name, "gameIntensity" ) && s_rtpc_gameIntensity ) {
		return s_rtpc_gameIntensity->value;
	}
	if ( !Q_stricmp( name, "combat" ) && s_rtpc_combat ) {
		return s_rtpc_combat->value;
	}
	for ( i = 0; i < snd_rtpcCount; ++i ) {
		if ( !Q_stricmp( snd_rtpcs[i].name, name ) ) {
			return snd_rtpcs[i].value;
		}
	}
	return 0.0f;
}

qboolean S_Mixer_PlayEvent( const char *eventName, const vec3_t origin, int entnum ) {
	int i;
	snd_event_t *ev;
	sfxHandle_t sfx;
	int channel;

	if ( !eventName || !eventName[0] ) {
		return qfalse;
	}

	S_Mixer_EnsureEvents();

	for ( i = 0; i < snd_eventCount; ++i ) {
		if ( Q_stricmp( snd_events[i].name, eventName ) ) {
			continue;
		}
		ev = &snd_events[i];
		if ( !ev->loaded ) {
			sfx = S_RegisterSound( ev->sample, qfalse );
			ev->sfx = sfx;
			ev->loaded = qtrue;
			if ( sfx >= 0 && sfx < s_numSfx && s_knownSfx[sfx].defaultSound ) {
				Com_Printf( S_COLOR_YELLOW "Mixer: event '%s' sample missing: %s\n", eventName, ev->sample );
				return qfalse;
			}
		}

		channel = CHAN_AUTO;
		switch ( ev->bus ) {
		case SND_BUS_UI: channel = CHAN_LOCAL_SOUND; break;
		case SND_BUS_VOICE: channel = CHAN_VOICE; break;
		default: channel = CHAN_AUTO; break;
		}

		S_Mixer_ReplayPush( eventName, ev->sample, channel,
			S_Mixer_SourceGain( ev->bus, channel ) * ev->volume, origin );

		if ( origin ) {
			vec3_t org;

			VectorCopy( origin, org );
			S_StartSound( org, entnum, channel, ev->sfx );
		} else {
			S_StartLocalSound( ev->sfx, channel );
		}
		return qtrue;
	}

	Com_Printf( S_COLOR_YELLOW "Mixer: unknown event '%s'\n", eventName );
	return qfalse;
}

static void S_Mixer_Info_f( void ) {
	int i;
	const snd_state_t *st;

	Com_Printf( "----- Audio Mixer (Wwise-style) -----\n" );
	Com_Printf( "enabled: %s\n", ( s_mixer_enable && s_mixer_enable->integer ) ? "yes" : "no" );
	Com_Printf( "buses: sfx=%.2f ui=%.2f voice=%.2f music=%.2f amb=%.2f\n",
		s_bus_sfx ? s_bus_sfx->value : 1.0f,
		s_bus_ui ? s_bus_ui->value : 1.0f,
		s_bus_voice ? s_bus_voice->value : 1.0f,
		s_bus_music ? s_bus_music->value : 1.0f,
		s_bus_amb ? s_bus_amb->value : 1.0f );
	Com_Printf( "duck: %.2f (voice until %d)\n", snd_duckCurrent, snd_voiceActiveUntilMs );
	Com_Printf( "propagation max: %.0f\n", s_mixer_propagation_max ? s_mixer_propagation_max->value : 0.0f );
	Com_Printf( "events: %d  replay: %d/%d\n", snd_eventCount, snd_replayCount, snd_replayCap );

	for ( i = 0; i < snd_stateGroupCount; ++i ) {
		st = NULL;
		if ( snd_stateGroups[i].activeIndex >= 0 &&
			snd_stateGroups[i].activeIndex < snd_stateGroups[i].stateCount ) {
			st = &snd_stateGroups[i].states[snd_stateGroups[i].activeIndex];
		}
		Com_Printf( "state %s: %s\n", snd_stateGroups[i].group,
			st ? st->name : "(none)" );
	}
	Com_Printf( "-----------------------------------\n" );
}

static void S_Mixer_PlayEvent_f( void ) {
	vec3_t origin;
	int entnum;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: snd_playevent <name> [entnum]\n" );
		return;
	}
	entnum = ( Cmd_Argc() >= 3 ) ? atoi( Cmd_Argv( 2 ) ) : 0;
	VectorClear( origin );
	S_Mixer_PlayEvent( Cmd_Argv( 1 ), origin, entnum );
}

static void S_Mixer_SetState_f( void ) {
	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: snd_setstate <group> <state>\n" );
		return;
	}
	S_Mixer_SetState( Cmd_Argv( 1 ), Cmd_Argv( 2 ) );
}

static void S_Mixer_SetRTPC_f( void ) {
	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: snd_setrtpc <name> <value>\n" );
		return;
	}
	S_Mixer_SetRTPC( Cmd_Argv( 1 ), (float)atof( Cmd_Argv( 2 ) ) );
}

static void S_Mixer_ReplayDump_f( void ) {
	fileHandle_t f;
	const char *path;
	int i;
	int idx;
	snd_replay_entry_t *e;
	char line[512];

	if ( !snd_replay || snd_replayCount <= 0 ) {
		Com_Printf( "Mixer replay: empty\n" );
		return;
	}

	path = ( Cmd_Argc() >= 2 ) ? Cmd_Argv( 1 ) : "mixer_replay.csv";
	f = FS_FOpenFileWrite( path );
	if ( !f ) {
		Com_Printf( S_COLOR_YELLOW "Mixer: could not write %s\n", path );
		return;
	}

	FS_Printf( f, "time_ms,event,sample,channel,bus_gain,x,y,z\n" );
	for ( i = 0; i < snd_replayCount; ++i ) {
		idx = ( snd_replayHead - snd_replayCount + i + snd_replayCap ) % snd_replayCap;
		e = &snd_replay[idx];
		Com_sprintf( line, sizeof( line ), "%d,%s,%s,%d,%.4f,%.2f,%.2f,%.2f\n",
			e->timeMs, e->event, e->sample, e->entchannel, e->busGain, e->x, e->y, e->z );
		FS_Write( line, (int)strlen( line ), f );
	}
	FS_FCloseFile( f );
	Com_Printf( "Mixer: dumped %d replay entries to %s\n", snd_replayCount, path );
}

static void S_Mixer_ReplayClear_f( void ) {
	snd_replayHead = 0;
	snd_replayCount = 0;
	Com_Printf( "Mixer replay: cleared\n" );
}

void S_Mixer_Init( void ) {
	int cap;

	s_mixer_enable = Cvar_Get( "s_mixer_enable", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_mixer_enable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_mixer_enable, "Enable Wwise-style runtime bus mixer (OpenAL)." );

	s_bus_sfx = Cvar_Get( "s_bus_sfx", "1.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_bus_sfx, "0", "2", CV_FLOAT );
	Cvar_SetDescription( s_bus_sfx, "SFX bus gain." );

	s_bus_ui = Cvar_Get( "s_bus_ui", "1.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_bus_ui, "0", "2", CV_FLOAT );
	Cvar_SetDescription( s_bus_ui, "UI/menu bus gain." );

	s_bus_voice = Cvar_Get( "s_bus_voice", "1.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_bus_voice, "0", "2", CV_FLOAT );
	Cvar_SetDescription( s_bus_voice, "Voice/announcer bus gain." );

	s_bus_music = Cvar_Get( "s_bus_music", "1.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_bus_music, "0", "2", CV_FLOAT );
	Cvar_SetDescription( s_bus_music, "Music bus gain (before s_musicVolume)." );

	s_bus_amb = Cvar_Get( "s_bus_amb", "1.0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_bus_amb, "0", "2", CV_FLOAT );
	Cvar_SetDescription( s_bus_amb, "Ambient/loop bus gain." );

	s_mixer_duck_enable = Cvar_Get( "s_mixer_duck_enable", "1", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_mixer_duck_enable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_mixer_duck_enable, "Auto-duck SFX/amb/music when voice plays." );

	s_mixer_duck_amount = Cvar_Get( "s_mixer_duck_amount", "0.35", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_mixer_duck_amount, "0", "1", CV_FLOAT );
	Cvar_SetDescription( s_mixer_duck_amount, "Duck depth when voice is active." );

	s_mixer_duck_attack_ms = Cvar_Get( "s_mixer_duck_attack_ms", "80", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_mixer_duck_attack_ms, "1", "2000", CV_INTEGER );
	Cvar_SetDescription( s_mixer_duck_attack_ms, "Duck attack time in ms." );

	s_mixer_duck_release_ms = Cvar_Get( "s_mixer_duck_release_ms", "350", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_mixer_duck_release_ms, "1", "5000", CV_INTEGER );
	Cvar_SetDescription( s_mixer_duck_release_ms, "Duck release time in ms." );

	s_mixer_propagation_max = Cvar_Get( "s_mixer_propagation_max", "1536", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_mixer_propagation_max, "0", "8192", CV_FLOAT );
	Cvar_SetDescription( s_mixer_propagation_max,
		"Max distance for spatial acoustics propagation (0=unlimited, Wwise-style room limit)." );

	s_mixer_replay_enable = Cvar_Get( "s_mixer_replay_enable", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_mixer_replay_enable, "0", "1", CV_INTEGER );
	Cvar_SetDescription( s_mixer_replay_enable, "Record play events for snd_replay_dump." );

	s_mixer_replay_capacity = Cvar_Get( "s_mixer_replay_capacity", "256", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_mixer_replay_capacity, "32", "4096", CV_INTEGER );
	Cvar_SetDescription( s_mixer_replay_capacity, "Replay ring buffer capacity." );

	s_rtpc_gameIntensity = Cvar_Get( "s_rtpc_gameIntensity", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_rtpc_gameIntensity, "0", "1", CV_FLOAT );
	Cvar_SetDescription( s_rtpc_gameIntensity, "RTPC: game intensity (synced from s_musicIntensity)." );

	s_rtpc_combat = Cvar_Get( "s_rtpc_combat", "0", CVAR_ARCHIVE_ND );
	Cvar_CheckRange( s_rtpc_combat, "0", "1", CV_FLOAT );
	Cvar_SetDescription( s_rtpc_combat, "RTPC: combat mix boost for SFX bus." );

	s_mixer_state_gameplay = Cvar_Get( "s_mixer_state_gameplay", "default", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( s_mixer_state_gameplay, "Gameplay state group (default, combat, paused)." );

	s_mixer_state_location = Cvar_Get( "s_mixer_state_location", "default", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( s_mixer_state_location, "Location state group (default, underwater, interior)." );

	snd_stateGroupCount = 0;
	snd_rtpcCount = 0;
	snd_duckCurrent = 0.0f;
	snd_voiceActiveUntilMs = 0;
	snd_listenerValid = qfalse;
	VectorClear( snd_listenerOrigin );

	S_Mixer_RegisterBuiltinStates();

	cap = s_mixer_replay_capacity ? s_mixer_replay_capacity->integer : SND_MIXER_REPLAY_DEFAULT;
	cap = ( cap < 32 ) ? 32 : ( cap > 4096 ? 4096 : cap );
	snd_replayCap = cap;
	snd_replay = (snd_replay_entry_t *)Z_Malloc( (size_t)cap * sizeof( *snd_replay ) );
	snd_replayHead = 0;
	snd_replayCount = 0;

	Cmd_AddCommand( "snd_playevent", S_Mixer_PlayEvent_f );
	Cmd_AddCommand( "snd_setstate", S_Mixer_SetState_f );
	Cmd_AddCommand( "snd_setrtpc", S_Mixer_SetRTPC_f );
	Cmd_AddCommand( "snd_mixer_info", S_Mixer_Info_f );
	Cmd_AddCommand( "snd_replay_dump", S_Mixer_ReplayDump_f );
	Cmd_AddCommand( "snd_replay_clear", S_Mixer_ReplayClear_f );
	Cmd_AddCommand( "snd_mixer_reload", S_Mixer_ReloadEvents );

	Com_Printf( "Audio mixer: Wwise-style buses/RTPC/states enabled (s_mixer_enable=%d)\n",
		s_mixer_enable->integer );
}

void S_Mixer_Shutdown( void ) {
	Cmd_RemoveCommand( "snd_playevent" );
	Cmd_RemoveCommand( "snd_setstate" );
	Cmd_RemoveCommand( "snd_setrtpc" );
	Cmd_RemoveCommand( "snd_mixer_info" );
	Cmd_RemoveCommand( "snd_replay_dump" );
	Cmd_RemoveCommand( "snd_replay_clear" );
	Cmd_RemoveCommand( "snd_mixer_reload" );

	if ( snd_replay ) {
		Z_Free( snd_replay );
		snd_replay = NULL;
	}
	snd_replayCap = 0;
	snd_replayCount = 0;
}
