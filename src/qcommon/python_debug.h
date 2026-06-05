#ifndef PYTHON_DEBUG_H
#define PYTHON_DEBUG_H

void PyDebug_InitCvars( void );
void Cmd_PyReload_f( void );
void Cmd_PyList_f( void );
void Cmd_PyDump_f( void );
void Cmd_PyExec_f( void );

void PyDebug_Frame( int msec, int realMsec );
void PyDebug_EmitEvent( const char *eventName, const char *s0, const char *s1, int i0, int i1 );

#endif
