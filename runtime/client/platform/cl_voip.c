/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

VoIP implementation using Opus codec.

Pipeline:
  Capture (SDL/OpenAL) -> Opus encode -> MSG_Write -> Network
  -> MSG_Read -> Opus decode -> S_VoipSamples playback

Lip flap:
  Per-client RMS power (local capture + remote decode) drives jaw/mouth
  morph targets on nearby player head models via RE_SetEntityMorphWeight.
===========================================================================
*/

#include "client.h"
#include "cl_voip.h"
#include "../../game/g_facial.h"

#include <math.h>

#ifdef USE_OPUS
#include <opus.h>

static OpusEncoder  *voipEncoder = NULL;
static OpusDecoder  *voipDecoders[VOIP_MAX_CLIENTS];

static cvar_t *cl_voip;
static cvar_t *cl_voipSend;
static cvar_t *cl_voipGainDuringCapture;
static cvar_t *cl_voipShowMeter;
static cvar_t *cl_voipLipFlap;
static cvar_t *cl_voipLipFlapScale;
static cvar_t *cl_voipLipFlapThresh;
static cvar_t *cl_voipLipFlapDecay;
static cvar_t *cl_voipLipFlapMatch;
static cvar_t *cl_voipLipFlapMorph;
static cvar_t *cl_voipLipFlapRate;
static cvar_t *cl_voipLipFlapFacs;

static qboolean voipInitialized = qfalse;
static qboolean voipCapturing = qfalse;
static float    voipPower = 0.0f;
static float    voipClientPower[VOIP_MAX_CLIENTS];
static int      voipClientLastTalk[VOIP_MAX_CLIENTS];
static uint32_t voipOutSequence = 0;
static uint32_t voipInSequence[VOIP_MAX_CLIENTS];

static int16_t  captureBuffer[VOIP_FRAME_SAMPLES * 4];
static byte     encodedBuffer[VOIP_MAX_PACKET];

/* Pending Opus frame for CL_WritePacket (sequence + payload). */
static byte     voipPendingData[VOIP_MAX_PACKET + 8];
static int      voipPendingLen = 0;

/* Mic self-test: capture briefly and report peak RMS without sending. */
static int      voipTestUntil = 0;
static float    voipTestPeak = 0.0f;
static qboolean voipTestOwnedCapture = qfalse;

/* Capture functions from the active sound backend (SDL or OpenAL).
   Weak symbols provide no-op fallback when neither backend has capture. */
void SNDDMA_StartCapture( void ) __attribute__((weak));
int  SNDDMA_AvailableCaptureSamples( void ) __attribute__((weak));
void SNDDMA_Capture( int samples, byte *data ) __attribute__((weak));
void SNDDMA_StopCapture( void ) __attribute__((weak));

#ifdef __GNUC__
void SNDDMA_StartCapture( void ) {}
int  SNDDMA_AvailableCaptureSamples( void ) { return 0; }
void SNDDMA_Capture( int samples, byte *data ) { (void)samples; (void)data; }
void SNDDMA_StopCapture( void ) {}
#endif

static float CL_VoIP_DistSquared( const vec3_t a, const vec3_t b ) {
	float dx = a[0] - b[0];
	float dy = a[1] - b[1];
	float dz = a[2] - b[2];
	return dx * dx + dy * dy + dz * dz;
}

static void CL_VoIP_SetClientPower( int clientNum, float rms ) {
	if ( clientNum < 0 || clientNum >= VOIP_MAX_CLIENTS ) {
		return;
	}
	if ( rms < 0.0f ) {
		rms = 0.0f;
	}
	if ( rms > voipClientPower[clientNum] ) {
		voipClientPower[clientNum] = rms;
	} else {
		voipClientPower[clientNum] = voipClientPower[clientNum] * 0.55f + rms * 0.45f;
	}
	voipClientLastTalk[clientNum] = cls.realtime;
}

static void CL_VoIP_DecayClientPower( void ) {
	int i;
	float decay;

	if ( !cl_voipLipFlap || !cl_voipLipFlap->integer ) {
		return;
	}

	decay = cl_voipLipFlapDecay ? cl_voipLipFlapDecay->value : 0.85f;
	if ( decay < 0.1f ) {
		decay = 0.1f;
	} else if ( decay > 0.99f ) {
		decay = 0.99f;
	}

	for ( i = 0; i < VOIP_MAX_CLIENTS; i++ ) {
		if ( voipClientPower[i] <= 0.0f ) {
			continue;
		}
		if ( cls.realtime - voipClientLastTalk[i] < 40 ) {
			continue;
		}
		voipClientPower[i] *= decay;
		if ( voipClientPower[i] < 0.001f ) {
			voipClientPower[i] = 0.0f;
		}
	}
}

/*
===============
CL_VoIP_MatchClientNearOrigin

Find the player client whose body/head is closest to a refEntity origin.
===============
*/
static int CL_VoIP_MatchClientNearOrigin( const vec3_t origin ) {
	int i;
	int bestClient = -1;
	float matchDist;
	float bestDist;
	float headZ;

	if ( cls.state != CA_ACTIVE || !cl.snap.valid ) {
		return -1;
	}

	matchDist = cl_voipLipFlapMatch ? cl_voipLipFlapMatch->value : 80.0f;
	if ( matchDist < 16.0f ) {
		matchDist = 16.0f;
	}
	bestDist = matchDist * matchDist;
	headZ = 48.0f;

	for ( i = 0; i < cl.snap.numEntities; i++ ) {
		const entityState_t *es = &cl.parseEntities[
			( cl.snap.parseEntitiesNum + i ) & ( MAX_PARSE_ENTITIES - 1 ) ];
		vec3_t head;
		float dBody;
		float dHead;
		float d;
		int clientNum;

		if ( es->eType != ET_PLAYER ) {
			continue;
		}

		clientNum = es->clientNum;
		if ( clientNum < 0 || clientNum >= VOIP_MAX_CLIENTS ) {
			clientNum = es->number;
		}
		if ( clientNum < 0 || clientNum >= VOIP_MAX_CLIENTS ) {
			continue;
		}

		dBody = CL_VoIP_DistSquared( origin, es->origin );
		VectorCopy( es->origin, head );
		head[2] += headZ;
		dHead = CL_VoIP_DistSquared( origin, head );
		d = dBody < dHead ? dBody : dHead;

		if ( d < bestDist ) {
			bestDist = d;
			bestClient = clientNum;
		}
	}

	/* Predicted local player may be absent from the entity list in first person. */
	if ( cl.snap.ps.clientNum >= 0 && cl.snap.ps.clientNum < VOIP_MAX_CLIENTS ) {
		vec3_t head;
		float dBody;
		float dHead;
		float d;

		dBody = CL_VoIP_DistSquared( origin, cl.snap.ps.origin );
		VectorCopy( cl.snap.ps.origin, head );
		head[2] += cl.snap.ps.viewheight > 0 ? (float)cl.snap.ps.viewheight : headZ;
		dHead = CL_VoIP_DistSquared( origin, head );
		d = dBody < dHead ? dBody : dHead;
		if ( d < bestDist ) {
			bestClient = cl.snap.ps.clientNum;
		}
	}

	return bestClient;
}

static void CL_VoIP_ApplyMorphNames( refEntity_t *ent, float weight ) {
	char morphList[MAX_CVAR_VALUE_STRING];
	char *p;
	char *start;

	if ( !re.SetEntityMorphWeight ) {
		return;
	}

	if ( !cl_voipLipFlapMorph || !cl_voipLipFlapMorph->string[0] ) {
		re.SetEntityMorphWeight( ent, "jaw", weight );
		re.SetEntityMorphWeight( ent, "mouthOpen", weight );
		return;
	}

	Q_strncpyz( morphList, cl_voipLipFlapMorph->string, sizeof( morphList ) );
	p = morphList;
	while ( *p ) {
		while ( *p == ',' || *p == ';' || *p == ' ' || *p == '\t' ) {
			p++;
		}
		if ( !*p ) {
			break;
		}
		start = p;
		while ( *p && *p != ',' && *p != ';' && *p != ' ' && *p != '\t' ) {
			p++;
		}
		if ( *p ) {
			*p++ = '\0';
		}
		if ( start[0] ) {
			re.SetEntityMorphWeight( ent, start, weight );
		}
	}
}

void CL_VoIP_ApplyLipFlap( refEntity_t *ent ) {
	int clientNum;
	int i;
	float power;
	float scale;
	float thresh;
	float weight;
	float rate;
	float flap;
	qboolean anyTalking = qfalse;

	if ( !ent || ent->reType != RT_MODEL ) {
		return;
	}
	if ( !cl_voipLipFlap || !cl_voipLipFlap->integer ) {
		return;
	}
	if ( !re.SetEntityMorphWeight ) {
		return;
	}

	for ( i = 0; i < VOIP_MAX_CLIENTS; i++ ) {
		if ( voipClientPower[i] > 0.0f ) {
			anyTalking = qtrue;
			break;
		}
	}
	if ( !anyTalking ) {
		return;
	}

	clientNum = CL_VoIP_MatchClientNearOrigin( ent->origin );
	if ( clientNum < 0 ) {
		return;
	}

	power = voipClientPower[clientNum];
	thresh = cl_voipLipFlapThresh ? cl_voipLipFlapThresh->value : 0.02f;
	if ( power < thresh ) {
		return;
	}

	scale = cl_voipLipFlapScale ? cl_voipLipFlapScale->value : 4.0f;
	weight = power * scale;
	if ( weight > 1.0f ) {
		weight = 1.0f;
	}

	/* Light oscillation so sustained speech reads as lip motion, not a static open jaw. */
	rate = cl_voipLipFlapRate ? cl_voipLipFlapRate->value : 12.0f;
	flap = 0.62f + 0.38f * sinf( (float)cls.realtime * 0.001f * rate + (float)clientNum * 1.7f );
	weight *= flap;

	CL_VoIP_ApplyMorphNames( ent, weight );
}

/*
===============
CL_VoIP_SyncFacs

Drive FACS AU25/AU26 from per-client VoIP RMS so Face morphs track speech.
Auto-creates a face instance per talking client when needed.
===============
*/
static void CL_VoIP_SyncFacs( void ) {
	int i;
	float scale;
	float thresh;

	if ( !cl_voipLipFlapFacs || !cl_voipLipFlapFacs->integer ) {
		return;
	}
	if ( !cl_voipLipFlap || !cl_voipLipFlap->integer ) {
		return;
	}

	scale = cl_voipLipFlapScale ? cl_voipLipFlapScale->value : 4.0f;
	thresh = cl_voipLipFlapThresh ? cl_voipLipFlapThresh->value : 0.02f;

	for ( i = 0; i < VOIP_MAX_CLIENTS; i++ ) {
		faceHandle_t h;
		float power = voipClientPower[i];
		float weight = 0.0f;

		if ( power >= thresh ) {
			weight = power * scale;
			if ( weight > 1.0f ) {
				weight = 1.0f;
			}
		}

		h = Face_FindByEntityNum( i );
		if ( weight <= 0.0f ) {
			if ( h >= 0 ) {
				Face_SetAU( h, FACS_AU26, 0.0f );
				Face_SetAU( h, FACS_AU25, 0.0f );
			}
			continue;
		}

		if ( h < 0 ) {
			h = Face_Create( i );
		}
		if ( h < 0 ) {
			continue;
		}

		Face_SetAU( h, FACS_AU26, weight );
		Face_SetAU( h, FACS_AU25, weight * 0.4f );
	}
}

static void CL_VoIP_Transmit_f( void ) {
	CL_VoIP_Transmit( 0 );
}

static void CL_VoIP_StopTransmit_f( void ) {
	CL_VoIP_StopTransmit();
}

static void CL_VoIP_Status_f( void ) {
	Com_Printf( "----- VoIP status -----\n" );
	Com_Printf( "  cl_voip: %d  initialized: %s  capturing: %s  sending: %s\n",
		cl_voip ? cl_voip->integer : 0,
		voipInitialized ? "yes" : "no",
		voipCapturing ? "yes" : "no",
		( cl_voipSend && cl_voipSend->integer ) ? "yes" : "no" );
	Com_Printf( "  power: %.3f  pending: %d bytes  outSeq: %u\n",
		voipPower, voipPendingLen, voipOutSequence );
	Com_Printf( "  Hold CAPSLOCK/V (+voip) to talk. Run voiptest to check the mic.\n" );
	Com_Printf( "  Mic devices: s_aldevices   Prefer mic: seta s_openalCaptureDevice \"name\"\n" );
	Com_Printf( "-----------------------\n" );
}

static void CL_VoIP_Test_f( void ) {
	if ( !cl_voip || !cl_voip->integer ) {
		Com_Printf( "VoIP test: cl_voip is 0 — enable with seta cl_voip 1\n" );
		return;
	}
	if ( !voipInitialized ) {
		Com_Printf( "VoIP test: codec not initialized yet\n" );
		return;
	}

	voipTestPeak = 0.0f;
	voipTestUntil = cls.realtime + 1500;
	if ( !voipCapturing ) {
		SNDDMA_StartCapture();
		voipCapturing = qtrue;
		voipTestOwnedCapture = qtrue;
	} else {
		voipTestOwnedCapture = qfalse;
	}
	Com_Printf( "VoIP test: speak into your mic for 1.5 seconds...\n" );
}

void CL_VoIP_Init( void ) {
	int err;

	cl_voip     = Cvar_Get( "cl_voip", "1", CVAR_ARCHIVE );
	cl_voipSend = Cvar_Get( "cl_voipSend", "0", 0 );
	cl_voipGainDuringCapture = Cvar_Get( "cl_voipGainDuringCapture", "0.2", CVAR_ARCHIVE );
	cl_voipShowMeter = Cvar_Get( "cl_voipShowMeter", "1", CVAR_ARCHIVE );
	cl_voipLipFlap = Cvar_Get( "cl_voipLipFlap", "1", CVAR_ARCHIVE );
	cl_voipLipFlapScale = Cvar_Get( "cl_voipLipFlapScale", "4.0", CVAR_ARCHIVE );
	cl_voipLipFlapThresh = Cvar_Get( "cl_voipLipFlapThresh", "0.02", CVAR_ARCHIVE );
	cl_voipLipFlapDecay = Cvar_Get( "cl_voipLipFlapDecay", "0.85", CVAR_ARCHIVE );
	cl_voipLipFlapMatch = Cvar_Get( "cl_voipLipFlapMatch", "80", CVAR_ARCHIVE );
	cl_voipLipFlapMorph = Cvar_Get( "cl_voipLipFlapMorph", "jaw,mouthOpen,mouth_open,jaw_open", CVAR_ARCHIVE );
	cl_voipLipFlapRate = Cvar_Get( "cl_voipLipFlapRate", "12", CVAR_ARCHIVE );
	cl_voipLipFlapFacs = Cvar_Get( "cl_voipLipFlapFacs", "1", CVAR_ARCHIVE );

	Cvar_SetDescription( cl_voip, "Enable VoIP proximity voice chat (0 = off, 1 = on). Requires OpenAL capture." );
	Cvar_SetDescription( cl_voipLipFlap, "Drive head-model jaw/mouth morphs from VoIP voice power." );
	Cvar_SetDescription( cl_voipLipFlapScale, "Multiply VoIP RMS into morph weight (clamped to 1)." );
	Cvar_SetDescription( cl_voipLipFlapThresh, "Minimum VoIP RMS before lip flap applies." );
	Cvar_SetDescription( cl_voipLipFlapDecay, "Per-frame power decay after speech stops (0.1-0.99)." );
	Cvar_SetDescription( cl_voipLipFlapMatch, "Max distance (world units) to match a refEntity to a talking player." );
	Cvar_SetDescription( cl_voipLipFlapMorph, "Comma-separated morph target names to drive (IQM/glTF)." );
	Cvar_SetDescription( cl_voipLipFlapRate, "Lip flap oscillation rate in Hz while talking." );
	Cvar_SetDescription( cl_voipLipFlapFacs, "Drive FACS AU25/AU26 from VoIP RMS (see docs/FACS.md)." );

	Cmd_AddCommand( "+voip", CL_VoIP_Transmit_f );
	Cmd_AddCommand( "-voip", CL_VoIP_StopTransmit_f );
	Cmd_AddCommand( "+voiprecord", CL_VoIP_Transmit_f );
	Cmd_AddCommand( "-voiprecord", CL_VoIP_StopTransmit_f );
	Cmd_AddCommand( "voipstatus", CL_VoIP_Status_f );
	Cmd_AddCommand( "voiptest", CL_VoIP_Test_f );

	Com_Memset( voipClientPower, 0, sizeof( voipClientPower ) );
	Com_Memset( voipClientLastTalk, 0, sizeof( voipClientLastTalk ) );
	voipPendingLen = 0;

	Com_Printf( "VoIP lip flap: %s (morph %s, FACS %s)\n",
		cl_voipLipFlap->integer ? "enabled" : "disabled",
		cl_voipLipFlapMorph->string,
		cl_voipLipFlapFacs->integer ? "AU25/AU26" : "off" );

	if ( !cl_voip->integer ) {
		Com_Printf( "VoIP: disabled (cl_voip 0)\n" );
		return;
	}

	voipEncoder = opus_encoder_create( VOIP_SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &err );
	if ( err != OPUS_OK || !voipEncoder ) {
		Com_Printf( S_COLOR_RED "VoIP: failed to create Opus encoder (%s)\n", opus_strerror( err ) );
		return;
	}

	opus_encoder_ctl( voipEncoder, OPUS_SET_BITRATE( 24000 ) );
	opus_encoder_ctl( voipEncoder, OPUS_SET_SIGNAL( OPUS_SIGNAL_VOICE ) );
	opus_encoder_ctl( voipEncoder, OPUS_SET_COMPLEXITY( 5 ) );

	Com_Memset( voipDecoders, 0, sizeof( voipDecoders ) );

	voipInitialized = qtrue;
	Com_Printf( "VoIP: initialized (Opus %s, %d Hz, proximity chat)\n", opus_get_version_string(), VOIP_SAMPLE_RATE );
}

void CL_VoIP_Shutdown( void ) {
	int i;

	Cmd_RemoveCommand( "+voip" );
	Cmd_RemoveCommand( "-voip" );
	Cmd_RemoveCommand( "+voiprecord" );
	Cmd_RemoveCommand( "-voiprecord" );
	Cmd_RemoveCommand( "voipstatus" );
	Cmd_RemoveCommand( "voiptest" );

	if ( voipCapturing ) {
		SNDDMA_StopCapture();
		voipCapturing = qfalse;
	}

	if ( voipEncoder ) {
		opus_encoder_destroy( voipEncoder );
		voipEncoder = NULL;
	}

	for ( i = 0; i < VOIP_MAX_CLIENTS; i++ ) {
		if ( voipDecoders[i] ) {
			opus_decoder_destroy( voipDecoders[i] );
			voipDecoders[i] = NULL;
		}
	}

	voipInitialized = qfalse;
	voipPendingLen = 0;
	Com_Memset( voipClientPower, 0, sizeof( voipClientPower ) );
}

void CL_VoIP_Transmit( int mode ) {
	if ( !voipInitialized ) return;

	(void)mode;

	if ( !voipCapturing ) {
		SNDDMA_StartCapture();
		voipCapturing = qtrue;
	}

	Cvar_Set( "cl_voipSend", "1" );
}

void CL_VoIP_StopTransmit( void ) {
	if ( !voipInitialized ) return;

	if ( voipCapturing ) {
		SNDDMA_StopCapture();
		voipCapturing = qfalse;
	}

	Cvar_Set( "cl_voipSend", "0" );
}

float CL_VoIP_GetPower( void ) {
	return voipInitialized ? voipPower : 0.0f;
}

float CL_VoIP_GetClientPower( int clientNum ) {
	if ( clientNum < 0 || clientNum >= VOIP_MAX_CLIENTS ) {
		return 0.0f;
	}
	return voipClientPower[clientNum];
}

int CL_VoIP_IsEnabled( void ) {
	return cl_voip && cl_voip->integer;
}

int CL_VoIP_IsSending( void ) {
	return cl_voipSend && cl_voipSend->integer;
}

int CL_VoIP_GetShowMeter( void ) {
	return cl_voipShowMeter && cl_voipShowMeter->integer;
}

void CL_VoIP_Frame( void ) {
	CL_VoIP_DecayClientPower();
	CL_VoIP_SyncFacs();

	/* Late-enable if cl_voip was turned on after init (e.g. autoexec). */
	if ( !voipInitialized && cl_voip && cl_voip->integer ) {
		int err;
		voipEncoder = opus_encoder_create( VOIP_SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &err );
		if ( err == OPUS_OK && voipEncoder ) {
			opus_encoder_ctl( voipEncoder, OPUS_SET_BITRATE( 24000 ) );
			opus_encoder_ctl( voipEncoder, OPUS_SET_SIGNAL( OPUS_SIGNAL_VOICE ) );
			opus_encoder_ctl( voipEncoder, OPUS_SET_COMPLEXITY( 5 ) );
			Com_Memset( voipDecoders, 0, sizeof( voipDecoders ) );
			voipInitialized = qtrue;
			Com_Printf( "VoIP: late-initialized (Opus proximity chat)\n" );
		}
	}

	if ( !voipInitialized || !voipCapturing ) return;
	if ( !cl_voip || !cl_voip->integer ) return;

	while ( SNDDMA_AvailableCaptureSamples() >= VOIP_FRAME_SAMPLES ) {
		int encodedLen;
		float rms = 0.0f;
		int i;

		SNDDMA_Capture( VOIP_FRAME_SAMPLES, (byte *)captureBuffer );

		for ( i = 0; i < VOIP_FRAME_SAMPLES; i++ ) {
			float s = (float)captureBuffer[i] / 32768.0f;
			rms += s * s;
		}
		voipPower = sqrtf( rms / VOIP_FRAME_SAMPLES );

		if ( voipTestUntil > 0 && voipPower > voipTestPeak ) {
			voipTestPeak = voipPower;
		}

		if ( cl_voipSend && cl_voipSend->integer && clc.clientNum >= 0 && clc.clientNum < VOIP_MAX_CLIENTS ) {
			CL_VoIP_SetClientPower( clc.clientNum, voipPower );
		}

		/* Mic self-test listens without sending. */
		if ( voipTestUntil > 0 && !( cl_voipSend && cl_voipSend->integer ) ) {
			continue;
		}

		/* Only encode while push-to-talk is held. */
		if ( !cl_voipSend || !cl_voipSend->integer ) {
			continue;
		}

		encodedLen = opus_encode( voipEncoder, captureBuffer, VOIP_FRAME_SAMPLES,
			encodedBuffer, VOIP_MAX_PACKET );

		if ( encodedLen > 0 && cls.state == CA_ACTIVE ) {
			uint32_t seq = voipOutSequence++;
			/* Opaque blob: sequence (le32) + opus payload — server relays as-is. */
			voipPendingData[0] = (byte)( seq & 0xff );
			voipPendingData[1] = (byte)( ( seq >> 8 ) & 0xff );
			voipPendingData[2] = (byte)( ( seq >> 16 ) & 0xff );
			voipPendingData[3] = (byte)( ( seq >> 24 ) & 0xff );
			Com_Memcpy( voipPendingData + 4, encodedBuffer, encodedLen );
			voipPendingLen = 4 + encodedLen;
		}
	}

	if ( voipTestUntil > 0 && cls.realtime >= voipTestUntil ) {
		Com_Printf( "VoIP test: peak level %.3f%s\n", voipTestPeak,
			voipTestPeak < 0.005f
				? " — almost silence (wrong device, muted, or no mic)"
				: ( voipTestPeak < 0.02f ? " — quiet (ok if speaking softly)" : " — mic is picking up audio" ) );
		voipTestUntil = 0;
		if ( voipTestOwnedCapture && !( cl_voipSend && cl_voipSend->integer ) ) {
			SNDDMA_StopCapture();
			voipCapturing = qfalse;
			voipTestOwnedCapture = qfalse;
		}
	}
}

/*
================
CL_VoIP_WritePacket
Write pending Opus voice into the client->server packet (before clc_EOF).
================
*/
void CL_VoIP_WritePacket( msg_t *msg ) {
	if ( !msg || voipPendingLen <= 0 ) {
		return;
	}
	if ( !voipInitialized || !cl_voip || !cl_voip->integer ) {
		voipPendingLen = 0;
		return;
	}
	if ( msg->cursize + 4 + voipPendingLen >= msg->maxsize ) {
		return;
	}

	MSG_WriteByte( msg, clc_voipOpus );
	MSG_WriteShort( msg, voipPendingLen );
	MSG_WriteData( msg, voipPendingData, voipPendingLen );
	voipPendingLen = 0;
}

void CL_VoIP_ParsePacket( int sender, const byte *data, int len ) {
	int err;
	int16_t pcmBuffer[VOIP_FRAME_SAMPLES];
	int decodedSamples;
	uint32_t sequence;

	if ( !voipInitialized || sender < 0 || sender >= VOIP_MAX_CLIENTS ) return;

	if ( len < 4 ) return;
	sequence = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
		((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
	data += 4;
	len -= 4;

	if ( sequence < voipInSequence[sender] ) {
		Com_DPrintf( "VoIP: dropped out-of-order packet from %d (seq %u < %u)\n",
			sender, sequence, voipInSequence[sender] );
		return;
	}
	voipInSequence[sender] = sequence + 1;

	if ( !voipDecoders[sender] ) {
		voipDecoders[sender] = opus_decoder_create( VOIP_SAMPLE_RATE, 1, &err );
		if ( err != OPUS_OK || !voipDecoders[sender] ) {
			Com_DPrintf( "VoIP: failed to create decoder for client %d\n", sender );
			return;
		}
	}

	decodedSamples = opus_decode( voipDecoders[sender], data, len,
		pcmBuffer, VOIP_FRAME_SAMPLES, 0 );

	if ( decodedSamples > 0 ) {
		vec3_t origin;
		int i;
		qboolean haveOrigin = qfalse;
		float rms = 0.0f;

		for ( i = 0; i < decodedSamples; i++ ) {
			float s = (float)pcmBuffer[i] / 32768.0f;
			rms += s * s;
		}
		CL_VoIP_SetClientPower( sender, sqrtf( rms / (float)decodedSamples ) );

		VectorClear( origin );
		if ( cls.state == CA_ACTIVE && cl.snap.valid ) {
			for ( i = 0; i < cl.snap.numEntities; i++ ) {
				const entityState_t *es = &cl.parseEntities[
					( cl.snap.parseEntitiesNum + i ) & ( MAX_PARSE_ENTITIES - 1 ) ];
				if ( es->number == sender ) {
					VectorCopy( es->origin, origin );
					haveOrigin = qtrue;
					break;
				}
			}
		}
		/* Server already proximity-filtered; OpenAL spatialize when we have a world origin. */
		S_VoipSamples( sender, origin, decodedSamples, VOIP_SAMPLE_RATE, 2, 1, (byte *)pcmBuffer, 1.0f );
		(void)haveOrigin;
	}
}

#else /* !USE_OPUS */

static void CL_VoIP_Transmit_f( void ) {
	CL_VoIP_Transmit( 0 );
}

static void CL_VoIP_StopTransmit_f( void ) {
	CL_VoIP_StopTransmit();
}

void CL_VoIP_Init( void ) {
	Cvar_Get( "cl_voipLipFlap", "0", CVAR_ARCHIVE );
	Cmd_AddCommand( "+voip", CL_VoIP_Transmit_f );
	Cmd_AddCommand( "-voip", CL_VoIP_StopTransmit_f );
	Com_Printf( "VoIP: not available (compiled without USE_OPUS)\n" );
}
void CL_VoIP_Shutdown( void ) {
	Cmd_RemoveCommand( "+voip" );
	Cmd_RemoveCommand( "-voip" );
}
void CL_VoIP_Frame( void ) {}
void CL_VoIP_WritePacket( msg_t *msg ) { (void)msg; }
void CL_VoIP_Transmit( int mode ) { (void)mode; }
void CL_VoIP_ParsePacket( int sender, const byte *data, int len ) { (void)sender; (void)data; (void)len; }
void CL_VoIP_StopTransmit( void ) {}
float CL_VoIP_GetPower( void ) { return 0.0f; }
float CL_VoIP_GetClientPower( int clientNum ) { (void)clientNum; return 0.0f; }
int CL_VoIP_IsEnabled( void ) { return 0; }
int CL_VoIP_IsSending( void ) { return 0; }
int CL_VoIP_GetShowMeter( void ) { return 0; }
void CL_VoIP_ApplyLipFlap( refEntity_t *ent ) { (void)ent; }

#endif /* USE_OPUS */
