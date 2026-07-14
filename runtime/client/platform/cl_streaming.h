/*
===========================================================================
Optional live-streaming controls for RTMP-compatible idTech3-tv / Owncast.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void CL_Streaming_Init( void );
void CL_Streaming_Shutdown( void );
qboolean CL_Streaming_EngineCaptureActive( void );
int CL_Streaming_EngineCaptureFPS( void );

#ifdef __cplusplus
}
#endif
