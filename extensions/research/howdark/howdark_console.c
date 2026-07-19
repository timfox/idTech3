/*
===========================================================================
How Dark is Dark — console commands (Filip & Vávra arXiv:2601.05094).
===========================================================================
*/

#include "howdark/howdark.h"

#include "qcommon/qcommon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cvar_t *cl_howdark_enable;
static qboolean howdark_console_registered = qfalse;

static qboolean HowDark_Enabled( void )
{
	if ( !cl_howdark_enable || !cl_howdark_enable->integer ) {
		Com_Printf( "[howdark] disabled (cl_howdark_enable 0)\n" );
		return qfalse;
	}
	return qtrue;
}

static void HowDark_Cmd_Paper_f( void )
{
	if ( !HowDark_Enabled() ) {
		return;
	}
	Com_Printf( "[howdark] Filip & Vávra — How Dark is Dark? arXiv:2601.05094\n" );
	Com_Printf( "[howdark] Dense isotropic BRDF of black materials: Vantablack, Musou,\n" );
	Com_Printf( "[howdark] velvet/fabrics, and matte coatings; TIS/THR + perceived darkness.\n" );
	Com_Printf( "[howdark] Conclusion: ultra-black/fabrics attenuate diffuse+specular across\n" );
	Com_Printf( "[howdark] angles; coatings show strong grazing specular rise. Scaffold uses\n" );
	Com_Printf( "[howdark] calibrated tables — not measured EXR BRDFs (see docs/HOWDARK.md).\n" );
}

static void HowDark_Cmd_Status_f( void )
{
	Com_Printf( "[howdark] cl_howdark_enable=%d materials=%d\n",
		cl_howdark_enable ? cl_howdark_enable->integer : 0,
		HowDark_MaterialCount() );
	Com_Printf( "[howdark] commands: howdark_paper howdark_list howdark_metrics\n" );
	Com_Printf( "[howdark]           howdark_rank howdark_compare howdark_advice howdark_status\n" );
}

static void HowDark_Cmd_List_f( void )
{
	int i;

	if ( !HowDark_Enabled() ) {
		return;
	}

	Com_Printf( "[howdark] id  name                 class                 albedo\n" );
	for ( i = 0; i < HowDark_MaterialCount(); i++ ) {
		const howdark_material_t *m = HowDark_GetMaterial( i );
		Com_Printf( "  %d  %-20s %-20s %.5f\n",
			m->id, m->name, m->className, m->albedo );
	}
}

static void HowDark_PrintMetrics( int id )
{
	const howdark_material_t *m = HowDark_GetMaterial( id );
	float p1, p50, p99;
	float angles[4] = { 0.0f, 45.0f, 75.0f, 85.0f };
	int a;

	if ( !m ) {
		return;
	}

	HowDark_LuminancePercentiles( id, &p1, &p50, &p99 );
	Com_Printf( "[howdark] %s (%s)\n", m->name, m->className );
	Com_Printf( "  albedo=%.5f  lum P1=%.5f P50=%.5f P99=%.5f  tisMean=%.3f\n",
		m->albedo, p1, p50, p99, m->tisMean );
	Com_Printf( "  theta_i   THR        TIS      Rs\n" );
	for ( a = 0; a < 4; a++ ) {
		float th = angles[a];
		Com_Printf( "  %5.0f   %.6f   %.3f   %.6f\n",
			th, HowDark_THR( id, th ), HowDark_TIS( id, th ), HowDark_Specular( id, th ) );
	}
	Com_Printf( "  perceived darkness: I1=%.0f I10=%.0f I100=%.0f\n",
		HowDark_PerceivedDarkness( id, 1 ),
		HowDark_PerceivedDarkness( id, 10 ),
		HowDark_PerceivedDarkness( id, 100 ) );
}

static void HowDark_Cmd_Metrics_f( void )
{
	int id;

	if ( !HowDark_Enabled() ) {
		return;
	}
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "usage: howdark_metrics <id|name>\n" );
		return;
	}
	id = HowDark_FindMaterial( Cmd_Argv( 1 ) );
	if ( id < 0 ) {
		Com_Printf( S_COLOR_YELLOW "[howdark] unknown material '%s'\n", Cmd_Argv( 1 ) );
		return;
	}
	HowDark_PrintMetrics( id );
}

static void HowDark_Cmd_Rank_f( void )
{
	int scale = 100;
	int ids[HOWDARK_MATERIAL_COUNT];
	int n, i;

	if ( !HowDark_Enabled() ) {
		return;
	}
	if ( Cmd_Argc() >= 2 ) {
		scale = atoi( Cmd_Argv( 1 ) );
	}
	if ( scale != 1 && scale != 10 && scale != 100 ) {
		Com_Printf( "usage: howdark_rank [1|10|100]\n" );
		return;
	}

	n = HowDark_RankByDarkness( scale, ids, HOWDARK_MATERIAL_COUNT );
	Com_Printf( "[howdark] perceived darkness ranking (intensity %d, Fig. 8):\n", scale );
	for ( i = 0; i < n; i++ ) {
		const howdark_material_t *m = HowDark_GetMaterial( ids[i] );
		Com_Printf( "  #%d  %-20s  score=%.0f\n",
			i + 1, m->name, HowDark_PerceivedDarkness( ids[i], scale ) );
	}
}

static void HowDark_Cmd_Compare_f( void )
{
	int a, b;

	if ( !HowDark_Enabled() ) {
		return;
	}
	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "usage: howdark_compare <a> <b>\n" );
		return;
	}
	a = HowDark_FindMaterial( Cmd_Argv( 1 ) );
	b = HowDark_FindMaterial( Cmd_Argv( 2 ) );
	if ( a < 0 || b < 0 ) {
		Com_Printf( S_COLOR_YELLOW "[howdark] unknown material id/name\n" );
		return;
	}
	Com_Printf( "[howdark] --- A ---\n" );
	HowDark_PrintMetrics( a );
	Com_Printf( "[howdark] --- B ---\n" );
	HowDark_PrintMetrics( b );
}

static void HowDark_Cmd_Advice_f( void )
{
	const char *useCase = "optical";

	if ( !HowDark_Enabled() ) {
		return;
	}
	if ( Cmd_Argc() >= 2 ) {
		useCase = Cmd_Argv( 1 );
	}
	Com_Printf( "[howdark] advice (%s): %s\n", useCase, HowDark_SelectAdvice( useCase ) );
}

void HowDark_ConsoleInit( void )
{
	if ( howdark_console_registered ) {
		return;
	}

	cl_howdark_enable = Cvar_Get( "cl_howdark_enable", "0", CVAR_ARCHIVE );
	Cmd_AddCommand( "howdark_paper", HowDark_Cmd_Paper_f );
	Cmd_AddCommand( "howdark_list", HowDark_Cmd_List_f );
	Cmd_AddCommand( "howdark_metrics", HowDark_Cmd_Metrics_f );
	Cmd_AddCommand( "howdark_rank", HowDark_Cmd_Rank_f );
	Cmd_AddCommand( "howdark_compare", HowDark_Cmd_Compare_f );
	Cmd_AddCommand( "howdark_advice", HowDark_Cmd_Advice_f );
	Cmd_AddCommand( "howdark_status", HowDark_Cmd_Status_f );

	Com_Printf( "[howdark] Filip & Vávra black materials (cl_howdark_enable %d)\n",
		cl_howdark_enable->integer );
	howdark_console_registered = qtrue;
}

void HowDark_ConsoleShutdown( void )
{
	howdark_console_registered = qfalse;
}
