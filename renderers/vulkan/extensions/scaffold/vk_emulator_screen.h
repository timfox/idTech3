#pragma once

void R_Emulator_Init( void );
void R_Emulator_Shutdown( void );
void RE_EmulatorUploadFrame( const byte *rgba, int width, int height );
