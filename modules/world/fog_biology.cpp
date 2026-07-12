/* C++20 migration: extern "C" API boundary preserved. */
#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>

extern "C" {
#include "q_shared.h"
#include "qcommon.h"
#include "fog_biology.h"
}

static cvar_t *r_fogBiology;
static cvar_t *r_fogBiologySite;
static cvar_t *r_fogBiologyCoastKm;
static cvar_t *r_fogBiologyWindMarine;
static cvar_t *r_fogBiologyAuto;
static cvar_t *r_fogBiologyCoastAuto;
static cvar_t *r_fogBiologyCoastAxis;
static cvar_t *r_fogBiologyCoastOrigin;
static cvar_t *r_fogBiologyCoastUnitsPerKm;

static fogBioSite_t s_site;
static fogBioPhase_t s_phase;
static qboolean s_fogActive;
static qboolean s_loggedEnable;
static qboolean s_hasPlayerOrigin;
static int s_postFogFrames;
static vec3_t s_playerOrigin;
static fogBioCommunity_t s_current;

#define FOG_BIO_POST_FOG_FRAMES 300

[[nodiscard]] static float FB_Clamp01( float v )
{
	return std::clamp( v, 0.0f, 1.0f );
}

[[nodiscard]] static float FB_Shannon( std::span<const float> weights )
{
	float h = 0.0f;
	for ( float w : weights ) {
		if ( w > 1e-6f ) {
			h -= w * std::log( w );
		}
	}
	return h;
}

static void FB_NormalizePhyla( std::span<float> phylum )
{
	float sum = 0.0f;
	const int count = static_cast<int>( phylum.size() );
	for ( int i = 0; i < count; i++ ) {
		if ( phylum[i] < 0.0f ) {
			phylum[i] = 0.0f;
		}
		sum += phylum[i];
	}
	if ( sum <= 1e-6f ) {
		phylum[FOG_BIO_PHYLUM_PROTEOBACTERIA] = 1.0f;
		return;
	}
	for ( int i = 0; i < count; i++ ) {
		phylum[i] /= sum;
	}
}

static float FB_MarineSiteBase( fogBioSite_t site )
{
	return ( site == FOG_BIO_SITE_MAINE ) ? 0.72f : 0.38f;
}

static float FB_ComputeMarineInfluenceInternal( fogBioSite_t site, float coastKm, float wind01 )
{
	float distFalloff = 1.0f / ( 1.0f + coastKm * 0.08f );
	float wind = 0.35f + 0.65f * FB_Clamp01( wind01 );
	return FB_Clamp01( FB_MarineSiteBase( site ) * distFalloff * wind );
}

static void FB_ApplyMarineTaxa( float *phylum, float marineInfluence )
{
	float transfer;

	transfer = 0.18f * marineInfluence;
	phylum[FOG_BIO_PHYLUM_PROTEOBACTERIA] += transfer * 0.55f;
	phylum[FOG_BIO_PHYLUM_CYANOBACTERIA] += transfer * 0.25f;
	phylum[FOG_BIO_PHYLUM_BACTEROIDETES] += transfer * 0.20f;

	phylum[FOG_BIO_PHYLUM_ACTINOBACTERIA] -= transfer * 0.35f;
	phylum[FOG_BIO_PHYLUM_FIRMICUTES] -= transfer * 0.25f;
}

static void FB_ApplyPhaseDeltas( fogBioSite_t site, fogBioPhase_t phase, float *phylum )
{
	if ( phase == FOG_BIO_PHASE_FOG ) {
		if ( site == FOG_BIO_SITE_MAINE ) {
			phylum[FOG_BIO_PHYLUM_BACTEROIDETES] *= 0.56f;
		} else {
			phylum[FOG_BIO_PHYLUM_ASCOMYCOTA] += 0.15f;
		}
		phylum[FOG_BIO_PHYLUM_PROTEOBACTERIA] += 0.04f;
	} else if ( phase == FOG_BIO_PHASE_POST_FOG ) {
		if ( site == FOG_BIO_SITE_MAINE ) {
			phylum[FOG_BIO_PHYLUM_PROTEOBACTERIA] *= 0.92f;
			phylum[FOG_BIO_PHYLUM_BACTEROIDETES] *= 0.88f;
		} else {
			phylum[FOG_BIO_PHYLUM_ASCOMYCOTA] *= 0.70f;
			phylum[FOG_BIO_PHYLUM_PROTEOBACTERIA] *= 0.90f;
		}
	}
}

static void FB_BasePhylumProfile( fogBioSite_t site, fogBioPhase_t phase, float *phylum )
{
	Com_Memset( phylum, 0, sizeof( float ) * FOG_BIO_PHYLUM_COUNT );

	if ( site == FOG_BIO_SITE_MAINE ) {
		phylum[FOG_BIO_PHYLUM_PROTEOBACTERIA] = 0.34f;
		phylum[FOG_BIO_PHYLUM_BACTEROIDETES] = ( phase == FOG_BIO_PHASE_CLEAR ) ? 0.201f : 0.113f;
		phylum[FOG_BIO_PHYLUM_ACTINOBACTERIA] = 0.18f;
		phylum[FOG_BIO_PHYLUM_FIRMICUTES] = 0.12f;
		phylum[FOG_BIO_PHYLUM_CYANOBACTERIA] = 0.03f;
	} else {
		phylum[FOG_BIO_PHYLUM_PROTEOBACTERIA] = 0.31f;
		phylum[FOG_BIO_PHYLUM_BACTEROIDETES] = 0.16f;
		phylum[FOG_BIO_PHYLUM_ACTINOBACTERIA] = 0.17f;
		phylum[FOG_BIO_PHYLUM_FIRMICUTES] = 0.14f;
		phylum[FOG_BIO_PHYLUM_CYANOBACTERIA] = 0.04f;
		phylum[FOG_BIO_PHYLUM_ASCOMYCOTA] = ( phase == FOG_BIO_PHASE_FOG ) ? 0.201f : 0.0047f;
	}
}

static float FB_RhodospirillalesFraction( fogBioSite_t site, fogBioPhase_t phase )
{
	if ( site == FOG_BIO_SITE_MAINE ) {
		return ( phase == FOG_BIO_PHASE_FOG ) ? 0.07f : 0.002f;
	}
	return ( phase == FOG_BIO_PHASE_FOG ) ? 0.04f : 0.02f;
}

static float FB_PathogenTaxaScore( fogBioSite_t site, fogBioPhase_t phase,
	float marineInfluence, const float *phylum, float depositionMult )
{
	float score;

	(void)site;
	score = 0.12f + 0.35f * marineInfluence;
	score += phylum[FOG_BIO_PHYLUM_PROTEOBACTERIA] * 0.25f;
	score += phylum[FOG_BIO_PHYLUM_BACTEROIDETES] * 0.08f;
	if ( phase == FOG_BIO_PHASE_FOG ) {
		score += 0.18f * ( depositionMult / 2.9f );
	} else if ( phase == FOG_BIO_PHASE_POST_FOG ) {
		score += 0.08f;
	}
	return FB_Clamp01( score );
}

static void FB_BuildCommunity( fogBioSite_t site, fogBioPhase_t phase, float marineInfluence,
	fogBioCommunity_t *out )
{
	float phylum[FOG_BIO_PHYLUM_COUNT];

	if ( !out ) {
		return;
	}

	FB_BasePhylumProfile( site, phase, phylum );
	FB_ApplyMarineTaxa( phylum, marineInfluence );
	if ( phase != FOG_BIO_PHASE_CLEAR ) {
		FB_ApplyPhaseDeltas( site, phase, phylum );
	}
	FB_NormalizePhyla( phylum );

	Com_Memcpy( out->phylum, phylum, sizeof( phylum ) );
	out->marineFraction = marineInfluence;
	out->oceanOtuFraction = 0.01f + 0.74f * marineInfluence;
	out->depositionMultiplier = ( phase == FOG_BIO_PHASE_FOG ) ? 2.9f : 1.0f;
	out->culturableRichness = ( phase == FOG_BIO_PHASE_FOG ) ? 2.0f : 1.0f;

	out->shannonDiversity = FB_Shannon( std::span<const float>( phylum, FOG_BIO_PHYLUM_COUNT ) );
	if ( phase == FOG_BIO_PHASE_FOG ) {
		out->shannonDiversity *= 1.12f;
	} else if ( phase == FOG_BIO_PHASE_POST_FOG ) {
		out->shannonDiversity *= 1.05f;
	}
	out->shannonDiversity *= 0.85f + 0.15f * marineInfluence;

	out->gramNegativeFraction = phylum[FOG_BIO_PHYLUM_PROTEOBACTERIA]
		+ phylum[FOG_BIO_PHYLUM_BACTEROIDETES];
	out->rhodospirillalesFraction = FB_RhodospirillalesFraction( site, phase );
	out->pathogenTaxaScore = FB_PathogenTaxaScore( site, phase, marineInfluence, phylum,
		out->depositionMultiplier );
	if ( phase == FOG_BIO_PHASE_POST_FOG ) {
		out->culturableRichness *= 0.85f;
	}
}

static void FB_LogEnableOnce( void )
{
	if ( s_loggedEnable ) {
		return;
	}
	s_loggedEnable = qtrue;
	Com_Printf( "[fog_biology] enabled site=%s coast_km=%.1f auto=%d\n",
		( s_site == FOG_BIO_SITE_MAINE ) ? "maine" : "namib",
		r_fogBiologyCoastKm ? r_fogBiologyCoastKm->value : 0.0f,
		r_fogBiologyAuto ? r_fogBiologyAuto->integer : 0 );
}

extern "C" {

void FogBiology_Init( void )
{
	r_fogBiology = Cvar_Get( "r_fogBiology", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_fogBiology,
		"Enable fog bioaerosol ecology model (Evans et al. 2019 drivers)." );
	r_fogBiologySite = Cvar_Get( "r_fogBiologySite", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_fogBiologySite, "Site preset: 0=Maine coastal, 1=Namib inland." );
	r_fogBiologyCoastKm = Cvar_Get( "r_fogBiologyCoastKm", "10", CVAR_ARCHIVE );
	Cvar_SetDescription( r_fogBiologyCoastKm,
		"Distance from coast (km) — modulates marine microbial influence." );
	r_fogBiologyWindMarine = Cvar_Get( "r_fogBiologyWindMarine", "0.8", CVAR_ARCHIVE );
	Cvar_SetDescription( r_fogBiologyWindMarine,
		"Marine wind component 0-1 (onshore advection of ocean aerosols)." );
	r_fogBiologyAuto = Cvar_Get( "r_fogBiologyAuto", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( r_fogBiologyAuto,
		"Auto-detect fog from r_volumetricFog when simulating bioaerosol phases." );
	r_fogBiologyCoastAuto = Cvar_Get( "r_fogBiologyCoastAuto", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_fogBiologyCoastAuto,
		"Derive r_fogBiologyCoastKm from player distance to coast origin each frame." );
	r_fogBiologyCoastAxis = Cvar_Get( "r_fogBiologyCoastAxis", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_fogBiologyCoastAxis,
		"World axis for coast distance when r_fogBiologyCoastAuto 1 (0=X, 1=Y)." );
	r_fogBiologyCoastOrigin = Cvar_Get( "r_fogBiologyCoastOrigin", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_fogBiologyCoastOrigin,
		"World coordinate of coastline when r_fogBiologyCoastAuto 1." );
	r_fogBiologyCoastUnitsPerKm = Cvar_Get( "r_fogBiologyCoastUnitsPerKm", "512", CVAR_ARCHIVE );
	Cvar_SetDescription( r_fogBiologyCoastUnitsPerKm,
		"World units per km for r_fogBiologyCoastAuto coast distance." );

	Cvar_SetDescription(
		Cvar_Get( "r_fogBiologySyncPhase", "clear", CVAR_ROM | CVAR_NORESTART ),
		"Live phase mirror (clear/fog/post_fog) — updated by FogBiology_Frame." );
	Cvar_SetDescription(
		Cvar_Get( "r_fogBiologySyncMarine", "0", CVAR_ROM | CVAR_NORESTART ),
		"Live marine influence 0-1 (ImGui/telemetry mirror)." );
	Cvar_SetDescription(
		Cvar_Get( "r_fogBiologySyncShannon", "0", CVAR_ROM | CVAR_NORESTART ),
		"Live Shannon diversity mirror." );
	Cvar_SetDescription(
		Cvar_Get( "r_fogBiologySyncDeposition", "1", CVAR_ROM | CVAR_NORESTART ),
		"Live deposition multiplier mirror." );
	Cvar_SetDescription(
		Cvar_Get( "r_fogBiologySyncPathogen", "0", CVAR_ROM | CVAR_NORESTART ),
		"Live pathogen deposition risk 0-1 mirror." );
	Cvar_SetDescription(
		Cvar_Get( "r_fogBiologySyncCoastKm", "0", CVAR_ROM | CVAR_NORESTART ),
		"Live coast distance (km) mirror — updates with coast auto." );
	Cvar_SetDescription(
		Cvar_Get( "r_fogBiologySyncOceanOtu", "0", CVAR_ROM | CVAR_NORESTART ),
		"Live ocean OTU fraction 0-1 mirror (paper: 1-75%% in fog)." );
	Cvar_SetDescription(
		Cvar_Get( "r_fogBiologySyncGramNeg", "0", CVAR_ROM | CVAR_NORESTART ),
		"Live gram-negative fraction (Proteobacteria+Bacteroidetes)." );
	Cvar_SetDescription(
		Cvar_Get( "r_fogBiologySyncRhodo", "0", CVAR_ROM | CVAR_NORESTART ),
		"Live Rhodospirillales relative abundance proxy." );

	s_site = FOG_BIO_SITE_MAINE;
	s_phase = FOG_BIO_PHASE_CLEAR;
	s_fogActive = qfalse;
	s_postFogFrames = 0;
	s_loggedEnable = qfalse;
	s_hasPlayerOrigin = qfalse;
	Com_Memset( s_playerOrigin, 0, sizeof( s_playerOrigin ) );
	Com_Memset( &s_current, 0, sizeof( s_current ) );

	Cmd_AddCommand( "fog_biology_status", FogBiology_Status_f );
	Cmd_AddCommand( "fog_biology_compare", FogBiology_Compare_f );
	Cmd_AddCommand( "fog_biology_poll", FogBiology_Poll_f );
	Cmd_AddCommand( "fog_biology_sweep", FogBiology_Sweep_f );
	Cmd_AddCommand( "fog_biology_genera", FogBiology_Genera_f );
	Cmd_AddCommand( "fog_biology_paper", FogBiology_Paper_f );
}

void FogBiology_Shutdown( void )
{
	Cmd_RemoveCommand( "fog_biology_status" );
	Cmd_RemoveCommand( "fog_biology_compare" );
	Cmd_RemoveCommand( "fog_biology_poll" );
	Cmd_RemoveCommand( "fog_biology_sweep" );
	Cmd_RemoveCommand( "fog_biology_genera" );
	Cmd_RemoveCommand( "fog_biology_paper" );
	s_loggedEnable = qfalse;
}

qboolean FogBiology_Enabled( void )
{
	return r_fogBiology && r_fogBiology->integer;
}

void FogBiology_SetSite( fogBioSite_t site )
{
	if ( site < FOG_BIO_SITE_MAINE || site > FOG_BIO_SITE_NAMIB ) {
		return;
	}
	s_site = site;
}

fogBioSite_t FogBiology_GetSite( void )
{
	return s_site;
}

void FogBiology_SetCoastDistanceKm( float km )
{
	if ( r_fogBiologyCoastKm ) {
		Cvar_SetValue( r_fogBiologyCoastKm->name, km );
	}
}

void FogBiology_SetMarineWind( float wind01 )
{
	if ( r_fogBiologyWindMarine ) {
		Cvar_SetValue( r_fogBiologyWindMarine->name, wind01 );
	}
}

void FogBiology_SetFogActive( qboolean active )
{
	s_fogActive = active;
}

void FogBiology_SetPlayerOrigin( const vec3_t origin )
{
	if ( origin ) {
		VectorCopy( origin, s_playerOrigin );
		s_hasPlayerOrigin = qtrue;
	} else {
		s_hasPlayerOrigin = qfalse;
	}
}

static void FB_ApplyCoastAuto( void )
{
	float axisCoord;
	float coastKm;
	float unitsPerKm;
	int axis;

	if ( !r_fogBiologyCoastAuto || !r_fogBiologyCoastAuto->integer || !s_hasPlayerOrigin ) {
		return;
	}
	if ( !r_fogBiologyCoastKm ) {
		return;
	}

	axis = r_fogBiologyCoastAxis ? r_fogBiologyCoastAxis->integer : 0;
	if ( axis < 0 || axis > 1 ) {
		axis = 0;
	}
	unitsPerKm = r_fogBiologyCoastUnitsPerKm ? r_fogBiologyCoastUnitsPerKm->value : 512.0f;
	if ( unitsPerKm <= 1.0f ) {
		unitsPerKm = 512.0f;
	}

	axisCoord = s_playerOrigin[axis];
	coastKm = fabsf( axisCoord - ( r_fogBiologyCoastOrigin ? r_fogBiologyCoastOrigin->value : 0.0f ) )
		/ unitsPerKm;
	Cvar_SetValue( r_fogBiologyCoastKm->name, coastKm );
}

float FogBiology_GetCoastDistanceKm( void )
{
	return r_fogBiologyCoastKm ? r_fogBiologyCoastKm->value : 10.0f;
}

float FogBiology_GetMarineInfluence( void )
{
	float coastKm = r_fogBiologyCoastKm ? r_fogBiologyCoastKm->value : 10.0f;
	float wind = r_fogBiologyWindMarine ? r_fogBiologyWindMarine->value : 0.8f;
	fogBioSite_t site = s_site;

	if ( r_fogBiologySite && r_fogBiologySite->integer ) {
		site = FOG_BIO_SITE_NAMIB;
	} else if ( r_fogBiologySite ) {
		site = ( fogBioSite_t )r_fogBiologySite->integer;
		if ( site > FOG_BIO_SITE_NAMIB ) {
			site = FOG_BIO_SITE_MAINE;
		}
	}
	return FB_ComputeMarineInfluenceInternal( site, coastKm, wind );
}

float FogBiology_GetPathogenDepositionRisk( void )
{
	float risk;
	float phaseScale;

	if ( !FogBiology_Enabled() ) {
		return 0.0f;
	}

	if ( s_phase == FOG_BIO_PHASE_CLEAR ) {
		phaseScale = 0.15f;
	} else if ( s_phase == FOG_BIO_PHASE_POST_FOG ) {
		phaseScale = 0.55f;
	} else {
		phaseScale = 1.0f;
	}

	risk = phaseScale * s_current.depositionMultiplier / 2.9f;
	risk *= 0.25f + 0.75f * s_current.marineFraction;
	risk *= 0.5f + 0.5f * ( s_current.shannonDiversity / 3.5f );
	return FB_Clamp01( risk );
}

void FogBiology_GetCommunity( fogBioPhase_t phase, fogBioCommunity_t *out )
{
	fogBioSite_t site = FogBiology_GetSite();
	if ( r_fogBiologySite && r_fogBiologySite->integer >= 0 ) {
		site = ( r_fogBiologySite->integer ) ? FOG_BIO_SITE_NAMIB : FOG_BIO_SITE_MAINE;
	}
	FB_BuildCommunity( site, phase, FogBiology_GetMarineInfluence(), out );
}

void FogBiology_GetCurrentCommunity( fogBioCommunity_t *out )
{
	if ( out ) {
		*out = s_current;
	}
}

fogBioPhase_t FogBiology_GetPhase( void )
{
	return s_phase;
}

const char *FogBiology_PhylumName( fogBioPhylum_t phylum )
{
	switch ( phylum ) {
	case FOG_BIO_PHYLUM_PROTEOBACTERIA: return "Proteobacteria";
	case FOG_BIO_PHYLUM_BACTEROIDETES: return "Bacteroidetes";
	case FOG_BIO_PHYLUM_ACTINOBACTERIA: return "Actinobacteria";
	case FOG_BIO_PHYLUM_FIRMICUTES: return "Firmicutes";
	case FOG_BIO_PHYLUM_CYANOBACTERIA: return "Cyanobacteria";
	case FOG_BIO_PHYLUM_ASCOMYCOTA: return "Ascomycota";
	default: return "Other";
	}
}

const char *FogBiology_PhaseName( fogBioPhase_t phase )
{
	switch ( phase ) {
	case FOG_BIO_PHASE_CLEAR: return "clear";
	case FOG_BIO_PHASE_FOG: return "fog";
	case FOG_BIO_PHASE_POST_FOG: return "post_fog";
	default: return "unknown";
	}
}

static void FB_SyncMirrorCvars( void )
{
	float pathRisk = FogBiology_GetPathogenDepositionRisk();

	Cvar_Set2( "r_fogBiologySyncPhase", FogBiology_PhaseName( s_phase ), qtrue );
	Cvar_Set2( "r_fogBiologySyncMarine", va( "%.4f", FogBiology_GetMarineInfluence() ), qtrue );
	Cvar_Set2( "r_fogBiologySyncShannon", va( "%.4f", s_current.shannonDiversity ), qtrue );
	Cvar_Set2( "r_fogBiologySyncDeposition", va( "%.4f", s_current.depositionMultiplier ), qtrue );
	Cvar_Set2( "r_fogBiologySyncPathogen", va( "%.4f", pathRisk ), qtrue );
	Cvar_Set2( "r_fogBiologySyncCoastKm", va( "%.4f", FogBiology_GetCoastDistanceKm() ), qtrue );
	Cvar_Set2( "r_fogBiologySyncOceanOtu", va( "%.4f", s_current.oceanOtuFraction ), qtrue );
	Cvar_Set2( "r_fogBiologySyncGramNeg", va( "%.4f", s_current.gramNegativeFraction ), qtrue );
	Cvar_Set2( "r_fogBiologySyncRhodo", va( "%.4f", s_current.rhodospirillalesFraction ), qtrue );
}

void FogBiology_Frame( void )
{
	qboolean fogNow;
	fogBioPhase_t prevPhase;

	if ( !FogBiology_Enabled() ) {
		return;
	}

	FB_LogEnableOnce();

	if ( r_fogBiologySite ) {
		s_site = ( r_fogBiologySite->integer ) ? FOG_BIO_SITE_NAMIB : FOG_BIO_SITE_MAINE;
	}

	FB_ApplyCoastAuto();

	fogNow = s_fogActive;
	if ( r_fogBiologyAuto && r_fogBiologyAuto->integer ) {
		fogNow = fogNow || ( Cvar_VariableIntegerValue( "r_volumetricFog" ) != 0 );
	}

	prevPhase = s_phase;
	if ( fogNow ) {
		s_phase = FOG_BIO_PHASE_FOG;
		s_postFogFrames = 0;
	} else if ( prevPhase == FOG_BIO_PHASE_FOG ) {
		s_phase = FOG_BIO_PHASE_POST_FOG;
		s_postFogFrames = FOG_BIO_POST_FOG_FRAMES;
	} else if ( s_phase == FOG_BIO_PHASE_POST_FOG ) {
		if ( s_postFogFrames > 0 ) {
			s_postFogFrames--;
		} else {
			s_phase = FOG_BIO_PHASE_CLEAR;
		}
	}

	FogBiology_GetCommunity( s_phase, &s_current );

	{
		float pathRisk = FogBiology_GetPathogenDepositionRisk();
		if ( s_current.depositionMultiplier > 1.5f && pathRisk > 0.65f ) {
			Com_DPrintf( "[fog_biology] elevated pathogen deposition risk=%.2f phase=%s\n",
				pathRisk, FogBiology_PhaseName( s_phase ) );
		}
	}

	FB_SyncMirrorCvars();
}

void FogBiology_Poll_f( void )
{
	if ( !FogBiology_Enabled() ) {
		Com_Printf( "[fog_biology] disabled (set r_fogBiology 1)\n" );
		return;
	}
	Com_Printf( "[fog_biology] poll phase=%s coast_km=%.1f marine=%.2f shannon=%.2f deposition=%.1fx pathogen=%.2f\n",
		FogBiology_PhaseName( s_phase ),
		FogBiology_GetCoastDistanceKm(),
		FogBiology_GetMarineInfluence(),
		s_current.shannonDiversity,
		s_current.depositionMultiplier,
		FogBiology_GetPathogenDepositionRisk() );
}

void FogBiology_Sweep_f( void )
{
	static const float coastSamples[] = { 0.01f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f, 80.0f };
	int i;
	fogBioSite_t site = FogBiology_GetSite();
	float wind = r_fogBiologyWindMarine ? r_fogBiologyWindMarine->value : 0.8f;

	if ( r_fogBiologySite ) {
		site = ( r_fogBiologySite->integer ) ? FOG_BIO_SITE_NAMIB : FOG_BIO_SITE_MAINE;
	}

	Com_Printf( "[fog_biology] sweep site=%s wind=%.2f (coast_km -> marine)\n",
		( site == FOG_BIO_SITE_MAINE ) ? "maine" : "namib", wind );
	for ( i = 0; i < (int)( sizeof( coastSamples ) / sizeof( coastSamples[0] ) ); i++ ) {
		float marine = FB_ComputeMarineInfluenceInternal( site, coastSamples[i], wind );
		Com_Printf( "  %.2f km -> marine=%.3f\n", coastSamples[i], marine );
	}
}

static void FB_PrintCommunity( const char *label, const fogBioCommunity_t *c )
{
	int i;
	Com_Printf( "[fog_biology] %s phase shannon=%.2f marine=%.2f ocean_otu=%.0f%% deposition=%.1fx richness=%.1fx gram_neg=%.0f%% rhodo=%.1f%% pathogen_taxa=%.2f\n",
		label, c->shannonDiversity, c->marineFraction, c->oceanOtuFraction * 100.0f,
		c->depositionMultiplier, c->culturableRichness, c->gramNegativeFraction * 100.0f,
		c->rhodospirillalesFraction * 100.0f, c->pathogenTaxaScore );
	for ( i = 0; i < FOG_BIO_PHYLUM_COUNT; i++ ) {
		if ( c->phylum[i] >= 0.05f ) {
			Com_Printf( "  %s: %.1f%%\n", FogBiology_PhylumName( (fogBioPhylum_t)i ),
				c->phylum[i] * 100.0f );
		}
	}
}

void FogBiology_Status_f( void )
{
	fogBioCommunity_t clearComm, fogComm;

	Com_Printf( "[fog_biology] enabled=%d site=%s phase=%s coast_km=%.1f wind_marine=%.2f\n",
		FogBiology_Enabled(),
		( FogBiology_GetSite() == FOG_BIO_SITE_MAINE ) ? "maine" : "namib",
		FogBiology_PhaseName( s_phase ),
		r_fogBiologyCoastKm ? r_fogBiologyCoastKm->value : 0.0f,
		r_fogBiologyWindMarine ? r_fogBiologyWindMarine->value : 0.0f );
	if ( !FogBiology_Enabled() ) {
		return;
	}
	FB_PrintCommunity( "current", &s_current );
	FogBiology_GetCommunity( FOG_BIO_PHASE_CLEAR, &clearComm );
	FogBiology_GetCommunity( FOG_BIO_PHASE_FOG, &fogComm );
	Com_Printf( "[fog_biology] clear vs fog shannon delta=%+.2f deposition fog/clear=%.1fx\n",
		fogComm.shannonDiversity - clearComm.shannonDiversity,
		fogComm.depositionMultiplier / clearComm.depositionMultiplier );
}

void FogBiology_Compare_f( void )
{
	fogBioCommunity_t maineFog, namibFog;

	if ( !FogBiology_Enabled() ) {
		Com_Printf( "[fog_biology] set r_fogBiology 1 first\n" );
		return;
	}

	FB_BuildCommunity( FOG_BIO_SITE_MAINE, FOG_BIO_PHASE_FOG,
		FB_ComputeMarineInfluenceInternal( FOG_BIO_SITE_MAINE, 0.01f, 0.9f ), &maineFog );
	FB_BuildCommunity( FOG_BIO_SITE_NAMIB, FOG_BIO_PHASE_FOG,
		FB_ComputeMarineInfluenceInternal( FOG_BIO_SITE_NAMIB, 50.0f, 0.7f ), &namibFog );

	Com_Printf( "[fog_biology] Maine coastal fog marine=%.2f ocean_otu=%.0f%%\n",
		maineFog.marineFraction, maineFog.oceanOtuFraction * 100.0f );
	Com_Printf( "[fog_biology] Namib 50km inland marine=%.2f ocean_otu=%.0f%%\n",
		namibFog.marineFraction, namibFog.oceanOtuFraction * 100.0f );
	Com_Printf( "[fog_biology] Maine Bacteroidetes=%.1f%% Namib Ascomycota=%.1f%%\n",
		maineFog.phylum[FOG_BIO_PHYLUM_BACTEROIDETES] * 100.0f,
		namibFog.phylum[FOG_BIO_PHYLUM_ASCOMYCOTA] * 100.0f );
	Com_Printf( "[fog_biology] Maine fog gram-neg=%.0f%% Rhodospirillales~%.1f%%\n",
		maineFog.gramNegativeFraction * 100.0f, maineFog.rhodospirillalesFraction * 100.0f );
	Com_Printf( "[fog_biology] paper data: SRA SRP155760 | doi:10.1016/j.scitotenv.2018.08.045\n" );
}

void FogBiology_Genera_f( void )
{
	fogBioSite_t site = FogBiology_GetSite();

	if ( r_fogBiologySite ) {
		site = ( r_fogBiologySite->integer ) ? FOG_BIO_SITE_NAMIB : FOG_BIO_SITE_MAINE;
	}

	Com_Printf( "[fog_biology] dominant fog genera (Evans et al. 2019 Table 1, site=%s phase=%s)\n",
		( site == FOG_BIO_SITE_MAINE ) ? "maine" : "namib",
		FogBiology_PhaseName( s_phase ) );
	if ( site == FOG_BIO_SITE_MAINE ) {
		Com_Printf( "  Massilia, Pantoea, Pseudoalteromonas, Duganella (marine/soil)\n" );
		Com_Printf( "  Oceanospirillales, Flavobacteriaceae, Acetobacteraceae\n" );
	} else {
		Com_Printf( "  Geodermatophilus, Erwinia, Bacillus, Marinomonas (marine)\n" );
		Com_Printf( "  Fimetariella, Arthrinium, Periconia (Namib fungi)\n" );
	}
	Com_Printf( "  pathogen_taxa_score=%.2f (heuristic from deposition+marine+Proteobacteria)\n",
		s_current.pathogenTaxaScore );
}

void FogBiology_Paper_f( void )
{
	fogBioCommunity_t clearComm, fogComm, postComm;
	fogBioCommunity_t maineFog, namibFog;
	float maineMarine, namibMarine;

	if ( !FogBiology_Enabled() ) {
		Com_Printf( "[fog_biology] set r_fogBiology 1 for replication report\n" );
	}

	Com_Printf( "[fog_biology] Evans et al. 2019 replication (engine model vs published trends)\n" );
	Com_Printf( "  citation: Sci Total Environ 647:1547-1556 | SRA SRP155760\n" );

	FogBiology_GetCommunity( FOG_BIO_PHASE_CLEAR, &clearComm );
	FogBiology_GetCommunity( FOG_BIO_PHASE_FOG, &fogComm );
	FogBiology_GetCommunity( FOG_BIO_PHASE_POST_FOG, &postComm );

	maineMarine = FB_ComputeMarineInfluenceInternal( FOG_BIO_SITE_MAINE, 0.03f, 0.9f );
	namibMarine = FB_ComputeMarineInfluenceInternal( FOG_BIO_SITE_NAMIB, 50.0f, 0.7f );
	FB_BuildCommunity( FOG_BIO_SITE_MAINE, FOG_BIO_PHASE_FOG, maineMarine, &maineFog );
	FB_BuildCommunity( FOG_BIO_SITE_NAMIB, FOG_BIO_PHASE_FOG, namibMarine, &namibFog );

	Com_Printf( "  [finding] fog alpha diversity > clear: model %+.2f shannon (paper Fig. 5)\n",
		fogComm.shannonDiversity - clearComm.shannonDiversity );
	Com_Printf( "  [finding] culturable deposition ~3x in fog: model %.1fx (paper Fig. 4)\n",
		fogComm.depositionMultiplier / clearComm.depositionMultiplier );
	Com_Printf( "  [finding] culturable richness ~2x in fog: model %.1fx (paper Fig. 4)\n",
		fogComm.culturableRichness / clearComm.culturableRichness );
	Com_Printf( "  [finding] ocean OTU 1-75%%: model %.0f%% (coast) .. %.0f%% (50km Namib)\n",
		maineFog.oceanOtuFraction * 100.0f, namibFog.oceanOtuFraction * 100.0f );
	Com_Printf( "  [finding] Maine fog lowers Bacteroidetes: model %.1f%% fog vs %.1f%% clear\n",
		maineFog.phylum[FOG_BIO_PHYLUM_BACTEROIDETES] * 100.0f,
		clearComm.phylum[FOG_BIO_PHYLUM_BACTEROIDETES] * 100.0f );
	Com_Printf( "  [finding] Namib fog enriches Ascomycota: model %.1f%% (paper Table S7 ~20.1%%)\n",
		namibFog.phylum[FOG_BIO_PHYLUM_ASCOMYCOTA] * 100.0f );
	Com_Printf( "  [finding] gram-negative dominance: model %.0f%% in fog (paper Sec. 4.1)\n",
		fogComm.gramNegativeFraction * 100.0f );
	Com_Printf( "  [finding] Rhodospirillales fog>clear Maine: model %.1f%% vs 0.2%% (paper)\n",
		fogComm.rhodospirillalesFraction * 100.0f );
	Com_Printf( "  [current] phase=%s coast_km=%.2f post_fog richness=%.1fx\n",
		FogBiology_PhaseName( s_phase ), FogBiology_GetCoastDistanceKm(),
		postComm.culturableRichness );
}

#ifdef FOG_BIOLOGY_UNIT_TEST
void FogBiology_ResetForTest( void )
{
	s_site = FOG_BIO_SITE_MAINE;
	s_phase = FOG_BIO_PHASE_CLEAR;
	s_fogActive = qfalse;
	s_postFogFrames = 0;
	s_hasPlayerOrigin = qfalse;
	Com_Memset( &s_current, 0, sizeof( s_current ) );
}

float FogBiology_ComputeMarineInfluenceForTest( fogBioSite_t site, float coastKm, float wind01 )
{
	return FB_ComputeMarineInfluenceInternal( site, coastKm, wind01 );
}

void FogBiology_BuildCommunityForTest( fogBioSite_t site, fogBioPhase_t phase,
	float marineInfluence, fogBioCommunity_t *out )
{
	FB_BuildCommunity( site, phase, marineInfluence, out );
}
#endif
}
