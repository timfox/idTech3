/* C++20 migration: extern "C" API boundary preserved. */
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <span>

extern "C" {
#include "q_shared.h"
#include "qcommon.h"
#include "genetic_gan.h"
}

static cvar_t *cl_geneticGan;
static cvar_t *cl_geneticGanDim;
static cvar_t *cl_geneticGanMutationRate;
static cvar_t *cl_geneticGanCrossoverBlend;

typedef struct geneticGanSlot_s {
	qboolean active;
	char label[64];
	float genes[GENETIC_GAN_MAX_DIM];
	float fitness;
	int generation;
	char outputVfs[MAX_QPATH];
} geneticGanSlot_t;

static geneticGanSlot_t s_slots[GENETIC_GAN_MAX_SLOTS];
static int s_dim;
static qboolean s_loggedEnable;

static geneticGanJobStatus_t s_jobStatus;
static char s_jobError[1024];
static int s_jobSlot;

static cvar_t *cl_geneticGanSyncJob;
static char cl_geneticGanSyncJobStr[32];
static cvar_t *cl_geneticGanSyncSlot;
static char cl_geneticGanSyncSlotStr[32];
static cvar_t *cl_geneticGanSyncCount;
static char cl_geneticGanSyncCountStr[32];

static qboolean GG_FeatureActive( void )
{
#ifdef GENETIC_GAN_UNIT_TEST
	return qtrue;
#endif
	return GeneticGan_Enabled();
}

[[nodiscard]] static float GG_Clamp01( float v )
{
	return std::clamp( v, 0.0f, 1.0f );
}

static int GG_EffectiveDim( void )
{
	int dim;

	if ( s_dim > 0 ) {
		return s_dim;
	}
	dim = cl_geneticGanDim ? cl_geneticGanDim->integer : 32;
	if ( dim < 4 ) {
		dim = 4;
	}
	if ( dim > GENETIC_GAN_MAX_DIM ) {
		dim = GENETIC_GAN_MAX_DIM;
	}
	return dim;
}

static float GG_Rand01( void )
{
	return (float)( rand() & 0x7fff ) / 32767.0f;
}

static float GG_RandSigned( void )
{
	return GG_Rand01() * 2.0f - 1.0f;
}

static int GG_AllocSlot( void )
{
	int i;

	for ( i = 0; i < GENETIC_GAN_MAX_SLOTS; i++ ) {
		if ( !s_slots[i].active ) {
			return i;
		}
	}
	return -1;
}

static void GG_FillRandom( geneticGanSlot_t *slot )
{
	const int dim = GG_EffectiveDim();
	std::span<float> genes( slot->genes, GENETIC_GAN_MAX_DIM );
	for ( int i = 0; i < dim; i++ ) {
		genes[i] = GG_Rand01();
	}
	for ( int i = dim; i < GENETIC_GAN_MAX_DIM; i++ ) {
		genes[i] = 0.0f;
	}
}

static void GG_UpdateSyncCvars( void )
{
	int count = 0;
	int i;

	for ( i = 0; i < GENETIC_GAN_MAX_SLOTS; i++ ) {
		if ( s_slots[i].active ) {
			count++;
		}
	}
	Com_sprintf( cl_geneticGanSyncJobStr, sizeof( cl_geneticGanSyncJobStr ), "%d", (int)s_jobStatus );
	Com_sprintf( cl_geneticGanSyncSlotStr, sizeof( cl_geneticGanSyncSlotStr ), "%d", s_jobSlot );
	Com_sprintf( cl_geneticGanSyncCountStr, sizeof( cl_geneticGanSyncCountStr ), "%d", count );
}

extern "C" {

void GeneticGan_Init( void )
{
	cl_geneticGan = Cvar_Get( "cl_geneticGan", "1", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_geneticGan, "0", "1", CV_INTEGER );
	Cvar_SetDescription( cl_geneticGan,
		"Enable genetic genome + optional GAN mesh decode API for procedural body evolution." );

	cl_geneticGanDim = Cvar_Get( "cl_geneticGanDim", "32", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_geneticGanDim, "4", "64", CV_INTEGER );
	Cvar_SetDescription( cl_geneticGanDim, "Latent genome vector length used for breed/mutate/decode." );

	cl_geneticGanMutationRate = Cvar_Get( "cl_geneticGanMutationRate", "0.05", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_geneticGanMutationRate, "0", "1", CV_FLOAT );
	Cvar_SetDescription( cl_geneticGanMutationRate, "Default per-gene mutation probability for breed()." );

	cl_geneticGanCrossoverBlend = Cvar_Get( "cl_geneticGanCrossoverBlend", "0.5", CVAR_ARCHIVE );
	Cvar_CheckRange( cl_geneticGanCrossoverBlend, "0", "1", CV_FLOAT );
	Cvar_SetDescription( cl_geneticGanCrossoverBlend,
		"Parent blend weight for crossover (0=uniform swap, 1=always average)." );

	cl_geneticGanSyncJob = Cvar_Get( "cl_geneticGanSyncJob", "0", CVAR_ROM );
	cl_geneticGanSyncJob->string = cl_geneticGanSyncJobStr;
	cl_geneticGanSyncSlot = Cvar_Get( "cl_geneticGanSyncSlot", "-1", CVAR_ROM );
	cl_geneticGanSyncSlot->string = cl_geneticGanSyncSlotStr;
	cl_geneticGanSyncCount = Cvar_Get( "cl_geneticGanSyncCount", "0", CVAR_ROM );
	cl_geneticGanSyncCount->string = cl_geneticGanSyncCountStr;

	s_dim = 0;
	s_loggedEnable = qfalse;
	s_jobStatus = GENETIC_GAN_JOB_IDLE;
	s_jobError[0] = '\0';
	s_jobSlot = -1;
	Com_Memset( s_slots, 0, sizeof( s_slots ) );

	Cmd_AddCommand( "genetic_gan_status", GeneticGan_Status_f );
	Cmd_AddCommand( "genome_create", GeneticGan_Create_f );
	Cmd_AddCommand( "genome_breed", GeneticGan_Breed_f );
	Cmd_AddCommand( "genome_mutate", GeneticGan_Mutate_f );
	Cmd_AddCommand( "genome_fitness", GeneticGan_Fitness_f );
	Cmd_AddCommand( "genome_phenotype", GeneticGan_Phenotype_f );

	if ( cl_geneticGan->integer && !s_loggedEnable ) {
		Com_Printf( "Genetic GAN: enabled (dim %d, mutation %.3f)\n",
			GG_EffectiveDim(), cl_geneticGanMutationRate->value );
		s_loggedEnable = qtrue;
	} else if ( !cl_geneticGan->integer ) {
		Com_Printf( "Genetic GAN: disabled (cl_geneticGan 0; docs/GENETIC_GAN.md)\n" );
	}
	GG_UpdateSyncCvars();
}

void GeneticGan_Shutdown( void )
{
	Cmd_RemoveCommand( "genetic_gan_status" );
	Cmd_RemoveCommand( "genome_create" );
	Cmd_RemoveCommand( "genome_breed" );
	Cmd_RemoveCommand( "genome_mutate" );
	Cmd_RemoveCommand( "genome_fitness" );
	Cmd_RemoveCommand( "genome_phenotype" );
}

qboolean GeneticGan_Enabled( void )
{
	return cl_geneticGan && cl_geneticGan->integer != 0;
}

int GeneticGan_GetDim( void )
{
	return GG_EffectiveDim();
}

qboolean GeneticGan_IsActive( int slot )
{
	if ( slot < 0 || slot >= GENETIC_GAN_MAX_SLOTS ) {
		return qfalse;
	}
	return s_slots[slot].active;
}

int GeneticGan_Count( void )
{
	int count = 0;
	int i;

	for ( i = 0; i < GENETIC_GAN_MAX_SLOTS; i++ ) {
		if ( s_slots[i].active ) {
			count++;
		}
	}
	return count;
}

int GeneticGan_CreateRandom( const char *label )
{
	geneticGanSlot_t *slot;
	int idx;

	if ( !GG_FeatureActive() ) {
		return -1;
	}
	idx = GG_AllocSlot();
	if ( idx < 0 ) {
		Com_Printf( S_COLOR_YELLOW "GeneticGan: slot pool full (%d)\n", GENETIC_GAN_MAX_SLOTS );
		return -1;
	}
	slot = &s_slots[idx];
	Com_Memset( slot, 0, sizeof( *slot ) );
	slot->active = qtrue;
	slot->fitness = 0.0f;
	slot->generation = 0;
	if ( label && label[0] ) {
		Q_strncpyz( slot->label, label, sizeof( slot->label ) );
	} else {
		Com_sprintf( slot->label, sizeof( slot->label ), "genome_%d", idx );
	}
	GG_FillRandom( slot );
	GG_UpdateSyncCvars();
	return idx;
}

int GeneticGan_CreateFromGenes( const char *label, const float *genes, int dim )
{
	geneticGanSlot_t *slot;
	int idx;
	int i;
	int useDim;

	if ( !GG_FeatureActive() || !genes ) {
		return -1;
	}
	idx = GG_AllocSlot();
	if ( idx < 0 ) {
		return -1;
	}
	useDim = GG_EffectiveDim();
	if ( dim > 0 && dim < useDim ) {
		useDim = dim;
	}
	slot = &s_slots[idx];
	Com_Memset( slot, 0, sizeof( *slot ) );
	slot->active = qtrue;
	if ( label && label[0] ) {
		Q_strncpyz( slot->label, label, sizeof( slot->label ) );
	} else {
		Com_sprintf( slot->label, sizeof( slot->label ), "genome_%d", idx );
	}
	for ( i = 0; i < useDim; i++ ) {
		slot->genes[i] = GG_Clamp01( genes[i] );
	}
	GG_UpdateSyncCvars();
	return idx;
}

float GeneticGan_GetGene( int slot, int index )
{
	if ( !GeneticGan_IsActive( slot ) || index < 0 || index >= GG_EffectiveDim() ) {
		return 0.0f;
	}
	return s_slots[slot].genes[index];
}

qboolean GeneticGan_SetGene( int slot, int index, float value )
{
	if ( !GeneticGan_IsActive( slot ) || index < 0 || index >= GG_EffectiveDim() ) {
		return qfalse;
	}
	s_slots[slot].genes[index] = GG_Clamp01( value );
	return qtrue;
}

void GeneticGan_SetFitness( int slot, float fitness )
{
	if ( GeneticGan_IsActive( slot ) ) {
		s_slots[slot].fitness = fitness;
	}
}

float GeneticGan_GetFitness( int slot )
{
	if ( !GeneticGan_IsActive( slot ) ) {
		return 0.0f;
	}
	return s_slots[slot].fitness;
}

const char *GeneticGan_GetLabel( int slot )
{
	if ( !GeneticGan_IsActive( slot ) ) {
		return "";
	}
	return s_slots[slot].label;
}

int GeneticGan_GetGeneration( int slot )
{
	if ( !GeneticGan_IsActive( slot ) ) {
		return 0;
	}
	return s_slots[slot].generation;
}

int GeneticGan_Breed( int parentA, int parentB, float mutationRate, const char *childLabel )
{
	geneticGanSlot_t *child;
	geneticGanSlot_t *pa;
	geneticGanSlot_t *pb;
	int idx;
	int i;
	int dim;
	float blend;
	float rate;

	if ( !GG_FeatureActive() ) {
		return -1;
	}
	if ( !GeneticGan_IsActive( parentA ) || !GeneticGan_IsActive( parentB ) ) {
		Com_Printf( S_COLOR_YELLOW "GeneticGan: breed requires two active parent slots\n" );
		return -1;
	}
	idx = GG_AllocSlot();
	if ( idx < 0 ) {
		return -1;
	}
	pa = &s_slots[parentA];
	pb = &s_slots[parentB];
	child = &s_slots[idx];
	Com_Memset( child, 0, sizeof( *child ) );
	child->active = qtrue;
	child->generation = ( pa->generation > pb->generation ? pa->generation : pb->generation ) + 1;
	child->fitness = 0.0f;
	if ( childLabel && childLabel[0] ) {
		Q_strncpyz( child->label, childLabel, sizeof( child->label ) );
	} else {
		Com_sprintf( child->label, sizeof( child->label ), "child_%d_of_%d_%d", idx, parentA, parentB );
	}

	dim = GG_EffectiveDim();
	blend = cl_geneticGanCrossoverBlend ? cl_geneticGanCrossoverBlend->value : 0.5f;
	blend = GG_Clamp01( blend );
	rate = mutationRate >= 0.0f ? mutationRate :
		( cl_geneticGanMutationRate ? cl_geneticGanMutationRate->value : 0.05f );
	rate = GG_Clamp01( rate );

	for ( i = 0; i < dim; i++ ) {
		float g;
		if ( blend >= 0.999f ) {
			g = 0.5f * ( pa->genes[i] + pb->genes[i] );
		} else if ( GG_Rand01() < 0.5f ) {
			g = pa->genes[i];
		} else {
			g = pb->genes[i];
		}
		if ( GG_Rand01() < rate ) {
			g += GG_RandSigned() * 0.15f;
		}
		child->genes[i] = GG_Clamp01( g );
	}
	GG_UpdateSyncCvars();
	return idx;
}

int GeneticGan_Mutate( int slot, float rate, float strength )
{
	geneticGanSlot_t *s;
	int i;
	int dim;
	float r;
	float st;

	if ( !GeneticGan_IsActive( slot ) ) {
		return -1;
	}
	s = &s_slots[slot];
	dim = GG_EffectiveDim();
	r = rate >= 0.0f ? rate :
		( cl_geneticGanMutationRate ? cl_geneticGanMutationRate->value : 0.05f );
	st = strength > 0.0f ? strength : 0.15f;
	r = GG_Clamp01( r );

	for ( i = 0; i < dim; i++ ) {
		if ( GG_Rand01() < r ) {
			s->genes[i] = GG_Clamp01( s->genes[i] + GG_RandSigned() * st );
		}
	}
	s->generation++;
	GG_UpdateSyncCvars();
	return slot;
}

int GeneticGan_SelectBest( void )
{
	int best = -1;
	float bestFit = -1e30f;
	int i;

	for ( i = 0; i < GENETIC_GAN_MAX_SLOTS; i++ ) {
		if ( s_slots[i].active && s_slots[i].fitness > bestFit ) {
			bestFit = s_slots[i].fitness;
			best = i;
		}
	}
	return best;
}

int GeneticGan_SelectTournament( int k )
{
	int picks = k > 0 ? k : 3;
	int best = -1;
	float bestFit = -1e30f;
	int attempts;
	int i;

	if ( GeneticGan_Count() == 0 ) {
		return -1;
	}
	for ( attempts = 0; attempts < picks * 4; attempts++ ) {
		i = rand() % GENETIC_GAN_MAX_SLOTS;
		if ( !s_slots[i].active ) {
			continue;
		}
		if ( s_slots[i].fitness > bestFit ) {
			bestFit = s_slots[i].fitness;
			best = i;
		}
	}
	return best;
}

void GeneticGan_GetPhenotype( int slot, geneticGanPhenotype_t *out )
{
	int i;
	int dim;
	int morphCount;

	if ( !out ) {
		return;
	}
	Com_Memset( out, 0, sizeof( *out ) );
	if ( !GeneticGan_IsActive( slot ) ) {
		return;
	}
	dim = GG_EffectiveDim();
	out->bodyScale = 0.75f + 0.5f * GeneticGan_GetGene( slot, 0 );
	out->limbLength = 0.6f + 0.8f * GeneticGan_GetGene( slot, 1 );
	out->headSize = 0.5f + 0.7f * GeneticGan_GetGene( slot, 2 );
	out->torsoWidth = 0.6f + 0.8f * GeneticGan_GetGene( slot, 3 );
	out->agility = GeneticGan_GetGene( slot, 4 );
	out->mass = 0.5f + GeneticGan_GetGene( slot, 5 );

	morphCount = dim < GENETIC_GAN_MORPH_OUT ? dim : GENETIC_GAN_MORPH_OUT;
	for ( i = 0; i < morphCount; i++ ) {
		out->morphWeights[i] = GeneticGan_GetGene( slot, i );
	}
	out->morphCount = morphCount;
}

void GeneticGan_GetMorphWeights( int slot, float *out, int maxOut, int *countOut )
{
	geneticGanPhenotype_t pheno;
	int i;

	if ( countOut ) {
		*countOut = 0;
	}
	if ( !out || maxOut <= 0 ) {
		return;
	}
	GeneticGan_GetPhenotype( slot, &pheno );
	for ( i = 0; i < pheno.morphCount && i < maxOut; i++ ) {
		out[i] = pheno.morphWeights[i];
	}
	if ( countOut ) {
		*countOut = ( pheno.morphCount < maxOut ) ? pheno.morphCount : maxOut;
	}
}

qboolean GeneticGan_WriteGenomeJson( int slot, const char *fullPath )
{
	FILE *f;
	int i;
	int dim;

	if ( !GeneticGan_IsActive( slot ) || !fullPath || !fullPath[0] ) {
		return qfalse;
	}
	f = fopen( fullPath, "wb" );
	if ( !f ) {
		return qfalse;
	}
	dim = GG_EffectiveDim();
	fprintf( f, "{\n  \"slot\": %d,\n  \"label\": \"%s\",\n  \"dim\": %d,\n  \"fitness\": %.6f,\n  \"generation\": %d,\n  \"genes\": [",
		slot, s_slots[slot].label, dim, s_slots[slot].fitness, s_slots[slot].generation );
	for ( i = 0; i < dim; i++ ) {
		fprintf( f, "%s%.6f", ( i > 0 ) ? ", " : "", s_slots[slot].genes[i] );
	}
	fprintf( f, "]\n}\n" );
	fclose( f );
	return qtrue;
}

qboolean GeneticGan_ReadGenomeJson( int slot, const char *fullPath )
{
	/* Minimal loader: games can round-trip via create + manual setGene; full parse deferred. */
	(void)slot;
	(void)fullPath;
	return qfalse;
}

void GeneticGan_SetJobStatus( geneticGanJobStatus_t status, const char *errorMsg )
{
	s_jobStatus = status;
	if ( errorMsg && errorMsg[0] ) {
		Q_strncpyz( s_jobError, errorMsg, sizeof( s_jobError ) );
	} else {
		s_jobError[0] = '\0';
	}
	GG_UpdateSyncCvars();
}

geneticGanJobStatus_t GeneticGan_GetJobStatus( void )
{
	return s_jobStatus;
}

const char *GeneticGan_GetJobError( void )
{
	return s_jobError;
}

int GeneticGan_GetJobSlot( void )
{
	return s_jobSlot;
}

void GeneticGan_SetJobSlot( int slot )
{
	s_jobSlot = slot;
	GG_UpdateSyncCvars();
}

void GeneticGan_SetSlotOutput( int slot, const char *vfsPath )
{
	if ( GeneticGan_IsActive( slot ) && vfsPath ) {
		Q_strncpyz( s_slots[slot].outputVfs, vfsPath, sizeof( s_slots[slot].outputVfs ) );
	}
}

const char *GeneticGan_GetSlotOutput( int slot )
{
	if ( !GeneticGan_IsActive( slot ) ) {
		return "";
	}
	return s_slots[slot].outputVfs;
}

void GeneticGan_Status_f( void )
{
	int i;
	int dim = GG_EffectiveDim();

	Com_Printf( "Genetic GAN: %s  dim=%d  slots=%d/%d  job=%d slot=%d\n",
		GeneticGan_Enabled() ? "enabled" : "disabled",
		dim, GeneticGan_Count(), GENETIC_GAN_MAX_SLOTS,
		(int)s_jobStatus, s_jobSlot );
	for ( i = 0; i < GENETIC_GAN_MAX_SLOTS; i++ ) {
		if ( s_slots[i].active ) {
			Com_Printf( "  [%d] %s gen=%d fitness=%.3f output=%s\n",
				i, s_slots[i].label, s_slots[i].generation, s_slots[i].fitness,
				s_slots[i].outputVfs[0] ? s_slots[i].outputVfs : "-" );
		}
	}
}

void GeneticGan_Create_f( void )
{
	const char *label;
	int slot;

	if ( !GeneticGan_Enabled() ) {
		Com_Printf( S_COLOR_YELLOW "genome_create: set cl_geneticGan 1\n" );
		return;
	}
	label = ( Cmd_Argc() >= 2 ) ? Cmd_ArgsFrom( 1 ) : NULL;
	slot = GeneticGan_CreateRandom( label );
	if ( slot >= 0 ) {
		Com_Printf( "GeneticGan: created slot %d (%s)\n", slot, GeneticGan_GetLabel( slot ) );
	}
}

void GeneticGan_Breed_f( void )
{
	int a, b;
	float rate;
	int child;

	if ( !GeneticGan_Enabled() ) {
		Com_Printf( S_COLOR_YELLOW "genome_breed: set cl_geneticGan 1\n" );
		return;
	}
	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: genome_breed <parentA> <parentB> [mutation_rate] [child_label]\n" );
		return;
	}
	a = atoi( Cmd_Argv( 1 ) );
	b = atoi( Cmd_Argv( 2 ) );
	rate = ( Cmd_Argc() >= 4 ) ? (float)atof( Cmd_Argv( 3 ) ) : -1.0f;
	child = GeneticGan_Breed( a, b, rate, ( Cmd_Argc() >= 5 ) ? Cmd_ArgsFrom( 4 ) : NULL );
	if ( child >= 0 ) {
		Com_Printf( "GeneticGan: child slot %d from %d x %d\n", child, a, b );
	}
}

void GeneticGan_Mutate_f( void )
{
	int slot;
	float rate;
	float strength;

	if ( !GeneticGan_Enabled() ) {
		Com_Printf( S_COLOR_YELLOW "genome_mutate: set cl_geneticGan 1\n" );
		return;
	}
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: genome_mutate <slot> [rate] [strength]\n" );
		return;
	}
	slot = atoi( Cmd_Argv( 1 ) );
	rate = ( Cmd_Argc() >= 3 ) ? (float)atof( Cmd_Argv( 2 ) ) : -1.0f;
	strength = ( Cmd_Argc() >= 4 ) ? (float)atof( Cmd_Argv( 3 ) ) : 0.15f;
	if ( GeneticGan_Mutate( slot, rate, strength ) >= 0 ) {
		Com_Printf( "GeneticGan: mutated slot %d\n", slot );
	}
}

void GeneticGan_Fitness_f( void )
{
	int slot;
	float fit;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: genome_fitness <slot> <value>\n" );
		return;
	}
	slot = atoi( Cmd_Argv( 1 ) );
	fit = (float)atof( Cmd_Argv( 2 ) );
	GeneticGan_SetFitness( slot, fit );
	Com_Printf( "GeneticGan: slot %d fitness -> %.3f\n", slot, fit );
}

void GeneticGan_Phenotype_f( void )
{
	geneticGanPhenotype_t pheno;
	int slot;
	int i;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: genome_phenotype <slot>\n" );
		return;
	}
	slot = atoi( Cmd_Argv( 1 ) );
	GeneticGan_GetPhenotype( slot, &pheno );
	if ( !GeneticGan_IsActive( slot ) ) {
		Com_Printf( S_COLOR_YELLOW "GeneticGan: inactive slot %d\n", slot );
		return;
	}
	Com_Printf( "Phenotype slot %d: scale=%.2f limbs=%.2f head=%.2f torso=%.2f agility=%.2f mass=%.2f\n",
		slot, pheno.bodyScale, pheno.limbLength, pheno.headSize, pheno.torsoWidth, pheno.agility, pheno.mass );
	Com_Printf( "  morph[%d]:", pheno.morphCount );
	for ( i = 0; i < pheno.morphCount; i++ ) {
		Com_Printf( " %.2f", pheno.morphWeights[i] );
	}
	Com_Printf( "\n" );
}

#ifdef GENETIC_GAN_UNIT_TEST
void GeneticGan_ResetForTest( void )
{
	Com_Memset( s_slots, 0, sizeof( s_slots ) );
	s_dim = 0;
	s_jobStatus = GENETIC_GAN_JOB_IDLE;
	s_jobSlot = -1;
}

void GeneticGan_SetDimForTest( int dim )
{
	s_dim = dim;
}

int GeneticGan_BreedForTest( int parentA, int parentB, float mutationRate )
{
	return GeneticGan_Breed( parentA, parentB, mutationRate, "test_child" );
}
#endif
}
