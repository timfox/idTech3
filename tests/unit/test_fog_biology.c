/*
 * Unit tests: fog bioaerosol ecology (Evans et al. 2019 drivers).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "world/fog_biology.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_marine_coast_decay( void )
{
	float nearCoast, inland;

	nearCoast = FogBiology_ComputeMarineInfluenceForTest( FOG_BIO_SITE_MAINE, 0.01f, 0.9f );
	inland = FogBiology_ComputeMarineInfluenceForTest( FOG_BIO_SITE_NAMIB, 50.0f, 0.7f );
	ASSERT( nearCoast > inland, "coastal marine influence exceeds inland Namib" );
	ASSERT( nearCoast > 0.5f, "Maine near-coast marine influence significant" );
	ASSERT( inland > 0.05f, "Namib 50km still shows marine signature" );
	return 0;
}

static int test_fog_deposition_diversity( void )
{
	fogBioCommunity_t clearComm, fogComm;

	FogBiology_BuildCommunityForTest( FOG_BIO_SITE_MAINE, FOG_BIO_PHASE_CLEAR, 0.6f, &clearComm );
	FogBiology_BuildCommunityForTest( FOG_BIO_SITE_MAINE, FOG_BIO_PHASE_FOG, 0.6f, &fogComm );

	ASSERT( fogComm.shannonDiversity > clearComm.shannonDiversity, "fog alpha diversity > clear" );
	ASSERT( fogComm.depositionMultiplier > clearComm.depositionMultiplier * 2.0f,
		"fog deposition ~3x clear (culturable proxy)" );
	ASSERT( fogComm.culturableRichness >= 2.0f, "fog culturable richness ~2x" );
	return 0;
}

static int test_site_phase_deltas( void )
{
	fogBioCommunity_t maineFog, namibFog, maineClear;

	FogBiology_BuildCommunityForTest( FOG_BIO_SITE_MAINE, FOG_BIO_PHASE_FOG, 0.7f, &maineFog );
	FogBiology_BuildCommunityForTest( FOG_BIO_SITE_MAINE, FOG_BIO_PHASE_CLEAR, 0.7f, &maineClear );
	FogBiology_BuildCommunityForTest( FOG_BIO_SITE_NAMIB, FOG_BIO_PHASE_FOG, 0.35f, &namibFog );

	ASSERT( maineFog.phylum[FOG_BIO_PHYLUM_BACTEROIDETES] <
		maineClear.phylum[FOG_BIO_PHYLUM_BACTEROIDETES],
		"Maine fog lowers Bacteroidetes vs clear" );
	ASSERT( namibFog.phylum[FOG_BIO_PHYLUM_ASCOMYCOTA] > 0.10f,
		"Namib fog enriches Ascomycota fungi" );
	return 0;
}

static int test_phase_transition( void )
{
	FogBiology_ResetForTest();
	FogBiology_SetFogActive( qtrue );
	FogBiology_Frame();
	ASSERT( FogBiology_GetPhase() == FOG_BIO_PHASE_FOG, "active fog phase" );

	FogBiology_SetFogActive( qfalse );
	FogBiology_Frame();
	ASSERT( FogBiology_GetPhase() == FOG_BIO_PHASE_POST_FOG, "post-fog after clearing" );
	return 0;
}

static int test_pathogen_risk( void )
{
	float fogRisk;

	FogBiology_ResetForTest();
	FogBiology_SetFogActive( qtrue );
	FogBiology_Frame();
	fogRisk = FogBiology_GetPathogenDepositionRisk();
	ASSERT( fogRisk > 0.25f, "fog pathogen risk elevated" );
	ASSERT( fogRisk <= 1.0f, "pathogen risk clamped" );
	return 0;
}

static int test_coast_auto( void )
{
	vec3_t origin;

	FogBiology_ResetForTest();
	Cvar_SetValue( "r_fogBiologyCoastAuto", 1.0f );
	Cvar_SetValue( "r_fogBiologyCoastOrigin", 0.0f );
	Cvar_SetValue( "r_fogBiologyCoastUnitsPerKm", 512.0f );
	Cvar_SetValue( "r_fogBiologyCoastAxis", 0.0f );

	origin[0] = 25600.0f;
	origin[1] = 0.0f;
	origin[2] = 0.0f;
	FogBiology_SetPlayerOrigin( origin );
	FogBiology_Frame();

	ASSERT( Cvar_VariableValue( "r_fogBiologyCoastKm" ) > 49.0f
		&& Cvar_VariableValue( "r_fogBiologyCoastKm" ) < 51.0f,
		"coast auto derives ~50 km from player X" );
	return 0;
}

static int test_paper_gram_rhodo( void )
{
	fogBioCommunity_t clearComm, fogComm;

	FogBiology_BuildCommunityForTest( FOG_BIO_SITE_MAINE, FOG_BIO_PHASE_CLEAR, 0.7f, &clearComm );
	FogBiology_BuildCommunityForTest( FOG_BIO_SITE_MAINE, FOG_BIO_PHASE_FOG, 0.7f, &fogComm );

	ASSERT( fogComm.gramNegativeFraction > 0.40f, "fog gram-negative dominance (paper Sec. 4.1)" );
	ASSERT( fogComm.rhodospirillalesFraction > clearComm.rhodospirillalesFraction,
		"Rhodospirillales enriched in fog vs clear (paper)" );
	ASSERT( fogComm.oceanOtuFraction >= 0.01f && fogComm.oceanOtuFraction <= 0.75f,
		"ocean OTU fraction in paper range 1-75%" );
	ASSERT( fogComm.pathogenTaxaScore > clearComm.pathogenTaxaScore,
		"pathogen taxa score higher in fog" );
	return 0;
}

static int test_sync_cvars( void )
{
	FogBiology_ResetForTest();
	FogBiology_SetFogActive( qtrue );
	FogBiology_Frame();

	ASSERT( !Q_stricmp( Cvar_VariableString( "r_fogBiologySyncPhase" ), "fog" ),
		"sync phase mirrors fog" );
	ASSERT( Cvar_VariableValue( "r_fogBiologySyncMarine" ) > 0.1f,
		"sync marine positive" );
	ASSERT( Cvar_VariableValue( "r_fogBiologySyncDeposition" ) > 1.0f,
		"sync deposition elevated in fog" );
	ASSERT( Cvar_VariableValue( "r_fogBiologySyncPathogen" ) > 0.0f,
		"sync pathogen non-zero" );
	ASSERT( Cvar_VariableValue( "r_fogBiologySyncCoastKm" ) >= 0.0f,
		"sync coast km present" );
	return 0;
}

int main( int argc, char **argv )
{
	(void)argc;
	(void)argv;

	FogBiology_Init();
#ifdef FOG_BIOLOGY_UNIT_TEST
	FogBiology_ResetForTest();
#endif

	if ( test_marine_coast_decay() ) return 1;
	if ( test_fog_deposition_diversity() ) return 1;
	if ( test_site_phase_deltas() ) return 1;
	if ( test_phase_transition() ) return 1;
	if ( test_pathogen_risk() ) return 1;
	if ( test_coast_auto() ) return 1;
	if ( test_paper_gram_rhodo() ) return 1;
	if ( test_sync_cvars() ) return 1;

	printf( "OK: fog_biology unit tests passed\n" );
	return 0;
}
