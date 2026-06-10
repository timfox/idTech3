/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Fog bioaerosol ecology — Evans et al. 2019 (Sci Total Environ 647) drivers:
marine vs soil sources, fog deposition, pre/post-fog community shifts.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

typedef enum {
	FOG_BIO_SITE_MAINE = 0,
	FOG_BIO_SITE_NAMIB = 1
} fogBioSite_t;

typedef enum {
	FOG_BIO_PHYLUM_PROTEOBACTERIA = 0,
	FOG_BIO_PHYLUM_BACTEROIDETES,
	FOG_BIO_PHYLUM_ACTINOBACTERIA,
	FOG_BIO_PHYLUM_FIRMICUTES,
	FOG_BIO_PHYLUM_CYANOBACTERIA,
	FOG_BIO_PHYLUM_ASCOMYCOTA,
	FOG_BIO_PHYLUM_COUNT
} fogBioPhylum_t;

typedef enum {
	FOG_BIO_PHASE_CLEAR = 0,
	FOG_BIO_PHASE_FOG,
	FOG_BIO_PHASE_POST_FOG
} fogBioPhase_t;

typedef struct fogBioCommunity_s {
	float phylum[FOG_BIO_PHYLUM_COUNT];
	float shannonDiversity;
	float marineFraction;
	float oceanOtuFraction;
	float depositionMultiplier;
	float culturableRichness;
	float gramNegativeFraction;
	float rhodospirillalesFraction;
	float pathogenTaxaScore;
} fogBioCommunity_t;

void     FogBiology_Init( void );
void     FogBiology_Shutdown( void );

qboolean FogBiology_Enabled( void );
void     FogBiology_SetSite( fogBioSite_t site );
fogBioSite_t FogBiology_GetSite( void );

void     FogBiology_SetCoastDistanceKm( float km );
void     FogBiology_SetMarineWind( float wind01 );
void     FogBiology_SetFogActive( qboolean active );
void     FogBiology_SetPlayerOrigin( const vec3_t origin );

void     FogBiology_Frame( void );
fogBioPhase_t FogBiology_GetPhase( void );

float    FogBiology_GetCoastDistanceKm( void );
float    FogBiology_GetMarineInfluence( void );
float    FogBiology_GetPathogenDepositionRisk( void );
void     FogBiology_GetCommunity( fogBioPhase_t phase, fogBioCommunity_t *out );
void     FogBiology_GetCurrentCommunity( fogBioCommunity_t *out );

const char *FogBiology_PhylumName( fogBioPhylum_t phylum );
const char *FogBiology_PhaseName( fogBioPhase_t phase );

void     FogBiology_Status_f( void );
void     FogBiology_Compare_f( void );
void     FogBiology_Poll_f( void );
void     FogBiology_Sweep_f( void );
void     FogBiology_Genera_f( void );
void     FogBiology_Paper_f( void );

#ifdef FOG_BIOLOGY_UNIT_TEST
void     FogBiology_ResetForTest( void );
float    FogBiology_ComputeMarineInfluenceForTest( fogBioSite_t site, float coastKm, float wind01 );
void     FogBiology_BuildCommunityForTest( fogBioSite_t site, fogBioPhase_t phase,
	float marineInfluence, fogBioCommunity_t *out );
#endif

#ifdef __cplusplus
}
#endif
