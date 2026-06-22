#ifndef CSHARP_DEBUG_H
#define CSHARP_DEBUG_H

void Cmd_CsReload_f( void );
void Cmd_CsList_f( void );
void Cmd_CsDump_f( void );
void CsDebug_InitCvars( void );
void CsDebug_Frame( int msec, int realMsec );
void CsDebug_EmitEvent( const char *eventName, const char *s0, const char *s1, int i0, int i1 );

#endif
