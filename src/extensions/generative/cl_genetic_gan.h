#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef USE_GENETIC_GAN
void CL_GeneticGan_Init( void );
void CL_GeneticGan_Shutdown( void );
void CL_GeneticGan_Frame( void );
#else
static inline void CL_GeneticGan_Init( void ) {}
static inline void CL_GeneticGan_Shutdown( void ) {}
static inline void CL_GeneticGan_Frame( void ) {}
#endif

#ifdef __cplusplus
}
#endif
