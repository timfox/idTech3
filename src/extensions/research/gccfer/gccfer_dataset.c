/*
===========================================================================
GCC-FER dataset statistics (Table II) and label helpers.
===========================================================================
*/

#include "gccfer/gccfer.h"

#include <string.h>

/* Table II — culture × expression counts */
static const int culture_expr[GCCFER_NUM_CULTURES][GCCFER_NUM_EXPRESSIONS] = {
	/* Caucasian */
	{ 1041, 299, 597, 1206, 986, 881, 791 },
	/* East Asian */
	{ 747, 257, 481, 890, 630, 908, 643 },
	/* South Asian */
	{ 844, 285, 303, 1288, 2326, 1109, 471 },
	/* African */
	{ 828, 437, 255, 2040, 1977, 1163, 251 }
};

static const float culture_pct[GCCFER_NUM_CULTURES] = {
	24.2f, 19.0f, 27.7f, 29.0f
};

static const int expr_totals[GCCFER_NUM_EXPRESSIONS] = {
	3460, 1278, 1636, 5424, 5919, 4061, 2156
};

static gccfer_culture_row_t dataset_rows[GCCFER_NUM_CULTURES];
static qboolean dataset_built = qfalse;

static void Gccfer_BuildDatasetRows( void )
{
	int c;

	if ( dataset_built ) {
		return;
	}

	for ( c = 0; c < GCCFER_NUM_CULTURES; c++ ) {
		dataset_rows[c].by_expr.angry = culture_expr[c][GCCFER_EXPR_ANGRY];
		dataset_rows[c].by_expr.disgust = culture_expr[c][GCCFER_EXPR_DISGUST];
		dataset_rows[c].by_expr.fear = culture_expr[c][GCCFER_EXPR_FEAR];
		dataset_rows[c].by_expr.happy = culture_expr[c][GCCFER_EXPR_HAPPY];
		dataset_rows[c].by_expr.neutral = culture_expr[c][GCCFER_EXPR_NEUTRAL];
		dataset_rows[c].by_expr.sad = culture_expr[c][GCCFER_EXPR_SAD];
		dataset_rows[c].by_expr.surprise = culture_expr[c][GCCFER_EXPR_SURPRISE];
		dataset_rows[c].by_expr.total =
			dataset_rows[c].by_expr.angry + dataset_rows[c].by_expr.disgust +
			dataset_rows[c].by_expr.fear + dataset_rows[c].by_expr.happy +
			dataset_rows[c].by_expr.neutral + dataset_rows[c].by_expr.sad +
			dataset_rows[c].by_expr.surprise;
		dataset_rows[c].pct_of_total = culture_pct[c];
	}
	dataset_built = qtrue;
}

const char *Gccfer_ExpressionName( gccfer_expression_t expr )
{
	switch ( expr ) {
	case GCCFER_EXPR_ANGRY: return "angry";
	case GCCFER_EXPR_DISGUST: return "disgust";
	case GCCFER_EXPR_FEAR: return "fear";
	case GCCFER_EXPR_HAPPY: return "happy";
	case GCCFER_EXPR_NEUTRAL: return "neutral";
	case GCCFER_EXPR_SAD: return "sad";
	case GCCFER_EXPR_SURPRISE: return "surprise";
	default: return "unknown";
	}
}

const char *Gccfer_CultureName( gccfer_culture_t culture )
{
	switch ( culture ) {
	case GCCFER_CULTURE_CAUCASIAN: return "caucasian";
	case GCCFER_CULTURE_EAST_ASIAN: return "east_asian";
	case GCCFER_CULTURE_SOUTH_ASIAN: return "south_asian";
	case GCCFER_CULTURE_AFRICAN: return "african";
	case GCCFER_CULTURE_GLOBAL: return "global";
	default: return "unknown";
	}
}

int Gccfer_TotalSamples( void )
{
	return 23934;
}

const gccfer_culture_row_t *Gccfer_DatasetTable( void )
{
	Gccfer_BuildDatasetRows();
	return dataset_rows;
}

const gccfer_expr_counts_t *Gccfer_ExpressionTotals( void )
{
	static gccfer_expr_counts_t totals;
	static qboolean init = qfalse;
	int i;

	if ( !init ) {
		memset( &totals, 0, sizeof( totals ) );
		for ( i = 0; i < GCCFER_NUM_EXPRESSIONS; i++ ) {
			switch ( (gccfer_expression_t)i ) {
			case GCCFER_EXPR_ANGRY: totals.angry = expr_totals[i]; break;
			case GCCFER_EXPR_DISGUST: totals.disgust = expr_totals[i]; break;
			case GCCFER_EXPR_FEAR: totals.fear = expr_totals[i]; break;
			case GCCFER_EXPR_HAPPY: totals.happy = expr_totals[i]; break;
			case GCCFER_EXPR_NEUTRAL: totals.neutral = expr_totals[i]; break;
			case GCCFER_EXPR_SAD: totals.sad = expr_totals[i]; break;
			case GCCFER_EXPR_SURPRISE: totals.surprise = expr_totals[i]; break;
			default: break;
			}
		}
		totals.total = Gccfer_TotalSamples();
		init = qtrue;
	}
	return &totals;
}

int Gccfer_CountFor( gccfer_culture_t culture, gccfer_expression_t expr )
{
	if ( culture < 0 || culture >= GCCFER_NUM_CULTURES ||
		expr < 0 || expr >= GCCFER_NUM_EXPRESSIONS ) {
		return 0;
	}
	Gccfer_BuildDatasetRows();
	return ( (const int *)culture_expr )[culture * GCCFER_NUM_EXPRESSIONS + expr];
}
