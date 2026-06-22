/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Genetic + GAN procedural body evolution — genome slots, crossover, mutation,
fitness ranking, and optional external latent-to-mesh decode hooks.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

#define GENETIC_GAN_MAX_SLOTS   32
#define GENETIC_GAN_MAX_DIM     64
#define GENETIC_GAN_MORPH_OUT   8

typedef enum {
	GENETIC_GAN_JOB_IDLE = 0,
	GENETIC_GAN_JOB_RUNNING,
	GENETIC_GAN_JOB_COMPLETED,
	GENETIC_GAN_JOB_FAILED
} geneticGanJobStatus_t;

typedef struct geneticGanPhenotype_s {
	float bodyScale;
	float limbLength;
	float headSize;
	float torsoWidth;
	float agility;
	float mass;
	float morphWeights[GENETIC_GAN_MORPH_OUT];
	int morphCount;
} geneticGanPhenotype_t;

typedef struct geneticGanSlotInfo_s {
	int slot;
	qboolean active;
	char label[64];
	float fitness;
	int generation;
	char outputVfs[MAX_QPATH];
} geneticGanSlotInfo_t;

void     GeneticGan_Init( void );
void     GeneticGan_Shutdown( void );

qboolean GeneticGan_Enabled( void );
int      GeneticGan_GetDim( void );

int      GeneticGan_CreateRandom( const char *label );
int      GeneticGan_CreateFromGenes( const char *label, const float *genes, int dim );
qboolean GeneticGan_IsActive( int slot );
int      GeneticGan_Count( void );

float    GeneticGan_GetGene( int slot, int index );
qboolean GeneticGan_SetGene( int slot, int index, float value );
void     GeneticGan_SetFitness( int slot, float fitness );
float    GeneticGan_GetFitness( int slot );
const char *GeneticGan_GetLabel( int slot );
int      GeneticGan_GetGeneration( int slot );

int      GeneticGan_Breed( int parentA, int parentB, float mutationRate, const char *childLabel );
int      GeneticGan_Mutate( int slot, float rate, float strength );
int      GeneticGan_SelectBest( void );
int      GeneticGan_SelectTournament( int k );

void     GeneticGan_GetPhenotype( int slot, geneticGanPhenotype_t *out );
void     GeneticGan_GetMorphWeights( int slot, float *out, int maxOut, int *countOut );

qboolean GeneticGan_WriteGenomeJson( int slot, const char *fullPath );
qboolean GeneticGan_ReadGenomeJson( int slot, const char *fullPath );

void     GeneticGan_SetJobStatus( geneticGanJobStatus_t status, const char *errorMsg );
geneticGanJobStatus_t GeneticGan_GetJobStatus( void );
const char *GeneticGan_GetJobError( void );
int      GeneticGan_GetJobSlot( void );
void     GeneticGan_SetJobSlot( int slot );
void     GeneticGan_SetSlotOutput( int slot, const char *vfsPath );
const char *GeneticGan_GetSlotOutput( int slot );

void     GeneticGan_Status_f( void );
void     GeneticGan_Create_f( void );
void     GeneticGan_Breed_f( void );
void     GeneticGan_Mutate_f( void );
void     GeneticGan_Fitness_f( void );
void     GeneticGan_Phenotype_f( void );

#ifdef GENETIC_GAN_UNIT_TEST
void     GeneticGan_ResetForTest( void );
void     GeneticGan_SetDimForTest( int dim );
int      GeneticGan_BreedForTest( int parentA, int parentB, float mutationRate );
#endif

#ifdef __cplusplus
}
#endif
