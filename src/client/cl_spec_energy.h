/*
===========================================================================
Spectral-Energy Guided Attention — external FLUX hi-res generation hook.
See docs/SPEC_ENERGY.md and external/flux_spec_energy/README.md.
===========================================================================
*/

#ifndef CL_SPEC_ENERGY_H
#define CL_SPEC_ENERGY_H

void CL_SpecEnergyInit( void );
void CL_SpecEnergyShutdown( void );
void CL_SpecEnergyFrame( void );
void CL_SpecEnergyGenerate_f( void );
void CL_SpecEnergyStatus_f( void );
void CL_SpecEnergyCancel_f( void );
void CL_SpecEnergyView_f( void );

#endif /* CL_SPEC_ENERGY_H */
