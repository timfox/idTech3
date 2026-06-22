/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

VoIP pipeline: microphone capture -> Opus encode -> network -> decode -> playback.
===========================================================================
*/

#ifndef CL_VOIP_H
#define CL_VOIP_H

#include "../../qcommon/q_shared.h"

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
int     CL_VoIP_IsEnabled( void );
int     CL_VoIP_IsSending( void );
int     CL_VoIP_GetShowMeter( void );

#endif /* CL_VOIP_H */
