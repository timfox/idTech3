#ifndef ENGINE_DB_H
#define ENGINE_DB_H

#include "q_shared.h"

void     EngineDB_Init( void );
void     EngineDB_Shutdown( void );
qboolean EngineDB_IsAvailable( void );
const char *EngineDB_GetPath( void );

qboolean EngineDB_Exec( const char *sql );
qboolean EngineDB_QueryOne( const char *sql, char *out, int outSize );

qboolean EngineDB_ProfileSet( const char *key, const char *value );
qboolean EngineDB_ProfileGet( const char *key, char *out, int outSize );
qboolean EngineDB_ProfileDelete( const char *key );

qboolean EngineDB_SaveWriteSlot( int slot, const char *label, int protocolVersion, const char *modVersion, unsigned checksum );
qboolean EngineDB_SaveReadSlot( int slot, char *labelOut, int labelLen, int *protoOut );

#endif
