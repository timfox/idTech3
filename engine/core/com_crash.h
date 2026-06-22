#ifndef COM_CRASH_H
#define COM_CRASH_H

void Com_Crash_Init( void );
void Com_Crash_OnFatal( const char *reason );
void Com_Crash_OnSignal( int sig );

#endif /* COM_CRASH_H */
