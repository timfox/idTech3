/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client USD scene tools (FreeUSD). See docs/FREEUSD.md.
===========================================================================
*/

#ifndef CL_USD_H
#define CL_USD_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_FREEUSD
void CL_USD_Init( void );
#else
static inline void CL_USD_Init( void ) {}
#endif

#ifdef __cplusplus
}
#endif

#endif /* CL_USD_H */
