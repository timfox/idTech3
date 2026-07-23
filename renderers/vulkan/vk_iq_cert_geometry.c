/*
===========================================================================
Phase 1.5 — IQ certification fixture scenarios (ROI definitions).
===========================================================================
*/

#include "tr_local.h"
#include "vk_iq_cert_geometry.h"

#ifdef USE_VULKAN

static iqCertScenario_t s_scenario;
static qboolean s_armed;
static qboolean s_cmds;

void vk_iq_cert_geometry_clear( void )
{
	Com_Memset( &s_scenario, 0, sizeof( s_scenario ) );
	s_armed = qfalse;
}

void vk_iq_cert_geometry_arm( const iqCertScenario_t *scenario )
{
	if ( !scenario ) {
		vk_iq_cert_geometry_clear();
		return;
	}
	s_scenario = *scenario;
	s_scenario.armedThisFrame = qtrue;
	s_armed = qtrue;
}

qboolean vk_iq_cert_geometry_armed( void )
{
	return s_armed;
}

const iqCertScenario_t *vk_iq_cert_geometry_scenario( void )
{
	return s_armed ? &s_scenario : NULL;
}

static void IQ_AddRoi( iqCertScenario_t *out, float u0, float v0, float u1, float v1,
	float luma, float edgeW, float vel, const char *label )
{
	iqCertRoi_t *r;
	if ( !out || out->roiCount >= IQ_CERT_MAX_ROIS ) {
		return;
	}
	r = &out->rois[out->roiCount++];
	r->u0 = u0; r->v0 = v0; r->u1 = u1; r->v1 = v1;
	r->expectLuma = luma;
	r->expectEdgeWidthPx = edgeW;
	r->expectVelocityMag = vel;
	Q_strncpyz( r->label, label ? label : "", sizeof( r->label ) );
}

void vk_iq_cert_geometry_make_firefly( iqCertScenario_t *out )
{
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, "firefly_spike_vs_sheet", sizeof( out->name ) );
	out->fixture = IQ_CERT_FIXTURE_FIREFLY;
	out->seed = 0xF1F1u;
	out->fireflySpikeLuma = 64.0f;
	out->fireflySheetLuma = 2.0f;
	IQ_AddRoi( out, 0.45f, 0.45f, 0.55f, 0.55f, 64.0f, 0.0f, 0.0f, "spike" );
	IQ_AddRoi( out, 0.10f, 0.10f, 0.40f, 0.40f, 2.0f, 0.0f, 0.0f, "sheet" );
}

void vk_iq_cert_geometry_make_edge_vert( iqCertScenario_t *out )
{
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, "edge_vertical_1px", sizeof( out->name ) );
	out->fixture = IQ_CERT_FIXTURE_EDGE_VERT;
	out->seed = 0xE001u;
	out->edgeContrast = 0.9f;
	IQ_AddRoi( out, 0.48f, 0.20f, 0.52f, 0.80f, 0.5f, 1.0f, 0.0f, "vert_edge" );
}

void vk_iq_cert_geometry_make_edge_diag( iqCertScenario_t *out )
{
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, "edge_diagonal", sizeof( out->name ) );
	out->fixture = IQ_CERT_FIXTURE_EDGE_DIAG;
	out->seed = 0xE002u;
	out->edgeContrast = 0.85f;
	IQ_AddRoi( out, 0.30f, 0.30f, 0.70f, 0.70f, 0.5f, 1.5f, 0.0f, "diag_edge" );
}

void vk_iq_cert_geometry_make_roughness_ladder( iqCertScenario_t *out )
{
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, "roughness_ladder", sizeof( out->name ) );
	out->fixture = IQ_CERT_FIXTURE_ROUGHNESS_LADDER;
	out->seed = 0xA011u;
	IQ_AddRoi( out, 0.05f, 0.40f, 0.95f, 0.60f, 0.0f, 0.0f, 0.0f, "rough_ramp" );
}

void vk_iq_cert_geometry_make_motion_stripe( iqCertScenario_t *out )
{
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, "motion_stripe", sizeof( out->name ) );
	out->fixture = IQ_CERT_FIXTURE_MOTION_STRIPE;
	out->seed = 0xB071u;
	IQ_AddRoi( out, 0.20f, 0.45f, 0.80f, 0.55f, 0.0f, 0.0f, 8.0f, "motion" );
}

void vk_iq_cert_geometry_make_gbuffer_ramps( iqCertScenario_t *out )
{
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, "gbuffer_quant_ramps", sizeof( out->name ) );
	out->fixture = IQ_CERT_FIXTURE_GBUFFER_RAMPS;
	out->seed = 0xCB01u;
	IQ_AddRoi( out, 0.05f, 0.05f, 0.95f, 0.25f, 0.0f, 0.0f, 0.0f, "normal_ramp" );
	IQ_AddRoi( out, 0.05f, 0.30f, 0.95f, 0.50f, 0.0f, 0.0f, 0.0f, "rough_ramp" );
}

static void IQ_Geom_Status_f( void )
{
	const iqCertScenario_t *sc = vk_iq_cert_geometry_scenario();
	if ( !sc ) {
		ri.Printf( PRINT_ALL, "iq_cert_geometry: not armed\n" );
		return;
	}
	ri.Printf( PRINT_ALL, "iq_cert_geometry: %s fixture=%d rois=%u seed=0x%x\n",
		sc->name, (int)sc->fixture, sc->roiCount, sc->seed );
}

void vk_iq_cert_geometry_register( void )
{
	if ( s_cmds ) {
		return;
	}
	s_cmds = qtrue;
	vk_iq_cert_geometry_clear();
	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "iq_cert_geometry_status", IQ_Geom_Status_f );
	}
}

#endif /* USE_VULKAN */
