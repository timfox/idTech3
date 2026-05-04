/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client demo record/playback: extracted from cl_main.c for modularization.
===========================================================================
*/
#pragma once

void CL_Demo_InitCommands( void );
void CL_Demo_ShutdownCommands( void );
/* Called from CL_PacketEvent when recording a live connection to a .dm_* file. */
void CL_Demo_WriteServerPacket( msg_t *msg, int headerBytes );
