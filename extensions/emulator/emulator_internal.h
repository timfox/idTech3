/*
 * idTech3 Emulator — internal client/process API.
 */
#pragma once

#include "emulator_types.h"

void Emulator_Process_Init( void );
void Emulator_Process_Shutdown( void );

qboolean Emulator_Process_Start( const char *diskImage );
void Emulator_Process_Stop( void );
void Emulator_Process_GetStatus( emulatorStatus_t *out );

void Emulator_Frame_Init( void );
void Emulator_Frame_Shutdown( void );
void Emulator_Frame_SetSize( int width, int height );
void Emulator_Frame_GetMetrics( int *width, int *height, uint32_t *frameIndex );

/* Returns qtrue when a new RGBA frame was written into rgbaOut (width*height*4). */
qboolean Emulator_Frame_Pump( byte *rgbaOut, int rgbaBytes, int *width, int *height );

qboolean Emulator_Frame_ShmAttached( void );

void Emulator_Input_Init( void );
void Emulator_Input_Shutdown( void );
void Emulator_Input_SetCapture( qboolean capture );
qboolean Emulator_Input_CaptureActive( void );
qboolean Emulator_Input_Push( emulatorInputType_t type, int key, int ascii );
void Emulator_Input_GetStatus( uint32_t *writeIdx, uint32_t *readIdx, uint32_t *dropped );

void Emulator_Console_Register( void );
void Emulator_Console_Unregister( void );
