#ifndef G_ENGINE_SYSTEMS_H
#define G_ENGINE_SYSTEMS_H

#include "q_shared.h"

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

#define ENGINE_SAVE_PROTOCOL_VERSION 1

void    EngineSave_Init( void );
void    EngineSave_Shutdown( void );
qboolean EngineSave_WriteSlot( int slot, const char *label );
qboolean EngineSave_ReadSlot( int slot, char *labelOut, int labelLen );
int     EngineSave_LastSlot( void );
int     EngineSave_ProtocolVersion( void );

qboolean EngineProfile_Set( const char *key, const char *value );
qboolean EngineProfile_Get( const char *key, char *out, int outSize );
qboolean EngineProfile_Delete( const char *key );
qboolean EngineDatabase_IsAvailable( void );
const char *EngineDatabase_GetPath( void );
qboolean EngineDatabase_Exec( const char *sql );
qboolean EngineDatabase_QueryOne( const char *sql, char *out, int outSize );

void    EngineQuest_Init( void );
void    EngineQuest_Shutdown( void );
int     EngineQuest_Add( const char *id, const char *title, const char *stage );
qboolean EngineQuest_SetStage( const char *id, const char *stage );
const char *EngineQuest_GetStage( const char *id );
int     EngineQuest_Count( void );

void    EngineDialogue_Init( void );
void    EngineDialogue_Shutdown( void );
int     EngineDialogue_Start( const char *speaker, const char *text );
int     EngineDialogue_StartEx( const char *speaker, const char *text, const char *locKey,
	const char *voice, float duration );
qboolean EngineDialogue_AddChoice( int lineIndex, const char *label, const char *nextId );
qboolean EngineDialogue_Get( int index, char *speakerOut, int speakerSize,
	char *textOut, int textSize, char *locKeyOut, int locKeySize,
	float *durationOut, int *choiceCountOut );
qboolean EngineDialogue_GetChoice( int lineIndex, int choiceIndex,
	char *labelOut, int labelSize, char *nextOut, int nextSize );
void    EngineDialogue_Clear( void );
int     EngineDialogue_ActiveCount( void );

#endif
