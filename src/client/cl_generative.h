/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if defined( USE_TRELLIS ) || defined( USE_GENETIC_GAN ) || defined( USE_FLUX )
void CL_GenerativeFrame( void );
#else
static inline void CL_GenerativeFrame( void ) {}
#endif

#ifdef __cplusplus
}
#endif
