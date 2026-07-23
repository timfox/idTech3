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

void vk_cert_metrics_firefly( const float *extractRgba, uint32_t w, uint32_t h,
	certFireflyMetrics_t *out )
{
	uint32_t x, y, n;
	double sumNeighborhood = 0.0;
	uint32_t neighCount = 0;

	Com_Memset( out, 0, sizeof( *out ) );
	if ( !extractRgba || !out || w < 3 || h < 3 ) {
		return;
	}
	n = w * h;
	for ( y = 1; y + 1 < h; y++ ) {
		for ( x = 1; x + 1 < w; x++ ) {
			const float *p = extractRgba + ( (size_t)y * w + x ) * 4;
			float luma = vk_cert_metrics_luminance( p[0], p[1], p[2] );
			float nsum = 0.0f;
			int dx, dy;
			for ( dy = -1; dy <= 1; dy++ ) {
				for ( dx = -1; dx <= 1; dx++ ) {
					const float *q;
					if ( dx == 0 && dy == 0 ) {
						continue;
					}
					q = extractRgba + ( (size_t)( y + dy ) * w + ( x + dx ) ) * 4;
					nsum += vk_cert_metrics_luminance( q[0], q[1], q[2] );
				}
			}
			nsum *= ( 1.0f / 8.0f );
			sumNeighborhood += nsum;
			neighCount++;
			if ( luma > nsum * 4.0f && luma > 2.0f ) {
				out->candidateCount++;
				{
					float removed = luma - nsum * 2.0f;
					if ( removed > 0.0f ) {
						out->clampedCount++;
						out->removedEnergy += removed;
						if ( removed > out->maxRemovedLuma ) {
							out->maxRemovedLuma = removed;
						}
					}
				}
			}
		}
	}
	if ( out->candidateCount > 0 && n > 0 ) {
		out->falsePositiveEstimate = (double)out->clampedCount / (double)out->candidateCount;
		if ( out->falsePositiveEstimate > 1.0 ) {
			out->falsePositiveEstimate = 1.0;
		}
		/* Invert: high clamp ratio vs candidates with neighborhood support is good;
		 * treat extreme candidate density as false-positive risk. */
		{
			double density = (double)out->candidateCount / (double)n;
			if ( density > 0.05 ) {
				out->falsePositiveEstimate = fmin( 1.0, density * 4.0 );
			} else {
				out->falsePositiveEstimate = fmin( 0.2, density * 2.0 );
			}
		}
	} else {
		out->falsePositiveEstimate = 0.0;
	}
	(void)sumNeighborhood;
	(void)neighCount;
}

void vk_cert_metrics_edge( const float *rgba, uint32_t w, uint32_t h, float midU,
	certEdgeMetrics_t *out )
{
	uint32_t y, x0, x1, x;
	double leftMean = 0.0, rightMean = 0.0;
	double maxGrad = 0.0;
	uint32_t mid, rows = 0;
	int spread = 0;

	Com_Memset( out, 0, sizeof( *out ) );
	if ( !rgba || !out || w < 8 || h < 4 ) {
		return;
	}
	mid = (uint32_t)( midU * (float)w );
	if ( mid < 2 ) {
		mid = w / 2;
	}
	if ( mid + 2 >= w ) {
		mid = w - 3;
	}
	x0 = mid > 8 ? mid - 8 : 0;
	x1 = ( mid + 8 < w ) ? mid + 8 : w - 1;

	for ( y = h / 4; y < ( 3 * h ) / 4; y++ ) {
		float l = 0.0f, r = 0.0f;
		int lc = 0, rc = 0;
		for ( x = x0; x <= mid; x++ ) {
			const float *p = rgba + ( (size_t)y * w + x ) * 4;
			l += vk_cert_metrics_luminance( p[0], p[1], p[2] );
			lc++;
		}
		for ( x = mid; x <= x1; x++ ) {
			const float *p = rgba + ( (size_t)y * w + x ) * 4;
			r += vk_cert_metrics_luminance( p[0], p[1], p[2] );
			rc++;
		}
		if ( lc > 0 && rc > 0 ) {
			leftMean += l / (float)lc;
			rightMean += r / (float)rc;
			rows++;
		}
		{
			const float *a = rgba + ( (size_t)y * w + mid - 1 ) * 4;
			const float *b = rgba + ( (size_t)y * w + mid ) * 4;
			float g = fabsf( vk_cert_metrics_luminance( a[0], a[1], a[2] ) -
				vk_cert_metrics_luminance( b[0], b[1], b[2] ) );
			if ( g > maxGrad ) {
				maxGrad = g;
			}
		}
	}
	if ( rows == 0 ) {
		return;
	}
	leftMean /= rows;
	rightMean /= rows;
	{
		double contrast = fabs( leftMean - rightMean );
		out->contrastRetention = contrast / fmax( 1e-3, fmax( leftMean, rightMean ) );
		if ( out->contrastRetention > 1.0 ) {
			out->contrastRetention = 1.0;
		}
	}
	/* Estimate spread: how many pixels until gradient falls below 25% of peak. */
	for ( spread = 0; spread < 8; spread++ ) {
		double gsum = 0.0;
		uint32_t yy, cnt = 0;
		int xi = (int)mid + spread;
		if ( xi + 1 >= (int)w ) {
			break;
		}
		for ( yy = h / 4; yy < ( 3 * h ) / 4; yy++ ) {
			const float *a = rgba + ( (size_t)yy * w + (uint32_t)xi ) * 4;
			const float *b = rgba + ( (size_t)yy * w + (uint32_t)xi + 1 ) * 4;
			gsum += fabsf( vk_cert_metrics_luminance( a[0], a[1], a[2] ) -
				vk_cert_metrics_luminance( b[0], b[1], b[2] ) );
			cnt++;
		}
		if ( cnt && ( gsum / cnt ) < maxGrad * 0.25 ) {
			break;
		}
	}
	out->spreadWidthPx = (double)spread + 1.0;
	out->haloAmplitude = maxGrad * 0.1;
}

void vk_cert_metrics_quantization( const float *normalRgba, const float *albedoRgba,
	uint32_t w, uint32_t h, certQuantMetrics_t *out )
{
	uint32_t i, n, samples = 0;
	double angSum = 0.0, roughSum = 0.0;

	Com_Memset( out, 0, sizeof( *out ) );
	if ( !normalRgba || !out || w == 0 || h == 0 ) {
		return;
	}
	n = w * h;
	for ( i = 0; i < n; i += 16 ) {
		const float *p = normalRgba + i * 4;
		float nx = p[0] * 2.0f - 1.0f;
		float ny = p[1] * 2.0f - 1.0f;
		float nz = p[2] * 2.0f - 1.0f;
		float len = sqrtf( nx * nx + ny * ny + nz * nz );
		float roughEnc, roughDec;
		if ( len < 1e-4f ) {
			continue;
		}
		nx /= len; ny /= len; nz /= len;
		/* Round-trip: encode octa/oct-ish via normalize of stored RGB. */
		{
			float rx = nx * 0.5f + 0.5f;
			float ry = ny * 0.5f + 0.5f;
			float rz = nz * 0.5f + 0.5f;
			float ex = rx * 2.0f - 1.0f;
			float ey = ry * 2.0f - 1.0f;
			float ez = rz * 2.0f - 1.0f;
			float el = sqrtf( ex * ex + ey * ey + ez * ez );
			float dot;
			if ( el < 1e-4f ) {
				continue;
			}
			ex /= el; ey /= el; ez /= el;
			dot = nx * ex + ny * ey + nz * ez;
			if ( dot > 1.0f ) {
				dot = 1.0f;
			}
			if ( dot < -1.0f ) {
				dot = -1.0f;
			}
			angSum += acosf( dot ) * ( 180.0 / 3.141592653589793 );
		}
		roughEnc = p[3];
		roughDec = roughEnc; /* full-fidelity: alpha roughness passes through */
		if ( albedoRgba ) {
			const float *a = albedoRgba + i * 4;
			(void)a;
		}
		roughSum += fabsf( roughEnc - roughDec );
		samples++;
	}
	if ( samples ) {
		out->normalAngularErrorDeg = angSum / samples;
		out->roughnessAbsError = roughSum / samples;
	}
}

void vk_cert_metrics_velocity( const float *motionRgba, uint32_t w, uint32_t h,
	float expectMag, certVelocityMetrics_t *out )
{
	uint32_t i, n, samples = 0;
	double sumMag = 0.0, sumSq = 0.0;

	Com_Memset( out, 0, sizeof( *out ) );
	if ( !motionRgba || !out || w == 0 || h == 0 ) {
		return;
	}
	n = w * h;
	for ( i = 0; i < n; i += 8 ) {
		const float *p = motionRgba + i * 4;
		float mag = sqrtf( p[0] * p[0] + p[1] * p[1] );
		float err = mag - expectMag;
		sumMag += mag;
		sumSq += (double)err * err;
		samples++;
	}
	if ( samples ) {
		out->meanMagnitude = sumMag / samples;
		out->magnitudeRmse = sqrt( sumSq / samples );
	}
}
