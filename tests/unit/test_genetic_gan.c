/*
 * Unit tests: genetic genome + phenotype API.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcommon/q_shared.h"
#include "qcommon/qcommon.h"
#include "world/genetic_gan.h"

#define ASSERT(cond, msg) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL: %s\n", msg); \
		return 1; \
	} \
} while (0)

static int test_create_and_genes( void )
{
	int slot;
	float g;

	GeneticGan_ResetForTest();
	GeneticGan_SetDimForTest( 8 );
	GeneticGan_Init();
	slot = GeneticGan_CreateRandom( "alpha" );
	ASSERT( slot >= 0, "create slot" );
	ASSERT( GeneticGan_Count() == 1, "one active slot" );
	g = GeneticGan_GetGene( slot, 0 );
	ASSERT( g >= 0.0f && g <= 1.0f, "gene in unit range" );
	GeneticGan_Shutdown();
	return 0;
}

static int test_breed_generation( void )
{
	int a, b, child;

	GeneticGan_ResetForTest();
	GeneticGan_SetDimForTest( 8 );
	GeneticGan_Init();
	a = GeneticGan_CreateRandom( "a" );
	b = GeneticGan_CreateRandom( "b" );
	child = GeneticGan_BreedForTest( a, b, 0.0f );
	ASSERT( child >= 0, "child created" );
	ASSERT( GeneticGan_GetGeneration( child ) == 1, "child generation incremented" );
	ASSERT( GeneticGan_Count() == 3, "three slots" );
	GeneticGan_Shutdown();
	return 0;
}

static int test_fitness_select( void )
{
	int s0, s1, best;

	GeneticGan_ResetForTest();
	GeneticGan_SetDimForTest( 4 );
	GeneticGan_Init();
	s0 = GeneticGan_CreateRandom( "low" );
	s1 = GeneticGan_CreateRandom( "high" );
	GeneticGan_SetFitness( s0, 0.2f );
	GeneticGan_SetFitness( s1, 0.9f );
	best = GeneticGan_SelectBest();
	ASSERT( best == s1, "select best fitness" );
	GeneticGan_Shutdown();
	return 0;
}

static int test_phenotype_mapping( void )
{
	geneticGanPhenotype_t pheno;
	int slot;

	GeneticGan_ResetForTest();
	GeneticGan_SetDimForTest( 8 );
	GeneticGan_Init();
	slot = GeneticGan_CreateRandom( "pheno" );
	GeneticGan_SetGene( slot, 0, 1.0f );
	GeneticGan_SetGene( slot, 1, 0.0f );
	GeneticGan_GetPhenotype( slot, &pheno );
	ASSERT( pheno.bodyScale > 1.0f, "body scale from gene0" );
	ASSERT( pheno.limbLength < 1.0f, "limb length from gene1" );
	ASSERT( pheno.morphCount > 0, "morph weights exported" );
	GeneticGan_Shutdown();
	return 0;
}

static int test_mutate( void )
{
	int slot;
	int genBefore;

	GeneticGan_ResetForTest();
	GeneticGan_SetDimForTest( 16 );
	GeneticGan_Init();
	slot = GeneticGan_CreateRandom( "mut" );
	genBefore = GeneticGan_GetGeneration( slot );
	GeneticGan_Mutate( slot, 1.0f, 0.5f );
	ASSERT( GeneticGan_GetGeneration( slot ) == genBefore + 1, "mutate bumps generation" );
	GeneticGan_Shutdown();
	return 0;
}

int main( void )
{
	if ( test_create_and_genes() ) return 1;
	if ( test_breed_generation() ) return 1;
	if ( test_fitness_select() ) return 1;
	if ( test_phenotype_mapping() ) return 1;
	if ( test_mutate() ) return 1;
	printf( "PASS: genetic_gan unit tests\n" );
	return 0;
}
