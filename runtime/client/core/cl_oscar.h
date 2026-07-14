#ifndef CL_OSCAR_H
#define CL_OSCAR_H

struct lua_State;

void CL_Oscar_Init( void );
void CL_Oscar_Shutdown( void );
void CL_Oscar_Frame( void );
void CL_Oscar_RegisterLua( struct lua_State *L );

#endif /* CL_OSCAR_H */
