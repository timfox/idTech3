/*
===========================================================================
Phase 2.6A — certification metrics.
===========================================================================
*/

#include "tr_local.h"
#include "vk_cert_metrics.h"
#include "vk_transparency_lab.h"

#include <math.h>

void vk_cert_metrics_clear( certMetrics_t *m )
{
	if ( m ) {
		Com_Memset( m, 0, sizeof( *m ) );
		m->weightMin = 1e30;
		m->weightMax = -1e30;
	}
}

float vk_cert_metrics_luminance( float r, float g, float b )
{
	return vk_transparency_lab_relative_luminance( r, g, b );
}

static qboolean cert_finite3( float r, float g, float b, certMetrics_t *m )
{
	if ( !isfinite( r ) || !isfinite( g ) || !isfinite( b ) ) {
		if ( ( isnan( r ) || isnan( g ) || isnan( b ) ) ) {
			m->nanCount++;
		} else {
			m->infCount++;
		}
		return qfalse;
	}
	return qtrue;
}

void vk_cert_metrics_compare_rgba( const float *a, const float *b, uint32_t w, uint32_t h,
	const uint8_t *mask, certMetrics_t *out )
{
	uint32_t i, n;
	double sumAbs = 0.0, sumSq = 0.0, sumHue = 0.0, sumRel = 0.0;
	double maxAbs = 0.0, maxHue = 0.0, maxRel = 0.0;

	vk_cert_metrics_clear( out );
	if ( !a || !b || !out || w == 0 || h == 0 ) {
		return;
	}
	n = w * h;
	out->pixelCount = n;
	for ( i = 0; i < n; i++ ) {
		const float *pa = a + i * 4;
		const float *pb = b + i * 4;
		float dr, dg, db, absErr, la, lb, rel, hue;
		if ( mask && !mask[i] ) {
			continue;
		}
		if ( !cert_finite3( pa[0], pa[1], pa[2], out ) || !cert_finite3( pb[0], pb[1], pb[2], out ) ) {
			continue;
		}
		out->validPixelCount++;
		dr = pa[0] - pb[0];
		dg = pa[1] - pb[1];
		db = pa[2] - pb[2];
		absErr = fmaxf( fabsf( dr ), fmaxf( fabsf( dg ), fabsf( db ) ) );
		sumAbs += absErr;
		sumSq += (double)dr * dr + (double)dg * dg + (double)db * db;
		if ( absErr > maxAbs ) {
			maxAbs = absErr;
		}
		la = vk_cert_metrics_luminance( pa[0], pa[1], pa[2] );
		lb = vk_cert_metrics_luminance( pb[0], pb[1], pb[2] );
		rel = fabsf( la - lb ) / fmaxf( 1e-6f, fmaxf( fabsf( la ), fabsf( lb ) ) );
		sumRel += rel;
		if ( rel > maxRel ) {
			maxRel = rel;
		}
		hue = vk_transparency_lab_hue_error( pa[0], pa[1], pa[2], pb[0], pb[1], pb[2] );
		sumHue += hue;
		if ( hue > maxHue ) {
			maxHue = hue;
		}
	}
	if ( out->validPixelCount > 0 ) {
		out->meanAbsRgb = sumAbs / out->validPixelCount;
		out->maxAbsRgb = maxAbs;
		out->rmse = sqrt( sumSq / ( out->validPixelCount * 3.0 ) );
		out->meanRelLum = sumRel / out->validPixelCount;
		out->maxRelLum = maxRel;
		out->meanHue = sumHue / out->validPixelCount;
		out->maxHue = maxHue;
	}
}

void vk_cert_metrics_empty_pixels( const float *fogSceneRgba, const float *accumRgba,
	const float *revealR, const float *resolvedRgba, uint32_t w, uint32_t h,
	float eps, certMetrics_t *out )
{
	uint32_t i, n;
	double sumErr = 0.0, maxErr = 0.0;
	uint32_t empty = 0, modified = 0;

	vk_cert_metrics_clear( out );
	if ( !fogSceneRgba || !accumRgba || !revealR || !resolvedRgba || !out ) {
		return;
	}
	n = w * h;
	out->pixelCount = n;
	for ( i = 0; i < n; i++ ) {
		float weight = accumRgba[i * 4 + 3];
		float reveal = revealR[i];
		const float *fog = fogSceneRgba + i * 4;
		const float *res = resolvedRgba + i * 4;
		float err;
		if ( !cert_finite3( fog[0], fog[1], fog[2], out ) || !cert_finite3( res[0], res[1], res[2], out ) ) {
			continue;
		}
		out->validPixelCount++;
		if ( !( weight <= 1e-6f && reveal >= ( 1.0f - 1e-4f ) ) ) {
			continue;
		}
		empty++;
		err = fmaxf( fabsf( res[0] - fog[0] ), fmaxf( fabsf( res[1] - fog[1] ), fabsf( res[2] - fog[2] ) ) );
		sumErr += err;
		if ( err > maxErr ) {
			maxErr = err;
		}
		if ( err > eps ) {
			modified++;
		}
	}
	out->modifiedEmptyPixels = modified;
	out->maxEmptyPixelError = maxErr;
	out->meanEmptyPixelError = empty ? ( sumErr / empty ) : 0.0;
	/* stash empty count in blackFrameCorruption unused slot? use weightInvalid as empty count */
	out->weightInvalid = empty;
}

void vk_cert_metrics_revealage( const float *gpuReveal, const float *expected, uint32_t count,
	certMetrics_t *out )
{
	uint32_t i;
	double sum = 0.0, maxE = 0.0;
	vk_cert_metrics_clear( out );
	if ( !gpuReveal || !expected || !out ) {
		return;
	}
	out->pixelCount = count;
	for ( i = 0; i < count; i++ ) {
		float e = fabsf( gpuReveal[i] - expected[i] );
		if ( !isfinite( gpuReveal[i] ) || !isfinite( expected[i] ) ) {
			out->nanCount++;
			continue;
		}
		out->validPixelCount++;
		sum += e;
		if ( e > maxE ) {
			maxE = e;
		}
	}
	out->revealageError = out->validPixelCount ? ( sum / out->validPixelCount ) : 0.0;
	out->maxAbsRgb = maxE;
}

void vk_cert_metrics_weights( const float *weights, uint32_t count,
	float minW, float maxW, certMetrics_t *out )
{
	uint32_t i;
	double sum = 0.0;
	vk_cert_metrics_clear( out );
	if ( !weights || !out ) {
		return;
	}
	out->pixelCount = count;
	out->weightMin = 1e30;
	out->weightMax = -1e30;
	for ( i = 0; i < count; i++ ) {
		float w = weights[i];
		if ( !isfinite( w ) || w < 0.0f ) {
			out->weightInvalid++;
			continue;
		}
		out->validPixelCount++;
		sum += w;
		if ( w < out->weightMin ) {
			out->weightMin = w;
		}
		if ( w > out->weightMax ) {
			out->weightMax = w;
		}
		if ( w <= minW + 1e-6f ) {
			out->weightLowClamps++;
		}
		if ( w >= maxW - 1e-6f ) {
			out->weightHighClamps++;
		}
	}
	out->weightMean = out->validPixelCount ? ( sum / out->validPixelCount ) : 0.0;
}
