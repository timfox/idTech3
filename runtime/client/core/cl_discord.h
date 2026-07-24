#ifndef CL_DISCORD_H
#define CL_DISCORD_H

#ifdef __EMSCRIPTEN__

static ID_INLINE void CL_Discord_Init( void ) {}
static ID_INLINE void CL_Discord_Frame( void ) {}
static ID_INLINE void CL_Discord_Shutdown( void ) {}

#else

void CL_Discord_Init( void );
void CL_Discord_Frame( void );
void CL_Discord_Shutdown( void );

typedef struct discordConn_s discordConn_t;

discordConn_t *Sys_DiscordConnect( void );
int  Sys_DiscordRead( discordConn_t *c, void *buf, int len );
int  Sys_DiscordWrite( discordConn_t *c, const void *buf, int len );
void Sys_DiscordClose( discordConn_t *c );

#endif

#endif
