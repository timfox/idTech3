/*
===========================================================================
Phase 2.6B — deterministic WBOIT certification geometry fixtures.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_oit_cert_geometry.h"
#include "vk_wboit_production_cert.h"

#include <math.h>

static qboolean s_cmds;
static oitCertScenario_t s_armed;
static qboolean s_hasArmed;
static qboolean s_drawnThisFrame;
static shader_t *s_paneShader;
static shader_t *s_additiveShader;

static void OIT_Cert_EnsureShaders( void )
{
	qhandle_t h;
	if ( s_paneShader && s_additiveShader ) {
		return;
	}
	if ( !tr.whiteImage ) {
		return;
	}
	h = RE_RegisterShaderFromImage( "cert/wboit_pane", LIGHTMAP_NONE, tr.whiteImage, qtrue );
	s_paneShader = R_GetShaderByHandle( h );
	if ( s_paneShader && !s_paneShader->defaultShader && s_paneShader->stages[0] ) {
		s_paneShader->sort = SS_BLEND0;
		s_paneShader->stages[0]->stateBits =
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	h = RE_RegisterShaderFromImage( "cert/wboit_additive", LIGHTMAP_NONE, tr.whiteImage, qtrue );
	s_additiveShader = R_GetShaderByHandle( h );
	if ( s_additiveShader && !s_additiveShader->defaultShader && s_additiveShader->stages[0] ) {
		s_additiveShader->sort = SS_BLEND1;
		s_additiveShader->stages[0]->stateBits =
			GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
	}
}

void vk_oit_cert_geometry_clear( void )
{
	Com_Memset( &s_armed, 0, sizeof( s_armed ) );
	s_hasArmed = qfalse;
	s_drawnThisFrame = qfalse;
}

void vk_oit_cert_geometry_arm( const oitCertScenario_t *scenario )
{
	if ( !scenario ) {
		vk_oit_cert_geometry_clear();
		return;
	}
	s_armed = *scenario;
	s_hasArmed = qtrue;
	s_drawnThisFrame = qfalse;
	ri.Printf( PRINT_ALL, "[VK][OIT-cert] armed scenario '%s' panes=%u seed=%u\n",
		s_armed.name, s_armed.paneCount, s_armed.seed );
}

qboolean vk_oit_cert_geometry_armed( void )
{
	return s_hasArmed;
}

const oitCertScenario_t *vk_oit_cert_geometry_scenario( void )
{
	return s_hasArmed ? &s_armed : NULL;
}

qboolean vk_oit_cert_geometry_was_drawn( void )
{
	return s_drawnThisFrame;
}

void vk_oit_cert_geometry_expect_source_over( const float layerRgb[3], float opacity,
	const float fogRgb[3], float outRgb[3] )
{
	vk_wboit_cert_source_over( layerRgb, opacity, fogRgb, outRgb );
}

void vk_oit_cert_geometry_make_empty( oitCertScenario_t *out )
{
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, "empty_pixel", sizeof( out->name ) );
	out->seed = 1;
	out->expectEmpty = qtrue;
	out->expectRevealage = 1.0f;
}

void vk_oit_cert_geometry_make_single_layer( oitCertScenario_t *out, float opacity,
	const float color[3], float viewDepth )
{
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, "single_layer", sizeof( out->name ) );
	out->seed = 2;
	out->paneCount = 1;
	out->panes[0].color[0] = color[0];
	out->panes[0].color[1] = color[1];
	out->panes[0].color[2] = color[2];
	out->panes[0].opacity = opacity;
	out->panes[0].viewDepth = viewDepth;
	out->panes[0].halfWidth = 64.0f;
	out->panes[0].halfHeight = 64.0f;
	out->panes[0].blend = OIT_CERT_BLEND_ALPHA;
	out->panes[0].layerId = 0;
	out->expectSingleOpacity = opacity;
	out->expectSingleColor[0] = color[0];
	out->expectSingleColor[1] = color[1];
	out->expectSingleColor[2] = color[2];
	out->expectRevealage = 1.0f - opacity;
}

void vk_oit_cert_geometry_make_revealage_layers( oitCertScenario_t *out, const float *alphas, int count,
	float viewDepth )
{
	int i;
	float product = 1.0f;
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, "revealage_layers", sizeof( out->name ) );
	out->seed = 3;
	if ( count > OIT_CERT_MAX_PANES ) {
		count = OIT_CERT_MAX_PANES;
	}
	out->paneCount = (uint32_t)count;
	for ( i = 0; i < count; i++ ) {
		float a = alphas[i];
		if ( a < 0.0f ) {
			a = 0.0f;
		} else if ( a > 1.0f ) {
			a = 1.0f;
		}
		out->panes[i].color[0] = 0.2f + 0.1f * (float)( i % 3 );
		out->panes[i].color[1] = 0.3f;
		out->panes[i].color[2] = 0.8f - 0.05f * (float)i;
		out->panes[i].opacity = a;
		out->panes[i].viewDepth = viewDepth + (float)i * 0.5f;
		out->panes[i].halfWidth = 48.0f;
		out->panes[i].halfHeight = 48.0f;
		out->panes[i].blend = OIT_CERT_BLEND_ALPHA;
		out->panes[i].layerId = (uint32_t)i;
		product *= ( 1.0f - a );
	}
	out->expectRevealage = product;
}

void vk_oit_cert_geometry_make_weight_ladder( oitCertScenario_t *out )
{
	static const float opacities[] = { 0.05f, 0.15f, 0.35f, 0.55f, 0.85f };
	static const float depths[] = { 32.f, 128.f, 512.f, 1024.f, 2048.f };
	int i;
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, "weight_ladder", sizeof( out->name ) );
	out->seed = 4;
	out->paneCount = 5;
	for ( i = 0; i < 5; i++ ) {
		out->panes[i].color[0] = out->panes[i].color[1] = out->panes[i].color[2] = 0.5f;
		out->panes[i].opacity = opacities[i];
		out->panes[i].viewDepth = depths[i];
		out->panes[i].halfWidth = 24.0f;
		out->panes[i].halfHeight = 24.0f;
		out->panes[i].blend = OIT_CERT_BLEND_ALPHA;
		out->panes[i].layerId = (uint32_t)i;
	}
}

void vk_oit_cert_geometry_make_order_rgb( oitCertScenario_t *out, int permutation )
{
	/* Three RGB panes; permutation shuffles draw order 0..5. */
	static const int orders[6][3] = {
		{ 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 },
		{ 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 }
	};
	static const float colors[3][3] = {
		{ 1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f }
	};
	int i;
	const int *ord;
	if ( permutation < 0 ) {
		permutation = 0;
	}
	if ( permutation > 5 ) {
		permutation = 5;
	}
	ord = orders[permutation];
	Com_Memset( out, 0, sizeof( *out ) );
	Com_sprintf( out->name, sizeof( out->name ), "order_rgb_p%d", permutation );
	out->seed = 10u + (uint32_t)permutation;
	out->paneCount = 3;
	out->expectRevealage = ( 1.0f - 0.4f ) * ( 1.0f - 0.4f ) * ( 1.0f - 0.4f );
	for ( i = 0; i < 3; i++ ) {
		int src = ord[i];
		out->panes[i].color[0] = colors[src][0];
		out->panes[i].color[1] = colors[src][1];
		out->panes[i].color[2] = colors[src][2];
		out->panes[i].opacity = 0.4f;
		out->panes[i].viewDepth = 200.0f + (float)src * 2.0f; /* near-equal */
		out->panes[i].halfWidth = 40.0f;
		out->panes[i].halfHeight = 40.0f;
		out->panes[i].blend = OIT_CERT_BLEND_ALPHA;
		out->panes[i].layerId = (uint32_t)src;
	}
}

void vk_oit_cert_geometry_make_fog_depth_ladder( oitCertScenario_t *out )
{
	static const float depths[] = { 64.f, 256.f, 768.f, 1536.f };
	int i;
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, "fog_depth_ladder", sizeof( out->name ) );
	out->seed = 20;
	out->paneCount = 4;
	for ( i = 0; i < 4; i++ ) {
		out->panes[i].color[0] = 0.9f;
		out->panes[i].color[1] = 0.9f;
		out->panes[i].color[2] = 0.2f;
		out->panes[i].opacity = 0.35f;
		out->panes[i].viewDepth = depths[i];
		out->panes[i].halfWidth = 32.0f;
		out->panes[i].halfHeight = 32.0f;
		out->panes[i].blend = OIT_CERT_BLEND_ALPHA;
		out->panes[i].fogged = qtrue;
		out->panes[i].layerId = (uint32_t)i;
	}
	out->expectRevealage = powf( 1.0f - 0.35f, 4.0f );
}

void vk_oit_cert_geometry_make_additive_over_glass( oitCertScenario_t *out )
{
	Com_Memset( out, 0, sizeof( *out ) );
	Q_strncpyz( out->name, "additive_over_glass", sizeof( out->name ) );
	out->seed = 30;
	out->paneCount = 2;
	/* Glass */
	out->panes[0].color[0] = 0.6f;
	out->panes[0].color[1] = 0.8f;
	out->panes[0].color[2] = 1.0f;
	out->panes[0].opacity = 0.3f;
	out->panes[0].viewDepth = 180.0f;
	out->panes[0].halfWidth = 50.0f;
	out->panes[0].halfHeight = 50.0f;
	out->panes[0].blend = OIT_CERT_BLEND_ALPHA;
	out->panes[0].layerId = 0;
	/* Additive spark in front */
	out->panes[1].color[0] = 2.0f;
	out->panes[1].color[1] = 1.5f;
	out->panes[1].color[2] = 0.2f;
	out->panes[1].opacity = 0.0f; /* independent emission — additive path */
	out->panes[1].viewDepth = 160.0f;
	out->panes[1].halfWidth = 16.0f;
	out->panes[1].halfHeight = 16.0f;
	out->panes[1].blend = OIT_CERT_BLEND_ADDITIVE;
	out->panes[1].layerId = 1;
	out->expectRevealage = 1.0f - 0.3f; /* additive must not change revealage */
}

static void OIT_Cert_EmitPane( const oitCertPane_t *pane, shader_t *sh )
{
	vec3_t center, right, up, p[4];
	int i, base;
	byte r, g, b, a;
	const float *fwd = backEnd.viewParms.or.axis[0];
	const float *rt = backEnd.viewParms.or.axis[1];
	const float *upy = backEnd.viewParms.or.axis[2];

	if ( !sh || !pane ) {
		return;
	}
	VectorMA( backEnd.viewParms.or.origin, pane->viewDepth, fwd, center );
	VectorScale( rt, pane->halfWidth, right );
	VectorScale( upy, pane->halfHeight, up );

	VectorSubtract( center, right, p[0] ); VectorSubtract( p[0], up, p[0] );
	VectorAdd( center, right, p[1] ); VectorSubtract( p[1], up, p[1] );
	VectorAdd( center, right, p[2] ); VectorAdd( p[2], up, p[2] );
	VectorSubtract( center, right, p[3] ); VectorAdd( p[3], up, p[3] );

	/* Encode HDR-ish radiance into 8-bit vertex color with opacity in A.
	 * For HDR emissive additive, clamp channel to 255 (lab uses moderate values). */
	r = (byte)( Com_Clamp( 0.0f, 255.0f, pane->color[0] * 255.0f ) + 0.5f );
	g = (byte)( Com_Clamp( 0.0f, 255.0f, pane->color[1] * 255.0f ) + 0.5f );
	b = (byte)( Com_Clamp( 0.0f, 255.0f, pane->color[2] * 255.0f ) + 0.5f );
	if ( pane->blend == OIT_CERT_BLEND_ADDITIVE ) {
		a = 255;
	} else {
		a = (byte)( Com_Clamp( 0.0f, 255.0f, pane->opacity * 255.0f ) + 0.5f );
	}

	if ( tess.numVertexes + 4 >= SHADER_MAX_VERTEXES ||
		tess.numIndexes + 6 >= SHADER_MAX_INDEXES ) {
		RB_EndSurface();
		RB_BeginSurface( sh, 0 );
	}

	base = tess.numVertexes;
	for ( i = 0; i < 4; i++ ) {
		VectorCopy( p[i], tess.xyz[base + i] );
		VectorCopy( fwd, tess.normal[base + i] );
		VectorScale( tess.normal[base + i], -1.0f, tess.normal[base + i] );
		tess.texCoords[0][base + i][0] = ( i == 1 || i == 2 ) ? 1.0f : 0.0f;
		tess.texCoords[0][base + i][1] = ( i >= 2 ) ? 1.0f : 0.0f;
		tess.vertexColors[base + i].rgba[0] = r;
		tess.vertexColors[base + i].rgba[1] = g;
		tess.vertexColors[base + i].rgba[2] = b;
		tess.vertexColors[base + i].rgba[3] = a;
	}
	tess.indexes[tess.numIndexes + 0] = base + 0;
	tess.indexes[tess.numIndexes + 1] = base + 1;
	tess.indexes[tess.numIndexes + 2] = base + 2;
	tess.indexes[tess.numIndexes + 3] = base + 0;
	tess.indexes[tess.numIndexes + 4] = base + 2;
	tess.indexes[tess.numIndexes + 5] = base + 3;
	tess.numVertexes += 4;
	tess.numIndexes += 6;
}

void vk_oit_cert_geometry_draw_bucket( int bucketFilter )
{
	uint32_t i;
	shader_t *alphaSh;
	shader_t *addSh;
	qboolean drew = qfalse;

	if ( !s_hasArmed || !backEnd.oitAccumPass ) {
		return;
	}
	OIT_Cert_EnsureShaders();
	alphaSh = s_paneShader;
	addSh = s_additiveShader;
	if ( !alphaSh || alphaSh->defaultShader ) {
		ri.Printf( PRINT_WARNING, "[VK][OIT-cert] pane shader unavailable\n" );
		return;
	}

	/* Alpha bucket (or uncategorized). */
	if ( bucketFilter != 2 ) {
		qboolean any = qfalse;
		for ( i = 0; i < s_armed.paneCount; i++ ) {
			if ( s_armed.panes[i].blend == OIT_CERT_BLEND_ALPHA ) {
				any = qtrue;
				break;
			}
		}
		if ( any || s_armed.expectEmpty ) {
			RB_BeginSurface( alphaSh, 0 );
			for ( i = 0; i < s_armed.paneCount; i++ ) {
				if ( s_armed.panes[i].blend == OIT_CERT_BLEND_ALPHA ) {
					OIT_Cert_EmitPane( &s_armed.panes[i], alphaSh );
					drew = qtrue;
				}
			}
			RB_EndSurface();
		}
	}

	/* Additive bucket. */
	if ( bucketFilter == 2 || bucketFilter == 0 ) {
		qboolean any = qfalse;
		if ( !addSh || addSh->defaultShader ) {
			addSh = alphaSh;
		}
		for ( i = 0; i < s_armed.paneCount; i++ ) {
			if ( s_armed.panes[i].blend == OIT_CERT_BLEND_ADDITIVE ) {
				any = qtrue;
				break;
			}
		}
		if ( any && bucketFilter != 1 ) {
			RB_BeginSurface( addSh, 0 );
			for ( i = 0; i < s_armed.paneCount; i++ ) {
				if ( s_armed.panes[i].blend == OIT_CERT_BLEND_ADDITIVE ) {
					OIT_Cert_EmitPane( &s_armed.panes[i], addSh );
					drew = qtrue;
				}
			}
			RB_EndSurface();
		}
	}

	if ( drew || s_armed.expectEmpty ) {
		s_drawnThisFrame = qtrue;
		s_armed.drawnThisFrame = qtrue;
	}
}

static void OIT_CertGeom_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"oit_cert_geometry_status: armed=%d name=%s panes=%u drawn=%d\n"
		"  expectEmpty=%d expectRevealage=%g\n",
		s_hasArmed ? 1 : 0,
		s_hasArmed ? s_armed.name : "-",
		s_hasArmed ? s_armed.paneCount : 0,
		s_drawnThisFrame ? 1 : 0,
		s_armed.expectEmpty ? 1 : 0,
		s_armed.expectRevealage );
}

void vk_oit_cert_geometry_register( void )
{
	if ( s_cmds ) {
		return;
	}
	ri.Cmd_AddCommand( "oit_cert_geometry_status", OIT_CertGeom_Status_f );
	s_cmds = qtrue;
	ri.Printf( PRINT_ALL, "[VK][OIT-cert] Phase 2.6B geometry fixtures ready\n" );
}
