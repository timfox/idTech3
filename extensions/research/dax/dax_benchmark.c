/*
===========================================================================
DaX benchboard statistics and task taxonomy (Table 1, Figure 1).
===========================================================================
*/

#include "dax/dax.h"

static const dax_category_row_t category_table[DAX_NUM_LEVEL2_CATEGORIES] = {
	{ DAX_DOMAIN_DIAGNOSTIC, DAX_CAT_DISEASE_ENTITY, 16 },
	{ DAX_DOMAIN_DIAGNOSTIC, DAX_CAT_HISTOLOGIC_GRADING, 13 },
	{ DAX_DOMAIN_DIAGNOSTIC, DAX_CAT_ANATOMIC_STAGING, 26 },
	{ DAX_DOMAIN_BIOMARKER, DAX_CAT_GENOMIC_ALTERATIONS, 49 },
	{ DAX_DOMAIN_BIOMARKER, DAX_CAT_IHC_RECEPTOR, 7 },
	{ DAX_DOMAIN_BIOMARKER, DAX_CAT_COMPOSITE_IMMUNE, 17 },
	{ DAX_DOMAIN_SPECIMEN, DAX_CAT_TISSUE_ORIGIN, 9 },
	{ DAX_DOMAIN_PROGNOSIS, DAX_CAT_TREATMENT_RESPONSE, 8 },
	{ DAX_DOMAIN_PROGNOSIS, DAX_CAT_SURVIVAL_OUTCOMES, 16 },
};

const char *Dax_DomainName( dax_domain_t domain )
{
	switch ( domain ) {
	case DAX_DOMAIN_DIAGNOSTIC: return "diagnostic_pathology";
	case DAX_DOMAIN_BIOMARKER: return "biomarker_molecular";
	case DAX_DOMAIN_SPECIMEN: return "tissue_specimen_context";
	case DAX_DOMAIN_PROGNOSIS: return "risk_response_prognosis";
	default: return "unknown";
	}
}

const char *Dax_CategoryName( dax_category_t cat )
{
	switch ( cat ) {
	case DAX_CAT_DISEASE_ENTITY: return "disease_entity_histologic_diagnosis";
	case DAX_CAT_HISTOLOGIC_GRADING: return "histologic_grading_dysplasia";
	case DAX_CAT_ANATOMIC_STAGING: return "anatomic_staging_invasion";
	case DAX_CAT_GENOMIC_ALTERATIONS: return "genomic_epigenetic_pathway";
	case DAX_CAT_IHC_RECEPTOR: return "single_marker_ihc";
	case DAX_CAT_COMPOSITE_IMMUNE: return "composite_molecular_immune";
	case DAX_CAT_TISSUE_ORIGIN: return "tissue_origin_sampling";
	case DAX_CAT_TREATMENT_RESPONSE: return "treatment_response_residual";
	case DAX_CAT_SURVIVAL_OUTCOMES: return "survival_recurrence_progression";
	default: return "unknown";
	}
}

dax_benchmark_stats_t Dax_BenchmarkStats( void )
{
	dax_benchmark_stats_t s;
	s.num_tasks = DAX_NUM_BENCHMARK_TASKS;
	s.num_datasets = DAX_NUM_BENCHMARK_DATASETS;
	s.num_patients = 28182;
	s.num_slides = 34394;
	s.pretrain_wsis = 104569;
	return s;
}

const dax_category_row_t *Dax_CategoryTable( int *count )
{
	if ( count ) {
		*count = DAX_NUM_LEVEL2_CATEGORIES;
	}
	return category_table;
}

float Dax_AnchorMagnification( int index )
{
	static const float mags[DAX_NUM_ANCHOR_MAGS] = { 2.5f, 5.0f, 10.0f, 20.0f };
	if ( index < 0 || index >= DAX_NUM_ANCHOR_MAGS ) {
		return 20.0f;
	}
	return mags[index];
}

void Dax_Stage2CropPair( int index, int *global_px, int *local_px )
{
	static const int pairs[3][2] = {
		{ 512, 224 },
		{ 384, 168 },
		{ 768, 336 },
	};
	if ( index < 0 || index >= 3 ) {
		index = 0;
	}
	if ( global_px ) {
		*global_px = pairs[index][0];
	}
	if ( local_px ) {
		*local_px = pairs[index][1];
	}
}
