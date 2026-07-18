/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

VoIP pipeline: microphone capture -> Opus encode -> network -> decode -> playback.
Optional head-model lip flap driven by per-client voice power.
===========================================================================
*/

#ifndef CL_VOIP_H
#define CL_VOIP_H

#include "q_shared.h"
#include "../../../renderers/common/tr_types.h"

#define VOIP_SAMPLE_RATE     48000
#define VOIP_FRAME_SAMPLES   960
#define VOIP_MAX_PACKET      4000
#define VOIP_MAX_CLIENTS     64

void    CL_VoIP_Init( void );
void    CL_VoIP_Shutdown( void );
void    CL_VoIP_Frame( void );
void    CL_VoIP_Transmit( int mode );
void    CL_VoIP_ParsePacket( int sender, const byte *data, int len );
void    CL_VoIP_StopTransmit( void );
float   CL_VoIP_GetPower( void );
float   CL_VoIP_GetClientPower( int clientNum );
int     CL_VoIP_IsEnabled( void );
int     CL_VoIP_IsSending( void );
int     CL_VoIP_GetShowMeter( void );

/* Drive jaw/mouth morph weights on player head models near talking clients. */
void    CL_VoIP_ApplyLipFlap( refEntity_t *ent );

/* Append pending Opus frame to an outbound client packet (before clc_EOF). */
void    CL_VoIP_WritePacket( msg_t *msg );

#endif /* CL_VOIP_H */
