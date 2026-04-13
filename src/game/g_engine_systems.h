#ifndef G_ENGINE_SYSTEMS_H
#define G_ENGINE_SYSTEMS_H

#include "../qcommon/q_shared.h"

void    EngineTelemetry_Init( void );
void    EngineTelemetry_Shutdown( void );
void    EngineTelemetry_Record( const char *name, double value );
double  EngineTelemetry_Get( const char *name );
void    EngineTelemetry_Clear( void );

void    EngineReplay_Init( void );
void    EngineReplay_Shutdown( void );
void    EngineReplay_BeginFrame( int serverTime );
void    EngineReplay_EndFrame( void );
int     EngineReplay_GetFrameIndex( void );
int     EngineReplay_GetBaseTime( void );

void    EngineSave_Init( void );
void    EngineSave_Shutdown( void );
qboolean EngineSave_WriteSlot( int slot, const char *label );
qboolean EngineSave_ReadSlot( int slot, char *labelOut, int labelLen );
int     EngineSave_LastSlot( void );

void    EngineQuest_Init( void );
void    EngineQuest_Shutdown( void );
int     EngineQuest_Add( const char *id, const char *title, const char *stage );
qboolean EngineQuest_SetStage( const char *id, const char *stage );
const char *EngineQuest_GetStage( const char *id );
int     EngineQuest_Count( void );

void    EngineDialogue_Init( void );
void    EngineDialogue_Shutdown( void );
int     EngineDialogue_Start( const char *speaker, const char *text );
void    EngineDialogue_Clear( void );
int     EngineDialogue_ActiveCount( void );

#endif
