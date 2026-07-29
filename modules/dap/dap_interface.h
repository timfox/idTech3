/*
===========================================================================
Native Debug Adapter Protocol transport for id Tech 3.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void DAP_Init( void );
void DAP_Shutdown( void );
void DAP_Frame( void );
int DAP_IsRunning( void );
int DAP_BuildProtocolMessage( const char *json, char *out, int outSize );
int DAP_HandleJsonForTest( const char *json, char *out, int outSize );
int DAP_HandleJsonWithTokenForTest( const char *json, const char *token, char *out, int outSize );
int DAP_IsAddressAllowedForTest( const char *address, int allowRemote, const char *token );

#ifdef __cplusplus
}
#endif
