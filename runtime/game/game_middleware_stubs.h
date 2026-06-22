#ifndef GAME_MIDDLEWARE_STUBS_H
#define GAME_MIDDLEWARE_STUBS_H

#ifndef USE_GAME_AI_MIDDLEWARE
void GameMiddleware_LogDisabled( void );
#else
static inline void GameMiddleware_LogDisabled( void ) {}
#endif

#endif
