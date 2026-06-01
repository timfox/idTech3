/*
===========================================================================
SEGA (Spectral-Energy Guided Attention) — external FLUX hi-res generation hook.
See docs/SEGA.md and external/sega/README.md.
===========================================================================
*/

#ifndef CL_SEGA_H
#define CL_SEGA_H

void CL_SegaInit( void );
void CL_SegaShutdown( void );
void CL_SegaFrame( void );
void CL_SegaGenerate_f( void );
void CL_SegaStatus_f( void );
void CL_SegaCancel_f( void );
void CL_SegaView_f( void );

#endif /* CL_SEGA_H */
