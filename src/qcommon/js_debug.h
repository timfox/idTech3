#ifndef JS_DEBUG_H
#define JS_DEBUG_H

void Cmd_JsReload_f( void );
void Cmd_JsList_f( void );
void Cmd_JsDump_f( void );
void Cmd_JsExec_f( void );
void Cmd_JsClearErrors_f( void );
void JsDebug_InitCvars( void );
void JsDebug_Frame( int msec, int realMsec );
void JsDebug_EmitEvent( const char *eventName, const char *s0, const char *s1, int i0, int i1 );
void JsDebug_SetCurrentMenu( int menu );

#endif
