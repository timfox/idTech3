/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

VoIP implementation using Opus codec.

Pipeline:
  Capture (SDL/OpenAL) -> Opus encode -> MSG_Write -> Network
  -> MSG_Read -> Opus decode -> S_VoipSamples playback
===========================================================================
*/

#include "client.h"
#include "cl_voip.h"

#ifdef USE_OPUS
#include <opus.h>

static OpusEncoder  *voipEncoder = NULL;
static OpusDecoder  *voipDecoders[VOIP_MAX_CLIENTS];

static cvar_t *cl_voip;
static cvar_t *cl_voipSend;
static cvar_t *cl_voipGainDuringCapture;
static cvar_t *cl_voipShowMeter;

static qboolean voipInitialized = qfalse;
static qboolean voipCapturing = qfalse;
static float    voipPower = 0.0f;

static int16_t  captureBuffer[VOIP_FRAME_SAMPLES * 4];
static byte     encodedBuffer[VOIP_MAX_PACKET];

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

void CL_VoIP_Init( void ) {
	int err;

	cl_voip     = Cvar_Get( "cl_voip", "0", CVAR_ARCHIVE );
	cl_voipSend = Cvar_Get( "cl_voipSend", "0", 0 );
	cl_voipGainDuringCapture = Cvar_Get( "cl_voipGainDuringCapture", "0.2", CVAR_ARCHIVE );
	cl_voipShowMeter = Cvar_Get( "cl_voipShowMeter", "1", CVAR_ARCHIVE );

	Cvar_SetDescription( cl_voip, "Enable VoIP voice chat (0 = off, 1 = on)." );

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
	Com_Printf( "VoIP: initialized (Opus %s, %d Hz)\n", opus_get_version_string(), VOIP_SAMPLE_RATE );
}

void CL_VoIP_Shutdown( void ) {
	int i;

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

void CL_VoIP_Frame( void ) {
	if ( !voipInitialized || !voipCapturing ) return;

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

		encodedLen = opus_encode( voipEncoder, captureBuffer, VOIP_FRAME_SAMPLES,
			encodedBuffer, VOIP_MAX_PACKET );

		if ( encodedLen > 0 && cls.state == CA_ACTIVE ) {
			msg_t buf;
			byte msgData[VOIP_MAX_PACKET + 16];

			MSG_Init( &buf, msgData, sizeof( msgData ) );
			MSG_WriteByte( &buf, clc_voipOpus );
			MSG_WriteShort( &buf, (int)encodedLen );
			MSG_WriteData( &buf, encodedBuffer, encodedLen );

			CL_AddReliableCommand( (const char *)msgData, qfalse );
		}
	}
}

void CL_VoIP_ParsePacket( int sender, const byte *data, int len ) {
	int err;
	int16_t pcmBuffer[VOIP_FRAME_SAMPLES];
	int decodedSamples;

	if ( !voipInitialized || sender < 0 || sender >= VOIP_MAX_CLIENTS ) return;

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
		vec3_t origin = { 0, 0, 0 };
		S_VoipSamples( sender, origin, decodedSamples, VOIP_SAMPLE_RATE, 2, 1, (byte *)pcmBuffer, 1.0f );
	}
}

#else /* !USE_OPUS */

void CL_VoIP_Init( void ) { Com_Printf( "VoIP: not available (compiled without USE_OPUS)\n" ); }
void CL_VoIP_Shutdown( void ) {}
void CL_VoIP_Frame( void ) {}
void CL_VoIP_Transmit( int mode ) { (void)mode; }
void CL_VoIP_ParsePacket( int sender, const byte *data, int len ) { (void)sender; (void)data; (void)len; }
void CL_VoIP_StopTransmit( void ) {}

#endif /* USE_OPUS */
