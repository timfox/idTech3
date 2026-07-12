/*
===========================================================================
Heuristic Acoustics (OpenAL)
===========================================================================
How to test (console):
  set s_acoustics_enable 1
  set s_acoustics_debug 2
  set s_acoustics_draw 1
  set s_acoustics_hz 20
  set s_acoustics_rays 12
  set s_acoustics_preset_force 3   // force preset id
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "cm_public.h"
#include "../../game/bg_public.h"
#include "../snd_local.h"

#include "snd_efx.h"
#include "../mixer/snd_mixer.h"

#ifdef USE_OPENAL

#include <math.h>
#include <AL/alc.h>

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
#ifndef AL_DIRECT_FILTER
#define AL_DIRECT_FILTER 0x20005
#endif
#ifndef AL_FILTER_TYPE
#define AL_FILTER_TYPE 0x8001
#endif
#ifndef AL_FILTER_LOWPASS
#define AL_FILTER_LOWPASS 0x0001
#endif
#ifndef AL_LOWPASS_GAIN
#define AL_LOWPASS_GAIN 0x0001
#endif
#ifndef AL_LOWPASS_GAINHF
#define AL_LOWPASS_GAINHF 0x0002
#endif

#ifndef AL_REVERB_DENSITY
#define AL_REVERB_DENSITY 0x0001
#endif
#ifndef AL_REVERB_DIFFUSION
#define AL_REVERB_DIFFUSION 0x0002
#endif
#ifndef AL_REVERB_GAIN
#define AL_REVERB_GAIN 0x0003
#endif
#ifndef AL_REVERB_GAINHF
#define AL_REVERB_GAINHF 0x0004
#endif
#ifndef AL_REVERB_DECAY_TIME
#define AL_REVERB_DECAY_TIME 0x0005
#endif
#ifndef AL_REVERB_DECAY_HFRATIO
#define AL_REVERB_DECAY_HFRATIO 0x0006
#endif
#ifndef AL_REVERB_REFLECTIONS_GAIN
#define AL_REVERB_REFLECTIONS_GAIN 0x0007
#endif
#ifndef AL_REVERB_REFLECTIONS_DELAY
#define AL_REVERB_REFLECTIONS_DELAY 0x0008
#endif
#ifndef AL_REVERB_LATE_REVERB_GAIN
#define AL_REVERB_LATE_REVERB_GAIN 0x0009
#endif
#ifndef AL_REVERB_LATE_REVERB_DELAY
#define AL_REVERB_LATE_REVERB_DELAY 0x000A
#endif
#ifndef AL_REVERB_AIR_ABSORPTION_GAINHF
#define AL_REVERB_AIR_ABSORPTION_GAINHF 0x000B
#endif
#ifndef AL_REVERB_ROOM_ROLLOFF_FACTOR
#define AL_REVERB_ROOM_ROLLOFF_FACTOR 0x000C
#endif

#ifndef AL_EAXREVERB_DENSITY
#define AL_EAXREVERB_DENSITY 0x0001
#endif
#ifndef AL_EAXREVERB_DIFFUSION
#define AL_EAXREVERB_DIFFUSION 0x0002
#endif
#ifndef AL_EAXREVERB_GAIN
#define AL_EAXREVERB_GAIN 0x0003
#endif
#ifndef AL_EAXREVERB_GAINHF
#define AL_EAXREVERB_GAINHF 0x0004
#endif
#ifndef AL_EAXREVERB_DECAY_TIME
#define AL_EAXREVERB_DECAY_TIME 0x0005
#endif
#ifndef AL_EAXREVERB_DECAY_HFRATIO
#define AL_EAXREVERB_DECAY_HFRATIO 0x0006
#endif
#ifndef AL_EAXREVERB_REFLECTIONS_GAIN
#define AL_EAXREVERB_REFLECTIONS_GAIN 0x0007
#endif
#ifndef AL_EAXREVERB_REFLECTIONS_DELAY
#define AL_EAXREVERB_REFLECTIONS_DELAY 0x0008
#endif
#ifndef AL_EAXREVERB_LATE_REVERB_GAIN
#define AL_EAXREVERB_LATE_REVERB_GAIN 0x0009
#endif
#ifndef AL_EAXREVERB_LATE_REVERB_DELAY
#define AL_EAXREVERB_LATE_REVERB_DELAY 0x000A
#endif
#ifndef AL_EAXREVERB_AIR_ABSORPTION_GAINHF
#define AL_EAXREVERB_AIR_ABSORPTION_GAINHF 0x000B
#endif
#ifndef AL_EAXREVERB_ROOM_ROLLOFF_FACTOR
#define AL_EAXREVERB_ROOM_ROLLOFF_FACTOR 0x000C
#endif

typedef void (AL_APIENTRY *LPALGENEFFECTS)(ALsizei, ALuint *);
typedef void (AL_APIENTRY *LPALDELETEEFFECTS)(ALsizei, const ALuint *);
typedef void (AL_APIENTRY *LPALEFFECTI)(ALuint, ALenum, ALint);
typedef void (AL_APIENTRY *LPALEFFECTF)(ALuint, ALenum, ALfloat);
typedef void (AL_APIENTRY *LPALGENAUXILIARYEFFECTSLOTS)(ALsizei, ALuint *);
typedef void (AL_APIENTRY *LPALDELETEAUXILIARYEFFECTSLOTS)(ALsizei, const ALuint *);
typedef void (AL_APIENTRY *LPALAUXILIARYEFFECTSLOTI)(ALuint, ALenum, ALint);
typedef void (AL_APIENTRY *LPALAUXILIARYEFFECTSLOTF)(ALuint, ALenum, ALfloat);
typedef void (AL_APIENTRY *LPALGENFILTERS)(ALsizei, ALuint *);
typedef void (AL_APIENTRY *LPALDELETEFILTERS)(ALsizei, const ALuint *);
typedef void (AL_APIENTRY *LPALFILTERI)(ALuint, ALenum, ALint);
typedef void (AL_APIENTRY *LPALFILTERF)(ALuint, ALenum, ALfloat);

#define S_ACOUSTICS_MAX_RAYS 32
#define S_ACOUSTICS_MAX_OCCLUSION 32

typedef enum {
	S_ACOUSTICS_REVERB_NONE = 0,
	S_ACOUSTICS_REVERB_STANDARD,
	S_ACOUSTICS_REVERB_EAX
} s_acoustics_reverb_type_t;

typedef struct {
	const char *name;
	float density;
	float diffusion;
	float gain;
	float gainHF;
	float decayTime;
	float decayHFRatio;
	float reflectionsGain;
	float reflectionsDelay;
	float lateReverbGain;
	float lateReverbDelay;
	float airAbsorptionGainHF;
	float roomRolloffFactor;
} s_acoustics_preset_t;

typedef struct {
	ALuint source;
	ALuint filter;
	int lastUseMs;
} s_acoustics_filter_slot_t;

typedef struct {
	qboolean inited;
	qboolean efxAvailable;
	qboolean warnedEfx;
	qboolean warnedDraw;
	s_acoustics_reverb_type_t reverbType;
	int64_t lastUpdateUs;
	int lastPrintMs;
	int lastFrameMs;
	int frameId;
	int occlusionTraces;
	int rayCountAdaptive;
	int raysUsed;
	float roomSize;
	float openness;
	float reflectivity;
	float occlusion;
	float forwardHitFrac;
	float nearTightness;
	float smRoomSize;
	float smOpenness;
	float smReflectivity;
	float smOcclusion;
	int currentPresetA;
	int currentPresetB;
	float currentPresetBlend;
	vec3_t listenerOrigin;
	vec3_t listenerForward;
	vec3_t listenerRight;
	vec3_t listenerUp;
	vec3_t rayDirs[S_ACOUSTICS_MAX_RAYS];
	int rayDirCount;

	LPALGENEFFECTS alGenEffects;
	LPALDELETEEFFECTS alDeleteEffects;
	LPALEFFECTI alEffecti;
	LPALEFFECTF alEffectf;
	LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
	LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
	LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti;
	LPALAUXILIARYEFFECTSLOTF alAuxiliaryEffectSlotf;
	LPALGENFILTERS alGenFilters;
	LPALDELETEFILTERS alDeleteFilters;
	LPALFILTERI alFilteri;
	LPALFILTERF alFilterf;

	ALuint reverbEffect;
	ALuint reverbSlot;
	s_acoustics_filter_slot_t occlusionSlots[S_ACOUSTICS_MAX_OCCLUSION];
} s_acoustics_state_t;

static s_acoustics_state_t s_acoustics;

static const s_acoustics_preset_t s_acoustics_presets[] = {
	{ "outdoor",     0.20f, 0.20f, 0.05f, 0.80f, 0.80f, 0.90f, 0.02f, 0.01f, 0.05f, 0.02f, 0.25f, 0.00f },
	{ "small_room",  0.90f, 0.80f, 0.25f, 0.70f, 0.90f, 0.60f, 0.12f, 0.01f, 0.30f, 0.03f, 0.20f, 0.00f },
	{ "medium_room", 0.80f, 0.70f, 0.30f, 0.75f, 1.40f, 0.65f, 0.10f, 0.01f, 0.55f, 0.04f, 0.22f, 0.00f },
	{ "large_hall",  0.70f, 0.60f, 0.35f, 0.70f, 2.60f, 0.75f, 0.07f, 0.02f, 0.80f, 0.05f, 0.24f, 0.00f },
	{ "tunnel",      0.60f, 0.50f, 0.28f, 0.60f, 2.00f, 0.55f, 0.08f, 0.02f, 0.60f, 0.04f, 0.30f, 0.00f },
	{ "cavern",      0.50f, 0.40f, 0.40f, 0.65f, 3.20f, 0.80f, 0.05f, 0.03f, 0.95f, 0.06f, 0.28f, 0.00f }
};

static float S_Acoustics_Clamp01( float v ) {
	if ( v < 0.0f ) {
		return 0.0f;
	}
	if ( v > 1.0f ) {
		return 1.0f;
	}
	return v;
}

static float S_Acoustics_Lerp( float a, float b, float t ) {
	return a + ( b - a ) * t;
}

static int S_Acoustics_ClampInt( int v, int minV, int maxV ) {
	if ( v < minV ) {
		return minV;
	}
	if ( v > maxV ) {
		return maxV;
	}
	return v;
}

static float S_Acoustics_ClampFloat( float v, float minV, float maxV ) {
	if ( v < minV ) {
		return minV;
	}
	if ( v > maxV ) {
		return maxV;
	}
	return v;
}

static float S_Acoustics_Smooth( float current, float target, int dtMs ) {
	float alpha;

	if ( !s_acoustics_smooth_ms || s_acoustics_smooth_ms->integer <= 0 ) {
		return target;
	}
	if ( dtMs <= 0 ) {
		return current;
	}

	alpha = (float)dtMs / (float)s_acoustics_smooth_ms->integer;
	alpha = S_Acoustics_ClampFloat( alpha,
		s_acoustics_smooth_min_alpha ? s_acoustics_smooth_min_alpha->value : 0.05f,
		s_acoustics_smooth_max_alpha ? s_acoustics_smooth_max_alpha->value : 0.5f );

	return current + ( target - current ) * alpha;
}

static void S_Acoustics_BuildRayDirs( void ) {
	int i = 0;
	vec3_t dirs[] = {
		{ 1.0f, 0.0f, 0.0f },   { -1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },   { 0.0f, -1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f },   { 0.0f, 0.0f, -1.0f },
		{ 1.0f, 1.0f, 0.0f },   { -1.0f, 1.0f, 0.0f },
		{ 1.0f, -1.0f, 0.0f },  { -1.0f, -1.0f, 0.0f },
		{ 1.0f, 0.0f, 1.0f },   { -1.0f, 0.0f, 1.0f },
		{ 1.0f, 0.0f, -1.0f },  { -1.0f, 0.0f, -1.0f },
		{ 0.0f, 1.0f, 1.0f },   { 0.0f, -1.0f, 1.0f },
		{ 0.0f, 1.0f, -1.0f },  { 0.0f, -1.0f, -1.0f },
		{ 1.0f, 1.0f, 1.0f },   { -1.0f, 1.0f, 1.0f },
		{ 1.0f, -1.0f, 1.0f },  { -1.0f, -1.0f, 1.0f },
		{ 1.0f, 1.0f, -1.0f },  { -1.0f, 1.0f, -1.0f },
		{ 1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, -1.0f },
		{ 0.5f, 0.5f, 0.0f },   { -0.5f, 0.5f, 0.0f },
		{ 0.5f, -0.5f, 0.0f },  { -0.5f, -0.5f, 0.0f },
		{ 0.0f, 0.5f, 0.5f },   { 0.0f, -0.5f, 0.5f },
		{ 0.0f, 0.5f, -0.5f },  { 0.0f, -0.5f, -0.5f }
	};

	int avail = sizeof( dirs ) / sizeof( dirs[0] );
	if ( avail > S_ACOUSTICS_MAX_RAYS ) {
		avail = S_ACOUSTICS_MAX_RAYS;
	}
	s_acoustics.rayDirCount = avail;
	for ( i = 0; i < avail; ++i ) {
		VectorNormalize2( dirs[i], s_acoustics.rayDirs[i] );
	}
}

static void S_Acoustics_LoadEfx( ALCdevice *device ) {
	void *sym;

	s_acoustics.efxAvailable = qfalse;
	s_acoustics.reverbType = S_ACOUSTICS_REVERB_NONE;

	if ( !device || ( s_acoustics_efx_enable && !s_acoustics_efx_enable->integer ) ) {
		return;
	}
	if ( alcIsExtensionPresent( device, "ALC_EXT_EFX" ) != ALC_TRUE ) {
		return;
	}

	sym = alGetProcAddress( "alGenEffects" );
	Com_Memcpy( &s_acoustics.alGenEffects, &sym, sizeof( s_acoustics.alGenEffects ) );
	sym = alGetProcAddress( "alDeleteEffects" );
	Com_Memcpy( &s_acoustics.alDeleteEffects, &sym, sizeof( s_acoustics.alDeleteEffects ) );
	sym = alGetProcAddress( "alEffecti" );
	Com_Memcpy( &s_acoustics.alEffecti, &sym, sizeof( s_acoustics.alEffecti ) );
	sym = alGetProcAddress( "alEffectf" );
	Com_Memcpy( &s_acoustics.alEffectf, &sym, sizeof( s_acoustics.alEffectf ) );
	sym = alGetProcAddress( "alGenAuxiliaryEffectSlots" );
	Com_Memcpy( &s_acoustics.alGenAuxiliaryEffectSlots, &sym, sizeof( s_acoustics.alGenAuxiliaryEffectSlots ) );
	sym = alGetProcAddress( "alDeleteAuxiliaryEffectSlots" );
	Com_Memcpy( &s_acoustics.alDeleteAuxiliaryEffectSlots, &sym, sizeof( s_acoustics.alDeleteAuxiliaryEffectSlots ) );
	sym = alGetProcAddress( "alAuxiliaryEffectSloti" );
	Com_Memcpy( &s_acoustics.alAuxiliaryEffectSloti, &sym, sizeof( s_acoustics.alAuxiliaryEffectSloti ) );
	sym = alGetProcAddress( "alAuxiliaryEffectSlotf" );
	Com_Memcpy( &s_acoustics.alAuxiliaryEffectSlotf, &sym, sizeof( s_acoustics.alAuxiliaryEffectSlotf ) );
	sym = alGetProcAddress( "alGenFilters" );
	Com_Memcpy( &s_acoustics.alGenFilters, &sym, sizeof( s_acoustics.alGenFilters ) );
	sym = alGetProcAddress( "alDeleteFilters" );
	Com_Memcpy( &s_acoustics.alDeleteFilters, &sym, sizeof( s_acoustics.alDeleteFilters ) );
	sym = alGetProcAddress( "alFilteri" );
	Com_Memcpy( &s_acoustics.alFilteri, &sym, sizeof( s_acoustics.alFilteri ) );
	sym = alGetProcAddress( "alFilterf" );
	Com_Memcpy( &s_acoustics.alFilterf, &sym, sizeof( s_acoustics.alFilterf ) );

	if ( !s_acoustics.alGenEffects || !s_acoustics.alGenAuxiliaryEffectSlots || !s_acoustics.alEffecti ) {
		return;
	}

	s_acoustics.alGenEffects( 1, &s_acoustics.reverbEffect );
	if ( !s_acoustics.reverbEffect ) {
		return;
	}

	s_acoustics.alEffecti( s_acoustics.reverbEffect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB );
	if ( alGetError() == AL_NO_ERROR ) {
		s_acoustics.reverbType = S_ACOUSTICS_REVERB_EAX;
	} else {
		s_acoustics.alEffecti( s_acoustics.reverbEffect, AL_EFFECT_TYPE, AL_EFFECT_REVERB );
		if ( alGetError() == AL_NO_ERROR ) {
			s_acoustics.reverbType = S_ACOUSTICS_REVERB_STANDARD;
		}
	}

	if ( s_acoustics.reverbType == S_ACOUSTICS_REVERB_NONE ) {
		return;
	}

	s_acoustics.alGenAuxiliaryEffectSlots( 1, &s_acoustics.reverbSlot );
	if ( !s_acoustics.reverbSlot ) {
		return;
	}

	s_acoustics.efxAvailable = qtrue;
}

static void S_Acoustics_ApplyPresetParams( const s_acoustics_preset_t *preset ) {
	if ( !s_acoustics.efxAvailable || !preset || !s_acoustics.alEffectf || !s_acoustics.alEffecti ) {
		return;
	}

	if ( s_acoustics.reverbType == S_ACOUSTICS_REVERB_EAX ) {
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_EAXREVERB_DENSITY, preset->density );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_EAXREVERB_DIFFUSION, preset->diffusion );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_EAXREVERB_GAIN, preset->gain );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_EAXREVERB_GAINHF, preset->gainHF );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_EAXREVERB_DECAY_TIME, preset->decayTime );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_EAXREVERB_DECAY_HFRATIO, preset->decayHFRatio );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_EAXREVERB_REFLECTIONS_GAIN, preset->reflectionsGain );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_EAXREVERB_REFLECTIONS_DELAY, preset->reflectionsDelay );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_EAXREVERB_LATE_REVERB_GAIN, preset->lateReverbGain );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_EAXREVERB_LATE_REVERB_DELAY, preset->lateReverbDelay );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, preset->airAbsorptionGainHF );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, preset->roomRolloffFactor );
	} else {
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_REVERB_DENSITY, preset->density );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_REVERB_DIFFUSION, preset->diffusion );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_REVERB_GAIN, preset->gain );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_REVERB_GAINHF, preset->gainHF );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_REVERB_DECAY_TIME, preset->decayTime );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_REVERB_DECAY_HFRATIO, preset->decayHFRatio );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_REVERB_REFLECTIONS_GAIN, preset->reflectionsGain );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_REVERB_REFLECTIONS_DELAY, preset->reflectionsDelay );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_REVERB_LATE_REVERB_GAIN, preset->lateReverbGain );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_REVERB_LATE_REVERB_DELAY, preset->lateReverbDelay );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_REVERB_AIR_ABSORPTION_GAINHF, preset->airAbsorptionGainHF );
		s_acoustics.alEffectf( s_acoustics.reverbEffect, AL_REVERB_ROOM_ROLLOFF_FACTOR, preset->roomRolloffFactor );
	}

	if ( s_acoustics.alAuxiliaryEffectSloti ) {
		s_acoustics.alAuxiliaryEffectSloti( s_acoustics.reverbSlot, AL_EFFECTSLOT_EFFECT, s_acoustics.reverbEffect );
	}
}

static s_acoustics_preset_t S_Acoustics_BlendPresets( const s_acoustics_preset_t *a, const s_acoustics_preset_t *b, float t ) {
	s_acoustics_preset_t out = *a;

	out.density = S_Acoustics_Lerp( a->density, b->density, t );
	out.diffusion = S_Acoustics_Lerp( a->diffusion, b->diffusion, t );
	out.gain = S_Acoustics_Lerp( a->gain, b->gain, t );
	out.gainHF = S_Acoustics_Lerp( a->gainHF, b->gainHF, t );
	out.decayTime = S_Acoustics_Lerp( a->decayTime, b->decayTime, t );
	out.decayHFRatio = S_Acoustics_Lerp( a->decayHFRatio, b->decayHFRatio, t );
	out.reflectionsGain = S_Acoustics_Lerp( a->reflectionsGain, b->reflectionsGain, t );
	out.reflectionsDelay = S_Acoustics_Lerp( a->reflectionsDelay, b->reflectionsDelay, t );
	out.lateReverbGain = S_Acoustics_Lerp( a->lateReverbGain, b->lateReverbGain, t );
	out.lateReverbDelay = S_Acoustics_Lerp( a->lateReverbDelay, b->lateReverbDelay, t );
	out.airAbsorptionGainHF = S_Acoustics_Lerp( a->airAbsorptionGainHF, b->airAbsorptionGainHF, t );
	out.roomRolloffFactor = S_Acoustics_Lerp( a->roomRolloffFactor, b->roomRolloffFactor, t );

	return out;
}

static void S_Acoustics_SelectPresets( void ) {
	int forced = ( s_acoustics_preset_force ? s_acoustics_preset_force->integer : -1 );
	float outdoorThreshold = s_acoustics_outdoor_threshold ? s_acoustics_outdoor_threshold->value : 0.55f;
	float tunnelThreshold = s_acoustics_tunnel_threshold ? s_acoustics_tunnel_threshold->value : 0.25f;
	float smallThreshold = s_acoustics_roomsize_small ? s_acoustics_roomsize_small->value : 0.20f;
	float mediumThreshold = s_acoustics_roomsize_medium ? s_acoustics_roomsize_medium->value : 0.50f;
	float blend = s_acoustics_preset_blend ? s_acoustics_preset_blend->value : 1.0f;
	int presetA = 1;
	int presetB = 2;
	float t = 0.0f;

	blend = S_Acoustics_Clamp01( blend );

	if ( forced >= 0 && forced < (int)( sizeof( s_acoustics_presets ) / sizeof( s_acoustics_presets[0] ) ) ) {
		s_acoustics.currentPresetA = forced;
		s_acoustics.currentPresetB = forced;
		s_acoustics.currentPresetBlend = 0.0f;
		return;
	}

	if ( s_acoustics.smOpenness >= outdoorThreshold ) {
		s_acoustics.currentPresetA = 0;
		s_acoustics.currentPresetB = 0;
		s_acoustics.currentPresetBlend = 0.0f;
		return;
	}

	if ( s_acoustics.smOpenness <= tunnelThreshold && s_acoustics.forwardHitFrac > 0.7f ) {
		s_acoustics.currentPresetA = 4;
		s_acoustics.currentPresetB = 4;
		s_acoustics.currentPresetBlend = 0.0f;
		return;
	}

	if ( s_acoustics.smRoomSize < smallThreshold ) {
		presetA = 1;
		presetB = 1;
		t = 0.0f;
	} else if ( s_acoustics.smRoomSize < mediumThreshold ) {
		presetA = 1;
		presetB = 2;
		t = ( s_acoustics.smRoomSize - smallThreshold ) / ( mediumThreshold - smallThreshold );
	} else if ( s_acoustics.smRoomSize < 0.75f ) {
		presetA = 2;
		presetB = 3;
		t = ( s_acoustics.smRoomSize - mediumThreshold ) / ( 0.75f - mediumThreshold );
	} else {
		presetA = 3;
		presetB = 5;
		t = S_Acoustics_Clamp01( ( s_acoustics.smRoomSize - 0.75f ) / 0.25f );
	}

	s_acoustics.currentPresetA = presetA;
	s_acoustics.currentPresetB = presetB;
	s_acoustics.currentPresetBlend = t * blend;
}

static void S_Acoustics_UpdatePreset( void ) {
	s_acoustics_preset_t blended;

	if ( !s_acoustics.efxAvailable || ( s_acoustics_bypass && s_acoustics_bypass->integer ) ) {
		return;
	}
	if ( s_acoustics_efx_enable && !s_acoustics_efx_enable->integer ) {
		return;
	}

	S_Acoustics_SelectPresets();
	blended = S_Acoustics_BlendPresets( &s_acoustics_presets[s_acoustics.currentPresetA],
		&s_acoustics_presets[s_acoustics.currentPresetB], s_acoustics.currentPresetBlend );

	blended.airAbsorptionGainHF = S_Acoustics_Clamp01( s_acoustics_air_absorb ? s_acoustics_air_absorb->value : 0.25f );
	{
		float scale = s_acoustics_decay_scale ? s_acoustics_decay_scale->value : 1.0f;
		scale = S_Acoustics_ClampFloat( scale, 0.1f, 2.0f );
		blended.decayTime *= scale;
		blended.lateReverbDelay *= scale;
	}
	S_Acoustics_ApplyPresetParams( &blended );

	if ( s_acoustics.alAuxiliaryEffectSlotf ) {
		float wet = s_acoustics_wet ? s_acoustics_wet->value : 0.35f;
		s_acoustics.alAuxiliaryEffectSlotf( s_acoustics.reverbSlot, AL_EFFECTSLOT_GAIN, S_Acoustics_Clamp01( wet ) );
	}
}

static int S_Acoustics_GetRayCount( void ) {
	int cvarRays = s_acoustics_rays ? s_acoustics_rays->integer : 12;
	int effort = s_acoustics_reflection_effort ? s_acoustics_reflection_effort->integer : 1;
	int capped = S_Acoustics_ClampInt( cvarRays, 6, S_ACOUSTICS_MAX_RAYS );

	if ( effort <= 0 ) {
		capped = S_Acoustics_ClampInt( capped / 2, 6, S_ACOUSTICS_MAX_RAYS );
	} else if ( effort >= 2 ) {
		capped = S_Acoustics_ClampInt( capped + capped / 2, 6, S_ACOUSTICS_MAX_RAYS );
	}

	if ( s_acoustics.rayCountAdaptive <= 0 ) {
		s_acoustics.rayCountAdaptive = capped;
	}

	if ( s_acoustics.rayCountAdaptive > capped ) {
		s_acoustics.rayCountAdaptive = capped;
	}

	return s_acoustics.rayCountAdaptive;
}

static int S_Acoustics_GetHz( void ) {
	int hz = s_acoustics_hz ? s_acoustics_hz->integer : 15;
	return S_Acoustics_ClampInt( hz, 1, 60 );
}

static qboolean S_Acoustics_ShouldUpdate( int64_t nowUs ) {
	int64_t intervalUs;

	if ( !s_acoustics_enable || !s_acoustics_enable->integer ) {
		return qfalse;
	}

	intervalUs = 1000000 / S_Acoustics_GetHz();
	if ( s_acoustics.lastUpdateUs == 0 ) {
		s_acoustics.lastUpdateUs = nowUs;
		return qtrue;
	}
	if ( ( nowUs - s_acoustics.lastUpdateUs ) < intervalUs ) {
		return qfalse;
	}

	s_acoustics.lastUpdateUs = nowUs;
	return qtrue;
}

static qboolean S_Acoustics_WorldReady( void ) {
	return ( CM_NumInlineModels() > 0 ) ? qtrue : qfalse;
}

static void S_Acoustics_DebugPrint( int nowMs ) {
	int interval = s_acoustics_print_interval_ms ? s_acoustics_print_interval_ms->integer : 1000;

	if ( !s_acoustics_debug || s_acoustics_debug->integer <= 0 ) {
		return;
	}
	if ( nowMs - s_acoustics.lastPrintMs < interval ) {
		return;
	}
	s_acoustics.lastPrintMs = nowMs;

	Com_Printf( "Acoustics: room=%.2f open=%.2f refl=%.2f occ=%.2f rays=%d preset=%s/%s blend=%.2f\n",
		s_acoustics.smRoomSize,
		s_acoustics.smOpenness,
		s_acoustics.smReflectivity,
		s_acoustics.smOcclusion,
		s_acoustics.raysUsed,
		s_acoustics_presets[s_acoustics.currentPresetA].name,
		s_acoustics_presets[s_acoustics.currentPresetB].name,
		s_acoustics.currentPresetBlend );

	if ( s_acoustics_debug->integer >= 2 ) {
		Com_Printf( "Acoustics: forward=%.2f near=%.2f efx=%s\n",
			s_acoustics.forwardHitFrac,
			s_acoustics.nearTightness,
			s_acoustics.efxAvailable ? "on" : "off" );
	}
}

static void S_Acoustics_ComputeMetrics( int dtMs ) {
	vec3_t mins = { 0.0f, 0.0f, 0.0f };
	vec3_t maxs = { 0.0f, 0.0f, 0.0f };
	float maxDist = s_acoustics_maxdist ? s_acoustics_maxdist->value : 1536.0f;
	float nearDist = s_acoustics_near ? s_acoustics_near->value : 64.0f;
	int rays = S_Acoustics_GetRayCount();
	int openCount = 0;
	float totalDist = 0.0f;
	int nearHits = 0;
	int i;
	int64_t startUs = Sys_Microseconds();
	int64_t budgetUs = ( s_acoustics_budget_us ? s_acoustics_budget_us->integer : 200 );

	s_acoustics.raysUsed = 0;
	if ( maxDist < 1.0f ) {
		maxDist = 1.0f;
	}
	nearDist = S_Acoustics_ClampFloat( nearDist, 1.0f, maxDist );

	for ( i = 0; i < rays; ++i ) {
		trace_t trace;
		vec3_t end;
		float dist;

		VectorMA( s_acoustics.listenerOrigin, maxDist, s_acoustics.rayDirs[i], end );
		CM_BoxTrace( &trace, s_acoustics.listenerOrigin, end, mins, maxs, 0, MASK_SOLID, qfalse );

		if ( trace.allsolid || trace.startsolid ) {
			dist = 0.0f;
		} else if ( trace.fraction < 1.0f ) {
			dist = trace.fraction * maxDist;
		} else {
			dist = maxDist;
			openCount++;
		}

		if ( dist <= nearDist ) {
			nearHits++;
		}

		totalDist += dist;
		s_acoustics.raysUsed++;
	}

	s_acoustics.roomSize = ( totalDist / (float)rays ) / maxDist;
	s_acoustics.openness = (float)openCount / (float)rays;
	s_acoustics.reflectivity = S_Acoustics_Clamp01( r_acoustics_reflectivity ? r_acoustics_reflectivity->value : 0.5f );
	s_acoustics.nearTightness = (float)nearHits / (float)rays;

	{
		trace_t trace;
		vec3_t end;
		float coneWeight = 0.707f;

		if ( s_acoustics_cone_deg ) {
			float halfRad = DEG2RAD( s_acoustics_cone_deg->value * 0.5f );
			coneWeight = cosf( halfRad );
			coneWeight = S_Acoustics_Clamp01( coneWeight );
		}

		VectorMA( s_acoustics.listenerOrigin, maxDist, s_acoustics.listenerForward, end );
		CM_BoxTrace( &trace, s_acoustics.listenerOrigin, end, mins, maxs, 0, MASK_SOLID, qfalse );

		if ( trace.allsolid || trace.startsolid ) {
			s_acoustics.forwardHitFrac = 0.0f;
			s_acoustics.occlusion = 1.0f;
		} else {
			s_acoustics.forwardHitFrac = trace.fraction;
			s_acoustics.occlusion = S_Acoustics_Clamp01( ( 1.0f - trace.fraction ) * ( 0.75f + 0.25f * coneWeight ) );
		}
	}

	if ( budgetUs > 0 ) {
		int64_t elapsedUs = Sys_Microseconds() - startUs;
		if ( elapsedUs > budgetUs ) {
			s_acoustics.rayCountAdaptive = S_Acoustics_ClampInt( rays - 2, 6, S_ACOUSTICS_MAX_RAYS );
		} else if ( s_acoustics.rayCountAdaptive < rays ) {
			s_acoustics.rayCountAdaptive = S_Acoustics_ClampInt( s_acoustics.rayCountAdaptive + 1, 6, S_ACOUSTICS_MAX_RAYS );
		}
	}

	s_acoustics.smRoomSize = S_Acoustics_Smooth( s_acoustics.smRoomSize, s_acoustics.roomSize, dtMs );
	s_acoustics.smOpenness = S_Acoustics_Smooth( s_acoustics.smOpenness, s_acoustics.openness, dtMs );
	s_acoustics.smReflectivity = S_Acoustics_Smooth( s_acoustics.smReflectivity, s_acoustics.reflectivity, dtMs );
	s_acoustics.smOcclusion = S_Acoustics_Smooth( s_acoustics.smOcclusion, s_acoustics.occlusion, dtMs );
}

void S_Acoustics_Init( void ) {
	ALCcontext *context = alcGetCurrentContext();
	ALCdevice *device = context ? alcGetContextsDevice( context ) : NULL;

	if ( s_acoustics.inited ) {
		return;
	}

	Com_Memset( &s_acoustics, 0, sizeof( s_acoustics ) );
	S_Acoustics_BuildRayDirs();
	S_Acoustics_LoadEfx( device );

	if ( s_acoustics_efx_enable && s_acoustics_efx_enable->integer &&
		!s_acoustics.efxAvailable && s_acoustics_warn_efx && s_acoustics_warn_efx->integer ) {
		Com_Printf( "Acoustics: OpenAL EFX not available, running dry.\n" );
		s_acoustics.warnedEfx = qtrue;
	}

	s_acoustics.inited = qtrue;
}

void S_Acoustics_Shutdown( void ) {
	int i;

	if ( !s_acoustics.inited ) {
		return;
	}

	if ( s_acoustics.alDeleteFilters ) {
		for ( i = 0; i < S_ACOUSTICS_MAX_OCCLUSION; ++i ) {
			if ( s_acoustics.occlusionSlots[i].filter ) {
				s_acoustics.alDeleteFilters( 1, &s_acoustics.occlusionSlots[i].filter );
				s_acoustics.occlusionSlots[i].filter = 0;
			}
		}
	}
	if ( s_acoustics.reverbSlot && s_acoustics.alDeleteAuxiliaryEffectSlots ) {
		s_acoustics.alDeleteAuxiliaryEffectSlots( 1, &s_acoustics.reverbSlot );
		s_acoustics.reverbSlot = 0;
	}
	if ( s_acoustics.reverbEffect && s_acoustics.alDeleteEffects ) {
		s_acoustics.alDeleteEffects( 1, &s_acoustics.reverbEffect );
		s_acoustics.reverbEffect = 0;
	}

	Com_Memset( &s_acoustics, 0, sizeof( s_acoustics ) );
}

void S_Acoustics_Reset( void ) {
	s_acoustics.lastUpdateUs = 0;
	s_acoustics.smRoomSize = 0.0f;
	s_acoustics.smOpenness = 0.0f;
	s_acoustics.smReflectivity = 0.0f;
	s_acoustics.smOcclusion = 0.0f;
	s_acoustics.currentPresetA = 0;
	s_acoustics.currentPresetB = 0;
	s_acoustics.currentPresetBlend = 0.0f;
}

void S_Acoustics_Frame( const vec3_t listenerOrigin, const vec3_t listenerForward,
	const vec3_t listenerRight, const vec3_t listenerUp ) {
	int64_t nowUs;
	int nowMs;
	int dtMs;

	if ( !s_acoustics.inited ) {
		return;
	}
	if ( !s_acoustics_enable || !s_acoustics_enable->integer ) {
		return;
	}
	if ( !S_Acoustics_WorldReady() ) {
		return;
	}

	VectorCopy( listenerOrigin, s_acoustics.listenerOrigin );
	VectorCopy( listenerForward, s_acoustics.listenerForward );
	VectorCopy( listenerRight, s_acoustics.listenerRight );
	VectorCopy( listenerUp, s_acoustics.listenerUp );

	s_acoustics.frameId++;
	s_acoustics.occlusionTraces = 0;

	nowUs = Sys_Microseconds();
	nowMs = Sys_Milliseconds();
	dtMs = s_acoustics.lastFrameMs > 0 ? ( nowMs - s_acoustics.lastFrameMs ) : 16;
	s_acoustics.lastFrameMs = nowMs;

	if ( !S_Acoustics_ShouldUpdate( nowUs ) ) {
		return;
	}

	S_Acoustics_ComputeMetrics( dtMs );
	S_Acoustics_UpdatePreset();
	S_Acoustics_DebugPrint( nowMs );

	if ( s_acoustics_draw && s_acoustics_draw->integer > 0 && !s_acoustics.warnedDraw ) {
		Com_Printf( "Acoustics: debug draw not supported in this build.\n" );
		s_acoustics.warnedDraw = qtrue;
	}
}

float S_Acoustics_SourceOcclusion( const vec3_t srcPos, const vec3_t listenerPos ) {
	trace_t trace;
	vec3_t mins = { 0.0f, 0.0f, 0.0f };
	vec3_t maxs = { 0.0f, 0.0f, 0.0f };

	if ( !s_acoustics_enable || !s_acoustics_enable->integer ) {
		return 0.0f;
	}
	if ( !S_Acoustics_WorldReady() ) {
		return 0.0f;
	}

	CM_BoxTrace( &trace, listenerPos, srcPos, mins, maxs, 0, MASK_SOLID, qfalse );
	if ( trace.allsolid || trace.startsolid ) {
		return 1.0f;
	}
	if ( trace.fraction >= 1.0f ) {
		return 0.0f;
	}
	return S_Acoustics_Clamp01( 1.0f - trace.fraction );
}

static s_acoustics_filter_slot_t *S_Acoustics_FindSlot( ALuint source ) {
	int i;
	int oldest = 0;
	int maxSlots = s_acoustics_occlusion_max_sources ? s_acoustics_occlusion_max_sources->integer : 16;

	maxSlots = S_Acoustics_ClampInt( maxSlots, 1, S_ACOUSTICS_MAX_OCCLUSION );

	for ( i = 0; i < maxSlots; ++i ) {
		if ( s_acoustics.occlusionSlots[i].source == source ) {
			return &s_acoustics.occlusionSlots[i];
		}
		if ( s_acoustics.occlusionSlots[i].source == 0 ) {
			return &s_acoustics.occlusionSlots[i];
		}
		if ( s_acoustics.occlusionSlots[i].lastUseMs < s_acoustics.occlusionSlots[oldest].lastUseMs ) {
			oldest = i;
		}
	}

	return &s_acoustics.occlusionSlots[oldest];
}

static void S_Acoustics_UpdateSourceFilter( ALuint source, float occlusion ) {
	s_acoustics_filter_slot_t *slot;
	float strength = s_acoustics_occlusion_strength ? s_acoustics_occlusion_strength->value : 0.8f;
	float minGain = s_acoustics_occlusion_min_gain ? s_acoustics_occlusion_min_gain->value : 0.35f;
	float hf = s_acoustics_occlusion_hf ? s_acoustics_occlusion_hf->value : 0.25f;
	float amount = S_Acoustics_Clamp01( occlusion * strength );
	float gain;
	float gainHF;

	if ( !s_acoustics.efxAvailable || !s_acoustics.alGenFilters || !s_acoustics.alFilteri || !s_acoustics.alFilterf ) {
		return;
	}
	if ( s_acoustics_efx_enable && !s_acoustics_efx_enable->integer ) {
		return;
	}

	slot = S_Acoustics_FindSlot( source );
	slot->source = source;
	slot->lastUseMs = Sys_Milliseconds();

	if ( !slot->filter ) {
		s_acoustics.alGenFilters( 1, &slot->filter );
		if ( !slot->filter ) {
			return;
		}
		s_acoustics.alFilteri( slot->filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS );
	}

	gain = S_Acoustics_Lerp( 1.0f, S_Acoustics_Clamp01( minGain ), amount );
	gainHF = S_Acoustics_Lerp( 1.0f, S_Acoustics_Clamp01( hf ), amount );
	s_acoustics.alFilterf( slot->filter, AL_LOWPASS_GAIN, gain );
	s_acoustics.alFilterf( slot->filter, AL_LOWPASS_GAINHF, gainHF );
	alSourcei( source, AL_DIRECT_FILTER, slot->filter );
}

void S_Acoustics_ApplySource( ALuint source, float baseGain, const vec3_t srcPos ) {
	float dry = s_acoustics_dry ? s_acoustics_dry->value : 1.0f;
	float occlusion = 0.0f;
	int nowMs;

	if ( !source ) {
		return;
	}
	if ( !s_acoustics_enable || !s_acoustics_enable->integer ) {
		return;
	}
	if ( srcPos && !S_Mixer_PropagationAllowed( srcPos ) ) {
		alSourcef( source, AL_GAIN, 0.0f );
		alSource3i( source, AL_AUXILIARY_SEND_FILTER, 0, 0, 0 );
		alSourcei( source, AL_DIRECT_FILTER, 0 );
		return;
	}

	dry = S_Acoustics_Clamp01( dry );
	alSourcef( source, AL_GAIN, baseGain * dry );

	if ( s_acoustics_bypass && s_acoustics_bypass->integer ) {
		alSource3i( source, AL_AUXILIARY_SEND_FILTER, 0, 0, 0 );
		alSourcei( source, AL_DIRECT_FILTER, 0 );
		return;
	}

	if ( s_acoustics.efxAvailable && s_acoustics_efx_enable && s_acoustics_efx_enable->integer && s_acoustics.reverbSlot ) {
		alSource3i( source, AL_AUXILIARY_SEND_FILTER, s_acoustics.reverbSlot, 0, 0 );
	} else {
		alSource3i( source, AL_AUXILIARY_SEND_FILTER, 0, 0, 0 );
	}

	if ( s_acoustics_occlusion_enable && s_acoustics_occlusion_enable->integer &&
		s_acoustics_occlusion_trace_sources && s_acoustics_occlusion_trace_sources->integer ) {
		int maxSources = s_acoustics_occlusion_max_sources ? s_acoustics_occlusion_max_sources->integer : 16;

		maxSources = S_Acoustics_ClampInt( maxSources, 1, S_ACOUSTICS_MAX_OCCLUSION );
		if ( s_acoustics.occlusionTraces < maxSources ) {
			occlusion = S_Acoustics_SourceOcclusion( srcPos, s_acoustics.listenerOrigin );
			s_acoustics.occlusionTraces++;
		}
		if ( occlusion > 0.0f ) {
			S_Acoustics_UpdateSourceFilter( source, occlusion );
		} else {
			alSourcei( source, AL_DIRECT_FILTER, 0 );
		}
	} else {
		alSourcei( source, AL_DIRECT_FILTER, 0 );
	}

	nowMs = Sys_Milliseconds();
	(void)nowMs;
}

#endif
